#include "config/config_project_manager.h"

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

namespace {

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

} // namespace

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

bool Iec104ConfigImporter::importAppDirectory(const QString &appDir,
                                              ConfigProject &project,
                                              ImportReport &report) const
{
    bool ok = true;
    QDir dir(appDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("104 APP 目录不存在"));
        return false;
    }

    const QString modelDir = dir.filePath(QStringLiteral("model"));
    const QString deviceDir = dir.filePath(QStringLiteral("dev"));

    ok = importModelDirectory(modelDir, project, report) && ok;
    ok = importDeviceDirectory(deviceDir, project, report) && ok;

    if (!project.southApps.contains(QStringLiteral("cepiec104"))) {
        project.southApps.append(QStringLiteral("cepiec104"));
    }

    return ok;
}

bool Iec104ConfigImporter::importModelDirectory(const QString &modelDir,
                                                ConfigProject &project,
                                                ImportReport &report) const
{
    QDir dir(modelDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Warning, modelDir, QStringLiteral("model 目录不存在"));
        return false;
    }

    bool ok = true;
    const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        ok = importModelFile(entry.filePath(), project, report) && ok;
    }

    return ok;
}

bool Iec104ConfigImporter::importDeviceDirectory(const QString &deviceDir,
                                                 ConfigProject &project,
                                                 ImportReport &report) const
{
    QDir dir(deviceDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Warning, deviceDir, QStringLiteral("dev 目录不存在"));
        return false;
    }

    bool ok = true;
    const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        ok = importDeviceFile(entry.filePath(), project, report) && ok;
    }

    return ok;
}

bool Iec104ConfigImporter::importModelFile(const QString &filePath,
                                           ConfigProject &project,
                                           ImportReport &report) const
{
    QJsonDocument document;
    QString errorMessage;
    if (!loadJsonDocument(filePath, document, errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject profile = root.value(QStringLiteral("profile")).toObject();
    if (profile.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("缺少 profile 节点"));
        return false;
    }

    ModelTemplate model;
    model.modelId = profile.value(QStringLiteral("model")).toString();
    model.name = model.modelId;
    model.displayName = profile.value(QStringLiteral("modelDesc")).toString();
    model.deviceType = profile.value(QStringLiteral("devType")).toString();
    model.manufacturerId = profile.value(QStringLiteral("manufacturerId")).toString();
    model.manufacturerDesc = profile.value(QStringLiteral("manufacturerDesc")).toString();
    model.version = profile.value(QStringLiteral("version")).toString();
    model.schema = root.value(QStringLiteral("schema")).toString();
    model.source = makeSourceInfo(filePath);
    model.ensureDefaultServices();

    const QJsonArray services = root.value(QStringLiteral("services")).toArray();
    if (services.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning, filePath, QStringLiteral("services 为空"));
    }

    const bool useIndexedServices = services.size() == 3;
    if (!useIndexedServices && !services.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("services 数量不是 3，已按启发式规则重新归类点位"));
    }

    for (int serviceIndex = 0; serviceIndex < services.size(); ++serviceIndex) {
        const QJsonObject serviceObject = services.at(serviceIndex).toObject();
        const QJsonArray points = serviceObject.value(QStringLiteral("DOs")).toArray();
        for (const QJsonValue &pointValue : points) {
            if (!pointValue.isObject()) {
                continue;
            }

            const QJsonObject pointObject = pointValue.toObject();
            const ModelServiceType serviceType = useIndexedServices
                ? serviceTypeFromIndex(serviceIndex)
                : inferServiceType(pointObject);
            ServiceTemplate *service = model.findService(serviceType);
            if (!service) {
                continue;
            }

            service->points.append(parsePointTemplate(pointObject, serviceType, model.source));
        }
    }

    if (model.modelId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("profile.model 不能为空"));
        return false;
    }

    project.models.append(model);
    ++report.importedModelCount;
    return true;
}

bool Iec104ConfigImporter::importDeviceFile(const QString &filePath,
                                            ConfigProject &project,
                                            ImportReport &report) const
{
    QJsonDocument document;
    QString errorMessage;
    if (!loadJsonDocument(filePath, document, errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
        return false;
    }

    const QJsonObject root = document.object();

    ProtocolDeviceInstance device;
    device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    device.appType = QStringLiteral("cepiec104");
    device.protocol = ProtocolType::Iec104;
    device.deviceId = root.value(QStringLiteral("DeviceId")).toString();
    device.deviceDesc = root.value(QStringLiteral("DeviceDesc")).toString();
    device.modelId = root.value(QStringLiteral("Model")).toString();
    device.transport.stationAddress = root.value(QStringLiteral("addr")).toString();
    device.transport.ip = root.value(QStringLiteral("ipa")).toString();
    device.transport.port = root.value(QStringLiteral("port")).toString();
    device.source = makeSourceInfo(filePath);
    device.transport.source = device.source;

    if (root.contains(QStringLiteral("ipb"))) {
        device.source.rawExtra.insert(QStringLiteral("ipb"), root.value(QStringLiteral("ipb")));
        device.transport.source.rawExtra.insert(QStringLiteral("ipb"), root.value(QStringLiteral("ipb")));
    }

    const QJsonArray bindings = root.value(QStringLiteral("meas_points")).toArray();
    for (const QJsonValue &bindingValue : bindings) {
        if (!bindingValue.isObject()) {
            continue;
        }

        const QJsonObject bindingObject = bindingValue.toObject();
        PointBinding binding;
        binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        binding.dataRef = bindingObject.value(QStringLiteral("dataRef")).toString();
        binding.pointRef = device.modelId + QLatin1Char('#') + binding.dataRef;
        binding.descriptionOverride = bindingObject.value(QStringLiteral("description")).toString();
        binding.address = bindingObject.value(QStringLiteral("dataIndex")).toString();
        binding.initValue = bindingObject.value(QStringLiteral("init_value")).toString();
        binding.selfSignalFlag = bindingObject.value(QStringLiteral("self_sig_flag")).toString();
        binding.source = device.source;
        device.bindings.append(binding);
    }

    if (device.deviceId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("DeviceId 不能为空"));
        return false;
    }

    if (device.modelId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning, filePath, QStringLiteral("Model 为空，后续无法正确建立点位引用"));
    }

    project.devices.append(device);
    ++report.importedDeviceCount;
    return true;
}

bool ModbusConfigImporter::importAppDirectory(const QString &appDir,
                                              ConfigProject &project,
                                              ImportReport &report) const
{
    bool ok = true;
    QDir dir(appDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("Modbus APP 目录不存在"));
        return false;
    }

    const QString modelDir = dir.filePath(QStringLiteral("model"));
    const QString deviceDir = dir.filePath(QStringLiteral("dev"));
    const QString iniPath = dir.filePath(QStringLiteral("etc/cepmodbus.ini"));

    project.modbus = importGlobalIniConfig(iniPath);
    ok = importModelDirectory(modelDir, project, report) && ok;
    ok = importDeviceDirectory(deviceDir, iniPath, project, report) && ok;

    if (!QFileInfo::exists(iniPath)) {
        report.addIssue(ImportIssueSeverity::Warning, iniPath, QStringLiteral("cepmodbus.ini 不存在，已仅导入设备 JSON"));
    }

    if (!project.southApps.contains(QStringLiteral("cepmodbus"))) {
        project.southApps.append(QStringLiteral("cepmodbus"));
    }

    return ok;
}

bool ModbusConfigImporter::importModelDirectory(const QString &modelDir,
                                                ConfigProject &project,
                                                ImportReport &report) const
{
    QDir dir(modelDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Warning, modelDir, QStringLiteral("model 目录不存在"));
        return false;
    }

    bool ok = true;
    const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        ok = importModelFile(entry.filePath(), project, report) && ok;
    }

    return ok;
}

bool ModbusConfigImporter::importDeviceDirectory(const QString &deviceDir,
                                                 const QString &iniPath,
                                                 ConfigProject &project,
                                                 ImportReport &report) const
{
    QDir dir(deviceDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Warning, deviceDir, QStringLiteral("dev 目录不存在"));
        return false;
    }

    bool ok = true;
    const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        ok = importDeviceFile(entry.filePath(), iniPath, project, report) && ok;
    }

    return ok;
}

bool ModbusConfigImporter::importModelFile(const QString &filePath,
                                           ConfigProject &project,
                                           ImportReport &report) const
{
    QJsonDocument document;
    QString errorMessage;
    if (!loadJsonDocument(filePath, document, errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject profile = root.value(QStringLiteral("profile")).toObject();
    if (profile.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("缺少 profile 节点"));
        return false;
    }

    ModelTemplate model;
    model.modelId = profile.value(QStringLiteral("model")).toString();
    model.name = model.modelId;
    model.displayName = profile.value(QStringLiteral("modelDesc")).toString();
    model.deviceType = profile.value(QStringLiteral("devType")).toString();
    model.manufacturerId = profile.value(QStringLiteral("manufacturerId")).toString();
    model.manufacturerDesc = profile.value(QStringLiteral("manufacturerDesc")).toString();
    model.version = profile.value(QStringLiteral("version")).toString();
    model.schema = root.value(QStringLiteral("schema")).toString();
    model.source = makeSourceInfo(filePath);
    model.ensureDefaultServices();

    const QJsonArray services = root.value(QStringLiteral("services")).toArray();
    const bool useIndexedServices = services.size() == 3;
    for (int serviceIndex = 0; serviceIndex < services.size(); ++serviceIndex) {
        const QJsonObject serviceObject = services.at(serviceIndex).toObject();
        const QJsonArray points = serviceObject.value(QStringLiteral("DOs")).toArray();
        for (const QJsonValue &pointValue : points) {
            if (!pointValue.isObject()) {
                continue;
            }

            const QJsonObject pointObject = pointValue.toObject();
            const ModelServiceType serviceType = useIndexedServices
                ? serviceTypeFromIndex(serviceIndex)
                : inferServiceType(pointObject);
            ServiceTemplate *service = model.findService(serviceType);
            if (service) {
                service->points.append(parsePointTemplate(pointObject, serviceType, model.source));
            }
        }
    }

    if (model.modelId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("profile.model 不能为空"));
        return false;
    }

    project.models.append(model);
    ++report.importedModelCount;
    return true;
}

bool ModbusConfigImporter::importDeviceFile(const QString &filePath,
                                            const QString &iniPath,
                                            ConfigProject &project,
                                            ImportReport &report) const
{
    QJsonDocument document;
    QString errorMessage;
    if (!loadJsonDocument(filePath, document, errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
        return false;
    }

    const QJsonObject root = document.object();

    ProtocolDeviceInstance device;
    device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    device.appType = QStringLiteral("cepmodbus");
    device.protocol = ProtocolType::Modbus;
    device.deviceId = jsonValueToString(root.value(QStringLiteral("DeviceId")));
    device.deviceDesc = root.value(QStringLiteral("DeviceDesc")).toString();
    device.modelId = root.value(QStringLiteral("Model")).toString();
    device.transport.stationAddress = jsonValueToString(root.value(QStringLiteral("addr")));
    device.transport.ip = root.value(QStringLiteral("ip")).toString();
    device.transport.port = jsonValueToString(root.value(QStringLiteral("port")));
    device.transport.serial = root.value(QStringLiteral("rtu")).toObject();
    device.source = makeSourceInfo(filePath);
    device.transport.source = device.source;

    const QString type = root.value(QStringLiteral("type")).toString().trimmed().toUpper();
    if (!type.isEmpty()) {
        device.transport.protocolOptions.insert(QStringLiteral("type"), type);
    }
    if (root.contains(QStringLiteral("debug"))) {
        device.transport.protocolOptions.insert(QStringLiteral("debug"), root.value(QStringLiteral("debug")));
    }

    device.modbus = importDeviceIniConfig(iniPath, device.deviceId, report);

    const QJsonArray bindings = root.value(QStringLiteral("meas_points")).toArray();
    for (const QJsonValue &bindingValue : bindings) {
        if (!bindingValue.isObject()) {
            continue;
        }

        const QJsonObject bindingObject = bindingValue.toObject();
        PointBinding binding;
        binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        binding.dataRef = bindingObject.value(QStringLiteral("dataRef")).toString();
        binding.pointRef = device.modelId + QLatin1Char('#') + binding.dataRef;
        binding.descriptionOverride = bindingObject.value(QStringLiteral("description")).toString();
        binding.address = jsonValueToString(bindingObject.value(QStringLiteral("dataIndex")));
        binding.initValue = jsonValueToString(bindingObject.value(QStringLiteral("init_value")));
        binding.selfSignalFlag = jsonValueToString(bindingObject.value(QStringLiteral("self_sig_flag")));
        binding.source = device.source;

        if (bindingObject.contains(QStringLiteral("scale"))) {
            binding.extensions.insert(QStringLiteral("modbusPointScale"), jsonValueToString(bindingObject.value(QStringLiteral("scale"))));
        }
        if (bindingObject.contains(QStringLiteral("precontrol_dataIndex"))) {
            binding.extensions.insert(QStringLiteral("precontrol_dataIndex"), bindingObject.value(QStringLiteral("precontrol_dataIndex")));
        }
        if (bindingObject.contains(QStringLiteral("linkto"))) {
            binding.extensions.insert(QStringLiteral("linkto"), bindingObject.value(QStringLiteral("linkto")));
        }
        if (bindingObject.contains(QStringLiteral("virdot"))) {
            binding.extensions.insert(QStringLiteral("virdot"), bindingObject.value(QStringLiteral("virdot")));
        }

        annotateModbusBinding(binding, device.modbus);
        if (binding.enabled
            && !binding.address.trimmed().isEmpty()
            && !binding.extensions.contains(QStringLiteral("modbusKind"))) {
            report.addIssue(ImportIssueSeverity::Warning,
                            filePath,
                            QStringLiteral("点位 %1 的 dataIndex=%2 未匹配到 cepmodbus.ini 中的轮询组或控制项")
                                .arg(binding.dataRef, binding.address));
        }

        device.bindings.append(binding);
    }

    if (device.deviceId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("DeviceId 不能为空"));
        return false;
    }

    if (type != QStringLiteral("TCP") && type != QStringLiteral("RTU")) {
        report.addIssue(ImportIssueSeverity::Warning, filePath, QStringLiteral("Modbus 设备 type 应为 TCP 或 RTU"));
    }

    if (type == QStringLiteral("TCP") && (device.transport.ip.isEmpty() || device.transport.port.isEmpty())) {
        report.addIssue(ImportIssueSeverity::Warning, filePath, QStringLiteral("TCP 设备缺少 ip 或 port"));
    }

    if (type == QStringLiteral("RTU")) {
        const QJsonObject rtu = device.transport.serial;
        if (rtu.value(QStringLiteral("serialPort")).toString().isEmpty()
            || jsonValueToString(rtu.value(QStringLiteral("baud"))).isEmpty()
            || jsonValueToString(rtu.value(QStringLiteral("dataBits"))).isEmpty()
            || jsonValueToString(rtu.value(QStringLiteral("stopBits"))).isEmpty()
            || rtu.value(QStringLiteral("parity")).toString().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Warning, filePath, QStringLiteral("RTU 设备缺少完整 rtu 参数"));
        }
    }

    project.devices.append(device);
    ++report.importedDeviceCount;
    return true;
}

ModbusGlobalConfig ModbusConfigImporter::importGlobalIniConfig(const QString &iniPath) const
{
    ModbusGlobalConfig config;
    if (!QFileInfo::exists(iniPath)) {
        return config;
    }

    QSettings settings(iniPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Base"));
    config.frameInterval = settings.value(QStringLiteral("frameInterval")).toString();
    config.hwVariant = settings.value(QStringLiteral("hw_variant")).toString();
    config.yxUploadPeriod = settings.value(QStringLiteral("YX_UploadPeriod")).toString();
    config.ycUploadPeriod = settings.value(QStringLiteral("YC_UploadPeriod")).toString();
    config.rawExtra = qSettingsGroupRawExtra(settings, {
        QStringLiteral("frameInterval"),
        QStringLiteral("hw_variant"),
        QStringLiteral("YX_UploadPeriod"),
        QStringLiteral("YC_UploadPeriod")
    });
    settings.endGroup();
    return config;
}

ModbusDeviceConfig ModbusConfigImporter::importDeviceIniConfig(const QString &iniPath,
                                                               const QString &deviceId,
                                                               ImportReport &report) const
{
    ModbusDeviceConfig config;
    if (!QFileInfo::exists(iniPath) || deviceId.trimmed().isEmpty()) {
        return config;
    }

    QSettings settings(iniPath, QSettings::IniFormat);
    const QString groupName = QStringLiteral("dev_%1").arg(deviceId);
    if (!settings.childGroups().contains(groupName)) {
        report.addIssue(ImportIssueSeverity::Warning,
                        iniPath,
                        QStringLiteral("未找到与 DeviceId=%1 对应的 [%2] 配置节")
                            .arg(deviceId, groupName));
        return config;
    }

    settings.beginGroup(groupName);
    config.yxType = settings.value(QStringLiteral("yx_type"), QStringLiteral("BIT")).toString();
    config.ycType = settings.value(QStringLiteral("yc_type"), QStringLiteral("WORD")).toString();
    config.ytType = settings.value(QStringLiteral("yt_type"), QStringLiteral("WORD")).toString();
    config.ycScale = settings.value(QStringLiteral("yc_scale"), QStringLiteral("1.0")).toString();
    config.ytScale = settings.value(QStringLiteral("yt_scale"), QStringLiteral("1.0")).toString();

    const int yxPollNum = settings.value(QStringLiteral("yx_poll_num"), 0).toInt();
    const int ycPollNum = settings.value(QStringLiteral("yc_poll_num"), 0).toInt();
    const int ykSetNum = settings.value(QStringLiteral("yk_set_num"), 0).toInt();
    const int ytSetNum = settings.value(QStringLiteral("yt_set_num"), 0).toInt();

    QSet<QString> knownKeys = {
        QStringLiteral("yx_type"),
        QStringLiteral("yc_type"),
        QStringLiteral("yt_type"),
        QStringLiteral("yc_scale"),
        QStringLiteral("yt_scale"),
        QStringLiteral("yx_poll_num"),
        QStringLiteral("yc_poll_num"),
        QStringLiteral("yk_set_num"),
        QStringLiteral("yt_set_num")
    };

    auto readPollGroups = [&](ModbusPointKind kind, const QString &prefix, int count, const QString &defaultType, const QString &defaultScale) {
        for (int index = 1; index <= count; ++index) {
            const QString key = QStringLiteral("%1_poll%2").arg(prefix).arg(index);
            const QString line = settings.value(key).toString();
            knownKeys.insert(key);
            if (!isCompleteModbusLine(line, 4)) {
                if (!line.trimmed().isEmpty()) {
                    report.addIssue(ImportIssueSeverity::Warning, iniPath, QStringLiteral("%1/%2 格式不完整").arg(groupName, key));
                }
                continue;
            }

            ModbusPollGroup group;
            group.kind = kind;
            group.order = index;
            group.groupNo = parseModbusLinePart(line, 1);
            group.funCode = parseModbusLinePart(line, 2);
            group.startAddr = parseModbusLinePart(line, 3);
            group.regNum = parseModbusLinePart(line, 4);

            const QString typeKey = QStringLiteral("%1_type%2").arg(prefix).arg(index);
            group.dataType = settings.value(typeKey, defaultType).toString();
            knownKeys.insert(typeKey);

            if (kind == ModbusPointKind::Yc) {
                const QString scaleKey = QStringLiteral("yc_scale%1").arg(index);
                group.scale = settings.value(scaleKey, defaultScale).toString();
                knownKeys.insert(scaleKey);
            } else {
                group.scale = QStringLiteral("1.0");
            }

            config.pollGroups.append(group);
        }
    };

    readPollGroups(ModbusPointKind::Yx, QStringLiteral("yx"), yxPollNum, config.yxType, QStringLiteral("1.0"));
    readPollGroups(ModbusPointKind::Yc, QStringLiteral("yc"), ycPollNum, config.ycType, config.ycScale);

    auto readSetPoints = [&](ModbusPointKind kind, const QString &prefix, int count, const QString &defaultType, const QString &defaultScale) {
        for (int index = 1; index <= count; ++index) {
            const QString key = QStringLiteral("%1_set%2").arg(prefix).arg(index);
            const QString line = settings.value(key).toString();
            knownKeys.insert(key);
            if (!isCompleteModbusLine(line, 4)) {
                if (!line.trimmed().isEmpty()) {
                    report.addIssue(ImportIssueSeverity::Warning, iniPath, QStringLiteral("%1/%2 格式不完整").arg(groupName, key));
                }
                continue;
            }

            ModbusSetPoint setPoint;
            setPoint.kind = kind;
            setPoint.order = index;
            setPoint.groupNo = parseModbusLinePart(line, 1);
            setPoint.entryNo = parseModbusLinePart(line, 2);
            setPoint.funCode = parseModbusLinePart(line, 3);
            setPoint.regAddr = parseModbusLinePart(line, 4);

            if (kind == ModbusPointKind::Yt) {
                const QString typeKey = QStringLiteral("yt_type%1").arg(index);
                const QString scaleKey = QStringLiteral("yt_scale%1").arg(index);
                setPoint.dataType = settings.value(typeKey, defaultType).toString();
                setPoint.scale = settings.value(scaleKey, defaultScale).toString();
                knownKeys.insert(typeKey);
                knownKeys.insert(scaleKey);
            } else {
                setPoint.dataType = QStringLiteral("WORD");
                setPoint.scale = QStringLiteral("1.0");
            }

            config.setPoints.append(setPoint);
        }
    };

    readSetPoints(ModbusPointKind::Yk, QStringLiteral("yk"), ykSetNum, QStringLiteral("WORD"), QStringLiteral("1.0"));
    readSetPoints(ModbusPointKind::Yt, QStringLiteral("yt"), ytSetNum, config.ytType, config.ytScale);

    config.rawExtra = qSettingsGroupRawExtra(settings, knownKeys);
    settings.endGroup();
    return config;
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

bool ConfigProjectManager::exportIec104AppDirectory(const QString &appDir,
                                                    ExportReport &report) const
{
    if (m_project.projectId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("当前没有可导出的配置工程"));
        return false;
    }

    const QList<ProtocolDeviceInstance> exportDevices = devicesForProtocol(m_project, ProtocolType::Iec104);
    const QList<ModelTemplate> exportModels = modelsForDeviceSet(m_project, modelIdsUsedByDevices(exportDevices));

    if (exportModels.isEmpty() && exportDevices.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning, appDir, QStringLiteral("当前工程没有 104 模型或设备可导出"));
        return false;
    }

    const QDir appDirInfo(appDir);
    if (!appDirInfo.exists()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("104 APP 目录不存在"));
        return false;
    }

    for (const ModelTemplate &model : exportModels) {
        if (model.modelId.trimmed().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("存在模型 modelId 为空，无法导出"));
        }

        const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
        if (!duplicateRefs.isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error,
                            model.source.filePath.isEmpty() ? model.modelId : model.source.filePath,
                            QStringLiteral("模型存在重复 DataRef：%1")
                                .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
        }
    }

    for (const ProtocolDeviceInstance &device : exportDevices) {
        if (device.deviceId.trimmed().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("存在设备 DeviceId 为空，无法导出"));
        }

        const QSet<QString> duplicateAddresses = duplicateBindingAddresses(device);
        if (!duplicateAddresses.isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("设备存在重复 104 地址：%1")
                                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，"))));
        }

        for (const PointBinding &binding : device.bindings) {
            if (binding.enabled && binding.address.trimmed().isEmpty()) {
                report.addIssue(ImportIssueSeverity::Error,
                                device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                                QStringLiteral("设备 %1 存在启用但未填写地址的点位：%2")
                                    .arg(device.deviceId, binding.dataRef));
                break;
            }
        }

        if (!device.modelId.trimmed().isEmpty() && !findModelById(m_project, device.modelId)) {
            report.addIssue(ImportIssueSeverity::Warning,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("设备引用的模型 %1 在当前工程中不存在，描述回填和排序将退化")
                                .arg(device.modelId));
        }
    }

    if (report.hasErrors()) {
        return false;
    }

    QDir mutableAppDir(appDir);
    const QString modelDirPath = mutableAppDir.filePath(QStringLiteral("model"));
    const QString deviceDirPath = mutableAppDir.filePath(QStringLiteral("dev"));
    if (!mutableAppDir.mkpath(QStringLiteral("model"))) {
        report.addIssue(ImportIssueSeverity::Error, modelDirPath, QStringLiteral("无法创建 model 目录"));
        return false;
    }
    if (!mutableAppDir.mkpath(QStringLiteral("dev"))) {
        report.addIssue(ImportIssueSeverity::Error, deviceDirPath, QStringLiteral("无法创建 dev 目录"));
        return false;
    }

    QString cleanupErrorMessage;
    if (!removeJsonFilesInDirectory(modelDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, modelDirPath, cleanupErrorMessage);
        return false;
    }
    if (!removeJsonFilesInDirectory(deviceDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, deviceDirPath, cleanupErrorMessage);
        return false;
    }

    for (const ModelTemplate &model : exportModels) {
        const QString filePath = QDir(modelDirPath).filePath(modelFileNameForExport(model));
        QString errorMessage;
        if (!writeJsonFile(filePath, serializeModel(model), errorMessage)) {
            report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
            return false;
        }
        ++report.exportedModelCount;
    }

    for (const ProtocolDeviceInstance &device : exportDevices) {
        const QString filePath = QDir(deviceDirPath).filePath(deviceFileNameForExport(device));
        QString errorMessage;
        if (!writeJsonFile(filePath, serializeDevice(device, findModelById(m_project, device.modelId)), errorMessage)) {
            report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
            return false;
        }
        ++report.exportedDeviceCount;
    }

    return true;
}

bool ConfigProjectManager::exportModbusAppDirectory(const QString &appDir,
                                                    ExportReport &report) const
{
    if (m_project.projectId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("当前没有可导出的配置工程"));
        return false;
    }

    const QList<ProtocolDeviceInstance> exportDevices = devicesForProtocol(m_project, ProtocolType::Modbus);
    const QList<ModelTemplate> exportModels = modelsForDeviceSet(m_project, modelIdsUsedByDevices(exportDevices));
    if (exportModels.isEmpty() && exportDevices.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning, appDir, QStringLiteral("当前工程没有 Modbus 模型或设备可导出"));
        return false;
    }

    for (const ModelTemplate &model : exportModels) {
        if (model.modelId.trimmed().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("存在 Modbus 模型 modelId 为空，无法导出"));
        }
        const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
        if (!duplicateRefs.isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error,
                            model.source.filePath.isEmpty() ? model.modelId : model.source.filePath,
                            QStringLiteral("Modbus 模型存在重复 DataRef：%1")
                                .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
        }
    }

    for (const ProtocolDeviceInstance &device : exportDevices) {
        if (device.deviceId.trimmed().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("存在 Modbus 设备 DeviceId 为空，无法导出"));
        }
        if (!device.modelId.trimmed().isEmpty() && !findModelById(m_project, device.modelId)) {
            report.addIssue(ImportIssueSeverity::Warning,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("Modbus 设备引用的模型 %1 在当前工程中不存在")
                                .arg(device.modelId));
        }

        const QString type = device.transport.protocolOptions.value(QStringLiteral("type")).toString(QStringLiteral("TCP")).trimmed().toUpper();
        if (type != QStringLiteral("TCP") && type != QStringLiteral("RTU")) {
            report.addIssue(ImportIssueSeverity::Error,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("Modbus 设备 %1 的 type 必须为 TCP 或 RTU").arg(device.deviceId));
        }
        bool portOk = false;
        device.transport.port.trimmed().toInt(&portOk);
        if (type == QStringLiteral("TCP") && (device.transport.ip.trimmed().isEmpty() || !portOk)) {
            report.addIssue(ImportIssueSeverity::Error,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("Modbus TCP 设备 %1 缺少 ip 或数字 port").arg(device.deviceId));
        }
        if (type == QStringLiteral("RTU")) {
            const QJsonObject rtu = device.transport.serial;
            if (jsonValueToString(rtu.value(QStringLiteral("serialPort"))).trimmed().isEmpty()
                || jsonValueToString(rtu.value(QStringLiteral("baud"))).trimmed().isEmpty()
                || jsonValueToString(rtu.value(QStringLiteral("dataBits"))).trimmed().isEmpty()
                || jsonValueToString(rtu.value(QStringLiteral("stopBits"))).trimmed().isEmpty()
                || jsonValueToString(rtu.value(QStringLiteral("parity"))).trimmed().isEmpty()) {
                report.addIssue(ImportIssueSeverity::Error,
                                device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                                QStringLiteral("Modbus RTU 设备 %1 缺少完整 rtu 参数").arg(device.deviceId));
            }
        }

        for (const PointBinding &binding : device.bindings) {
            if (!binding.enabled) {
                continue;
            }
            if (binding.address.trimmed().isEmpty()) {
                report.addIssue(ImportIssueSeverity::Error,
                                device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                                QStringLiteral("Modbus 设备 %1 存在启用但未生成 dataIndex 的点位：%2")
                                    .arg(device.deviceId, binding.dataRef));
                break;
            }
            if (!binding.extensions.contains(QStringLiteral("modbusRegisterAddress"))) {
                report.addIssue(ImportIssueSeverity::Error,
                                device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                                QStringLiteral("Modbus 设备 %1 的点位 %2 未填写寄存器地址")
                                    .arg(device.deviceId, binding.dataRef));
                break;
            }
        }
    }

    if (report.hasErrors()) {
        return false;
    }

    QDir mutableAppDir(appDir);
    if (!mutableAppDir.exists() && !QDir().mkpath(appDir)) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("无法创建 Modbus APP 目录"));
        return false;
    }

    const QString modelDirPath = mutableAppDir.filePath(QStringLiteral("model"));
    const QString deviceDirPath = mutableAppDir.filePath(QStringLiteral("dev"));
    const QString etcDirPath = mutableAppDir.filePath(QStringLiteral("etc"));
    if (!mutableAppDir.mkpath(QStringLiteral("model"))) {
        report.addIssue(ImportIssueSeverity::Error, modelDirPath, QStringLiteral("无法创建 model 目录"));
        return false;
    }
    if (!mutableAppDir.mkpath(QStringLiteral("dev"))) {
        report.addIssue(ImportIssueSeverity::Error, deviceDirPath, QStringLiteral("无法创建 dev 目录"));
        return false;
    }
    if (!mutableAppDir.mkpath(QStringLiteral("etc"))) {
        report.addIssue(ImportIssueSeverity::Error, etcDirPath, QStringLiteral("无法创建 etc 目录"));
        return false;
    }

    QString cleanupErrorMessage;
    if (!removeJsonFilesInDirectory(modelDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, modelDirPath, cleanupErrorMessage);
        return false;
    }
    if (!removeJsonFilesInDirectory(deviceDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, deviceDirPath, cleanupErrorMessage);
        return false;
    }

    for (const ModelTemplate &model : exportModels) {
        const QString filePath = QDir(modelDirPath).filePath(modelFileNameForExport(model));
        QString errorMessage;
        if (!writeJsonFile(filePath, serializeModel(model), errorMessage)) {
            report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
            return false;
        }
        ++report.exportedModelCount;
    }

    for (const ProtocolDeviceInstance &device : exportDevices) {
        const QString filePath = QDir(deviceDirPath).filePath(deviceFileNameForExport(device));
        QString errorMessage;
        if (!writeJsonFile(filePath, serializeModbusDevice(device, findModelById(m_project, device.modelId)), errorMessage)) {
            report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
            return false;
        }
        ++report.exportedDeviceCount;
    }

    QString errorMessage;
    const QString iniPath = QDir(etcDirPath).filePath(QStringLiteral("cepmodbus.ini"));
    if (!writeTextFile(iniPath, buildModbusIniContent(m_project, exportDevices), errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, iniPath, errorMessage);
        return false;
    }

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
