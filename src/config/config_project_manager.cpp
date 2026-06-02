#include "config/config_project_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

ConfigProject &ConfigProjectManager::project()
{
    return m_project;
}

const ConfigProject &ConfigProjectManager::project() const
{
    return m_project;
}

} // namespace configtool