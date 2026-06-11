#include "config/config_project_manager.h"
#include "config/config_project_manager_p.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace configtool {

using namespace detail;
bool ConfigProjectManager::exportIec104AppDirectory(const QString &appDir,
                                                    ExportReport &report) const
{
    if (m_project.projectId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("当前没有可导出的配置工程"));
        return false;
    }

    const QList<ProtocolDeviceInstance> exportDevices = devicesForProtocol(m_project, ProtocolType::Iec104);
    const QList<ModelTemplate> exportModels = modelsForDeviceSet(m_project, modelIdsUsedByDevices(exportDevices));
    const QHash<QString, QSet<QString>> duplicateAddressesByChannel =
        duplicateIec104BindingAddressesByChannel(exportDevices);

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

    QSet<QString> reportedDuplicateAddressChannels;
    for (const ProtocolDeviceInstance &device : exportDevices) {
        const QString channelKey = iec104ChannelKey(device);
        const QSet<QString> duplicateAddresses = duplicateAddressesByChannel.value(channelKey);
        if (!duplicateAddresses.isEmpty()) {
            if (reportedDuplicateAddressChannels.contains(channelKey)) {
                continue;
            }
            reportedDuplicateAddressChannels.insert(channelKey);
            report.addIssue(ImportIssueSeverity::Error,
                            appDir,
                            QStringLiteral("同通道 104 设备存在重复点位地址：%1（%2）")
                                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，")),
                                     iec104ChannelDisplayName(device)));
        }
    }

    QSet<QString> exportedDeviceFileNames;
    for (const ProtocolDeviceInstance &device : exportDevices) {
        if (device.deviceId.trimmed().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("存在设备 DeviceId 为空，无法导出"));
        }
        const QString deviceFileName = deviceFileNameForExport(device);
        if (exportedDeviceFileNames.contains(deviceFileName)) {
            report.addIssue(ImportIssueSeverity::Error,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("存在多个 104 设备将导出为同一个文件：%1，请检查 DeviceId 是否重复")
                                .arg(deviceFileName));
        } else {
            exportedDeviceFileNames.insert(deviceFileName);
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
    const QString northModelDirPath = QDir(modelDirPath).filePath(QStringLiteral("northmodel"));
    const QString deviceDirPath = mutableAppDir.filePath(QStringLiteral("dev"));
    if (!mutableAppDir.mkpath(QStringLiteral("model"))) {
        report.addIssue(ImportIssueSeverity::Error, modelDirPath, QStringLiteral("无法创建 model 目录"));
        return false;
    }
    if (!QDir(modelDirPath).mkpath(QStringLiteral("northmodel"))) {
        report.addIssue(ImportIssueSeverity::Error, northModelDirPath, QStringLiteral("无法创建 northmodel 目录"));
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
    if (!removeJsonFilesInDirectory(northModelDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, northModelDirPath, cleanupErrorMessage);
        return false;
    }
    if (!removeJsonFilesInDirectory(deviceDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, deviceDirPath, cleanupErrorMessage);
        return false;
    }

    if (exportModels.isEmpty() && exportDevices.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Info,
                        appDir,
                        QStringLiteral("当前工程没有 104 模型或设备可导出，已清空旧 104 模型和设备文件"));
        return true;
    }

    for (const ModelTemplate &model : exportModels) {
        const QString filePath = QDir(modelDirPath).filePath(modelFileNameForExport(model));
        QString errorMessage;
        if (!writeJsonFile(filePath, serializeModel(model), errorMessage)) {
            report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
            return false;
        }
        if (model.northVisible) {
            const QString northFilePath = QDir(northModelDirPath).filePath(modelFileNameForExport(model));
            if (!writeJsonFile(northFilePath, serializeNorthModel(model), errorMessage)) {
                report.addIssue(ImportIssueSeverity::Error, northFilePath, errorMessage);
                return false;
            }
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
    const QHash<QString, QSet<QString>> duplicateRegisterAddressesByChannel =
        duplicateModbusRegisterAddressesByTcpChannel(exportDevices);

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

    QSet<QString> exportedDeviceFileNames;
    QSet<QString> reportedDuplicateRegisterAddressChannels;
    for (const ProtocolDeviceInstance &device : exportDevices) {
        const QString type = device.transport.protocolOptions.value(QStringLiteral("type")).toString(QStringLiteral("TCP")).trimmed().toUpper();
        if (type == QStringLiteral("TCP")) {
            const QString channelKey = modbusTcpChannelKey(device);
            const QSet<QString> duplicateRegisterAddresses = duplicateRegisterAddressesByChannel.value(channelKey);
            if (!duplicateRegisterAddresses.isEmpty()
                && !reportedDuplicateRegisterAddressChannels.contains(channelKey)) {
                reportedDuplicateRegisterAddressChannels.insert(channelKey);
                report.addIssue(ImportIssueSeverity::Error,
                                appDir,
                                QStringLiteral("同 TCP 通道 Modbus 设备存在重复寄存器地址：%1（%2）")
                                    .arg(QStringList(duplicateRegisterAddresses.begin(), duplicateRegisterAddresses.end()).join(QStringLiteral("，")),
                                         modbusTcpChannelDisplayName(device)));
            }
        }

        if (device.deviceId.trimmed().isEmpty()) {
            report.addIssue(ImportIssueSeverity::Error, appDir, QStringLiteral("存在 Modbus 设备 DeviceId 为空，无法导出"));
        }
        const QString deviceFileName = deviceFileNameForExport(device);
        if (exportedDeviceFileNames.contains(deviceFileName)) {
            report.addIssue(ImportIssueSeverity::Error,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("存在多个 Modbus 设备将导出为同一个文件：%1，请检查 DeviceId 是否重复")
                                .arg(deviceFileName));
        } else {
            exportedDeviceFileNames.insert(deviceFileName);
        }
        if (!device.modelId.trimmed().isEmpty() && !findModelById(m_project, device.modelId)) {
            report.addIssue(ImportIssueSeverity::Warning,
                            device.source.filePath.isEmpty() ? device.deviceId : device.source.filePath,
                            QStringLiteral("Modbus 设备引用的模型 %1 在当前工程中不存在")
                                .arg(device.modelId));
        }

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
    const QString northModelDirPath = QDir(modelDirPath).filePath(QStringLiteral("northmodel"));
    const QString deviceDirPath = mutableAppDir.filePath(QStringLiteral("dev"));
    const QString etcDirPath = mutableAppDir.filePath(QStringLiteral("etc"));
    if (!mutableAppDir.mkpath(QStringLiteral("model"))) {
        report.addIssue(ImportIssueSeverity::Error, modelDirPath, QStringLiteral("无法创建 model 目录"));
        return false;
    }
    if (!QDir(modelDirPath).mkpath(QStringLiteral("northmodel"))) {
        report.addIssue(ImportIssueSeverity::Error, northModelDirPath, QStringLiteral("无法创建 northmodel 目录"));
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
    if (!removeJsonFilesInDirectory(northModelDirPath, cleanupErrorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, northModelDirPath, cleanupErrorMessage);
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
        if (model.northVisible) {
            const QString northFilePath = QDir(northModelDirPath).filePath(modelFileNameForExport(model));
            if (!writeJsonFile(northFilePath, serializeNorthModel(model), errorMessage)) {
                report.addIssue(ImportIssueSeverity::Error, northFilePath, errorMessage);
                return false;
            }
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

    if (exportModels.isEmpty() && exportDevices.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Info,
                        appDir,
                        QStringLiteral("当前工程没有 Modbus 模型或设备可导出，已清空旧 Modbus 模型和设备文件"));
        return true;
    }

    return true;
}

bool ConfigProjectManager::exportLogicCenterConfigFile(const QString &filePath,
                                                       ExportReport &report) const
{
    if (m_project.projectId.isEmpty()) {
        report.addIssue(ImportIssueSeverity::Error, filePath, QStringLiteral("当前没有可导出的配置工程"));
        return false;
    }

    appendConfigIssuesToReport(validateLogicCenterConfig(m_project.logicCenter, &m_project),
                               filePath,
                               report);
    if (report.hasErrors()) {
        return false;
    }

    const QFileInfo fileInfo(filePath);
    QDir dir(fileInfo.absolutePath());
    if (!dir.exists() && !QDir().mkpath(fileInfo.absolutePath())) {
        report.addIssue(ImportIssueSeverity::Error, fileInfo.absolutePath(), QStringLiteral("无法创建 LogicCenter etc 目录"));
        return false;
    }

    QString errorMessage;
    if (!writeJsonFile(filePath, serializeLogicCenterConfig(m_project.logicCenter), errorMessage)) {
        report.addIssue(ImportIssueSeverity::Error, filePath, errorMessage);
        return false;
    }

    return true;
}

} // namespace configtool


