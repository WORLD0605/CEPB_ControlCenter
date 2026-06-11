#include "mainwindow_config_p.h"
#include "network/ssh_client.h"

using namespace cepb_config_helpers;

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

void MainWindow::onImportIec104ConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString modbusAppDir = resolveModbusAppDir(projectRoot);
    const QString logicCenterAppDir = resolveLogicCenterAppDir(projectRoot);
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到 cepiec104、cepmodbus 或 cepLogicCenter 子目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ImportReport report;
    const QString projectName = QFileInfo(projectRoot).fileName().trimmed().isEmpty()
        ? QStringLiteral("配置工程")
        : QFileInfo(projectRoot).fileName();
    m_configProjectManager.createEmptyProject(projectName, projectRoot);
    bool ok = true;
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.importIec104AppDirectory(iec104AppDir, report) && ok;
    }
    if (!modbusAppDir.isEmpty()) {
        ok = m_configProjectManager.importModbusAppDirectory(modbusAppDir, report) && ok;
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
    // ---- IEC101 配置（可选） ----
    const QString iec101AppDir = resolveIec101ServiceChannelAppDir(projectRoot);
    if (!iec101AppDir.isEmpty()) {
        const QString iec101ConfigPath = QDir(iec101AppDir)
            .filePath(QStringLiteral("config/localhost.json"));
        if (QFileInfo::exists(iec101ConfigPath)) {
            loadIec101LocalhostConfigFromFile(iec101ConfigPath);
        } else {
            clearIec101ConfigPage();
            refreshIec101PointsFromDevices();
        }
    } else {
        clearIec101ConfigPage();
    }

    refreshConfigImportSummary(report);

    if (!ok && report.hasErrors()) {
        statusBar()->showMessage(QStringLiteral("配置导入失败"), 5000);
        return;
    }

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

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString resolvedModbusDir = resolveModbusAppDir(projectRoot);
    const QString resolvedLogicCenterDir = resolveLogicCenterAppDir(projectRoot);
    const QString modbusAppDir = resolvedModbusDir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("cepmodbus"))
        : resolvedModbusDir;
    const QString logicCenterAppDir = resolvedLogicCenterDir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"))
        : resolvedLogicCenterDir;
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到可导出的配置目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ExportReport report;
    bool ok = true;
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.exportIec104AppDirectory(iec104AppDir, report) && ok;
    }
    ok = m_configProjectManager.exportModbusAppDirectory(modbusAppDir, report) && ok;
    ok = m_configProjectManager.exportLogicCenterConfigFile(
        QDir(logicCenterAppDir).filePath(QStringLiteral("etc/LogicCenter_Config.json")),
        report) && ok;

    // ---- IEC101 配置导出 ----
    const QString resolvedIec101Dir = resolveIec101ServiceChannelAppDir(projectRoot);
    const QString iec101ExportDir = resolvedIec101Dir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("IEC101ServiceChannel"))
        : resolvedIec101Dir;
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
        duplicateModbusRegisterAddressesByTcpChannel(project.devices);
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
                if (binding.enabled && binding.address.trimmed().isEmpty()) {
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
            if (type == QStringLiteral("TCP")) {
                const QString channelKey = modbusTcpChannelKey(device);
                const QSet<QString> duplicateRegisterAddresses =
                    duplicateModbusRegisterAddressesByChannel.value(channelKey);
                if (!duplicateRegisterAddresses.isEmpty()
                    && !reportedDuplicateModbusChannels.contains(channelKey)) {
                    reportedDuplicateModbusChannels.insert(channelKey);
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                projectPath,
                                QStringLiteral("同 TCP 通道 Modbus 设备存在重复寄存器地址：%1（%2）")
                                    .arg(QStringList(duplicateRegisterAddresses.begin(), duplicateRegisterAddresses.end()).join(QStringLiteral("，")),
                                         modbusTcpChannelDisplayName(device)));
                }
            }
            if (type != QStringLiteral("TCP") && type != QStringLiteral("RTU")) {
                appendIssue(configtool::ImportIssueSeverity::Error,
                            devicePath,
                            QStringLiteral("Modbus 设备 %1 的 type 必须为 TCP 或 RTU").arg(device.deviceId));
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
                if (!binding.enabled) {
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
    }

    const QList<configtool::ConfigIssue> logicIssues =
        configtool::validateLogicCenterConfig(project.logicCenter, &project);
    for (const configtool::ConfigIssue &issue : logicIssues) {
        appendConfigIssueForIssueTable(issue,
                                       QStringLiteral("cepLogicCenter/etc/LogicCenter_Config.json"),
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
    statusBar()->showMessage(QStringLiteral("配置上传完成"), 8000);
    QMessageBox::information(this, QStringLiteral("上传配置"), QStringLiteral("配置上传完成。"));
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

    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    const QString password = fixedRemotePassword();
    const quint16 port = fixedRemoteSshPort().toUShort();
    const QString remoteBaseDir = configRemoteBaseDir();
    const QString remoteArchivePath = QStringLiteral("/tmp/cepb_config_download.tar.gz");
    const QString archivePath = QDir::temp().filePath(
        QStringLiteral("cepb_config_download_%1.tar.gz").arg(QUuid::createUuid().toString(QUuid::Id128)));
    const SshClient::Connection sshConnection{host, port, user, password};

    QStringList quotedPaths;
    for (const QString &relativePath : configTransferPathList()) {
        quotedPaths << remoteShellQuote(relativePath);
    }
    const QString packageCommand = QStringLiteral(
        "set -e; cd %1; paths=\"\"; for p in %2; do [ -e \"$p\" ] && { find \"$p\" -type f -print; paths=\"$paths $p\"; }; done; "
        "[ -n \"$paths\" ] || { echo 'no config paths found'; exit 2; }; tar -czf %3 $paths")
        .arg(remoteShellQuote(remoteBaseDir),
             quotedPaths.join(QLatin1Char(' ')),
             remoteShellQuote(remoteArchivePath));

    QProgressDialog progress(QStringLiteral("准备下载配置..."), QStringLiteral("取消"), 0, 4, this);
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

    setDownloadStep(2, QStringLiteral("本地解包配置..."));
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
    setDownloadStep(3, QStringLiteral("重新导入下载后的配置..."));
    onImportIec104ConfigClicked();
    progress.setValue(4);
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
    if (folderName.compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }
    if (folderName.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }
    if (folderName.compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }
    if (folderName.compare(QStringLiteral("IEC101ServiceChannel"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }

    return absolutePath;
}

QString MainWindow::resolveIec104AppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepiec104"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::resolveModbusAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepmodbus"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::resolveLogicCenterAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::resolveIec101ServiceChannelAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("IEC101ServiceChannel"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("IEC101ServiceChannel"));
    return QDir(appDir).exists() ? appDir : QString();
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

