#include "mainwindow_config_p.h"
#include "network/ssh_client.h"
#include "ui/data_upload_policy_editor.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QHostAddress>
#include <QSaveFile>
#include <QUrl>

using namespace cepb_config_helpers;

namespace {

const QString kDefaultGatewayId = QStringLiteral("00010002000300040005032");
const QString kDefaultGatewayName = QStringLiteral("南网科技边缘网关");
const QString kDefaultNorthMqttGatewayId = QStringLiteral("000100020003000400051234");
const QString kDefaultNorthMqttBrokerIp = QStringLiteral("192.168.0.16");
const QString kDefaultNorthMqttPort = QStringLiteral("1883");
const QString kNorthCepPoliciesMetadataKey = QStringLiteral("northCepDataUploadPolicies");
const QString kNorthMqttPoliciesMetadataKey = QStringLiteral("northMqttDataUploadPolicies");

struct ConfigAppDirMigration {
    QString newName;
    QStringList oldNames;
};

struct SerialPortUse {
    QString appName;
    QString configuredPort;
};

struct NetworkEndpointUse {
    QString appName;
    QString objectName;
    QString ip;
    QString port;
    QString filePath;
};

bool isDeviceForSerialCheck(const configtool::ProtocolDeviceInstance &device,
                            configtool::ProtocolType protocol)
{
    if (device.protocol == protocol) {
        return true;
    }
    if (protocol == configtool::ProtocolType::Modbus) {
        return device.appType.compare(QStringLiteral("South_Modbus"), Qt::CaseInsensitive) == 0
            || device.appType.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0;
    }
    if (protocol == configtool::ProtocolType::Dlt645) {
        return device.appType.compare(QStringLiteral("South_645"), Qt::CaseInsensitive) == 0
            || device.appType.compare(QStringLiteral("cepdlt645"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

QString serialPortResourceKey(const QString &configuredPort)
{
    const QString port = configuredPort.trimmed();
    QString alias = port.toUpper();
    alias.remove(QChar('_'));
    const QRegularExpressionMatch aliasMatch =
        QRegularExpression(QStringLiteral("^RS485([1-8])$")).match(alias);
    if (aliasMatch.hasMatch()) {
        return QStringLiteral("RS485_%1").arg(aliasMatch.captured(1));
    }

    const QMap<QString, QString> deviceToResource = {
        {QStringLiteral("/dev/ttyS13"), QStringLiteral("RS485_1")},
        {QStringLiteral("/dev/ttyS2"), QStringLiteral("RS485_2")},
        {QStringLiteral("/dev/ttyS7"), QStringLiteral("RS485_3")},
        {QStringLiteral("/dev/ttyS10"), QStringLiteral("RS485_4")},
        {QStringLiteral("/dev/ttyS11"), QStringLiteral("RS485_4")},
        {QStringLiteral("/dev/ttyS9"), QStringLiteral("RS485_5")},
        {QStringLiteral("/dev/ttyS14"), QStringLiteral("RS485_6")},
        {QStringLiteral("/dev/ttyS12"), QStringLiteral("RS485_7")},
        {QStringLiteral("/dev/ttyS6"), QStringLiteral("RS485_8")}
    };
    if (deviceToResource.contains(port)) {
        return deviceToResource.value(port);
    }
    if (port.startsWith(QStringLiteral("ttyS"))) {
        const QString devicePath = QStringLiteral("/dev/%1").arg(port);
        return deviceToResource.value(devicePath, devicePath);
    }
    return port;
}

QString serialPortResourceDisplayName(const QString &resourceKey)
{
    const QMap<QString, QString> displayNames = {
        {QStringLiteral("RS485_1"), QStringLiteral("RS485_1（/dev/ttyS13）")},
        {QStringLiteral("RS485_2"), QStringLiteral("RS485_2（/dev/ttyS2）")},
        {QStringLiteral("RS485_3"), QStringLiteral("RS485_3（/dev/ttyS7）")},
        {QStringLiteral("RS485_4"), QStringLiteral("RS485_4（/dev/ttyS10 或 /dev/ttyS11）")},
        {QStringLiteral("RS485_5"), QStringLiteral("RS485_5（/dev/ttyS9）")},
        {QStringLiteral("RS485_6"), QStringLiteral("RS485_6（/dev/ttyS14）")},
        {QStringLiteral("RS485_7"), QStringLiteral("RS485_7（/dev/ttyS12）")},
        {QStringLiteral("RS485_8"), QStringLiteral("RS485_8（/dev/ttyS6）")}
    };
    return displayNames.value(resourceKey, resourceKey);
}

QStringList serialPortConflictDescriptions(const configtool::ConfigProject &project,
                                           int iec101CommunicationMode,
                                           const QString &iec101SerialPort)
{
    QMap<QString, QList<SerialPortUse>> usesByResource;
    const auto addUse = [&usesByResource](const QString &appName, const QString &configuredPort) {
        const QString trimmedPort = configuredPort.trimmed();
        const QString resourceKey = serialPortResourceKey(trimmedPort);
        if (trimmedPort.isEmpty() || resourceKey.isEmpty()) {
            return;
        }

        QList<SerialPortUse> &uses = usesByResource[resourceKey];
        for (const SerialPortUse &use : uses) {
            if (use.appName == appName && use.configuredPort == trimmedPort) {
                return;
            }
        }
        uses.append({appName, trimmedPort});
    };

    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (isDeviceForSerialCheck(device, configtool::ProtocolType::Modbus)) {
            const QString type = device.transport.protocolOptions
                                     .value(QStringLiteral("type"))
                                     .toString(QStringLiteral("TCP"))
                                     .trimmed()
                                     .toUpper();
            if (type == QStringLiteral("RTU")) {
                addUse(QStringLiteral("南向 Modbus"),
                       device.transport.serial.value(QStringLiteral("serialPort")).toVariant().toString());
            }
            continue;
        }

        if (isDeviceForSerialCheck(device, configtool::ProtocolType::Dlt645)) {
            QString port = device.transport.serial
                               .value(QStringLiteral("serialPort"))
                               .toVariant()
                               .toString()
                               .trimmed();
            if (port.isEmpty()) {
                port = project.dlt645.serialPort.trimmed();
            }
            if (port.isEmpty()) {
                port = QStringLiteral("/dev/ttyS1");
            }
            addUse(QStringLiteral("南向 645"), port);
        }
    }

    if (iec101CommunicationMode == 0 || iec101CommunicationMode == 2) {
        addUse(QStringLiteral("北向 101"), iec101SerialPort);
    }

    QStringList conflictLines;
    for (auto it = usesByResource.cbegin(); it != usesByResource.cend(); ++it) {
        QMap<QString, QStringList> portsByApp;
        for (const SerialPortUse &use : it.value()) {
            QStringList &ports = portsByApp[use.appName];
            if (!ports.contains(use.configuredPort)) {
                ports.append(use.configuredPort);
            }
        }
        if (portsByApp.size() < 2) {
            continue;
        }

        QStringList appDescriptions;
        for (auto appIt = portsByApp.cbegin(); appIt != portsByApp.cend(); ++appIt) {
            appDescriptions.append(QStringLiteral("%1（配置值：%2）")
                                       .arg(appIt.key(), appIt.value().join(QStringLiteral("、"))));
        }
        conflictLines.append(QStringLiteral("%1：%2")
                                 .arg(serialPortResourceDisplayName(it.key()),
                                      appDescriptions.join(QStringLiteral("；"))));
    }

    return conflictLines;
}

QString serialPortConflictMessage(const configtool::ConfigProject &project,
                                  int iec101CommunicationMode,
                                  const QString &iec101SerialPort)
{
    const QStringList conflictLines = serialPortConflictDescriptions(
        project, iec101CommunicationMode, iec101SerialPort);
    if (conflictLines.isEmpty()) {
        return QString();
    }
    QStringList bulletLines;
    for (const QString &conflict : conflictLines) {
        bulletLines.append(QStringLiteral("• %1").arg(conflict));
    }
    return QStringLiteral("检测到多个 APP 使用了同一个串口，已取消导出：\n\n%1\n\n"
                          "请修改南向 Modbus、南向 645 或北向 101 的串口配置后重试。")
        .arg(bulletLines.join(QChar('\n')));
}

QString normalizedNetworkIp(const QString &configuredIp)
{
    const QString ip = configuredIp.trimmed();
    QHostAddress address;
    if (address.setAddress(ip)) {
        return address.toString().toLower();
    }
    return ip.toLower();
}

QString normalizedNetworkPort(const QString &configuredPort)
{
    const QString port = configuredPort.trimmed();
    bool ok = false;
    const int number = port.toInt(&ok);
    return ok ? QString::number(number) : port;
}

QString networkEndpointText(const NetworkEndpointUse &use)
{
    return QStringLiteral("%1:%2").arg(use.ip, use.port);
}

QList<configtool::ImportIssue> networkEndpointConflictIssues(
    const configtool::ConfigProject &project,
    const QString &projectPath,
    const QString &northCepManagementPort,
    const QString &northCepDataPort,
    const QString &northMqttBrokerIp,
    const QString &northMqttBrokerPort,
    int iec101CommunicationMode,
    const QString &iec101TcpRole,
    const QString &iec101CodeIp,
    const QString &iec101CodePort,
    const QString &iec104CodeIp,
    const QString &iec104CodePort)
{
    QList<configtool::ImportIssue> issues;
    QList<NetworkEndpointUse> clientUses;
    QList<NetworkEndpointUse> listenerUses;

    const auto addUse = [](QList<NetworkEndpointUse> &uses,
                           const QString &appName,
                           const QString &objectName,
                           const QString &ip,
                           const QString &port,
                           const QString &filePath) {
        const QString normalizedIp = normalizedNetworkIp(ip);
        const QString normalizedPort = normalizedNetworkPort(port);
        if (normalizedIp.isEmpty() || normalizedPort.isEmpty()) {
            return;
        }
        uses.append({appName, objectName, normalizedIp, normalizedPort, filePath});
    };

    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        const QString devicePath = device.source.filePath.trimmed().isEmpty()
            ? projectPath
            : device.source.filePath;
        if (device.protocol == configtool::ProtocolType::Iec104) {
            addUse(clientUses,
                   QStringLiteral("南向 IEC104"),
                   device.deviceId,
                   device.transport.ip,
                   device.transport.port,
                   devicePath);
            continue;
        }
        if (!configtool::isModbusDevice(device)) {
            continue;
        }
        const QString type = device.transport.protocolOptions
                                 .value(QStringLiteral("type"))
                                 .toString(QStringLiteral("TCP"))
                                 .trimmed()
                                 .toUpper();
        if (type == QStringLiteral("TCP")) {
            addUse(clientUses,
                   QStringLiteral("南向 Modbus"),
                   device.deviceId,
                   device.transport.ip,
                   device.transport.port,
                   devicePath);
        }
    }

    addUse(clientUses,
           QStringLiteral("North_Mqtt"),
           QStringLiteral("Broker"),
           northMqttBrokerIp,
           northMqttBrokerPort,
           QDir(projectPath).filePath(QStringLiteral("North_Mqtt/etc/mainstation.json")));

    addUse(listenerUses,
           QStringLiteral("North_CEP"),
           QStringLiteral("管理通道"),
           QStringLiteral("0.0.0.0"),
           northCepManagementPort,
           QDir(projectPath).filePath(QStringLiteral("North_CEP/etc/mainstation.json")));
    addUse(listenerUses,
           QStringLiteral("North_CEP"),
           QStringLiteral("数据通道"),
           QStringLiteral("0.0.0.0"),
           northCepDataPort,
           QDir(projectPath).filePath(QStringLiteral("North_CEP/etc/mainstation.json")));
    if (iec101CommunicationMode == 1 || iec101CommunicationMode == 2) {
        if (iec101TcpRole == QStringLiteral("client")) {
            addUse(clientUses,
                   QStringLiteral("North_101"),
                   QStringLiteral("主站"),
                   iec101CodeIp,
                   iec101CodePort,
                   QDir(projectPath).filePath(QStringLiteral("North_101/config/localhost.json")));
        } else {
            addUse(listenerUses,
                   QStringLiteral("North_101"),
                   QStringLiteral("TCP 通道"),
                   QStringLiteral("0.0.0.0"),
                   iec101CodePort,
                   QDir(projectPath).filePath(QStringLiteral("North_101/config/localhost.json")));
        }
    }
    addUse(listenerUses,
           QStringLiteral("North_104"),
           QStringLiteral("TCP 通道"),
           iec104CodeIp,
           iec104CodePort,
           QDir(projectPath).filePath(QStringLiteral("North_104/config/localhost.json")));

    QMap<QString, QList<NetworkEndpointUse>> clientUsesByEndpoint;
    for (const NetworkEndpointUse &use : clientUses) {
        clientUsesByEndpoint[networkEndpointText(use)].append(use);
    }
    for (auto it = clientUsesByEndpoint.cbegin(); it != clientUsesByEndpoint.cend(); ++it) {
        QMap<QString, QStringList> objectsByApp;
        for (const NetworkEndpointUse &use : it.value()) {
            QStringList &objects = objectsByApp[use.appName];
            if (!objects.contains(use.objectName)) {
                objects.append(use.objectName);
            }
        }
        if (objectsByApp.size() < 2) {
            continue;
        }

        QStringList participants;
        for (auto appIt = objectsByApp.cbegin(); appIt != objectsByApp.cend(); ++appIt) {
            participants.append(QStringLiteral("%1（%2）")
                                    .arg(appIt.key(), appIt.value().join(QStringLiteral("、"))));
        }
        configtool::ImportIssue issue;
        issue.severity = configtool::ImportIssueSeverity::Error;
        issue.filePath = it.value().constFirst().filePath;
        issue.message = QStringLiteral("网络客户端端点被多个 APP 重复配置：%1；%2")
                            .arg(it.key(), participants.join(QStringLiteral("；")));
        issues.append(issue);
    }

    for (int firstIndex = 0; firstIndex < listenerUses.size(); ++firstIndex) {
        const NetworkEndpointUse &first = listenerUses.at(firstIndex);
        for (int secondIndex = firstIndex + 1; secondIndex < listenerUses.size(); ++secondIndex) {
            const NetworkEndpointUse &second = listenerUses.at(secondIndex);
            if (first.appName == second.appName || first.port != second.port) {
                continue;
            }
            const bool ipOverlaps = first.ip == second.ip
                || first.ip == QStringLiteral("0.0.0.0")
                || second.ip == QStringLiteral("0.0.0.0");
            if (!ipOverlaps) {
                continue;
            }

            configtool::ImportIssue issue;
            issue.severity = configtool::ImportIssueSeverity::Error;
            issue.filePath = first.filePath;
            issue.message = QStringLiteral("网络监听端点冲突：%1 %2（%3）与 %4 %5（%6）；"
                                           "0.0.0.0 会占用本机所有 IPv4 地址")
                                .arg(first.appName,
                                     first.objectName,
                                     networkEndpointText(first),
                                     second.appName,
                                     second.objectName,
                                     networkEndpointText(second));
            issues.append(issue);
        }
    }

    return issues;
}

QList<ConfigAppDirMigration> configAppDirMigrations()
{
    return {
        {QStringLiteral("North_CEP"), {QStringLiteral("ServiceChannel")}},
        {QStringLiteral("North_101"), {QStringLiteral("IEC101ServiceChannel")}},
        {QStringLiteral("North_104"), {QStringLiteral("IEC104ServiceChannel")}},
        {QStringLiteral("North_Mqtt"), {QStringLiteral("MqttServiceChannel")}},
        {QStringLiteral("LogicCenter"), {QStringLiteral("cepLogicCenter")}},
        {QStringLiteral("South_Modbus"), {QStringLiteral("cepmodbus")}},
        {QStringLiteral("South_104"), {QStringLiteral("cepiec104")}},
        {QStringLiteral("South_645"), {QStringLiteral("cepdlt645")}}
    };
}

bool migrateOneConfigAppDir(const QDir &rootDir,
                            const QString &oldName,
                            const QString &newName,
                            configtool::ExportReport &report)
{
    const QString oldPath = rootDir.filePath(oldName);
    const QString newPath = rootDir.filePath(newName);
    const QFileInfo oldInfo(oldPath);
    if (!oldInfo.exists() || !oldInfo.isDir()) {
        return true;
    }

    if (!QFileInfo::exists(newPath)) {
        if (!QDir().rename(oldPath, newPath)) {
            report.addIssue(configtool::ImportIssueSeverity::Error,
                            oldPath,
                            QStringLiteral("无法将旧配置目录 %1 重命名为 %2").arg(oldName, newName));
            return false;
        }
        report.addIssue(configtool::ImportIssueSeverity::Info,
                        newPath,
                        QStringLiteral("已将旧配置目录 %1 重命名为 %2").arg(oldName, newName));
        return true;
    }

    const QString backupName = QStringLiteral("%1_legacy_%2")
        .arg(oldName, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss")));
    const QString backupPath = rootDir.filePath(backupName);
    if (!QDir().rename(oldPath, backupPath)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        oldPath,
                        QStringLiteral("目标目录 %1 已存在，且无法将旧目录 %2 备份为 %3")
                            .arg(newName, oldName, backupName));
        return false;
    }
    report.addIssue(configtool::ImportIssueSeverity::Warning,
                    backupPath,
                    QStringLiteral("目标目录 %1 已存在，旧目录 %2 已备份为 %3，本次保存写入新目录")
                        .arg(newName, oldName, backupName));
    return true;
}

bool migrateLegacyConfigAppDirs(const QString &projectRoot, configtool::ExportReport &report)
{
    const QFileInfo rootInfo(projectRoot);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        projectRoot,
                        QStringLiteral("配置工程目录不存在，无法迁移旧 APP 目录"));
        return false;
    }

    bool ok = true;
    const QDir rootDir(rootInfo.absoluteFilePath());
    for (const ConfigAppDirMigration &migration : configAppDirMigrations()) {
        for (const QString &oldName : migration.oldNames) {
            ok = migrateOneConfigAppDir(rootDir, oldName, migration.newName, report) && ok;
        }
    }
    return ok;
}

QString findIec101LocalhostConfig(const QString &projectRoot, configtool::ImportReport &report)
{
    const QDir rootDir(projectRoot);
    const QStringList appDirNames = {
        QStringLiteral("North_101"),
        QStringLiteral("IEC101ServiceChannel")
    };

    // 优先使用约定的标准文件名，即使新旧 APP 目录同时存在。
    for (const QString &appDirName : appDirNames) {
        const QString filePath = rootDir.filePath(
            QStringLiteral("%1/config/localhost.json").arg(appDirName));
        if (QFileInfo(filePath).isFile()) {
            return filePath;
        }
    }

    // 兼容旧设备使用 localhost-55.json 等带后缀的文件名。
    for (const QString &appDirName : appDirNames) {
        const QString configDirPath = rootDir.filePath(
            QStringLiteral("%1/config").arg(appDirName));
        QDir configDir(configDirPath);
        if (!configDir.exists()) {
            continue;
        }

        const QStringList matchingFiles = configDir.entryList(
            {QStringLiteral("localhost*.json")},
            QDir::Files | QDir::Readable,
            QDir::Name | QDir::IgnoreCase);
        if (matchingFiles.isEmpty()) {
            continue;
        }

        const QString selectedPath = configDir.filePath(matchingFiles.first());
        const QString detail = matchingFiles.size() == 1
            ? QStringLiteral("未找到 localhost.json，已兼容读取旧文件名 %1；下次保存将统一为 localhost.json")
                  .arg(matchingFiles.first())
            : QStringLiteral("未找到 localhost.json，发现多个兼容文件（%1），已按文件名顺序读取 %2；下次保存将统一为 localhost.json")
                  .arg(matchingFiles.join(QStringLiteral("，")), matchingFiles.first());
        report.addIssue(configtool::ImportIssueSeverity::Warning, selectedPath, detail);
        return selectedPath;
    }

    return QString();
}

bool removeAlternateIec101LocalhostConfigs(const QString &configDirPath,
                                           configtool::ExportReport &report)
{
    QDir configDir(configDirPath);
    const QStringList matchingFiles = configDir.entryList(
        {QStringLiteral("localhost*.json")},
        QDir::Files,
        QDir::Name | QDir::IgnoreCase);

    QStringList removedFiles;
    for (const QString &fileName : matchingFiles) {
        if (fileName == QStringLiteral("localhost.json")) {
            continue;
        }

        const QString filePath = configDir.filePath(fileName);
        if (!QFile::remove(filePath)) {
            report.addIssue(configtool::ImportIssueSeverity::Error,
                            filePath,
                            QStringLiteral("无法清理旧 IEC101 配置文件；为避免多个配置文件竞争，本次保存已取消"));
            return false;
        }
        removedFiles.append(fileName);
    }

    if (!removedFiles.isEmpty()) {
        report.addIssue(configtool::ImportIssueSeverity::Info,
                        configDirPath,
                        QStringLiteral("已将 IEC101 配置统一为 localhost.json，并清理旧文件：%1")
                            .arg(removedFiles.join(QStringLiteral("，"))));
    }
    return true;
}

bool writeNorthCepMainstationConfig(const QString &projectRoot,
                                    const QString &managementPort,
                                    const QString &dataPort,
                                    configtool::ExportReport &report)
{
    const QString etcDirPath = QDir(projectRoot).filePath(QStringLiteral("North_CEP/etc"));
    const QString filePath = QDir(etcDirPath).filePath(QStringLiteral("mainstation.json"));
    if (!QDir().mkpath(etcDirPath)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法创建 North_CEP 主站配置目录: %1").arg(etcDirPath));
        return false;
    }

    QJsonObject station;
    station.insert(QStringLiteral("sl_ip"), QStringLiteral("0.0.0.0"));
    station.insert(QStringLiteral("ms1_ip"), QStringLiteral("255.255.255.255"));
    station.insert(QStringLiteral("ms2_ip"), QStringLiteral("0.0.0.0"));
    station.insert(QStringLiteral("ms3_ip"), QStringLiteral("0.0.0.0"));
    station.insert(QStringLiteral("ms4_ip"), QStringLiteral("0.0.0.0"));
    station.insert(QStringLiteral("port1"), managementPort);
    station.insert(QStringLiteral("port2"), dataPort);

    QJsonObject root;
    root.insert(QStringLiteral("mainstation"), QJsonArray{station});

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法写入 North_CEP 主站配置文件: %1").arg(file.errorString()));
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool writeNorthMqttMainstationConfig(const QString &projectRoot,
                                     const QString &gatewayId,
                                     const QString &brokerIp,
                                     const QString &port,
                                     const QString &username,
                                     const QString &password,
                                     configtool::ExportReport &report)
{
    const QString etcDirPath = QDir(projectRoot).filePath(QStringLiteral("North_Mqtt/etc"));
    const QString filePath = QDir(etcDirPath).filePath(QStringLiteral("mainstation.json"));
    if (!QDir().mkpath(etcDirPath)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法创建 North_Mqtt 主站配置目录: %1").arg(etcDirPath));
        return false;
    }

    QJsonObject station;
    station.insert(QStringLiteral("broker_ip"), brokerIp);
    station.insert(QStringLiteral("port"), port);
    station.insert(QStringLiteral("username"), username);
    station.insert(QStringLiteral("password"), password);

    QJsonObject root;
    root.insert(QStringLiteral("gatewayId"), gatewayId);
    root.insert(QStringLiteral("mainstation"), QJsonArray{station});

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法写入 North_Mqtt 主站配置文件: %1").arg(output.errorString()));
        return false;
    }
    const QByteArray outputData = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (output.write(outputData) != outputData.size()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("写入 North_Mqtt 主站配置文件失败: %1").arg(output.errorString()));
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("提交 North_Mqtt 主站配置文件失败: %1").arg(output.errorString()));
        return false;
    }
    return true;
}

QStringList configAppFolderNames()
{
    QStringList names;
    for (const ConfigAppDirMigration &migration : configAppDirMigrations()) {
        names << migration.newName;
        names << migration.oldNames;
    }
    return names;
}

} // namespace

void MainWindow::clearNorthCepConfigPage()
{
    if (m_northCepGatewayIdEdit) {
        m_northCepGatewayIdEdit->setText(kDefaultGatewayId);
    }
    if (m_northCepGatewayNameEdit) {
        m_northCepGatewayNameEdit->setText(kDefaultGatewayName);
    }
    if (m_northCepManagementPortEdit) {
        m_northCepManagementPortEdit->setText(QStringLiteral("9901"));
    }
    if (m_northCepDataPortEdit) {
        m_northCepDataPortEdit->setText(QStringLiteral("9902"));
    }
    m_configProjectManager.project().metadata.remove(kNorthCepPoliciesMetadataKey);
    refreshNorthCepDataUploadEditor();
}

void MainWindow::loadNorthCepMainstationConfig(const QString &filePath, configtool::ImportReport &report)
{
    clearNorthCepConfigPage();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("无法读取 North_CEP 主站配置，已使用默认端口: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonArray stations = document.isObject()
        ? document.object().value(QStringLiteral("mainstation")).toArray()
        : QJsonArray();
    if (parseError.error != QJsonParseError::NoError || stations.isEmpty() || !stations.first().isObject()) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_CEP mainstation.json 格式无效，已使用默认端口"));
        return;
    }

    const QJsonObject station = stations.first().toObject();
    const auto portText = [&station](const QString &key) {
        const QJsonValue value = station.value(key);
        return value.isDouble() ? QString::number(value.toInt()) : value.toString().trimmed();
    };
    const auto isValidPort = [](const QString &text) {
        bool ok = false;
        const int port = text.toInt(&ok);
        return ok && port >= 1 && port <= 65535;
    };

    const QString managementPort = portText(QStringLiteral("port1"));
    const QString dataPort = portText(QStringLiteral("port2"));
    if (isValidPort(managementPort)) {
        m_northCepManagementPortEdit->setText(managementPort);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_CEP 管理通道端口无效，已使用默认值 9901"));
    }
    if (isValidPort(dataPort)) {
        m_northCepDataPortEdit->setText(dataPort);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_CEP 数据通道端口无效，已使用默认值 9902"));
    }
}

void MainWindow::clearNorthMqttConfigPage()
{
    if (m_northMqttGatewayIdEdit) {
        m_northMqttGatewayIdEdit->setText(kDefaultNorthMqttGatewayId);
    }
    if (m_northMqttBrokerIpEdit) {
        m_northMqttBrokerIpEdit->setText(kDefaultNorthMqttBrokerIp);
    }
    if (m_northMqttPortEdit) {
        m_northMqttPortEdit->setText(kDefaultNorthMqttPort);
    }
    if (m_northMqttUsernameEdit) {
        m_northMqttUsernameEdit->clear();
    }
    if (m_northMqttPasswordEdit) {
        m_northMqttPasswordEdit->clear();
    }
    m_configProjectManager.project().metadata.remove(kNorthMqttPoliciesMetadataKey);
    refreshMqttDataUploadSummary();
}

void MainWindow::loadNorthMqttMainstationConfig(const QString &filePath,
                                                configtool::ImportReport &report)
{
    clearNorthMqttConfigPage();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("无法读取 North_Mqtt 主站配置，已使用默认值: %1")
                            .arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject root = document.isObject() ? document.object() : QJsonObject();
    const QJsonArray stations = root.value(QStringLiteral("mainstation")).toArray();
    if (parseError.error != QJsonParseError::NoError || root.isEmpty()
        || stations.isEmpty() || !stations.first().isObject()) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_Mqtt mainstation.json 格式无效，已使用默认值"));
        return;
    }

    const QString gatewayId = root.value(QStringLiteral("gatewayId")).toString().trimmed();
    if (!gatewayId.isEmpty() && gatewayId.toUtf8().size() <= 24) {
        m_northMqttGatewayIdEdit->setText(gatewayId);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_Mqtt gatewayId 无效，已使用默认值"));
    }

    const QJsonObject station = stations.first().toObject();
    const QString brokerIp = station.value(QStringLiteral("broker_ip")).toString().trimmed();
    QHostAddress brokerAddress;
    if (brokerAddress.setAddress(brokerIp)
        && brokerAddress.protocol() == QAbstractSocket::IPv4Protocol) {
        m_northMqttBrokerIpEdit->setText(brokerIp);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_Mqtt broker_ip 无效，已使用默认值"));
    }

    const QJsonValue portValue = station.value(QStringLiteral("port"));
    const QString portText = portValue.isDouble()
        ? QString::number(portValue.toInt())
        : portValue.toString().trimmed();
    bool portOk = false;
    const int port = portText.toInt(&portOk);
    if (portOk && port >= 1 && port <= 65535) {
        m_northMqttPortEdit->setText(portText);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_Mqtt Broker 端口无效，已使用默认值 1883"));
    }

    m_northMqttUsernameEdit->setText(
        station.value(QStringLiteral("username")).toString());
    m_northMqttPasswordEdit->setText(
        station.value(QStringLiteral("password")).toString());
}

void MainWindow::loadNorthMqttSubDataConfig(const QString &filePath,
                                            configtool::ImportReport &report)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("无法读取 North_Mqtt 数据上送策略: %1")
                            .arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_Mqtt subData.cfg 格式无效: %1")
                            .arg(parseError.errorString()));
        return;
    }

    m_configProjectManager.project().metadata.insert(
        kNorthMqttPoliciesMetadataKey,
        DataUploadPolicyEditor::importBusinessPolicies(document.object()));
    refreshMqttDataUploadSummary();
}

void MainWindow::loadNorthCepSubDataConfig(const QString &filePath,
                                           configtool::ImportReport &report)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("无法读取 North_CEP 数据上送策略: %1")
                            .arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("North_CEP subData.cfg 格式无效: %1")
                            .arg(parseError.errorString()));
        return;
    }

    m_configProjectManager.project().metadata.insert(
        kNorthCepPoliciesMetadataKey,
        DataUploadPolicyEditor::importBusinessPolicies(document.object()));
    refreshNorthCepDataUploadEditor();
}

bool MainWindow::writeNorthCepSubDataConfig(const QString &projectRoot,
                                            configtool::ExportReport &report) const
{
    const QString cfgDirPath = QDir(projectRoot).filePath(QStringLiteral("North_CEP/cfg"));
    const QString filePath = QDir(cfgDirPath).filePath(QStringLiteral("subData.cfg"));
    if (!QDir().mkpath(cfgDirPath)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法创建 North_CEP 数据策略目录: %1").arg(cfgDirPath));
        return false;
    }

    const configtool::ConfigProject &project = m_configProjectManager.project();
    const QJsonObject savedPolicies = m_northCepDataUploadEditor
        ? m_northCepDataUploadEditor->policies()
        : project.metadata.value(kNorthCepPoliciesMetadataKey).toObject();
    const QJsonObject subData = DataUploadPolicyEditor::buildSubDataConfig(
        project, savedPolicies);

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法写入 North_CEP 数据上送策略: %1")
                            .arg(output.errorString()));
        return false;
    }
    const QByteArray data = QJsonDocument(subData).toJson(QJsonDocument::Indented);
    if (output.write(data) != data.size()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("写入 North_CEP 数据上送策略失败: %1")
                            .arg(output.errorString()));
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("提交 North_CEP 数据上送策略失败: %1")
                            .arg(output.errorString()));
        return false;
    }
    return true;
}

void MainWindow::refreshNorthCepDataUploadEditor()
{
    if (!m_northCepDataUploadEditor) {
        return;
    }
    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_northCepDataUploadEditor->setProject(
        project.projectId.trimmed().isEmpty() ? nullptr : &project,
        project.metadata.value(kNorthCepPoliciesMetadataKey).toObject());
}

bool MainWindow::writeNorthMqttSubDataConfig(const QString &projectRoot,
                                             configtool::ExportReport &report) const
{
    const QString cfgDirPath = QDir(projectRoot).filePath(QStringLiteral("North_Mqtt/cfg"));
    const QString filePath = QDir(cfgDirPath).filePath(QStringLiteral("subData.cfg"));
    if (!QDir().mkpath(cfgDirPath)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法创建 North_Mqtt 数据策略目录: %1").arg(cfgDirPath));
        return false;
    }

    const configtool::ConfigProject &project = m_configProjectManager.project();
    const QJsonObject savedPolicies = m_northMqttDataUploadEditor
        ? m_northMqttDataUploadEditor->policies()
        : project.metadata.value(kNorthMqttPoliciesMetadataKey).toObject();
    const QJsonObject subData = DataUploadPolicyEditor::buildSubDataConfig(
        project, savedPolicies);

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法写入 North_Mqtt 数据上送策略: %1")
                            .arg(output.errorString()));
        return false;
    }
    const QByteArray data = QJsonDocument(subData).toJson(QJsonDocument::Indented);
    if (output.write(data) != data.size()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("写入 North_Mqtt 数据上送策略失败: %1")
                            .arg(output.errorString()));
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("提交 North_Mqtt 数据上送策略失败: %1")
                            .arg(output.errorString()));
        return false;
    }
    return true;
}

void MainWindow::refreshMqttDataUploadSummary()
{
    if (!m_northMqttDataUploadEditor) {
        return;
    }
    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_northMqttDataUploadEditor->setProject(
        project.projectId.trimmed().isEmpty() ? nullptr : &project,
        project.metadata.value(kNorthMqttPoliciesMetadataKey).toObject());
}

void MainWindow::loadNorthCepSystemConfig(const QString &filePath,
                                          configtool::ImportReport &report)
{
    if (m_northCepGatewayIdEdit) {
        m_northCepGatewayIdEdit->setText(kDefaultGatewayId);
    }
    if (m_northCepGatewayNameEdit) {
        m_northCepGatewayNameEdit->setText(kDefaultGatewayName);
    }

    QFile file(filePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("无法读取 system.json 中的网关信息，已使用默认值: %1")
                            .arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("system.json 格式无效，网关 ID 和名称已使用默认值"));
        return;
    }

    const QJsonObject root = document.object();
    const QString gatewayId = root.value(QStringLiteral("gateWayId")).toString().trimmed();
    const QString gatewayName = root.value(QStringLiteral("gateWayName")).toString().trimmed();
    if (!gatewayId.isEmpty()) {
        m_northCepGatewayIdEdit->setText(gatewayId);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("gateWayId 缺失或为空，已使用默认值"));
    }
    if (!gatewayName.isEmpty()) {
        m_northCepGatewayNameEdit->setText(gatewayName);
    } else {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        filePath,
                        QStringLiteral("gateWayName 缺失或为空，已使用默认值"));
    }
}

bool MainWindow::validateNorthCepConfig(QString *errorMessage) const
{
    const QString gatewayId = m_northCepGatewayIdEdit
        ? m_northCepGatewayIdEdit->text().trimmed()
        : QString();
    const QString gatewayName = m_northCepGatewayNameEdit
        ? m_northCepGatewayNameEdit->text().trimmed()
        : QString();
    if (gatewayId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网关 ID 不能为空。");
        }
        return false;
    }
    if (gatewayId.toUtf8().size() > 24) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网关 ID 的 UTF-8 编码长度不能超过 24 字节。");
        }
        return false;
    }
    if (gatewayName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网关名称不能为空。");
        }
        return false;
    }
    if (gatewayName.toUtf8().size() > 63) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网关名称的 UTF-8 编码长度不能超过 63 字节。");
        }
        return false;
    }

    const QString managementPortText = m_northCepManagementPortEdit
        ? m_northCepManagementPortEdit->text().trimmed()
        : QString();
    const QString dataPortText = m_northCepDataPortEdit
        ? m_northCepDataPortEdit->text().trimmed()
        : QString();
    bool managementOk = false;
    bool dataOk = false;
    const int managementPort = managementPortText.toInt(&managementOk);
    const int dataPort = dataPortText.toInt(&dataOk);
    if (!managementOk || managementPort < 1 || managementPort > 65535) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("管理通道端口必须是 1～65535 之间的整数。");
        }
        return false;
    }
    if (!dataOk || dataPort < 1 || dataPort > 65535) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据通道端口必须是 1～65535 之间的整数。");
        }
        return false;
    }
    if (managementPort == dataPort) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("管理通道端口和数据通道端口不能相同。");
        }
        return false;
    }
    return true;
}

bool MainWindow::validateNorthMqttConfig(QString *errorMessage) const
{
    const QString gatewayId = m_northMqttGatewayIdEdit
        ? m_northMqttGatewayIdEdit->text().trimmed()
        : QString();
    if (gatewayId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网关 ID 不能为空。");
        }
        return false;
    }
    if (gatewayId.toUtf8().size() > 24) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网关 ID 的 UTF-8 编码长度不能超过 24 字节。");
        }
        return false;
    }

    const QString brokerIp = m_northMqttBrokerIpEdit
        ? m_northMqttBrokerIpEdit->text().trimmed()
        : QString();
    QHostAddress brokerAddress;
    if (!brokerAddress.setAddress(brokerIp)
        || brokerAddress.protocol() != QAbstractSocket::IPv4Protocol) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Broker IP 必须是有效的 IPv4 地址。");
        }
        return false;
    }

    const QString portText = m_northMqttPortEdit
        ? m_northMqttPortEdit->text().trimmed()
        : QString();
    bool portOk = false;
    const int port = portText.toInt(&portOk);
    if (!portOk || port < 1 || port > 65535) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Broker 端口必须是 1～65535 之间的整数。");
        }
        return false;
    }
    return true;
}

bool MainWindow::writeNorthCepSystemConfig(const QString &projectRoot,
                                           configtool::ExportReport &report) const
{
    const QString etcDirPath = QDir(projectRoot).filePath(QStringLiteral("etc"));
    const QString filePath = QDir(etcDirPath).filePath(QStringLiteral("system.json"));
    if (!QDir().mkpath(etcDirPath)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法创建 system.json 配置目录: %1").arg(etcDirPath));
        return false;
    }

    QJsonObject root;
    QFile existing(filePath);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
            report.addIssue(configtool::ImportIssueSeverity::Error,
                            filePath,
                            QStringLiteral("无法读取现有 system.json: %1").arg(existing.errorString()));
            return false;
        }
        const QByteArray existingData = existing.readAll();
        existing.close();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(existingData, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            report.addIssue(configtool::ImportIssueSeverity::Error,
                            filePath,
                            QStringLiteral("现有 system.json 格式无效，已停止覆盖"));
            return false;
        }
        root = document.object();
    }

    root.insert(QStringLiteral("gateWayId"), m_northCepGatewayIdEdit->text().trimmed());
    root.insert(QStringLiteral("gateWayName"), m_northCepGatewayNameEdit->text().trimmed());

    // 当前 North_CEP 仍依赖这组兼容字段，但暂不在上位机开放编辑。
    root.insert(QStringLiteral("sendPriv"),
                QStringLiteral("116fb2240aecd69a82dee47153abd8e467bba75bc502cf72d05922f6281482d1"));
    root.insert(QStringLiteral("sendPubX"),
                QStringLiteral("43ccdb8a0c97bef4c39c29f784cb6a5979449eefa6ba616b512513ebb6993344"));
    root.insert(QStringLiteral("sendPubY"),
                QStringLiteral("d37dda9c264909ae165dec4e256d978162165d92a6f9614612801d3469919394"));
    root.insert(QStringLiteral("recvPubX"),
                QStringLiteral("9c43b06e168e5bf6e68e4df1d75b54306beb82258aa14ea005f2acba5abfbb08"));
    root.insert(QStringLiteral("recvPubY"),
                QStringLiteral("c07bbabb74888422d4ce26142ededd78baa4b63b7fa27775f875c18834d9b7b7"));
    root.insert(QStringLiteral("baseHostPath"), QStringLiteral("/home/cepgateway/app"));
    root.insert(QStringLiteral("baseContainerPath"), QStringLiteral("/opt/app"));
    root.insert(QStringLiteral("baseEnvirPath"), QStringLiteral("/opt/app/lib"));
    root.insert(QStringLiteral("baseImage"), QStringLiteral("centos:latest"));

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("无法写入 system.json: %1").arg(output.errorString()));
        return false;
    }
    const QByteArray outputData = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (output.write(outputData) != outputData.size()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("写入 system.json 失败: %1").arg(output.errorString()));
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("提交 system.json 失败: %1").arg(output.errorString()));
        return false;
    }
    return true;
}

void MainWindow::onBrowseConfigImportDirClicked()
{
    const QString startDir = configBrowseStartDir();
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择配置工程目录"),
        startDir);
    if (!dir.isEmpty()) {
        const QString projectRoot = normalizedConfigProjectRoot(dir);
        m_configImportDirEdit->setText(projectRoot);
        QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
        settings.setValue(QStringLiteral("config/lastBrowseDir"), projectRoot);
        statusBar()->showMessage(QStringLiteral("已选择配置工程目录"), 5000);
    }
}

void MainWindow::tryAutoOpenLastConfig()
{
    if (!m_configImportDirEdit) {
        return;
    }

    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    QString projectRoot = settings.value(QStringLiteral("config/lastOpenedDir"))
                              .toString().trimmed();
    if (projectRoot.isEmpty()) {
        projectRoot = settings.value(QStringLiteral("config/lastBrowseDir"))
                          .toString().trimmed();
    }
    projectRoot = normalizedConfigProjectRoot(projectRoot);
    if (projectRoot.isEmpty() || !QFileInfo(projectRoot).isDir()) {
        return;
    }

    const bool hasImportableConfig = !resolveIec104AppDir(projectRoot).isEmpty()
        || !resolveModbusAppDir(projectRoot).isEmpty()
        || !resolveDlt645AppDir(projectRoot).isEmpty()
        || !resolveLogicCenterAppDir(projectRoot).isEmpty()
        || !resolveNorthCepAppDir(projectRoot).isEmpty()
        || !resolveNorthMqttAppDir(projectRoot).isEmpty()
        || !resolveIec101ServiceChannelAppDir(projectRoot).isEmpty()
        || !resolveIec104ServiceChannelAppDir(projectRoot).isEmpty();
    if (!hasImportableConfig) {
        statusBar()->showMessage(QStringLiteral("上次工程目录中未找到可自动打开的配置"), 5000);
        return;
    }

    m_configImportDirEdit->setText(projectRoot);
    statusBar()->showMessage(QStringLiteral("正在自动打开上次配置…"));
    onImportIec104ConfigClicked();
}

void MainWindow::onOpenConfigDirClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开工程目录"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QFileInfo projectInfo(projectRoot);
    if (!projectInfo.exists() || !projectInfo.isDir()) {
        QMessageBox::warning(this,
                             QStringLiteral("打开工程目录"),
                             QStringLiteral("工程目录不存在：\n%1").arg(projectRoot));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(projectInfo.absoluteFilePath()))) {
        QMessageBox::warning(this,
                             QStringLiteral("打开工程目录"),
                             QStringLiteral("无法打开工程目录：\n%1").arg(projectInfo.absoluteFilePath()));
    }
}

void MainWindow::onImportIec104ConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString modbusAppDir = resolveModbusAppDir(projectRoot);
    const QString dlt645AppDir = resolveDlt645AppDir(projectRoot);
    const QString logicCenterAppDir = resolveLogicCenterAppDir(projectRoot);
    const QString northCepAppDir = resolveNorthCepAppDir(projectRoot);
    const QString northMqttAppDir = resolveNorthMqttAppDir(projectRoot);
    const QString iec101AppDir = resolveIec101ServiceChannelAppDir(projectRoot);
    const QString iec104NorthAppDir = resolveIec104ServiceChannelAppDir(projectRoot);
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && dlt645AppDir.isEmpty()
        && logicCenterAppDir.isEmpty() && northCepAppDir.isEmpty()
        && northMqttAppDir.isEmpty() && iec101AppDir.isEmpty()
        && iec104NorthAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到可导入的 APP 配置目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);
    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    settings.setValue(QStringLiteral("config/lastBrowseDir"), projectRoot);

    configtool::ImportReport report;
    const QString projectName = QFileInfo(projectRoot).fileName().trimmed().isEmpty()
        ? QStringLiteral("配置工程")
        : QFileInfo(projectRoot).fileName();
    m_configProjectManager.createEmptyProject(projectName, projectRoot);
    bool ok = true;
    QString logRetentionError;
    if (!loadLogRetentionConfigFromProject(projectRoot, &logRetentionError)) {
        report.addIssue(configtool::ImportIssueSeverity::Warning,
                        QDir(projectRoot).filePath(QStringLiteral("etc/system.json")),
                        logRetentionError);
    }
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.importIec104AppDirectory(iec104AppDir, report) && ok;
    }
    if (!modbusAppDir.isEmpty()) {
        ok = m_configProjectManager.importModbusAppDirectory(modbusAppDir, report) && ok;
    }
    if (!dlt645AppDir.isEmpty()) {
        ok = m_configProjectManager.importDlt645AppDirectory(dlt645AppDir, report) && ok;
    }
    if (!logicCenterAppDir.isEmpty()) {
        const QString logicConfigPath = QDir(logicCenterAppDir)
            .filePath(QStringLiteral("etc/LogicCenter_Config.json"));
        if (QFileInfo::exists(logicConfigPath)) {
            ok = m_configProjectManager.importLogicCenterConfigFile(logicConfigPath, report) && ok;
        } else {
            report.addIssue(configtool::ImportIssueSeverity::Warning,
                            logicConfigPath,
                            QStringLiteral("未找到 LogicCenter_Config.json，已跳过 LogicCenter 配置导入"));
        }
    }
    // ---- North_CEP 配置（可选） ----
    if (!northCepAppDir.isEmpty()) {
        const QString mainstationPath = QDir(northCepAppDir)
            .filePath(QStringLiteral("etc/mainstation.json"));
        if (QFileInfo::exists(mainstationPath)) {
            loadNorthCepMainstationConfig(mainstationPath, report);
        } else {
            clearNorthCepConfigPage();
        }
        const QString subDataPath = QDir(northCepAppDir)
            .filePath(QStringLiteral("cfg/subData.cfg"));
        if (QFileInfo::exists(subDataPath)) {
            loadNorthCepSubDataConfig(subDataPath, report);
        } else {
            refreshNorthCepDataUploadEditor();
        }
    } else {
        clearNorthCepConfigPage();
    }
    loadNorthCepSystemConfig(
        QDir(projectRoot).filePath(QStringLiteral("etc/system.json")),
        report);
    // ---- North_Mqtt 配置（可选） ----
    if (!northMqttAppDir.isEmpty()) {
        const QString mainstationPath = QDir(northMqttAppDir)
            .filePath(QStringLiteral("etc/mainstation.json"));
        if (QFileInfo::exists(mainstationPath)) {
            loadNorthMqttMainstationConfig(mainstationPath, report);
        } else {
            clearNorthMqttConfigPage();
        }
        const QString subDataPath = QDir(northMqttAppDir)
            .filePath(QStringLiteral("cfg/subData.cfg"));
        if (QFileInfo::exists(subDataPath)) {
            loadNorthMqttSubDataConfig(subDataPath, report);
        } else {
            refreshMqttDataUploadSummary();
        }
    } else {
        clearNorthMqttConfigPage();
    }
    // ---- IEC101 配置（可选） ----
    if (!iec101AppDir.isEmpty()) {
        const QString iec101ConfigPath = findIec101LocalhostConfig(projectRoot, report);
        if (!iec101ConfigPath.isEmpty()) {
            loadIec101LocalhostConfigFromFile(iec101ConfigPath);
        } else {
            clearIec101ConfigPage();
            refreshIec101PointsFromDevices();
        }
    } else {
        clearIec101ConfigPage();
    }
    // ---- 北向 IEC104 配置（可选） ----
    if (!iec104NorthAppDir.isEmpty()) {
        const QString iec104ConfigPath = QDir(iec104NorthAppDir)
            .filePath(QStringLiteral("config/localhost.json"));
        if (QFileInfo::exists(iec104ConfigPath)) {
            loadIec104LocalhostConfigFromFile(iec104ConfigPath);
        } else {
            clearIec104ConfigPage();
            refreshIec104PointsFromDevices();
        }
    } else {
        clearIec104ConfigPage();
    }

    refreshConfigImportSummary(report);

    if (!ok && report.hasErrors()) {
        statusBar()->showMessage(QStringLiteral("配置导入失败"), 5000);
        return;
    }

    settings.setValue(QStringLiteral("config/lastOpenedDir"), projectRoot);
    statusBar()->showMessage(QStringLiteral("配置导入完成"), 5000);
}

void MainWindow::onExportIec104ConfigClicked()
{
    m_lastConfigExportOk = false;

    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QList<configtool::ImportIssue> networkIssues = networkEndpointConflictIssues(
        m_configProjectManager.project(),
        projectRoot,
        m_northCepManagementPortEdit ? m_northCepManagementPortEdit->text() : QString(),
        m_northCepDataPortEdit ? m_northCepDataPortEdit->text() : QString(),
        m_northMqttBrokerIpEdit ? m_northMqttBrokerIpEdit->text() : QString(),
        m_northMqttPortEdit ? m_northMqttPortEdit->text() : QString(),
        m_iec101CommModeCombo ? m_iec101CommModeCombo->currentData().toInt() : 1,
        m_iec101TcpRoleCombo ? m_iec101TcpRoleCombo->currentData().toString() : QStringLiteral("server"),
        m_iec101CodeIpEdit ? m_iec101CodeIpEdit->text() : QString(),
        m_iec101CodePortEdit ? m_iec101CodePortEdit->text() : QString(),
        m_iec104CodeIpEdit ? m_iec104CodeIpEdit->text() : QString(),
        m_iec104CodePortEdit ? m_iec104CodePortEdit->text() : QString());
    if (!networkIssues.isEmpty()) {
        refreshConfigIssueTable(networkIssues, QStringLiteral("导出"));
        if (m_mainTabWidget && m_configIssuePage) {
            m_mainTabWidget->setCurrentWidget(m_configIssuePage);
        }
        QMessageBox::warning(this,
                             QStringLiteral("网络配置冲突"),
                             QStringLiteral("检测到重复的 IP/端口配置，已取消保存。详细冲突已列入“问题列表”。"));
        statusBar()->showMessage(QStringLiteral("网络端点冲突，已取消保存"), 5000);
        return;
    }

    QString northCepError;
    if (!validateNorthCepConfig(&northCepError)) {
        showNorthConfigPage(m_northCepConfigPage);
        QMessageBox::warning(this, QStringLiteral("CEP配置"), northCepError);
        statusBar()->showMessage(QStringLiteral("CEP 配置校验失败"), 5000);
        return;
    }

    QString northMqttError;
    if (!validateNorthMqttConfig(&northMqttError)) {
        showNorthConfigPage(m_northMqttConfigPage);
        QMessageBox::warning(this, QStringLiteral("MQTT配置"), northMqttError);
        statusBar()->showMessage(QStringLiteral("MQTT 配置校验失败"), 5000);
        return;
    }

    QString iec101Error;
    if (!validateIec101Config(&iec101Error)) {
        showNorthConfigPage(m_iec101ConfigPage);
        QMessageBox::warning(this, QStringLiteral("IEC101配置"), iec101Error);
        statusBar()->showMessage(QStringLiteral("IEC101 配置校验失败"), 5000);
        return;
    }

    const QString serialConflict = serialPortConflictMessage(
        m_configProjectManager.project(),
        m_iec101CommModeCombo ? m_iec101CommModeCombo->currentData().toInt() : 1,
        m_iec101UsartNameEdit ? m_iec101UsartNameEdit->text() : QString());
    if (!serialConflict.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("串口冲突"), serialConflict);
        statusBar()->showMessage(QStringLiteral("串口配置冲突，已取消导出"), 5000);
        return;
    }

    configtool::ExportReport report;
    bool ok = migrateLegacyConfigAppDirs(projectRoot, report);

    QString logRetentionError;
    if (!saveLogRetentionConfigToProject(&logRetentionError)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        QDir(projectRoot).filePath(QStringLiteral("etc/system.json")),
                        logRetentionError);
        ok = false;
    }
    ok = writeNorthCepSystemConfig(projectRoot, report) && ok;

    const QString iec104AppDir = QDir(projectRoot).filePath(QStringLiteral("South_104"));
    const QString modbusAppDir = QDir(projectRoot).filePath(QStringLiteral("South_Modbus"));
    const QString dlt645AppDir = QDir(projectRoot).filePath(QStringLiteral("South_645"));
    const QString logicCenterAppDir = QDir(projectRoot).filePath(QStringLiteral("LogicCenter"));
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && dlt645AppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到可导出的配置目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    ok = m_configProjectManager.exportIec104AppDirectory(iec104AppDir, report) && ok;
    ok = m_configProjectManager.exportModbusAppDirectory(modbusAppDir, report) && ok;
    ok = m_configProjectManager.exportDlt645AppDirectory(dlt645AppDir, report) && ok;
    ok = m_configProjectManager.exportLogicCenterConfigFile(
        QDir(logicCenterAppDir).filePath(QStringLiteral("etc/LogicCenter_Config.json")),
        report) && ok;
    ok = writeNorthCepMainstationConfig(projectRoot,
                                        m_northCepManagementPortEdit->text().trimmed(),
                                        m_northCepDataPortEdit->text().trimmed(),
                                        report) && ok;
    ok = writeNorthCepSubDataConfig(projectRoot, report) && ok;
    ok = writeNorthMqttMainstationConfig(
        projectRoot,
        m_northMqttGatewayIdEdit->text().trimmed(),
        m_northMqttBrokerIpEdit->text().trimmed(),
        m_northMqttPortEdit->text().trimmed(),
        m_northMqttUsernameEdit->text(),
        m_northMqttPasswordEdit->text(),
        report) && ok;
    ok = writeNorthMqttSubDataConfig(projectRoot, report) && ok;

    // ---- IEC101 配置导出 ----
    const QString iec101ExportDir = QDir(projectRoot).filePath(QStringLiteral("North_101"));
    const QString iec101ConfigDir = QDir(iec101ExportDir).filePath(QStringLiteral("config"));
    const QString iec101ConfigPath = QDir(iec101ConfigDir).filePath(QStringLiteral("localhost.json"));
    if (!QDir().mkpath(iec101ConfigDir)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        iec101ConfigPath,
                        QStringLiteral("无法创建 IEC101 配置目录: %1").arg(iec101ConfigDir));
        ok = false;
    } else {
        const QJsonObject iec101Config = serializeIec101LocalhostConfig();
        QFile file(iec101ConfigPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            report.addIssue(configtool::ImportIssueSeverity::Error,
                            iec101ConfigPath,
                            QStringLiteral("无法写入 IEC101 配置文件: %1").arg(file.errorString()));
            ok = false;
        } else {
            file.write(QJsonDocument(iec101Config).toJson(QJsonDocument::Indented));
            file.close();
            ok = removeAlternateIec101LocalhostConfigs(iec101ConfigDir, report) && ok;
        }
    }

    // ---- 北向 IEC104 配置导出 ----
    const QString iec104NorthConfigDir = QDir(projectRoot).filePath(QStringLiteral("North_104/config"));
    const QString iec104NorthConfigPath = QDir(iec104NorthConfigDir).filePath(QStringLiteral("localhost.json"));
    if (!QDir().mkpath(iec104NorthConfigDir)) {
        report.addIssue(configtool::ImportIssueSeverity::Error,
                        iec104NorthConfigPath,
                        QStringLiteral("无法创建北向 IEC104 配置目录: %1").arg(iec104NorthConfigDir));
        ok = false;
    } else {
        QFile file(iec104NorthConfigPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            report.addIssue(configtool::ImportIssueSeverity::Error,
                            iec104NorthConfigPath,
                            QStringLiteral("无法写入北向 IEC104 配置文件: %1").arg(file.errorString()));
            ok = false;
        } else {
            file.write(QJsonDocument(serializeIec104LocalhostConfig()).toJson(QJsonDocument::Indented));
            file.close();
        }
    }

    QStringList issueLines;
    bool hasErrors = false;
    int warningCount = 0;
    int infoCount = 0;
    for (const configtool::ImportIssue &issue : report.issues) {
        const QString severity = importIssueSeverityText(issue.severity);
        hasErrors = hasErrors || issue.severity == configtool::ImportIssueSeverity::Error;
        if (issue.severity == configtool::ImportIssueSeverity::Warning) {
            ++warningCount;
        } else if (issue.severity == configtool::ImportIssueSeverity::Info) {
            ++infoCount;
        }
        issueLines << QStringLiteral("[%1] %2").arg(severity, issue.message);
    }

    if (!ok && hasErrors) {
        const QString detail = issueLines.isEmpty()
            ? QStringLiteral("导出失败，但未返回详细错误。")
            : QStringLiteral("导出失败，详细问题已更新到“问题列表”。");
        refreshConfigIssueTable(report.issues, QStringLiteral("导出"));
        QMessageBox::warning(this, QStringLiteral("导出失败"), detail);
        statusBar()->showMessage(QStringLiteral("配置导出失败"), 5000);
        return;
    }

    QString statusMessage = QStringLiteral("配置导出完成: 模型 %1，设备 %2")
        .arg(report.exportedModelCount)
        .arg(report.exportedDeviceCount);
    if (warningCount > 0) {
        statusMessage += QStringLiteral("，警告 %1 条").arg(warningCount);
    }
    if (infoCount > 0) {
        statusMessage += QStringLiteral("，提示 %1 条").arg(infoCount);
    }
    statusBar()->showMessage(statusMessage, 8000);
    refreshConfigIssueTable(report.issues, QStringLiteral("导出"));

    m_lastConfigExportOk = true;
}

void MainWindow::onCheckConfigIssuesClicked()
{
    const QList<configtool::ImportIssue> issues = collectCurrentConfigIssues();
    refreshConfigIssueTable(issues, QStringLiteral("检查"));
    if (m_mainTabWidget && m_configIssuePage) {
        m_mainTabWidget->setCurrentWidget(m_configIssuePage);
    }

    const int problemCount = importIssueProblemCount(issues);
    int infoCount = 0;
    for (const configtool::ImportIssue &issue : issues) {
        if (issue.severity == configtool::ImportIssueSeverity::Info) {
            ++infoCount;
        }
    }
    if (problemCount == 0) {
        statusBar()->showMessage(infoCount > 0
                                     ? QStringLiteral("当前检查完成：无错误或警告，提示 %1 条").arg(infoCount)
                                     : QStringLiteral("当前检查完成：未发现问题"),
                                 5000);
    } else {
        statusBar()->showMessage(QStringLiteral("当前检查完成：问题 %1 项，提示 %2 条")
                                     .arg(problemCount)
                                     .arg(infoCount),
                                 5000);
    }
}

void MainWindow::refreshConfigIssueTable(const QList<configtool::ImportIssue> &issues,
                                         const QString &source)
{
    if (!m_configIssueTable) {
        return;
    }

    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_configIssueTable->setRowCount(issues.size());
    for (int row = 0; row < issues.size(); ++row) {
        const configtool::ImportIssue &issue = issues.at(row);
        const QString severity = importIssueSeverityText(issue.severity);
        const QColor color = importIssueSeverityColor(issue.severity);

        QString targetType;
        QString targetKey;
        const QString filePath = QFileInfo(issue.filePath).absoluteFilePath();
        resolveLogicIssueTarget(issue.message, &targetType, &targetKey);
        if (targetType.isEmpty()) {
            for (const configtool::ModelTemplate &model : project.models) {
                const QString modelSource = model.source.filePath.trimmed().isEmpty()
                    ? QString()
                    : QFileInfo(model.source.filePath).absoluteFilePath();
                if ((!modelSource.isEmpty() && !issue.filePath.trimmed().isEmpty() && modelSource == filePath)
                    || issue.filePath == model.modelId
                    || issue.message.contains(model.modelId)) {
                    targetType = QStringLiteral("model");
                    targetKey = model.modelId;
                    break;
                }
            }
        }
        if (targetType.isEmpty()) {
            for (const configtool::ProtocolDeviceInstance &device : project.devices) {
                const QString deviceSource = device.source.filePath.trimmed().isEmpty()
                    ? QString()
                    : QFileInfo(device.source.filePath).absoluteFilePath();
                if ((!deviceSource.isEmpty() && !issue.filePath.trimmed().isEmpty() && deviceSource == filePath)
                    || issue.filePath == device.deviceId
                    || issue.message.contains(device.deviceId)) {
                    targetType = QStringLiteral("device");
                    targetKey = device.deviceId;
                    break;
                }
            }
        }
        if (targetType.isEmpty()) {
            if (issue.message.contains(QStringLiteral("IEC101"))) {
                targetType = QStringLiteral("iec101");
                targetKey = QStringLiteral("IEC101配置");
            } else if (issue.message.contains(QStringLiteral("计算点"))) {
                targetType = QStringLiteral("logic-computation");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            } else if (issue.message.contains(QStringLiteral("控制转换"))) {
                targetType = QStringLiteral("logic-control");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            } else if (issue.message.contains(QStringLiteral("在线联动"))) {
                targetType = QStringLiteral("logic-online");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            } else if (issue.message.contains(QStringLiteral("AGC/AVC"))) {
                targetType = QStringLiteral("logic-agcavc");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            }
        }

        const QString objectText = targetKey.isEmpty() ? issue.filePath : targetKey;
        const QStringList values = {source, severity, objectText, issue.message, issue.filePath};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setForeground(color);
            item->setData(ConfigIssueRoleTargetType, targetType);
            item->setData(ConfigIssueRoleTargetKey, targetKey);
            m_configIssueTable->setItem(row, column, item);
        }
    }
    if (m_configIssueCountValueLabel) {
        m_configIssueCountValueLabel->setText(QString::number(importIssueProblemCount(issues)));
    }
}

QList<configtool::ImportIssue> MainWindow::collectCurrentConfigIssues() const
{
    QList<configtool::ImportIssue> issues;
    const configtool::ConfigProject &project = m_configProjectManager.project();
    const QString projectPath = project.sourceRoot.isEmpty() ? project.projectName : project.sourceRoot;

    auto appendIssue = [&](configtool::ImportIssueSeverity severity,
                           const QString &filePath,
                           const QString &message) {
        configtool::ImportIssue issue;
        issue.severity = severity;
        issue.filePath = filePath;
        issue.message = message;
        issues.append(issue);
    };
    auto objectPath = [](const QString &sourcePath, const QString &fallback) {
        return sourcePath.trimmed().isEmpty() ? fallback : sourcePath;
    };

    if (project.projectId.trimmed().isEmpty()) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("当前没有可检查的配置工程"));
    }

    QString northCepError;
    if (!validateNorthCepConfig(&northCepError)) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    QDir(projectPath).filePath(QStringLiteral("North_CEP/etc/mainstation.json")),
                    QStringLiteral("North_CEP 配置：%1").arg(northCepError));
    }

    QString northMqttError;
    if (!validateNorthMqttConfig(&northMqttError)) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    QDir(projectPath).filePath(QStringLiteral("North_Mqtt/etc/mainstation.json")),
                    QStringLiteral("North_Mqtt 配置：%1").arg(northMqttError));
    }

    QString iec101Error;
    if (!validateIec101Config(&iec101Error)) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    QDir(projectPath).filePath(QStringLiteral("North_101/config/localhost.json")),
                    QStringLiteral("North_101 配置：%1").arg(iec101Error));
    }

    const QStringList serialConflicts = serialPortConflictDescriptions(
        project,
        m_iec101CommModeCombo ? m_iec101CommModeCombo->currentData().toInt() : 1,
        m_iec101UsartNameEdit ? m_iec101UsartNameEdit->text() : QString());
    for (const QString &conflict : serialConflicts) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("串口被多个 APP 重复占用：%1").arg(conflict));
    }

    issues.append(networkEndpointConflictIssues(
        project,
        projectPath,
        m_northCepManagementPortEdit ? m_northCepManagementPortEdit->text() : QString(),
        m_northCepDataPortEdit ? m_northCepDataPortEdit->text() : QString(),
        m_northMqttBrokerIpEdit ? m_northMqttBrokerIpEdit->text() : QString(),
        m_northMqttPortEdit ? m_northMqttPortEdit->text() : QString(),
        m_iec101CommModeCombo ? m_iec101CommModeCombo->currentData().toInt() : 1,
        m_iec101TcpRoleCombo ? m_iec101TcpRoleCombo->currentData().toString() : QStringLiteral("server"),
        m_iec101CodeIpEdit ? m_iec101CodeIpEdit->text() : QString(),
        m_iec101CodePortEdit ? m_iec101CodePortEdit->text() : QString(),
        m_iec104CodeIpEdit ? m_iec104CodeIpEdit->text() : QString(),
        m_iec104CodePortEdit ? m_iec104CodePortEdit->text() : QString()));

    QSet<QString> seenModelIds;
    QSet<QString> duplicateModelIds;
    for (const configtool::ModelTemplate &model : project.models) {
        const QString modelId = model.modelId.trimmed();
        if (modelId.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        objectPath(model.source.filePath, projectPath),
                        QStringLiteral("存在模型 modelId 为空"));
        } else if (seenModelIds.contains(modelId)) {
            duplicateModelIds.insert(modelId);
        } else {
            seenModelIds.insert(modelId);
        }

        const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
        if (!duplicateRefs.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        objectPath(model.source.filePath, model.modelId),
                        QStringLiteral("模型存在重复 DataRef：%1")
                            .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
        }
    }
    for (const QString &modelId : duplicateModelIds) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("模型 modelId 重复：%1").arg(modelId));
    }

    QSet<QString> seenDeviceIds;
    QSet<QString> duplicateDeviceIds;
    const QHash<QString, QSet<QString>> duplicateIec104AddressesByChannel =
        duplicateIec104BindingAddressesByChannel(project.devices);
    const QHash<QString, QSet<QString>> duplicateModbusRegisterAddressesByChannel =
        duplicateModbusRegisterAddressesByPhysicalChannel(project.devices);
    QSet<QString> reportedDuplicateIec104Channels;
    QSet<QString> reportedDuplicateModbusChannels;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        const QString deviceId = device.deviceId.trimmed();
        const QString devicePath = objectPath(device.source.filePath, device.deviceId);
        if (deviceId.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        devicePath.isEmpty() ? projectPath : devicePath,
                        QStringLiteral("存在设备 DeviceId 为空"));
        } else if (seenDeviceIds.contains(deviceId)) {
            duplicateDeviceIds.insert(deviceId);
        } else {
            seenDeviceIds.insert(deviceId);
        }

        if (!device.modelId.trimmed().isEmpty() && !findModelById(project, device.modelId)) {
            appendIssue(configtool::ImportIssueSeverity::Warning,
                        devicePath,
                        QStringLiteral("设备 %1 引用的模型 %2 在当前工程中不存在")
                            .arg(device.deviceId, device.modelId));
        }

        if (device.protocol == configtool::ProtocolType::Iec104) {
            const QString channelKey = iec104ChannelKey(device);
            const QSet<QString> duplicateAddresses = duplicateIec104AddressesByChannel.value(channelKey);
            if (!duplicateAddresses.isEmpty()) {
                if (!reportedDuplicateIec104Channels.contains(channelKey)) {
                    reportedDuplicateIec104Channels.insert(channelKey);
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                projectPath,
                                QStringLiteral("同通道 104 设备存在重复点位地址：%1（%2）")
                                    .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，")),
                                         iec104ChannelDisplayName(device)));
                }
            }
            for (const configtool::PointBinding &binding : device.bindings) {
                if (binding.enabled
                    && !configtool::isVirtualPointBinding(binding)
                    && binding.address.trimmed().isEmpty()) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("设备 %1 存在启用但未填写地址的点位：%2")
                                    .arg(device.deviceId, binding.dataRef));
                    break;
                }
            }
        } else if (isModbusDevice(device)) {
            const QString type = device.transport.protocolOptions
                                     .value(QStringLiteral("type"))
                                     .toString(QStringLiteral("TCP"))
                                     .trimmed()
                                     .toUpper();
            const bool virtualDevice = type == QStringLiteral("VIRTUAL");
            const QString channelKey = modbusPhysicalChannelKey(device);
            const QSet<QString> duplicateRegisterAddresses =
                duplicateModbusRegisterAddressesByChannel.value(channelKey);
            if (!duplicateRegisterAddresses.isEmpty()
                && !reportedDuplicateModbusChannels.contains(channelKey)) {
                reportedDuplicateModbusChannels.insert(channelKey);
                appendIssue(configtool::ImportIssueSeverity::Error,
                            projectPath,
                            QStringLiteral("同物理 Modbus 通道设备存在重复或重叠的寄存器地址：%1（%2）")
                                .arg(QStringList(duplicateRegisterAddresses.begin(), duplicateRegisterAddresses.end()).join(QStringLiteral("，")),
                                     modbusPhysicalChannelDisplayName(device)));
            }
            if (type != QStringLiteral("TCP") && type != QStringLiteral("RTU") && !virtualDevice) {
                appendIssue(configtool::ImportIssueSeverity::Error,
                            devicePath,
                            QStringLiteral("Modbus 设备 %1 的 type 必须为 TCP、RTU 或 VIRTUAL").arg(device.deviceId));
            }
            bool portOk = false;
            device.transport.port.trimmed().toInt(&portOk);
            if (type == QStringLiteral("TCP") && (device.transport.ip.trimmed().isEmpty() || !portOk)) {
                appendIssue(configtool::ImportIssueSeverity::Error,
                            devicePath,
                            QStringLiteral("Modbus TCP 设备 %1 缺少 ip 或数字 port").arg(device.deviceId));
            }
            if (type == QStringLiteral("RTU")) {
                const QJsonObject rtu = device.transport.serial;
                if (uiJsonValueToString(rtu.value(QStringLiteral("serialPort"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("baud"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("dataBits"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("stopBits"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("parity"))).trimmed().isEmpty()) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("Modbus RTU 设备 %1 缺少完整 rtu 参数").arg(device.deviceId));
                }
            }
            for (const configtool::PointBinding &binding : device.bindings) {
                if (!binding.enabled
                    || virtualDevice
                    || configtool::isVirtualPointBinding(binding)) {
                    continue;
                }
                if (binding.address.trimmed().isEmpty()) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("Modbus 设备 %1 存在启用但未生成 dataIndex 的点位：%2")
                                    .arg(device.deviceId, binding.dataRef));
                    break;
                }
                if (!binding.extensions.contains(QStringLiteral("modbusRegisterAddress"))) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("Modbus 设备 %1 的点位 %2 未填写寄存器地址")
                                    .arg(device.deviceId, binding.dataRef));
                    break;
                }
            }
        }
    }
    for (const QString &deviceId : duplicateDeviceIds) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("设备 DeviceId 重复：%1").arg(deviceId));
    }

    // IEC101 点表地址检查（重复 + 范围）
    if (m_iec101PointsTable && m_iec101PointsTable->rowCount() > 0) {
        const QString iec101ConfigPath = resolveIec101ServiceChannelAppDir(projectPath);
        const QString filePath = iec101ConfigPath.isEmpty()
            ? projectPath
            : QDir(iec101ConfigPath).filePath(QStringLiteral("config/localhost.json"));

        // 重复地址
        const QSet<QString> iec101Duplicates = checkIec101DuplicateAddresses();
        if (!iec101Duplicates.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("IEC101 点表中存在重复的北向地址：%1")
                            .arg(QStringList(iec101Duplicates.begin(), iec101Duplicates.end()).join(QStringLiteral("，"))));
        }

        // 地址范围错误（遥信 0x0001-0x3FFF，遥测 0x4001-0x5FFF，遥控/遥调 0x6001-0x7FFF）
        const QStringList iec101RangeErrors = checkIec101AddressRangeErrors();
        for (const QString &err : iec101RangeErrors) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("IEC101 %1").arg(err));
        }

        const QStringList normalizationErrors = checkIec101NormalizationRangeErrors();
        for (const QString &err : normalizationErrors) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("IEC101 %1").arg(err));
        }
    }

    // 北向 IEC104 点表地址检查（重复 + 范围）
    if (m_iec104PointsTable && m_iec104PointsTable->rowCount() > 0) {
        const QString appDir = resolveIec104ServiceChannelAppDir(projectPath);
        const QString filePath = appDir.isEmpty()
            ? projectPath
            : QDir(appDir).filePath(QStringLiteral("config/localhost.json"));
        const QSet<QString> duplicates = checkIec104DuplicateAddresses();
        if (!duplicates.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("北向 IEC104 点表中存在重复地址：%1")
                            .arg(QStringList(duplicates.begin(), duplicates.end()).join(QStringLiteral("，"))));
        }
        for (const QString &err : checkIec104AddressRangeErrors()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        filePath,
                        QStringLiteral("北向 IEC104 %1").arg(err));
        }
    }

    const QList<configtool::ConfigIssue> logicIssues =
        configtool::validateLogicCenterConfig(project.logicCenter, &project);
    for (const configtool::ConfigIssue &issue : logicIssues) {
        appendConfigIssueForIssueTable(issue,
                                       QStringLiteral("LogicCenter/etc/LogicCenter_Config.json"),
                                       issues);
    }

    return issues;
}

QStringList MainWindow::configTransferRelativePaths(const QString &projectRoot) const
{
    QStringList paths;
    const QDir rootDir(projectRoot);
    for (const QString &relativePath : configTransferPathList()) {
        if (rootDir.exists(relativePath)) {
            paths.append(relativePath);
        }
    }
    return paths;
}

QString MainWindow::deviceHost() const
{
    return m_ipEdit ? m_ipEdit->text().trimmed() : QString();
}

QString MainWindow::fixedRemoteUser() const
{
    return QStringLiteral("root");
}

QString MainWindow::fixedRemotePassword() const
{
    return QStringLiteral("root");
}

QString MainWindow::fixedRemoteSshPort() const
{
    return QStringLiteral("10022");
}

QString MainWindow::configRemoteTarget() const
{
    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    if (host.isEmpty()) {
        return QString();
    }
    return user.isEmpty() ? host : QStringLiteral("%1@%2").arg(user, host);
}

QString MainWindow::configRemoteBaseDir() const
{
    const QString baseDir = m_configRemoteBaseDirEdit
        ? m_configRemoteBaseDirEdit->text().trimmed()
        : QStringLiteral("/home/cepgateway/app");
    return baseDir.isEmpty() ? QStringLiteral("/home/cepgateway/app") : baseDir;
}

bool MainWindow::clearLocalConfigTransferPaths(const QString &projectRoot, QString *errorMessage) const
{
    const QFileInfo rootInfo(projectRoot);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("本地工作目录不存在：%1").arg(projectRoot);
        }
        return false;
    }

    const QString rootPath = rootInfo.absoluteFilePath();
    const QString rootCanonicalPath = QDir::cleanPath(rootInfo.canonicalFilePath());
    const QDir rootDir(rootPath);
    QStringList cleanupRelativePaths;
    for (const QStringList &candidateGroup : configDownloadPathCandidateGroups()) {
        cleanupRelativePaths.append(candidateGroup);
    }

    cleanupRelativePaths.removeDuplicates();

    for (const QString &relativePath : cleanupRelativePaths) {
        if (relativePath.trimmed().isEmpty()
            || QDir::isAbsolutePath(relativePath)
            || relativePath.contains(QStringLiteral(".."))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("配置路径不安全：%1").arg(relativePath);
            }
            return false;
        }

        const QString localPath = rootDir.absoluteFilePath(relativePath);
        const QFileInfo localInfo(localPath);
        if (!localInfo.exists()) {
            continue;
        }

        const QString localCanonicalPath = QDir::cleanPath(localInfo.canonicalFilePath());
        if (!rootCanonicalPath.isEmpty()
            && !localCanonicalPath.isEmpty()
            && localCanonicalPath != rootCanonicalPath
            && !localCanonicalPath.startsWith(rootCanonicalPath + QLatin1Char('/'))
            && !localCanonicalPath.startsWith(rootCanonicalPath + QLatin1Char('\\'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("拒绝清理工作目录之外的路径：%1").arg(localPath);
            }
            return false;
        }

        bool removed = false;
        if (localInfo.isDir()) {
            QDir localDir(localPath);
            removed = localDir.removeRecursively();
        } else {
            removed = QFile::remove(localPath);
        }

        if (!removed) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法删除本地配置路径：%1").arg(localPath);
            }
            return false;
        }
    }

    return true;
}

bool MainWindow::runConfigTransferProcess(const QString &program,
                                          const QStringList &arguments,
                                          const QString &title,
                                          QString *output,
                                          QProgressDialog *progress)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    if (progress) {
        progress->setLabelText(title);
        progress->show();
        QApplication::processEvents();
    }
    process.start(program, arguments);
    if (!process.waitForStarted(10000)) {
        if (output) {
            *output = process.errorString();
        }
        return false;
    }

    QByteArray outputBytes;
    QElapsedTimer elapsed;
    elapsed.start();
    while (!process.waitForFinished(100)) {
        outputBytes.append(process.readAllStandardOutput());
        if (progress) {
            const QString text = QString::fromUtf8(outputBytes);
            const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                progress->setLabelText(QStringLiteral("%1\n%2").arg(title, lines.last().trimmed()));
            }
            QApplication::processEvents();
            if (progress->wasCanceled()) {
                process.kill();
                process.waitForFinished(3000);
                if (output) {
                    *output = QStringLiteral("%1 已取消。").arg(title);
                }
                return false;
            }
        }
        if (elapsed.elapsed() > 180000) {
            process.kill();
            process.waitForFinished(3000);
            if (output) {
                *output = QStringLiteral("%1 超时。").arg(title);
            }
            return false;
        }
    }
    outputBytes.append(process.readAllStandardOutput());

    const QString text = QString::fromUtf8(outputBytes);
    if (output) {
        *output = text;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }
    return true;
}

void MainWindow::onUploadConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("请先选择配置工作区。"));
        return;
    }
    if (configRemoteTarget().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("请先填写设备地址。"));
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this,
        QStringLiteral("上传配置"),
        QStringLiteral("上传会先清空设备内对应配置目录，再写入当前工作区配置。\n\n请确认已经自行备份设备内原配置。\n\n是否继续上传？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    onExportIec104ConfigClicked();
    if (!m_lastConfigExportOk) {
        statusBar()->showMessage(QStringLiteral("导出未完成，已取消上传"), 5000);
        return;
    }

    const QStringList relativePaths = configTransferRelativePaths(projectRoot);
    if (relativePaths.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("当前工作区没有可上传的配置目录。"));
        return;
    }

    const QString archivePath = QDir::temp().filePath(
        QStringLiteral("cepb_config_upload_%1.tar.gz").arg(QUuid::createUuid().toString(QUuid::Id128)));
    QProgressDialog progress(QStringLiteral("准备上传配置..."), QStringLiteral("取消"), 0, 4, this);
    progress.setWindowTitle(QStringLiteral("上传配置"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    auto setUploadStep = [&progress](int value, const QString &text) {
        progress.setValue(value);
        progress.setLabelText(text);
        QApplication::processEvents();
    };

    QString output;
    QStringList tarArgs = {QStringLiteral("--options"), QStringLiteral("hdrcharset=UTF-8"),
                           QStringLiteral("-czvf"), archivePath, QStringLiteral("-C"), projectRoot};
    tarArgs.append(relativePaths);
    setUploadStep(0, QStringLiteral("本地打包配置..."));
    if (!runConfigTransferProcess(QStringLiteral("tar"), tarArgs, QStringLiteral("本地打包配置"), &output, &progress)) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("本地配置打包失败：\n%1").arg(output));
        return;
    }
    setUploadStep(1, QStringLiteral("本地打包完成。"));

    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    const QString password = fixedRemotePassword();
    const quint16 port = fixedRemoteSshPort().toUShort();
    const QString remoteBaseDir = configRemoteBaseDir();
    const QString remoteArchivePath = QStringLiteral("/tmp/cepb_config_upload.tar.gz");
    const SshClient::Connection sshConnection{host, port, user, password};

    QStringList cleanupParts;
    cleanupParts << QStringLiteral("set -e")
                 << QStringLiteral("mkdir -p %1").arg(remoteShellQuote(remoteBaseDir));
    for (const QString &relativePath : relativePaths) {
        const QString remotePath = remotePathJoin(remoteBaseDir, relativePath);
        cleanupParts << QStringLiteral("rm -rf %1").arg(remoteShellQuote(remotePath))
                     << QStringLiteral("mkdir -p %1").arg(remoteShellQuote(remotePath));
    }
    const QString cleanupCommand = cleanupParts.join(QStringLiteral("; "));
    const QString extractCommand = QStringLiteral("set -e; tar -xzf %1 -C %2; rm -f %1")
        .arg(remoteShellQuote(remoteArchivePath), remoteShellQuote(remoteBaseDir));

    bool ok = false;
    setUploadStep(1, QStringLiteral("清空设备内对应配置目录..."));
    SshClient::CommandResult commandResult = SshClient::execCommand(sshConnection, cleanupCommand);
    ok = commandResult.ok;
    output = commandResult.ok ? commandResult.output : commandResult.error;
    if (ok) {
        setUploadStep(2, QStringLiteral("上传配置压缩包..."));
        ok = SshClient::uploadFileScp(sshConnection, archivePath, remoteArchivePath, &output);
    }
    if (ok) {
        setUploadStep(3, QStringLiteral("设备端解包配置..."));
        commandResult = SshClient::execCommand(sshConnection, extractCommand);
        ok = commandResult.ok;
        output = commandResult.ok ? commandResult.output : commandResult.error;
    }

    QFile::remove(archivePath);
    if (!ok) {
        QMessageBox::warning(this,
                             QStringLiteral("上传配置"),
                             QStringLiteral("配置上传失败。\n\n设备 SSH 固定使用 root/root，端口 10022。\n\n%1").arg(output));
        statusBar()->showMessage(QStringLiteral("配置上传失败"), 5000);
        return;
    }

    progress.setValue(4);
    statusBar()->showMessage(QStringLiteral("配置上传成功"), 8000);
}

void MainWindow::onDownloadConfigClicked()
{
    if (configRemoteTarget().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("下载配置"), QStringLiteral("请先填写设备地址。"));
        return;
    }

    const QString startDir = configBrowseStartDir();
    const QString targetDir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择下载保存目录"), startDir);
    if (targetDir.isEmpty()) {
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this,
        QStringLiteral("下载配置"),
        QStringLiteral("下载会先清空本地工作目录内对应配置目录，再写入设备端配置。\n\n目标目录：%1\n\n请确认已经自行备份本地原配置。\n\n是否继续下载？")
            .arg(QDir::toNativeSeparators(targetDir)),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    const QString password = fixedRemotePassword();
    const quint16 port = fixedRemoteSshPort().toUShort();
    const QString remoteBaseDir = configRemoteBaseDir();
    const QString remoteArchivePath = QStringLiteral("/tmp/cepb_config_download.tar.gz");
    const QString archivePath = QDir::temp().filePath(
        QStringLiteral("cepb_config_download_%1.tar.gz").arg(QUuid::createUuid().toString(QUuid::Id128)));
    const SshClient::Connection sshConnection{host, port, user, password};

    QStringList selectPathCommands;
    for (const QStringList &candidateGroup : configDownloadPathCandidateGroups()) {
        QStringList quotedCandidates;
        for (const QString &relativePath : candidateGroup) {
            quotedCandidates << remoteShellQuote(relativePath);
        }
        selectPathCommands << QStringLiteral(
            "for p in %1; do if [ -e \"$p\" ]; then find \"$p\" -type f -print; paths=\"$paths $p\"; break; fi; done")
                                  .arg(quotedCandidates.join(QLatin1Char(' ')));
    }
    const QString packageCommand = QStringLiteral(
        "set -e; cd %1; paths=\"\"; %2; "
        "[ -n \"$paths\" ] || { echo 'no config paths found'; exit 2; }; tar -czf %3 $paths")
        .arg(remoteShellQuote(remoteBaseDir),
             selectPathCommands.join(QStringLiteral("; ")),
             remoteShellQuote(remoteArchivePath));

    QProgressDialog progress(QStringLiteral("准备下载配置..."), QStringLiteral("取消"), 0, 5, this);
    progress.setWindowTitle(QStringLiteral("下载配置"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    auto setDownloadStep = [&progress](int value, const QString &text) {
        progress.setValue(value);
        progress.setLabelText(text);
        QApplication::processEvents();
    };

    QString output;
    bool ok = false;
    setDownloadStep(0, QStringLiteral("设备端扫描并打包配置..."));
    SshClient::CommandResult commandResult = SshClient::execCommand(sshConnection, packageCommand);
    ok = commandResult.ok;
    output = commandResult.ok ? commandResult.output : commandResult.error;
    if (ok) {
        setDownloadStep(1, QStringLiteral("下载配置压缩包..."));
        ok = SshClient::downloadFileScp(sshConnection, remoteArchivePath, archivePath, &output);
    }
    if (ok) {
        SshClient::execCommand(sshConnection,
                               QStringLiteral("rm -f %1").arg(remoteShellQuote(remoteArchivePath)),
                               30000);
    }

    if (!ok) {
        QFile::remove(archivePath);
        QMessageBox::warning(this,
                             QStringLiteral("下载配置"),
                             QStringLiteral("配置下载失败。\n\n设备 SSH 固定使用 root/root，端口 10022。\n\n%1").arg(output));
        statusBar()->showMessage(QStringLiteral("配置下载失败"), 5000);
        return;
    }

    setDownloadStep(2, QStringLiteral("清空本地对应配置目录..."));
    QString cleanupErrorMessage;
    if (!clearLocalConfigTransferPaths(targetDir, &cleanupErrorMessage)) {
        QFile::remove(archivePath);
        QMessageBox::warning(this,
                             QStringLiteral("下载配置"),
                             QStringLiteral("本地配置目录清理失败：\n%1").arg(cleanupErrorMessage));
        statusBar()->showMessage(QStringLiteral("配置下载已取消"), 5000);
        return;
    }

    setDownloadStep(3, QStringLiteral("本地解包配置..."));
    if (!runConfigTransferProcess(QStringLiteral("tar"),
                                  {QStringLiteral("--options"), QStringLiteral("hdrcharset=UTF-8"),
                                   QStringLiteral("-xzvf"), archivePath, QStringLiteral("-C"), targetDir},
                                  QStringLiteral("本地解包配置"),
                                  &output,
                                  &progress)) {
        QFile::remove(archivePath);
        QMessageBox::warning(this, QStringLiteral("下载配置"), QStringLiteral("配置解包失败：\n%1").arg(output));
        return;
    }
    QFile::remove(archivePath);

    m_configImportDirEdit->setText(targetDir);
    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    settings.setValue(QStringLiteral("config/lastBrowseDir"), targetDir);
    setDownloadStep(4, QStringLiteral("重新导入下载后的配置..."));
    onImportIec104ConfigClicked();
    progress.setValue(5);
    statusBar()->showMessage(QStringLiteral("配置下载完成"), 8000);
}

QString MainWindow::normalizedConfigProjectRoot(const QString &selectedPath) const
{
    if (selectedPath.isEmpty()) {
        return QString();
    }

    const QFileInfo selectedInfo(selectedPath);
    const QString absolutePath = selectedInfo.absoluteFilePath();
    const QString folderName = selectedInfo.fileName().trimmed();
    const QStringList appFolderNames = configAppFolderNames();
    for (const QString &appFolderName : appFolderNames) {
        if (folderName.compare(appFolderName, Qt::CaseInsensitive) == 0) {
            return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
        }
    }

    return absolutePath;
}

static QString resolveAppDirByNames(const QString &projectRoot, const QStringList &appNames)
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    for (const QString &appName : appNames) {
        if (rootInfo.fileName().compare(appName, Qt::CaseInsensitive) == 0
            && rootInfo.isDir()) {
            return rootInfo.absoluteFilePath();
        }
    }

    for (const QString &appName : appNames) {
        const QString appDir = QDir(projectRoot).filePath(appName);
        if (QDir(appDir).exists()) {
            return appDir;
        }
    }

    return QString();
}

QString MainWindow::resolveNorthCepAppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("North_CEP"),
        QStringLiteral("ServiceChannel")
    });
}

QString MainWindow::resolveNorthMqttAppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("North_Mqtt"),
        QStringLiteral("MqttServiceChannel")
    });
}

QString MainWindow::resolveIec104AppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("South_104"),
        QStringLiteral("cepiec104")
    });
}

QString MainWindow::resolveModbusAppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("South_Modbus"),
        QStringLiteral("cepmodbus")
    });
}

QString MainWindow::resolveDlt645AppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("South_645"),
        QStringLiteral("cepdlt645")
    });
}

QString MainWindow::resolveLogicCenterAppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("LogicCenter"),
        QStringLiteral("cepLogicCenter")
    });
}

QString MainWindow::resolveIec101ServiceChannelAppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("North_101"),
        QStringLiteral("IEC101ServiceChannel")
    });
}

QString MainWindow::resolveIec104ServiceChannelAppDir(const QString &projectRoot) const
{
    return resolveAppDirByNames(projectRoot, {
        QStringLiteral("North_104"),
        QStringLiteral("IEC104ServiceChannel")
    });
}

QString MainWindow::configBrowseStartDir() const
{
    const QString currentValue = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (!currentValue.isEmpty() && QDir(currentValue).exists()) {
        return currentValue;
    }

    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    const QString lastDir = settings.value(QStringLiteral("config/lastBrowseDir")).toString().trimmed();
    if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
        return lastDir;
    }

    return QDir::homePath();
}

