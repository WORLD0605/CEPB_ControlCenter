#include "config/config_project_manager.h"
#include "config/config_project_manager_p.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

namespace configtool {

using namespace detail;
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

    if (!project.southApps.contains(QStringLiteral("South_104"))) {
        project.southApps.append(QStringLiteral("South_104"));
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
    applyNorthModelVisibility(modelDir, project, report);

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
    model.northVisible = !root.contains(QStringLiteral("northVisible"))
        || root.value(QStringLiteral("northVisible")).toBool(true);
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
    device.appType = QStringLiteral("South_104");
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

} // namespace configtool


