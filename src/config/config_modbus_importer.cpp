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

    if (!project.southApps.contains(QStringLiteral("South_Modbus"))) {
        project.southApps.append(QStringLiteral("South_Modbus"));
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
    applyNorthModelVisibility(modelDir, project, report);

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
    device.appType = QStringLiteral("South_Modbus");
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
    if (root.contains(QStringLiteral("responseTimeoutMs"))) {
        device.transport.protocolOptions.insert(QStringLiteral("responseTimeoutMs"), root.value(QStringLiteral("responseTimeoutMs")));
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
        if (bindingObject.contains(QStringLiteral("sourceIndex"))) {
            binding.extensions.insert(QStringLiteral("modbusSourceIndex"), bindingObject.value(QStringLiteral("sourceIndex")));
        }
        if (bindingObject.contains(QStringLiteral("bitIndex"))) {
            binding.extensions.insert(QStringLiteral("modbusBitIndex"), bindingObject.value(QStringLiteral("bitIndex")));
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

} // namespace configtool


