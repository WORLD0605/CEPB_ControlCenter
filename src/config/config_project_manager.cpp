#include "config/config_project_manager.h"
#include "config/config_project_manager_p.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QTextStream>
#include <QUuid>

#include <type_traits>

namespace configtool {

namespace detail {

SourceInfo makeSourceInfo(const QString &filePath)
{
    QFileInfo info(filePath);

    SourceInfo source;
    source.filePath = info.filePath();
    source.fileName = info.fileName();
    return source;
}

PointSignalType signalTypeForService(ModelServiceType type)
{
    switch (type) {
    case ModelServiceType::Measurement:
        return PointSignalType::Yc;
    case ModelServiceType::Status:
        return PointSignalType::Yx;
    case ModelServiceType::Control:
        return PointSignalType::Ctrl;
    }

    return PointSignalType::Yc;
}

ModelServiceType serviceTypeFromIndex(int index)
{
    switch (index) {
    case 0:
        return ModelServiceType::Measurement;
    case 1:
        return ModelServiceType::Status;
    case 2:
        return ModelServiceType::Control;
    default:
        return ModelServiceType::Measurement;
    }
}

ModelServiceType inferServiceType(const QJsonObject &pointObject)
{
    const QString doType = pointObject.value(QStringLiteral("DOtype")).toString().trimmed();
    const QString doName = pointObject.value(QStringLiteral("DOname")).toString().trimmed();
    const QString dataType = pointObject.value(QStringLiteral("datatype")).toString().trimmed();

    const QString upperDoType = doType.toUpper();
    const QString lowerDoName = doName.toLower();
    const QString lowerDataType = dataType.toLower();

    if (upperDoType.contains(QStringLiteral("CO"))
        || upperDoType.contains(QStringLiteral("SPC"))
        || upperDoType.contains(QStringLiteral("DPC"))
        || lowerDoName.contains(QStringLiteral("ctl"))
        || lowerDoName.contains(QStringLiteral("set"))) {
        return ModelServiceType::Control;
    }

    if (upperDoType.contains(QStringLiteral("SPS"))
        || upperDoType.contains(QStringLiteral("DPS"))
        || upperDoType.contains(QStringLiteral("INS"))
        || lowerDataType == QStringLiteral("bool")
        || lowerDataType == QStringLiteral("boolean")) {
        return ModelServiceType::Status;
    }

    return ModelServiceType::Measurement;
}

PointTemplate parsePointTemplate(const QJsonObject &pointObject,
                                 ModelServiceType serviceType,
                                 const SourceInfo &source)
{
    PointTemplate point;
    point.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    point.category = serviceType;
    point.signalType = signalTypeForService(serviceType);
    point.name = pointObject.value(QStringLiteral("DOname")).toString();
    point.description = pointObject.value(QStringLiteral("description")).toString();
    point.ldName = pointObject.value(QStringLiteral("LDname")).toString();
    point.lnType = pointObject.value(QStringLiteral("LNtype")).toString();
    point.lnInst = pointObject.value(QStringLiteral("LNinst")).toString();
    point.doName = pointObject.value(QStringLiteral("DOname")).toString();
    point.doType = pointObject.value(QStringLiteral("DOtype")).toString();
    point.dataType = pointObject.value(QStringLiteral("datatype")).toString();
    point.unit = pointObject.value(QStringLiteral("unit")).toString();
    point.deadZoneType = pointObject.value(QStringLiteral("deadzonetype")).toString();
    point.deadZoneValue = pointObject.value(QStringLiteral("deadzoneval")).toString();
    point.min = pointObject.value(QStringLiteral("min")).toString();
    point.max = pointObject.value(QStringLiteral("max")).toString();
    point.step = pointObject.value(QStringLiteral("step")).toString();
    point.maxLength = pointObject.value(QStringLiteral("maxlength")).toString();
    point.source = source;

    if (serviceType == ModelServiceType::Control) {
        const QString lowerDataType = point.dataType.toLower();
        point.controlKind = lowerDataType == QStringLiteral("boolean")
            || lowerDataType == QStringLiteral("dbool")
            ? ControlKind::RemoteControl
            : ControlKind::RemoteAdjust;
    }

    return point;
}

bool loadJsonDocument(const QString &filePath,
                      QJsonDocument &document,
                      QString &errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("无法打开文件");
        return false;
    }

    const QByteArray payload = file.readAll();
    file.close();

    QJsonParseError parseError;
    document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorMessage = QStringLiteral("JSON 解析失败: %1").arg(parseError.errorString());
        return false;
    }

    return true;
}

QString safeFileSegment(const QString &value,
                       const QString &fallback)
{
    QString result = value.trimmed();
    if (result.isEmpty()) {
        result = fallback.trimmed();
    }

    result.replace(QRegularExpression(QStringLiteral("[\\/:*?\"<>|]+")), QStringLiteral("_"));
    result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    result.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    result.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return result.isEmpty() ? QStringLiteral("unnamed") : result;
}

QString jsonValueToString(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        const int integer = value.toInt();
        return qFuzzyCompare(number + 1.0, static_cast<double>(integer) + 1.0)
            ? QString::number(integer)
            : QString::number(number);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return QString();
}

int parseModbusLinePart(const QString &line, int sectionIndex)
{
    return line.section(QLatin1Char('_'), sectionIndex, sectionIndex).trimmed().toInt(nullptr, 0);
}

bool isCompleteModbusLine(const QString &line, int expectedParts)
{
    if (line.trimmed().isEmpty()) {
        return false;
    }

    return line.split(QLatin1Char('_'), Qt::KeepEmptyParts).size() >= expectedParts + 1;
}

QJsonObject qSettingsGroupRawExtra(QSettings &settings,
                                   const QSet<QString> &knownKeys)
{
    QJsonObject rawExtra;
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        if (knownKeys.contains(key)) {
            continue;
        }
        rawExtra.insert(key, settings.value(key).toString());
    }
    return rawExtra;
}

bool modbusBindingMatchesPollGroup(const PointBinding &binding,
                                   const ModbusPollGroup &group)
{
    bool ok = false;
    const uint dataIndex = binding.address.toUInt(&ok, 0);
    return ok && static_cast<int>(dataIndex >> 16) == group.groupNo;
}

bool modbusBindingMatchesSetPoint(const PointBinding &binding,
                                  const ModbusSetPoint &setPoint)
{
    bool ok = false;
    const uint dataIndex = binding.address.toUInt(&ok, 0);
    if (!ok) {
        return false;
    }
    return static_cast<int>(dataIndex >> 16) == setPoint.groupNo
        && static_cast<int>(dataIndex & 0xffff) == setPoint.entryNo;
}

void annotateModbusBinding(PointBinding &binding,
                            const ModbusDeviceConfig &modbus)
{
    bool ok = false;
    const uint dataIndex = binding.address.toUInt(&ok, 0);
    if (!ok) {
        return;
    }

    const int groupNo = static_cast<int>(dataIndex >> 16);
    const int entryNo = static_cast<int>(dataIndex & 0xffff);
    binding.extensions.insert(QStringLiteral("modbusGroupNo"), groupNo);
    binding.extensions.insert(QStringLiteral("modbusEntryNo"), entryNo);

    for (const ModbusSetPoint &setPoint : modbus.setPoints) {
        if (modbusBindingMatchesSetPoint(binding, setPoint)) {
            binding.extensions.insert(QStringLiteral("modbusKind"), modbusPointKindId(setPoint.kind));
            binding.extensions.insert(QStringLiteral("modbusFunctionCode"), setPoint.funCode);
            binding.extensions.insert(QStringLiteral("modbusRegisterAddress"), setPoint.regAddr);
            binding.extensions.insert(QStringLiteral("modbusDataType"), setPoint.dataType);
            binding.extensions.insert(QStringLiteral("modbusScale"), setPoint.scale);
            return;
        }
    }

    for (const ModbusPollGroup &group : modbus.pollGroups) {
        if (modbusBindingMatchesPollGroup(binding, group)) {
            const int entryNo = static_cast<int>(dataIndex & 0xffff);
            const int regStep = group.dataType == QStringLiteral("DWORD")
                || group.dataType == QStringLiteral("DWORD_L")
                || group.dataType == QStringLiteral("FLOAT")
                ? 2
                : (group.dataType == QStringLiteral("FLOAT_L") ? 4 : 1);
            binding.extensions.insert(QStringLiteral("modbusKind"), modbusPointKindId(group.kind));
            binding.extensions.insert(QStringLiteral("modbusFunctionCode"), group.funCode);
            binding.extensions.insert(QStringLiteral("modbusRegisterAddress"), group.startAddr + ((entryNo - 1) * regStep));
            binding.extensions.insert(QStringLiteral("modbusStartAddress"), group.startAddr);
            binding.extensions.insert(QStringLiteral("modbusRegisterCount"), group.regNum);
            binding.extensions.insert(QStringLiteral("modbusDataType"), group.dataType);
            binding.extensions.insert(QStringLiteral("modbusScale"), group.scale);
            return;
        }
    }
}

bool isDeviceForProtocol(const ProtocolDeviceInstance &device,
                         ProtocolType protocol)
{
    if (device.protocol == protocol) {
        return true;
    }
    if (protocol == ProtocolType::Iec104) {
        return device.appType.compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0;
    }
    if (protocol == ProtocolType::Modbus) {
        return device.appType.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

QList<ProtocolDeviceInstance> devicesForProtocol(const ConfigProject &project,
                                                 ProtocolType protocol)
{
    QList<ProtocolDeviceInstance> devices;
    for (const ProtocolDeviceInstance &device : project.devices) {
        if (isDeviceForProtocol(device, protocol)) {
            devices.append(device);
        }
    }
    return devices;
}

QSet<QString> modelIdsUsedByDevices(const QList<ProtocolDeviceInstance> &devices)
{
    QSet<QString> modelIds;
    for (const ProtocolDeviceInstance &device : devices) {
        if (!device.modelId.trimmed().isEmpty()) {
            modelIds.insert(device.modelId);
        }
    }
    return modelIds;
}

QList<ModelTemplate> modelsForDeviceSet(const ConfigProject &project,
                                        const QSet<QString> &modelIds)
{
    QList<ModelTemplate> models;
    for (const ModelTemplate &model : project.models) {
        if (modelIds.contains(model.modelId)) {
            models.append(model);
        }
    }
    return models;
}

QString modbusKindString(ModbusPointKind kind)
{
    return modbusPointKindId(kind);
}

QString modbusPollKeyPrefix(ModbusPointKind kind)
{
    return kind == ModbusPointKind::Yx ? QStringLiteral("yx") : QStringLiteral("yc");
}

QString modbusSetKeyPrefix(ModbusPointKind kind)
{
    return kind == ModbusPointKind::Yk ? QStringLiteral("yk") : QStringLiteral("yt");
}

QString modbusIniPollLine(const ModbusPollGroup &group)
{
    return QStringLiteral("_%1_%2_%3_%4")
        .arg(group.groupNo)
        .arg(group.funCode)
        .arg(group.startAddr)
        .arg(group.regNum);
}

QString modbusIniSetLine(const ModbusSetPoint &setPoint)
{
    return QStringLiteral("_%1_%2_%3_%4")
        .arg(setPoint.groupNo)
        .arg(setPoint.entryNo)
        .arg(setPoint.funCode)
        .arg(setPoint.regAddr);
}

QString modelFileNameForExport(const ModelTemplate &model)
{
    const QFileInfo sourceInfo(model.source.filePath);
    if (!model.source.fileName.trimmed().isEmpty()
        && sourceInfo.exists()
        && sourceInfo.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
        return model.source.fileName;
    }

    const QString stem = !model.displayName.trimmed().isEmpty()
        ? model.displayName
        : model.modelId;
    return QStringLiteral("model_%1.json")
        .arg(safeFileSegment(stem, QStringLiteral("model")));
}

QString deviceFileNameForExport(const ProtocolDeviceInstance &device)
{
    const QFileInfo sourceInfo(device.source.filePath);
    if (!device.source.fileName.trimmed().isEmpty()
        && sourceInfo.exists()
        && sourceInfo.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
        return device.source.fileName;
    }

    const QString stem = !device.deviceDesc.trimmed().isEmpty()
        ? device.deviceDesc
        : device.deviceId;
    return QStringLiteral("device-%1.json")
        .arg(safeFileSegment(stem, QStringLiteral("device")));
}

QJsonObject serializePoint(const PointTemplate &point)
{
    QJsonObject object;
    object.insert(QStringLiteral("DOname"), point.doName);
    object.insert(QStringLiteral("DOtype"), point.doType);
    object.insert(QStringLiteral("LDname"), point.ldName);
    object.insert(QStringLiteral("LNinst"), point.lnInst);
    object.insert(QStringLiteral("LNtype"), point.lnType);
    object.insert(QStringLiteral("datatype"), point.dataType);
    object.insert(QStringLiteral("deadzonetype"), point.deadZoneType);
    object.insert(QStringLiteral("deadzoneval"), point.deadZoneValue);
    object.insert(QStringLiteral("description"), point.description);
    object.insert(QStringLiteral("max"), point.max);
    object.insert(QStringLiteral("maxlength"), point.maxLength);
    object.insert(QStringLiteral("min"), point.min);
    object.insert(QStringLiteral("step"), point.step);
    object.insert(QStringLiteral("unit"), point.unit);
    return object;
}

QString exportedServiceId(ModelServiceType type)
{
    switch (type) {
    case ModelServiceType::Measurement:
        return QStringLiteral("analog");
    case ModelServiceType::Status:
        return QStringLiteral("discrete");
    case ModelServiceType::Control:
        return QStringLiteral("control");
    }

    return QStringLiteral("analog");
}

QString exportedServiceDescription(ModelServiceType type)
{
    switch (type) {
    case ModelServiceType::Measurement:
        return QStringLiteral("遥测");
    case ModelServiceType::Status:
        return QStringLiteral("遥信");
    case ModelServiceType::Control:
        return QStringLiteral("控制");
    }

    return QStringLiteral("遥测");
}

QJsonObject serializeModel(const ModelTemplate &model)
{
    QJsonObject profile;
    profile.insert(QStringLiteral("devType"), model.deviceType);
    profile.insert(QStringLiteral("manufacturerDesc"), model.manufacturerDesc);
    profile.insert(QStringLiteral("manufacturerId"), model.manufacturerId);
    profile.insert(QStringLiteral("model"), model.modelId);
    profile.insert(QStringLiteral("modelDesc"), model.displayName);
    profile.insert(QStringLiteral("version"), model.version);

    QJsonArray servicesArray;
    for (ModelServiceType serviceType : {ModelServiceType::Measurement, ModelServiceType::Status, ModelServiceType::Control}) {
        QJsonObject serviceObject;
        QJsonArray pointsArray;
        const ServiceTemplate *service = model.findService(serviceType);
        if (service) {
            for (const PointTemplate &point : service->points) {
                pointsArray.append(serializePoint(point));
            }
        }
        serviceObject.insert(QStringLiteral("DOs"), pointsArray);
        serviceObject.insert(QStringLiteral("description"),
            service && !service->displayName.trimmed().isEmpty()
                ? service->displayName
                : exportedServiceDescription(serviceType));
        serviceObject.insert(QStringLiteral("serviceId"),
            service && !service->serviceId.trimmed().isEmpty()
                ? (service->serviceId == QStringLiteral("measurement")
                    ? exportedServiceId(serviceType)
                    : (service->serviceId == QStringLiteral("status")
                        ? exportedServiceId(serviceType)
                        : service->serviceId))
                : exportedServiceId(serviceType));
        servicesArray.append(serviceObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("profile"), profile);
    root.insert(QStringLiteral("schema"), model.schema);
    root.insert(QStringLiteral("services"), servicesArray);
    return root;
}

QHash<QString, QString> buildDataRefDescriptionMap(const ModelTemplate *model)
{
    QHash<QString, QString> descriptions;
    if (!model) {
        return descriptions;
    }

    for (const ServiceTemplate &service : model->services) {
        for (const PointTemplate &point : service.points) {
            descriptions.insert(point.dataRef(), point.description);
        }
    }

    return descriptions;
}

QHash<QString, int> buildModelPointOrderMap(const ModelTemplate *model)
{
    QHash<QString, int> orderMap;
    if (!model) {
        return orderMap;
    }

    int order = 0;
    for (const ServiceTemplate &service : model->services) {
        for (const PointTemplate &point : service.points) {
            orderMap.insert(point.dataRef(), order++);
        }
    }

    return orderMap;
}

QJsonObject serializeBinding(const PointBinding &binding,
                             const QHash<QString, QString> &descriptionMap)
{
    QJsonObject object;
    object.insert(QStringLiteral("dataIndex"), binding.address);
    object.insert(QStringLiteral("dataRef"), binding.dataRef);
    object.insert(QStringLiteral("description"), binding.descriptionOverride.isEmpty()
        ? descriptionMap.value(binding.dataRef)
        : binding.descriptionOverride);
    if (!binding.initValue.isEmpty()) {
        object.insert(QStringLiteral("init_value"), binding.initValue);
    }
    if (!binding.selfSignalFlag.isEmpty()) {
        object.insert(QStringLiteral("self_sig_flag"), binding.selfSignalFlag);
    }
    return object;
}

QList<PointBinding> bindingsForExport(const ProtocolDeviceInstance &device,
                                      const QHash<QString, int> &orderMap)
{
    QList<QPair<int, PointBinding>> orderedBindings;
    orderedBindings.reserve(device.bindings.size());

    for (int index = 0; index < device.bindings.size(); ++index) {
        const PointBinding &binding = device.bindings.at(index);
        if (!binding.enabled || binding.address.trimmed().isEmpty()) {
            continue;
        }

        const int order = orderMap.contains(binding.dataRef)
            ? orderMap.value(binding.dataRef)
            : (100000 + index);
        orderedBindings.append(qMakePair(order, binding));
    }

    std::sort(orderedBindings.begin(), orderedBindings.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });

    QList<PointBinding> result;
    result.reserve(orderedBindings.size());
    for (const auto &item : orderedBindings) {
        result.append(item.second);
    }
    return result;
}

QJsonObject serializeDevice(const ProtocolDeviceInstance &device,
                            const ModelTemplate *model)
{
    const QHash<QString, QString> descriptionMap = buildDataRefDescriptionMap(model);
    const QHash<QString, int> orderMap = buildModelPointOrderMap(model);
    const QList<PointBinding> exportBindings = bindingsForExport(device, orderMap);

    QJsonArray bindingArray;
    for (const PointBinding &binding : exportBindings) {
        bindingArray.append(serializeBinding(binding, descriptionMap));
    }

    QJsonObject root;
    root.insert(QStringLiteral("DeviceDesc"), device.deviceDesc);
    root.insert(QStringLiteral("DeviceId"), device.deviceId);
    root.insert(QStringLiteral("Model"), device.modelId);
    root.insert(QStringLiteral("addr"), device.transport.stationAddress);
    root.insert(QStringLiteral("ipa"), device.transport.ip);
    if (device.transport.source.rawExtra.contains(QStringLiteral("ipb"))) {
        root.insert(QStringLiteral("ipb"), device.transport.source.rawExtra.value(QStringLiteral("ipb")));
    }
    root.insert(QStringLiteral("meas_points"), bindingArray);
    root.insert(QStringLiteral("port"), device.transport.port);
    return root;
}

QJsonObject serializeModbusBinding(const PointBinding &binding,
                                   const QHash<QString, QString> &descriptionMap)
{
    QJsonObject object;
    object.insert(QStringLiteral("dataIndex"), binding.address);
    object.insert(QStringLiteral("dataRef"), binding.dataRef);
    object.insert(QStringLiteral("description"), binding.descriptionOverride.isEmpty()
        ? descriptionMap.value(binding.dataRef)
        : binding.descriptionOverride);
    const QJsonValue scaleValue = binding.extensions.value(QStringLiteral("modbusPointScale"));
    if (!scaleValue.isUndefined() && !scaleValue.isNull()) {
        object.insert(QStringLiteral("scale"), scaleValue);
    }
    if (!binding.initValue.isEmpty()) {
        object.insert(QStringLiteral("init_value"), binding.initValue);
    }
    if (!binding.selfSignalFlag.isEmpty()) {
        object.insert(QStringLiteral("self_sig_flag"), binding.selfSignalFlag);
    }
    if (binding.extensions.contains(QStringLiteral("precontrol_dataIndex"))) {
        object.insert(QStringLiteral("precontrol_dataIndex"), binding.extensions.value(QStringLiteral("precontrol_dataIndex")));
    }
    if (binding.extensions.contains(QStringLiteral("linkto"))) {
        object.insert(QStringLiteral("linkto"), binding.extensions.value(QStringLiteral("linkto")));
    }
    if (binding.extensions.contains(QStringLiteral("virdot"))) {
        object.insert(QStringLiteral("virdot"), binding.extensions.value(QStringLiteral("virdot")));
    }
    return object;
}

QJsonObject serializeModbusDevice(const ProtocolDeviceInstance &device,
                                  const ModelTemplate *model)
{
    const QHash<QString, QString> descriptionMap = buildDataRefDescriptionMap(model);
    const QHash<QString, int> orderMap = buildModelPointOrderMap(model);
    const QList<PointBinding> exportBindings = bindingsForExport(device, orderMap);

    QJsonArray bindingArray;
    for (const PointBinding &binding : exportBindings) {
        bindingArray.append(serializeModbusBinding(binding, descriptionMap));
    }

    const QString type = device.transport.protocolOptions.value(QStringLiteral("type")).toString(QStringLiteral("TCP")).trimmed().toUpper();

    QJsonObject root;
    root.insert(QStringLiteral("DeviceId"), device.deviceId);
    root.insert(QStringLiteral("DeviceDesc"), device.deviceDesc);
    root.insert(QStringLiteral("Model"), device.modelId);
    root.insert(QStringLiteral("addr"), device.transport.stationAddress);
    root.insert(QStringLiteral("type"), type);
    if (type == QStringLiteral("RTU")) {
        QJsonObject rtu;
        rtu.insert(QStringLiteral("serialPort"), jsonValueToString(device.transport.serial.value(QStringLiteral("serialPort"))));
        for (const QString &key : {QStringLiteral("baud"), QStringLiteral("dataBits"), QStringLiteral("stopBits")}) {
            bool ok = false;
            const int value = jsonValueToString(device.transport.serial.value(key)).toInt(&ok);
            rtu.insert(key, ok ? QJsonValue(value) : device.transport.serial.value(key));
        }
        rtu.insert(QStringLiteral("parity"), jsonValueToString(device.transport.serial.value(QStringLiteral("parity"))));
        root.insert(QStringLiteral("rtu"), rtu);
    } else {
        root.insert(QStringLiteral("ip"), device.transport.ip);
        bool ok = false;
        const int port = device.transport.port.toInt(&ok);
        root.insert(QStringLiteral("port"), ok ? QJsonValue(port) : QJsonValue(device.transport.port));
    }

    const QString debug = device.transport.protocolOptions.value(QStringLiteral("debug")).toString().trimmed();
    if (!debug.isEmpty() && debug.compare(QStringLiteral("off"), Qt::CaseInsensitive) != 0) {
        root.insert(QStringLiteral("debug"), debug);
    }
    root.insert(QStringLiteral("meas_points"), bindingArray);
    return root;
}

bool writeJsonFile(const QString &filePath,
                   const QJsonObject &object,
                   QString &errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        errorMessage = QStringLiteral("无法写入文件");
        return false;
    }

    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(payload) < 0) {
        errorMessage = QStringLiteral("写入 JSON 内容失败");
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool removeJsonFilesInDirectory(const QString &dirPath,
                                QString &errorMessage)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        return true;
    }

    const QFileInfoList files = dir.entryInfoList(
        QStringList() << QStringLiteral("*.json"),
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo &file : files) {
        if (!QFile::remove(file.absoluteFilePath())) {
            errorMessage = QStringLiteral("无法删除旧文件: %1").arg(file.absoluteFilePath());
            return false;
        }
    }

    return true;
}

bool writeTextFile(const QString &filePath,
                   const QString &content,
                   QString &errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        errorMessage = QStringLiteral("无法写入文件");
        return false;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    stream << content;
    if (stream.status() != QTextStream::Ok) {
        errorMessage = QStringLiteral("写入文本内容失败");
        file.close();
        return false;
    }

    file.close();
    return true;
}

QString buildModbusIniContent(const ConfigProject &project,
                              const QList<ProtocolDeviceInstance> &devices)
{
    QString content;
    QTextStream stream(&content);

    stream << "[Base]\n";
    stream << "frameInterval=" << (project.modbus.frameInterval.trimmed().isEmpty()
        ? QStringLiteral("100")
        : project.modbus.frameInterval.trimmed()) << "\n";
    if (!project.modbus.hwVariant.trimmed().isEmpty()) {
        stream << "hw_variant=" << project.modbus.hwVariant.trimmed() << "\n";
    }
    if (!project.modbus.yxUploadPeriod.trimmed().isEmpty()) {
        stream << "YX_UploadPeriod=" << project.modbus.yxUploadPeriod.trimmed() << "\n";
    }
    if (!project.modbus.ycUploadPeriod.trimmed().isEmpty()) {
        stream << "YC_UploadPeriod=" << project.modbus.ycUploadPeriod.trimmed() << "\n";
    }
    for (const QString &key : project.modbus.rawExtra.keys()) {
        stream << key << "=" << project.modbus.rawExtra.value(key).toString() << "\n";
    }

    for (const ProtocolDeviceInstance &device : devices) {
        QList<ModbusPollGroup> yxPolls;
        QList<ModbusPollGroup> ycPolls;
        QList<ModbusSetPoint> ykSets;
        QList<ModbusSetPoint> ytSets;

        for (const ModbusPollGroup &group : device.modbus.pollGroups) {
            if (group.kind == ModbusPointKind::Yx) {
                yxPolls.append(group);
            } else {
                ycPolls.append(group);
            }
        }
        for (const ModbusSetPoint &setPoint : device.modbus.setPoints) {
            if (setPoint.kind == ModbusPointKind::Yk) {
                ykSets.append(setPoint);
            } else {
                ytSets.append(setPoint);
            }
        }

        auto sortByOrder = [](const auto &left, const auto &right) {
            if (left.order != right.order) {
                return left.order < right.order;
            }
            if (left.groupNo != right.groupNo) {
                return left.groupNo < right.groupNo;
            }
            if constexpr (std::is_same_v<std::decay_t<decltype(left)>, ModbusSetPoint>) {
                return left.entryNo < right.entryNo;
            } else {
                return left.startAddr < right.startAddr;
            }
        };
        std::sort(yxPolls.begin(), yxPolls.end(), sortByOrder);
        std::sort(ycPolls.begin(), ycPolls.end(), sortByOrder);
        std::sort(ykSets.begin(), ykSets.end(), sortByOrder);
        std::sort(ytSets.begin(), ytSets.end(), sortByOrder);

        stream << "\n[dev_" << device.deviceId << "]\n";
        stream << "type=" << device.transport.protocolOptions.value(QStringLiteral("type")).toString(QStringLiteral("TCP")).toUpper() << "\n";
        stream << "yx_type=" << (device.modbus.yxType.trimmed().isEmpty() ? QStringLiteral("BIT") : device.modbus.yxType.trimmed()) << "\n";
        stream << "yc_type=" << (device.modbus.ycType.trimmed().isEmpty() ? QStringLiteral("WORD") : device.modbus.ycType.trimmed()) << "\n";
        stream << "yt_type=" << (device.modbus.ytType.trimmed().isEmpty() ? QStringLiteral("WORD") : device.modbus.ytType.trimmed()) << "\n";
        stream << "yx_poll_num=" << yxPolls.size() << "\n";
        stream << "yc_poll_num=" << ycPolls.size() << "\n";
        stream << "yk_set_num=" << ykSets.size() << "\n";
        stream << "yt_set_num=" << ytSets.size() << "\n";

        if (!device.modbus.ycScale.trimmed().isEmpty()) {
            stream << "yc_scale=" << device.modbus.ycScale.trimmed() << "\n";
        }
        if (!device.modbus.ytScale.trimmed().isEmpty()) {
            stream << "yt_scale=" << device.modbus.ytScale.trimmed() << "\n";
        }

        for (int index = 0; index < yxPolls.size(); ++index) {
            const ModbusPollGroup &group = yxPolls.at(index);
            const int order = index + 1;
            stream << "yx_poll" << order << "=" << modbusIniPollLine(group) << "\n";
            if (!group.dataType.trimmed().isEmpty() && group.dataType != device.modbus.yxType) {
                stream << "yx_type" << order << "=" << group.dataType.trimmed() << "\n";
            }
        }
        for (int index = 0; index < ycPolls.size(); ++index) {
            const ModbusPollGroup &group = ycPolls.at(index);
            const int order = index + 1;
            stream << "yc_poll" << order << "=" << modbusIniPollLine(group) << "\n";
            if (!group.dataType.trimmed().isEmpty() && group.dataType != device.modbus.ycType) {
                stream << "yc_type" << order << "=" << group.dataType.trimmed() << "\n";
            }
            if (!group.scale.trimmed().isEmpty() && group.scale != device.modbus.ycScale) {
                stream << "yc_scale" << order << "=" << group.scale.trimmed() << "\n";
            }
        }
        for (int index = 0; index < ykSets.size(); ++index) {
            stream << "yk_set" << (index + 1) << "=" << modbusIniSetLine(ykSets.at(index)) << "\n";
        }
        for (int index = 0; index < ytSets.size(); ++index) {
            const ModbusSetPoint &setPoint = ytSets.at(index);
            const int order = index + 1;
            stream << "yt_set" << order << "=" << modbusIniSetLine(setPoint) << "\n";
            if (!setPoint.dataType.trimmed().isEmpty() && setPoint.dataType != device.modbus.ytType) {
                stream << "yt_type" << order << "=" << setPoint.dataType.trimmed() << "\n";
            }
            if (!setPoint.scale.trimmed().isEmpty() && setPoint.scale != device.modbus.ytScale) {
                stream << "yt_scale" << order << "=" << setPoint.scale.trimmed() << "\n";
            }
        }

        for (const QString &key : device.modbus.rawExtra.keys()) {
            stream << key << "=" << device.modbus.rawExtra.value(key).toString() << "\n";
        }
    }

    return content;
}

const ModelTemplate *findModelById(const ConfigProject &project,
                                   const QString &modelId)
{
    for (const ModelTemplate &model : project.models) {
        if (model.modelId == modelId) {
            return &model;
        }
    }

    return nullptr;
}

QSet<QString> duplicateDataRefsForModel(const ModelTemplate &model)
{
    QSet<QString> seenRefs;
    QSet<QString> duplicateRefs;

    for (const ServiceTemplate &service : model.services) {
        for (const PointTemplate &point : service.points) {
            const QString ref = point.dataRef().trimmed();
            if (ref.isEmpty()) {
                continue;
            }

            if (seenRefs.contains(ref)) {
                duplicateRefs.insert(ref);
            } else {
                seenRefs.insert(ref);
            }
        }
    }

    return duplicateRefs;
}

QSet<QString> duplicateBindingAddresses(const ProtocolDeviceInstance &device)
{
    QSet<QString> seenAddresses;
    QSet<QString> duplicateAddresses;

    for (const PointBinding &binding : device.bindings) {
        if (!binding.enabled) {
            continue;
        }

        const QString address = binding.address.trimmed();
        if (address.isEmpty()) {
            continue;
        }

        if (seenAddresses.contains(address)) {
            duplicateAddresses.insert(address);
        } else {
            seenAddresses.insert(address);
        }
    }

    return duplicateAddresses;
}

} // namespace detail

using namespace detail;

bool ImportReport::hasErrors() const
{
    for (const ImportIssue &issue : issues) {
        if (issue.severity == ImportIssueSeverity::Error) {
            return true;
        }
    }

    return false;
}

void ImportReport::addIssue(ImportIssueSeverity severity,
                            const QString &filePath,
                            const QString &message)
{
    ImportIssue issue;
    issue.severity = severity;
    issue.filePath = filePath;
    issue.message = message;
    issues.append(issue);
}

bool ExportReport::hasErrors() const
{
    for (const ImportIssue &issue : issues) {
        if (issue.severity == ImportIssueSeverity::Error) {
            return true;
        }
    }

    return false;
}

void ExportReport::addIssue(ImportIssueSeverity severity,
                            const QString &filePath,
                            const QString &message)
{
    ImportIssue issue;
    issue.severity = severity;
    issue.filePath = filePath;
    issue.message = message;
    issues.append(issue);
}

ConfigProjectManager::ConfigProjectManager() = default;

void ConfigProjectManager::createEmptyProject(const QString &projectName,
                                             const QString &sourceRoot)
{
    m_project = ConfigProject();
    m_project.projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_project.projectName = projectName;
    m_project.sourceRoot = sourceRoot;
    m_project.northApp = QStringLiteral("ServiceChannel");
    m_project.logicApp = QStringLiteral("cepLogicCenter");
}

bool ConfigProjectManager::importIec104AppDirectory(const QString &appDir,
                                                    ImportReport &report)
{
    if (m_project.projectId.isEmpty()) {
        createEmptyProject(QStringLiteral("导入工程"), appDir);
    }

    return m_iec104Importer.importAppDirectory(appDir, m_project, report);
}

bool ConfigProjectManager::importModbusAppDirectory(const QString &appDir,
                                                    ImportReport &report)
{
    if (m_project.projectId.isEmpty()) {
        createEmptyProject(QStringLiteral("导入工程"), appDir);
    }

    return m_modbusImporter.importAppDirectory(appDir, m_project, report);
}

bool ConfigProjectManager::importLogicCenterConfigFile(const QString &filePath,
                                                       ImportReport &report)
{
    if (m_project.projectId.isEmpty()) {
        createEmptyProject(QStringLiteral("导入工程"), QFileInfo(filePath).absolutePath());
    }

    QJsonDocument document;
    QString errorMessage;
    if (!loadJsonDocument(filePath, document, errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
        return false;
    }

    m_project.logicCenter = parseLogicCenterConfig(document.object());
    m_project.logic = document.object();
    return true;
}

ConfigProject &ConfigProjectManager::project()
{
    return m_project;
}

const ConfigProject &ConfigProjectManager::project() const
{
    return m_project;
}

} // namespace configtool

