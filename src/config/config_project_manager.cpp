#include "config/config_project_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

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
        point.controlKind = lowerDataType.contains(QStringLiteral("float"))
            || lowerDataType.contains(QStringLiteral("double"))
            || lowerDataType.contains(QStringLiteral("int"))
            ? ControlKind::RemoteAdjust
            : ControlKind::RemoteControl;
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

bool ConfigProjectManager::exportIec104AppDirectory(const QString &appDir,
                                                    ExportReport &report) const
{
    if (m_project.projectId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("当前没有可导出的配置工程"));
        return false;
    }

    if (m_project.models.isEmpty() && m_project.devices.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("当前工程没有模型或设备可导出"));
        return false;
    }

    const QDir appDirInfo(appDir);
    if (!appDirInfo.exists()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("104 APP 目录不存在"));
        return false;
    }

    for (const ModelTemplate &model : m_project.models) {
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

    for (const ProtocolDeviceInstance &device : m_project.devices) {
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

    for (const ModelTemplate &model : m_project.models) {
        const QString filePath = QDir(modelDirPath).filePath(modelFileNameForExport(model));
        QString errorMessage;
        if (!writeJsonFile(filePath, serializeModel(model), errorMessage)) {
            report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
            return false;
        }
        ++report.exportedModelCount;
    }

    for (const ProtocolDeviceInstance &device : m_project.devices) {
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

ConfigProject &ConfigProjectManager::project()
{
    return m_project;
}

const ConfigProject &ConfigProjectManager::project() const
{
    return m_project;
}

} // namespace configtool