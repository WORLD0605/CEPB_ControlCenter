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

namespace {

int parseDlt645LinePart(const QString &line, int sectionIndex, int base = 10)
{
    return line.section(QLatin1Char('_'), sectionIndex, sectionIndex).trimmed().toInt(nullptr, base);
}

QString parseDlt645DiPart(const QString &line, int sectionIndex)
{
    return line.section(QLatin1Char('_'), sectionIndex, sectionIndex).trimmed().toUpper();
}

bool isCompleteDlt645Line(const QString &line, int expectedParts)
{
    if (line.trimmed().isEmpty()) {
        return false;
    }

    return line.split(QLatin1Char('_'), Qt::KeepEmptyParts).size() >= expectedParts + 1;
}

QString dlt645AddrGroupName(const QString &address)
{
    const QString trimmed = address.trimmed();
    bool ok = false;
    const quint64 rawAddress = trimmed.toULongLong(&ok, 16);
    if (!ok) {
        return QStringLiteral("dev_%1").arg(trimmed);
    }

    QString groupName = QStringLiteral("dev_");
    for (int index = 5; index >= 0; --index) {
        const quint8 byte = static_cast<quint8>((rawAddress >> (index * 8)) & 0xff);
        groupName += QString::number((byte >> 4) & 0x0f);
        groupName += QString::number(byte & 0x0f);
    }
    return groupName;
}

bool dlt645BindingMatchesPollGroup(const PointBinding &binding,
                                   const Dlt645PollGroup &group)
{
    bool ok = false;
    const uint dataIndex = binding.address.toUInt(&ok, 0);
    return ok && static_cast<int>(dataIndex >> 16) == group.groupNo;
}

bool dlt645BindingMatchesSetPoint(const PointBinding &binding,
                                  const Dlt645SetPoint &setPoint)
{
    bool ok = false;
    const uint dataIndex = binding.address.toUInt(&ok, 0);
    if (!ok) {
        return false;
    }

    return static_cast<int>(dataIndex >> 16) == setPoint.groupNo
        && static_cast<int>(dataIndex & 0xffff) == setPoint.entryNo;
}

void annotateDlt645Binding(PointBinding &binding,
                           const Dlt645DeviceConfig &dlt645)
{
    bool ok = false;
    const uint dataIndex = binding.address.toUInt(&ok, 0);
    if (!ok) {
        return;
    }

    const int groupNo = static_cast<int>(dataIndex >> 16);
    const int entryNo = static_cast<int>(dataIndex & 0xffff);
    binding.extensions.insert(QStringLiteral("dlt645GroupNo"), groupNo);
    binding.extensions.insert(QStringLiteral("dlt645EntryNo"), entryNo);

    for (const Dlt645SetPoint &setPoint : dlt645.setPoints) {
        if (dlt645BindingMatchesSetPoint(binding, setPoint)) {
            binding.extensions.insert(QStringLiteral("dlt645Kind"), dlt645PointKindId(setPoint.kind));
            binding.extensions.insert(QStringLiteral("dlt645FunctionCode"), setPoint.funCode);
            binding.extensions.insert(QStringLiteral("dlt645PointDI"), setPoint.di);
            binding.extensions.insert(QStringLiteral("dlt645DataLength"), setPoint.dataLength);
            binding.extensions.insert(QStringLiteral("dlt645DataType"), setPoint.dataType);
            return;
        }
    }

    for (const Dlt645PollGroup &group : dlt645.pollGroups) {
        if (dlt645BindingMatchesPollGroup(binding, group)) {
            binding.extensions.insert(QStringLiteral("dlt645Kind"), dlt645PointKindId(group.kind));
            binding.extensions.insert(QStringLiteral("dlt645FunctionCode"), group.funCode);
            binding.extensions.insert(QStringLiteral("dlt645PollDI"), group.pollDi);
            binding.extensions.insert(QStringLiteral("dlt645DataLength"), group.dataLength);
            binding.extensions.insert(QStringLiteral("dlt645DataType"), group.dataType);
            return;
        }
    }
}

} // namespace

bool Dlt645ConfigImporter::importAppDirectory(const QString &appDir,
                                              ConfigProject &project,
                                              ImportReport &report) const
{
    bool ok = true;
    QDir dir(appDir);
    if (!dir.exists()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("DLT645 APP 目录不存在"));
        return false;
    }

    const QString modelDir = dir.filePath(QStringLiteral("model"));
    const QString deviceDir = dir.filePath(QStringLiteral("dev"));
    const QString iniPath = dir.filePath(QStringLiteral("etc/cepdlt645.ini"));

    project.dlt645 = importGlobalIniConfig(iniPath);
    ok = importModelDirectory(modelDir, project, report) && ok;
    ok = importDeviceDirectory(deviceDir, iniPath, project, report) && ok;

    if (!QFileInfo::exists(iniPath)) {
        report.addIssue(ImportIssueSeverity::Warning, iniPath, QStringLiteral("cepdlt645.ini 不存在，已仅导入设备 JSON"));
    }

    if (!project.southApps.contains(QStringLiteral("cepdlt645"))) {
        project.southApps.append(QStringLiteral("cepdlt645"));
    }

    return ok;
}

bool Dlt645ConfigImporter::importModelDirectory(const QString &modelDir,
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

bool Dlt645ConfigImporter::importDeviceDirectory(const QString &deviceDir,
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

bool Dlt645ConfigImporter::importModelFile(const QString &filePath,
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

bool Dlt645ConfigImporter::importDeviceFile(const QString &filePath,
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
    device.appType = QStringLiteral("cepdlt645");
    device.protocol = ProtocolType::Dlt645;
    device.deviceId = jsonValueToString(root.value(QStringLiteral("DeviceId")));
    device.deviceDesc = root.value(QStringLiteral("DeviceDesc")).toString();
    device.modelId = root.value(QStringLiteral("Model")).toString();
    device.transport.stationAddress = jsonValueToString(root.value(QStringLiteral("addr")));
    device.transport.serial = root.value(QStringLiteral("rtu")).toObject();
    device.source = makeSourceInfo(filePath);
    device.transport.source = device.source;
    device.dlt645 = importDeviceIniConfig(iniPath, device.deviceId, device.transport.stationAddress, report);

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

        if (bindingObject.contains(QStringLiteral("linkto"))) {
            binding.extensions.insert(QStringLiteral("linkto"), bindingObject.value(QStringLiteral("linkto")));
        }
        if (bindingObject.contains(QStringLiteral("virdot"))) {
            binding.extensions.insert(QStringLiteral("virdot"), bindingObject.value(QStringLiteral("virdot")));
        }

        annotateDlt645Binding(binding, device.dlt645);
        for (const QString &key : {
                 QStringLiteral("dlt645Kind"),
                 QStringLiteral("dlt645PointDI"),
                 QStringLiteral("dlt645PollDI"),
                 QStringLiteral("dlt645FunctionCode"),
                 QStringLiteral("dlt645DataType"),
                 QStringLiteral("dlt645DataLength"),
                 QStringLiteral("dlt645GroupNo"),
                 QStringLiteral("dlt645EntryNo")
             }) {
            if (bindingObject.contains(key)) {
                binding.extensions.insert(key, bindingObject.value(key));
            }
        }
        if (binding.enabled
            && !binding.address.trimmed().isEmpty()
            && !binding.extensions.contains(QStringLiteral("dlt645Kind"))) {
            report.addIssue(ImportIssueSeverity::Warning,
                            filePath,
                            QStringLiteral("点位 %1 的 dataIndex=%2 未匹配到 cepdlt645.ini 中的采集组或控制项")
                                .arg(binding.dataRef, binding.address));
        }

        device.bindings.append(binding);
    }

    if (device.deviceId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("DeviceId 不能为空"));
        return false;
    }

    if (device.transport.stationAddress.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning, filePath, QStringLiteral("DLT645 设备缺少 addr 表地址"));
    }

    project.devices.append(device);
    ++report.importedDeviceCount;
    return true;
}

Dlt645GlobalConfig Dlt645ConfigImporter::importGlobalIniConfig(const QString &iniPath) const
{
    Dlt645GlobalConfig config;
    if (!QFileInfo::exists(iniPath)) {
        return config;
    }

    QSettings settings(iniPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("485_para"));
    config.serialPort = settings.value(QStringLiteral("serialPort"), QStringLiteral("/dev/ttyS1")).toString();
    config.baud = settings.value(QStringLiteral("baud"), QStringLiteral("9600")).toString();
    config.dataBits = settings.value(QStringLiteral("dataBits"), QStringLiteral("8")).toString();
    config.stopBits = settings.value(QStringLiteral("stopBits"), QStringLiteral("1")).toString();
    config.parity = settings.value(QStringLiteral("parity"), QStringLiteral("even")).toString();
    config.frameInterval = settings.value(QStringLiteral("frameInterval"), QStringLiteral("100")).toString();
    config.hwVariant = settings.value(QStringLiteral("hw_variant")).toString();
    config.rawExtra = qSettingsGroupRawExtra(settings, {
        QStringLiteral("serialPort"),
        QStringLiteral("baud"),
        QStringLiteral("dataBits"),
        QStringLiteral("stopBits"),
        QStringLiteral("parity"),
        QStringLiteral("frameInterval"),
        QStringLiteral("hw_variant")
    });
    settings.endGroup();
    return config;
}

Dlt645DeviceConfig Dlt645ConfigImporter::importDeviceIniConfig(const QString &iniPath,
                                                               const QString &deviceId,
                                                               const QString &address,
                                                               ImportReport &report) const
{
    Dlt645DeviceConfig config;
    if (!QFileInfo::exists(iniPath) || deviceId.trimmed().isEmpty()) {
        return config;
    }

    QSettings settings(iniPath, QSettings::IniFormat);
    const QString deviceGroupName = QStringLiteral("dev_%1").arg(deviceId);
    const QString addrGroupName = dlt645AddrGroupName(address);
    const QStringList groups = settings.childGroups();
    const QString groupName = groups.contains(deviceGroupName)
        ? deviceGroupName
        : (groups.contains(addrGroupName) ? addrGroupName : QString());
    if (groupName.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Warning,
                        iniPath,
                        QStringLiteral("未找到与 DeviceId=%1 或 addr=%2 对应的 DLT645 配置节")
                            .arg(deviceId, address));
        return config;
    }

    settings.beginGroup(groupName);
    config.userId = settings.value(QStringLiteral("userID"), QStringLiteral("0")).toString();
    config.password = settings.value(QStringLiteral("password"), QStringLiteral("0")).toString();
    config.yxType = settings.value(QStringLiteral("yx_type"), QStringLiteral("BIN")).toString();
    config.ycType = settings.value(QStringLiteral("yc_type"), QStringLiteral("BCD")).toString();
    config.ytType = settings.value(QStringLiteral("yt_type"), QStringLiteral("BCD")).toString();

    const int yxPollNum = settings.value(QStringLiteral("yx_poll_num"), 0).toInt();
    const int ycPollNum = settings.value(QStringLiteral("yc_poll_num"), 0).toInt();
    const int ykSetNum = settings.value(QStringLiteral("yk_set_num"), 0).toInt();
    const int ytSetNum = settings.value(QStringLiteral("yt_set_num"), 0).toInt();

    QSet<QString> knownKeys = {
        QStringLiteral("userID"),
        QStringLiteral("password"),
        QStringLiteral("yx_type"),
        QStringLiteral("yc_type"),
        QStringLiteral("yt_type"),
        QStringLiteral("yx_poll_num"),
        QStringLiteral("yc_poll_num"),
        QStringLiteral("yk_set_num"),
        QStringLiteral("yt_set_num")
    };

    auto readPollGroups = [&](Dlt645PointKind kind, const QString &prefix, int count, const QString &defaultType) {
        for (int index = 1; index <= count; ++index) {
            const QString key = QStringLiteral("%1_poll%2").arg(prefix).arg(index);
            const QString line = settings.value(key).toString();
            knownKeys.insert(key);
            if (!isCompleteDlt645Line(line, 4)) {
                if (!line.trimmed().isEmpty()) {
                    report.addIssue(ImportIssueSeverity::Warning, iniPath, QStringLiteral("%1/%2 格式不完整").arg(groupName, key));
                }
                continue;
            }

            Dlt645PollGroup group;
            group.kind = kind;
            group.order = index;
            group.groupNo = parseDlt645LinePart(line, 1);
            group.funCode = parseDlt645LinePart(line, 2, 16);
            group.pollDi = parseDlt645DiPart(line, 3);
            group.dataLength = parseDlt645LinePart(line, 4);

            const QString typeKey = QStringLiteral("%1_type%2").arg(prefix).arg(index);
            group.dataType = settings.value(typeKey, defaultType).toString();
            knownKeys.insert(typeKey);

            config.pollGroups.append(group);
        }
    };

    readPollGroups(Dlt645PointKind::Yx, QStringLiteral("yx"), yxPollNum, config.yxType);
    readPollGroups(Dlt645PointKind::Yc, QStringLiteral("yc"), ycPollNum, config.ycType);

    auto readSetPoints = [&](Dlt645PointKind kind, const QString &prefix, int count, const QString &defaultType) {
        for (int index = 1; index <= count; ++index) {
            const QString key = QStringLiteral("%1_set%2").arg(prefix).arg(index);
            const QString line = settings.value(key).toString();
            knownKeys.insert(key);
            const int expectedParts = kind == Dlt645PointKind::Yt ? 5 : 4;
            if (!isCompleteDlt645Line(line, expectedParts)) {
                if (!line.trimmed().isEmpty()) {
                    report.addIssue(ImportIssueSeverity::Warning, iniPath, QStringLiteral("%1/%2 格式不完整").arg(groupName, key));
                }
                continue;
            }

            Dlt645SetPoint setPoint;
            setPoint.kind = kind;
            setPoint.order = index;
            setPoint.groupNo = parseDlt645LinePart(line, 1);
            setPoint.entryNo = parseDlt645LinePart(line, 2);
            setPoint.funCode = parseDlt645LinePart(line, 3, 16);
            setPoint.di = parseDlt645DiPart(line, 4);
            setPoint.dataLength = kind == Dlt645PointKind::Yt ? parseDlt645LinePart(line, 5) : 1;

            if (kind == Dlt645PointKind::Yt) {
                const QString typeKey = QStringLiteral("yt_type%1").arg(index);
                setPoint.dataType = settings.value(typeKey, defaultType).toString();
                knownKeys.insert(typeKey);
            } else {
                setPoint.dataType = QStringLiteral("BIN");
            }

            config.setPoints.append(setPoint);
        }
    };

    readSetPoints(Dlt645PointKind::Yk, QStringLiteral("yk"), ykSetNum, QStringLiteral("BIN"));
    readSetPoints(Dlt645PointKind::Yt, QStringLiteral("yt"), ytSetNum, config.ytType);

    config.rawExtra = qSettingsGroupRawExtra(settings, knownKeys);
    settings.endGroup();
    return config;
}

} // namespace configtool
