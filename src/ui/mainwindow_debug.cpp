#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString controlTypeText(int ctrlType)
{
    switch (ctrlType) {
    case 0:
        return QStringLiteral("0 - 遥控选择");
    case 1:
        return QStringLiteral("1 - 遥控执行");
    case 2:
        return QStringLiteral("2 - 遥控取消");
    case 3:
        return QStringLiteral("3 - 遥调选择");
    case 4:
        return QStringLiteral("4 - 遥调执行");
    case 5:
        return QStringLiteral("5 - 遥调取消");
    case 6:
        return QStringLiteral("6 - 遥控直接控制");
    case 7:
        return QStringLiteral("7 - 遥调直接控制");
    default:
        return QString::number(ctrlType);
    }
}

bool isServiceChannelApp(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("ServiceChannel"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("IEC101ServiceChannel"), Qt::CaseInsensitive) == 0;
}

bool isKnownServiceChannelServiceId(const QString &serviceId)
{
    return serviceId == QStringLiteral("analog") ||
           serviceId == QStringLiteral("discrete") ||
           serviceId == QStringLiteral("control");
}

} // namespace

void MainWindow::onConnectClicked()
{
    QString host = m_ipEdit->text().trimmed();
    const AppConfig appConfig = currentAppConfig();
    quint16 port = appConfig.port;
    if (host.isEmpty()) {
        QMessageBox::warning(this, "警告", "IP地址不能为空");
        return;
    }

    m_client->setPromptPattern(appConfig.prompt);

    appendSystem("正在连接 " + host + ":" + QString::number(port) + " ...");
    m_client->connectToHost(host, port);
    m_connectBtn->setEnabled(false);
}

void MainWindow::onDisconnectClicked()
{
    m_client->disconnectFromHost();
}

void MainWindow::onSendClicked()
{
    QString cmd = m_cmdEdit->text().trimmed();
    if (cmd.isEmpty())
        return;
    appendSystem("=> " + cmd, "#aaaaaa");
    m_client->sendCommand(cmd);
    m_cmdEdit->clear();
}

void MainWindow::onQuickCommandClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn)
        return;
    QString cmd = btn->property("command").toString();
    appendSystem("=> " + cmd, "#aaaaaa");
    m_client->sendCommand(cmd);
}

void MainWindow::onAppSelectionChanged(int /*index*/)
{
    applyCurrentAppView();
}

void MainWindow::onConnected()
{
    const AppConfig appConfig = currentAppConfig();
    updateUIState(true);
    appendSystem("已连接", "#32cd32");
    m_statusLabel->setText("已连接 | " + m_ipEdit->text() + ":" + QString::number(appConfig.port));

    if (appConfig.viewMode == AppViewMode::DataTable) {
        m_deviceFilterCombo->setEnabled(true);
        m_serviceTypeFilterCombo->setEnabled(true);
        m_dataRefFilterEdit->setEnabled(true);
        m_descriptionFilterEdit->setEnabled(true);
        m_autoRefreshCombo->setEnabled(true);
        updateControlCommandUi();
        requestServiceChannelData(false);
        updateAutoRefreshTimer();
    }
}

void MainWindow::onDisconnected()
{
    m_autoRefreshTimer->stop();
    m_highlightRefreshTimer->stop();
    m_controlResponseTimer->stop();
    m_waitingControlResponse = false;
    updateUIState(false);
    appendSystem("已断开", "#ff4500");
    m_statusLabel->setText("未连接");

    if (currentAppConfig().viewMode == AppViewMode::DataTable) {
        m_serviceChannelItems.clear();
        m_previousServiceChannelItemMap.clear();
        m_timeHighlightUntilMap.clear();
        m_valueHighlightUntilMap.clear();
        m_controlStatusTextMap.clear();
        m_controlStatusColorMap.clear();
        refreshDeviceFilterOptions();
        m_dataTable->setRowCount(0);
        m_deviceFilterCombo->setEnabled(false);
        m_serviceTypeFilterCombo->setCurrentIndex(0);
        m_serviceTypeFilterCombo->setEnabled(false);
        m_dataRefFilterEdit->clear();
        m_dataRefFilterEdit->setEnabled(false);
        m_descriptionFilterEdit->clear();
        m_descriptionFilterEdit->setEnabled(false);
        m_autoRefreshCombo->setEnabled(false);
        m_serviceChannelDataFrozen = false;
        m_pendingDataFreezeMode.clear();
        if (m_controlStatusLabel) {
            m_controlStatusLabel->setText(QStringLiteral("状态: -"));
            m_controlStatusLabel->setStyleSheet(QString());
        }
        updateControlCommandUi();
    }
}

void MainWindow::onError(const QString &err)
{
    m_autoRefreshTimer->stop();
    m_highlightRefreshTimer->stop();
    m_controlResponseTimer->stop();
    m_waitingControlResponse = false;
    appendSystem("错误: " + err, "#ff4444");
    updateUIState(false);
    m_statusLabel->setText("连接错误");
}

void MainWindow::onLogLine(const QString &line)
{
    if (handleControlResponseLogLine(line)) {
        return;
    }

    appendLog(line);
}

void MainWindow::onCommandReply(const QString &reply)
{
    if (currentAppConfig().viewMode == AppViewMode::DataTable) {
        const QString appName = currentAppConfig().name;
        const QString pendingCommand = m_pendingDataTableCommand;
        m_pendingDataTableCommand.clear();

        if (pendingCommand == QStringLiteral("ctrlcmd")) {
            appendReply(reply);
            if (reply.startsWith(QStringLiteral("ctrlcmd sent:"), Qt::CaseInsensitive)) {
                appendSystem(QStringLiteral("控制命令已发送，等待 CTRLRESP..."), "#ffcc66");
            }
            updateControlCommandUi();
            return;
        }

        if (pendingCommand == QStringLiteral("datawrite")) {
            appendReply(reply);
            const QString replyText = reply.trimmed();
            const bool sent = replyText.startsWith(QStringLiteral("datawrite sent:"), Qt::CaseInsensitive);
            const bool rejected = replyText.startsWith(QStringLiteral("datawrite rejected:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("invalid value:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("usage:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("mqtt client unavailable"), Qt::CaseInsensitive);
            if (sent) {
                setControlStatus(m_pendingDataWriteDeviceId,
                                 m_pendingDataWriteDataRef,
                                 QStringLiteral("datawrite 成功 Value=%1 Quality=%2")
                                     .arg(m_pendingDataWriteValue, m_pendingDataWriteQuality),
                                 QColor(QStringLiteral("#32cd32")));
                appendSystem(QStringLiteral("datawrite 已发送，正在刷新数据..."), "#32cd32");
                requestServiceChannelData(false);
            } else if (rejected) {
                setControlStatus(m_pendingDataWriteDeviceId,
                                 m_pendingDataWriteDataRef,
                                 QStringLiteral("datawrite 失败: %1").arg(replyText),
                                 QColor(QStringLiteral("#ff4444")));
                appendSystem(QStringLiteral("datawrite 发送失败: %1").arg(replyText), "#ff4444");
            }
            m_pendingDataWriteDeviceId.clear();
            m_pendingDataWriteDataRef.clear();
            m_pendingDataWriteValue.clear();
            m_pendingDataWriteQuality.clear();
            updateControlCommandUi();
            return;
        }

        if (pendingCommand == QStringLiteral("datafreeze")) {
            appendReply(reply);
            const QString replyText = reply.trimmed();
            const bool rejected = replyText.startsWith(QStringLiteral("datafreeze rejected:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("invalid value:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("usage:"), Qt::CaseInsensitive) ||
                                  replyText.contains(QStringLiteral("error"), Qt::CaseInsensitive);
            if (!rejected) {
                if (m_pendingDataFreezeMode == QStringLiteral("on")) {
                    m_serviceChannelDataFrozen = true;
                    appendSystem(QStringLiteral("DataSpont 更新已冻结"), "#62a8ee");
                } else if (m_pendingDataFreezeMode == QStringLiteral("off")) {
                    m_serviceChannelDataFrozen = false;
                    appendSystem(QStringLiteral("DataSpont 更新已恢复"), "#32cd32");
                }
            } else {
                appendSystem(QStringLiteral("datafreeze 执行失败: %1").arg(replyText), "#ff4444");
            }
            m_pendingDataFreezeMode.clear();
            updateControlCommandUi();
            return;
        }

        QStringList dataReplyLines;
        for (const QString &line : reply.split('\n')) {
            if (!handleControlResponseLogLine(line)) {
                dataReplyLines.append(line);
            }
        }

        const QList<ServiceChannelDataItem> items = parseServiceChannelDataReply(dataReplyLines.join('\n'));
        if (items.isEmpty()) {
            if (pendingCommand == QStringLiteral("dataread") && !dataReplyLines.isEmpty()) {
                appendSystem(QStringLiteral("未解析到 %1 数据: %2").arg(appName, reply), "#ffcc66");
            } else {
                appendReply(reply);
            }
            updateControlCommandUi();
            return;
        }

        const QDateTime now = QDateTime::currentDateTime();
        const QDateTime highlightUntil = now.addSecs(3);
        for (const ServiceChannelDataItem &item : items) {
            const QString itemKey = serviceChannelItemKey(item);
            const ServiceChannelDataItem previousItem = m_previousServiceChannelItemMap.value(itemKey);
            const bool hasPreviousItem = !previousItem.deviceId.isEmpty() || !previousItem.dataRef.isEmpty();

            if (hasPreviousItem && previousItem.dataTime != item.dataTime) {
                m_timeHighlightUntilMap.insert(itemKey, highlightUntil);
            }
            if (hasPreviousItem && previousItem.value != item.value) {
                m_valueHighlightUntilMap.insert(itemKey, highlightUntil);
            }
        }

        m_serviceChannelItems = items;
        refreshDeviceFilterOptions();
        applyServiceChannelFilter();
        updateHighlightRefreshTimer();
        m_previousServiceChannelItemMap.clear();
        for (const ServiceChannelDataItem &item : m_serviceChannelItems) {
            m_previousServiceChannelItemMap.insert(serviceChannelItemKey(item), item);
        }
        appendSystem(QStringLiteral("%1 数据已加载，共 %2 条").arg(appName).arg(items.size()), "#87ceeb");
        updateControlCommandUi();
        return;
    }

    appendReply(reply);
}

void MainWindow::appendSystem(const QString &text, const QString &color)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QString("<span style='color:%1'>[%2] [SYS] %3</span>")
                       .arg(color, time, text.toHtmlEscaped());
    m_logView->append(html);
}

void MainWindow::appendLog(const QString &text)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QString("<span style='color:#888888'>[%1] %2</span>")
                       .arg(time, text.toHtmlEscaped());
    m_logView->append(html);
}

void MainWindow::appendReply(const QString &text)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    for (const QString &line : text.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        QString html = QString("<span style='color:#87ceeb'>[%1] %2</span>")
                           .arg(time, line.toHtmlEscaped());
        m_logView->append(html);
    }
}

void MainWindow::updateUIState(bool connected)
{
    const AppConfig appConfig = currentAppConfig();
    const bool terminalMode = appConfig.viewMode == AppViewMode::Terminal;

    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_sendBtn->setEnabled(connected && terminalMode);
    m_ipEdit->setEnabled(!connected);
    m_appCombo->setEnabled(!connected);
    m_cmdEdit->setEnabled(connected && terminalMode);
    m_refreshDataBtn->setEnabled(connected && !terminalMode);
    updateControlCommandUi();
    m_deviceFilterCombo->setEnabled(connected && !terminalMode);
    m_serviceTypeFilterCombo->setEnabled(connected && !terminalMode);
    m_dataRefFilterEdit->setEnabled(connected && !terminalMode);
    m_descriptionFilterEdit->setEnabled(connected && !terminalMode);
    m_autoRefreshCombo->setEnabled(connected && !terminalMode);

    for (auto *btn : findChildren<QPushButton*>()) {
        if (btn->property("command").isValid()) {
            btn->setEnabled(connected && terminalMode);
        }
    }
}

void MainWindow::applyCurrentAppView()
{
    const AppConfig appConfig = currentAppConfig();
    const bool terminalMode = appConfig.viewMode == AppViewMode::Terminal;

    m_contentStack->setCurrentIndex(terminalMode ? 0 : 1);
    m_cmdEdit->setPlaceholderText(terminalMode ? "输入命令后按回车..." : "当前 APP 使用数据展示视图");

    if (!m_client->isConnected()) {
        updateUIState(false);
        m_autoRefreshTimer->stop();
        m_highlightRefreshTimer->stop();
    } else {
        updateAutoRefreshTimer();
        updateHighlightRefreshTimer();
    }
    updateControlCommandUi();
}

AppConfig MainWindow::currentAppConfig() const
{
    const int configIndex = m_appCombo->currentData().toInt();
    if (configIndex >= 0 && configIndex < m_appConfigs.size()) {
        return m_appConfigs.at(configIndex);
    }

    return AppConfig{};
}

void MainWindow::requestServiceChannelData(bool logRequest)
{
    if (currentAppConfig().viewMode != AppViewMode::DataTable) {
        return;
    }

    if (!m_client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法刷新数据").arg(currentAppConfig().name), "#ffcc66");
        return;
    }

    if (m_client->isExecutingCommand()) {
        return;
    }

    if (logRequest) {
        appendSystem("=> dataread all", "#aaaaaa");
    }
    m_pendingDataTableCommand = QStringLiteral("dataread");
    m_client->sendCommand("dataread all");
    updateControlCommandUi();
}

void MainWindow::onDeviceFilterChanged(int /*index*/)
{
    applyServiceChannelFilter();
}

void MainWindow::onServiceTypeFilterChanged(int /*index*/)
{
    applyServiceChannelFilter();
}

void MainWindow::onDataRefFilterTextChanged(const QString & /*text*/)
{
    applyServiceChannelFilter();
}

void MainWindow::onDescriptionFilterTextChanged(const QString & /*text*/)
{
    applyServiceChannelFilter();
}

void MainWindow::onAutoRefreshIntervalChanged(int /*index*/)
{
    updateAutoRefreshTimer();
}

void MainWindow::onSendControlClicked()
{
    if (!m_dataTable || !m_dataTable->selectionModel()) {
        return;
    }

    const QModelIndex currentIndex = m_dataTable->currentIndex();
    if (currentIndex.isValid()) {
        openControlCommandDialog(currentIndex.row());
        return;
    }

    const QModelIndexList selectedIndexes = m_dataTable->selectionModel()->selectedIndexes();
    if (!selectedIndexes.isEmpty()) {
        openControlCommandDialog(selectedIndexes.first().row());
    }
}

void MainWindow::onDataTableCellDoubleClicked(int row, int /*column*/)
{
    if (isServiceChannelControlRow(row)) {
        openControlCommandDialog(row);
    } else if (isServiceChannelDataWriteRow(row)) {
        openDataWriteDialog(row);
    }
}

void MainWindow::onDataTableSelectionChanged()
{
    updateControlCommandUi();
}

QList<ServiceChannelDataItem> MainWindow::parseServiceChannelDataReply(const QString &reply) const
{
    static const QRegularExpression newLinePattern(
        R"(^([^\s]+)\s+([^\s]+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?)\s+([^\s]+)$)"
    );
    static const QRegularExpression legacyLinePattern(
        R"(^([^\s]+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?)\s+([^\s]+)$)"
    );

    QList<ServiceChannelDataItem> items;
    for (const QString &rawLine : reply.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QRegularExpressionMatch match = newLinePattern.match(line);
        const QString serviceId = match.hasMatch() ? match.captured(2).trimmed().toLower() : QString();
        const bool hasServiceId = match.hasMatch() && isKnownServiceChannelServiceId(serviceId);
        const QRegularExpressionMatch legacyMatch = hasServiceId ? QRegularExpressionMatch() : legacyLinePattern.match(line);
        const QRegularExpressionMatch activeMatch = hasServiceId ? match : legacyMatch;
        if (!activeMatch.hasMatch()) {
            continue;
        }

        const QString key = activeMatch.captured(1).trimmed();
        const int keySeparator = key.indexOf('#');
        if (keySeparator <= 0 || keySeparator >= key.size() - 1) {
            continue;
        }

        ServiceChannelDataItem item;
        item.deviceId = key.left(keySeparator);
        item.dataRef = key.mid(keySeparator + 1);
        if (hasServiceId) {
            item.serviceId = serviceId;
            item.description = activeMatch.captured(3).trimmed();
            item.dataTime = activeMatch.captured(4).trimmed();
            item.value = activeMatch.captured(5).trimmed();
        } else {
            item.description = activeMatch.captured(2).trimmed();
            item.dataTime = activeMatch.captured(3).trimmed();
            item.value = activeMatch.captured(4).trimmed();
        }
        items.append(item);
    }

    return items;
}

void MainWindow::populateServiceChannelTable(const QList<ServiceChannelDataItem> &items)
{
    const QColor defaultTextColor = serviceChannelDefaultTextColor();
    const QColor changedTextColor = serviceChannelChangedTextColor();
    const QDateTime now = QDateTime::currentDateTime();

    m_dataTable->setRowCount(items.size());

    for (int row = 0; row < items.size(); ++row) {
        const ServiceChannelDataItem &item = items.at(row);
        const QString itemKey = serviceChannelItemKey(item);
        const bool timeChanged = m_timeHighlightUntilMap.value(itemKey).isValid() &&
                                 m_timeHighlightUntilMap.value(itemKey) > now;
        const bool valueChanged = m_valueHighlightUntilMap.value(itemKey).isValid() &&
                                  m_valueHighlightUntilMap.value(itemKey) > now;

        auto *deviceIdItem = new QTableWidgetItem(item.deviceId);
        auto *dataRefItem = new QTableWidgetItem(item.dataRef);
        auto *serviceIdItem = new QTableWidgetItem(item.serviceId);
        auto *descriptionItem = new QTableWidgetItem(item.description);
        auto *dataTimeItem = new QTableWidgetItem(item.dataTime);
        auto *valueItem = new QTableWidgetItem(item.value);
        auto *controlStatusItem = new QTableWidgetItem(m_controlStatusTextMap.value(itemKey));

        deviceIdItem->setForeground(defaultTextColor);
        dataRefItem->setForeground(defaultTextColor);
        serviceIdItem->setForeground(defaultTextColor);
        descriptionItem->setForeground(defaultTextColor);
        dataTimeItem->setForeground(timeChanged ? changedTextColor : defaultTextColor);
        valueItem->setForeground(valueChanged ? changedTextColor : defaultTextColor);
        controlStatusItem->setForeground(m_controlStatusColorMap.value(itemKey, defaultTextColor));

        m_dataTable->setItem(row, 0, deviceIdItem);
        m_dataTable->setItem(row, 1, dataRefItem);
        m_dataTable->setItem(row, 2, serviceIdItem);
        m_dataTable->setItem(row, 3, descriptionItem);
        m_dataTable->setItem(row, 4, dataTimeItem);
        m_dataTable->setItem(row, 5, valueItem);
        m_dataTable->setItem(row, 6, controlStatusItem);
    }

    m_dataTable->resizeRowsToContents();
    updateControlCommandUi();
}

void MainWindow::refreshDeviceFilterOptions()
{
    QSignalBlocker blocker(m_deviceFilterCombo);

    const QString currentFilter = m_deviceFilterCombo->currentData().toString();
    QSet<QString> seenDeviceIds;
    QStringList deviceIds;

    for (const ServiceChannelDataItem &item : m_serviceChannelItems) {
        if (item.deviceId.isEmpty() || seenDeviceIds.contains(item.deviceId)) {
            continue;
        }

        seenDeviceIds.insert(item.deviceId);
        deviceIds.append(item.deviceId);
    }

    std::sort(deviceIds.begin(), deviceIds.end(), [](const QString &left, const QString &right) {
        const bool leftIsNumber = !left.isEmpty() && std::all_of(left.cbegin(), left.cend(), [](QChar ch) { return ch.isDigit(); });
        const bool rightIsNumber = !right.isEmpty() && std::all_of(right.cbegin(), right.cend(), [](QChar ch) { return ch.isDigit(); });

        if (leftIsNumber && rightIsNumber) {
            return left.toInt() < right.toInt();
        }

        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });

    m_deviceFilterCombo->clear();
    m_deviceFilterCombo->addItem("all", QString());
    QHash<QString, QString> deviceDescriptions;
    const configtool::ConfigProject &project = m_configProjectManager.project();
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        const QString deviceId = device.deviceId.trimmed();
        if (!deviceId.isEmpty() && !device.deviceDesc.trimmed().isEmpty()) {
            deviceDescriptions.insert(deviceId, device.deviceDesc.trimmed());
        }
    }

    for (const QString &deviceId : deviceIds) {
        const QString description = deviceDescriptions.value(deviceId);
        const QString displayText = description.isEmpty()
            ? deviceId
            : QStringLiteral("%1 - %2").arg(deviceId, description);
        m_deviceFilterCombo->addItem(displayText, deviceId);
    }

    const int restoredIndex = m_deviceFilterCombo->findData(currentFilter);
    m_deviceFilterCombo->setCurrentIndex(restoredIndex >= 0 ? restoredIndex : 0);
}

void MainWindow::applyServiceChannelFilter()
{
    const QString selectedDeviceId = m_deviceFilterCombo->currentData().toString();
    const QString selectedServiceId = m_serviceTypeFilterCombo->currentData().toString();
    const QString dataRefKeyword = m_dataRefFilterEdit->text().trimmed();
    const QString descriptionKeyword = m_descriptionFilterEdit->text().trimmed();
    QList<ServiceChannelDataItem> filteredItems;

    for (const ServiceChannelDataItem &item : m_serviceChannelItems) {
        const bool matchesDeviceId = selectedDeviceId.isEmpty() || item.deviceId == selectedDeviceId;
        const bool matchesServiceId = selectedServiceId.isEmpty() || item.serviceId == selectedServiceId;
        const bool matchesDataRef = dataRefKeyword.isEmpty() ||
                                    item.dataRef.contains(dataRefKeyword, Qt::CaseInsensitive);
        const bool matchesDescription = descriptionKeyword.isEmpty() ||
                                        item.description.contains(descriptionKeyword, Qt::CaseInsensitive);

        if (matchesDeviceId && matchesServiceId && matchesDataRef && matchesDescription) {
            filteredItems.append(item);
        }
    }

    populateServiceChannelTable(filteredItems);
}

void MainWindow::updateAutoRefreshTimer()
{
    if (!m_client->isConnected() || currentAppConfig().viewMode != AppViewMode::DataTable) {
        m_autoRefreshTimer->stop();
        return;
    }

    const int intervalMs = m_autoRefreshCombo->currentData().toInt();
    if (intervalMs <= 0) {
        m_autoRefreshTimer->stop();
        return;
    }

    m_autoRefreshTimer->start(intervalMs);
}

void MainWindow::updateHighlightRefreshTimer()
{
    const QDateTime now = QDateTime::currentDateTime();

    for (auto it = m_timeHighlightUntilMap.begin(); it != m_timeHighlightUntilMap.end(); ) {
        if (!it.value().isValid() || it.value() <= now) {
            it = m_timeHighlightUntilMap.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_valueHighlightUntilMap.begin(); it != m_valueHighlightUntilMap.end(); ) {
        if (!it.value().isValid() || it.value() <= now) {
            it = m_valueHighlightUntilMap.erase(it);
        } else {
            ++it;
        }
    }

    if (!m_client->isConnected() || currentAppConfig().viewMode != AppViewMode::DataTable) {
        m_highlightRefreshTimer->stop();
        return;
    }

    if (m_timeHighlightUntilMap.isEmpty() && m_valueHighlightUntilMap.isEmpty()) {
        m_highlightRefreshTimer->stop();
        return;
    }

    if (!m_highlightRefreshTimer->isActive()) {
        m_highlightRefreshTimer->start();
    }
}

void MainWindow::copySelectedTableCells()
{
    if (!m_dataTable->selectionModel()) {
        return;
    }

    const QModelIndexList indexes = m_dataTable->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return;
    }

    QModelIndexList sortedIndexes = indexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end(), [](const QModelIndex &left, const QModelIndex &right) {
        if (left.row() != right.row()) {
            return left.row() < right.row();
        }
        return left.column() < right.column();
    });

    QString copiedText;
    int currentRow = sortedIndexes.first().row();
    bool firstCellInRow = true;

    for (const QModelIndex &index : sortedIndexes) {
        if (index.row() != currentRow) {
            copiedText += '\n';
            currentRow = index.row();
            firstCellInRow = true;
        }

        if (!firstCellInRow) {
            copiedText += '\t';
        }

        copiedText += index.data().toString();
        firstCellInRow = false;
    }

    QApplication::clipboard()->setText(copiedText);
}

void MainWindow::updateControlCommandUi()
{
    if (!m_sendControlBtn || !m_dataTable) {
        return;
    }

    const bool canSendDataTableCommand = m_client->isConnected() &&
                                         !m_client->isExecutingCommand() &&
                                         isServiceChannelApp(currentAppConfig());
    const bool controlEnabled = canSendDataTableCommand &&
                                isServiceChannelControlRow(m_dataTable->currentRow());
    m_sendControlBtn->setEnabled(controlEnabled);
    if (m_dataFreezeBtn) {
        m_dataFreezeBtn->setEnabled(canSendDataTableCommand);
        updateServiceChannelDataFreezeUi();
    }
}

void MainWindow::updateServiceChannelDataFreezeUi()
{
    if (!m_dataFreezeBtn) {
        return;
    }

    if (!m_client->isConnected() || !isServiceChannelApp(currentAppConfig())) {
        m_dataFreezeBtn->setText(QStringLiteral("冻结数据"));
        m_dataFreezeBtn->setToolTip(QStringLiteral("发送 datafreeze on/off，冻结或恢复 DataSpont 更新内部值"));
        m_dataFreezeBtn->setStyleSheet(QString());
        return;
    }

    if (m_client->isExecutingCommand() && m_pendingDataTableCommand == QStringLiteral("datafreeze")) {
        m_dataFreezeBtn->setText(m_pendingDataFreezeMode == QStringLiteral("off")
            ? QStringLiteral("恢复中...")
            : QStringLiteral("冻结中..."));
        m_dataFreezeBtn->setStyleSheet(
            QStringLiteral("QPushButton { background-color: #4a5568; color: #dbe7f6; border-color: #7f8fa6; }"));
        return;
    }

    if (m_serviceChannelDataFrozen) {
        m_dataFreezeBtn->setText(QStringLiteral("恢复更新"));
        m_dataFreezeBtn->setToolTip(QStringLiteral("当前 DataSpont 更新已冻结，点击发送 datafreeze off 恢复"));
        m_dataFreezeBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton {"
                "  color: #eef7ff;"
                "  font-weight: 700;"
                "  border: 1px solid #7bb6ff;"
                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                "                              stop:0 #0b2a56, stop:0.45 #123f7a,"
                "                              stop:0.46 #1c5aa0, stop:0.52 #123f7a,"
                "                              stop:1 #071a38);"
                "}"
                "QPushButton:hover {"
                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                "                              stop:0 #12366a, stop:0.45 #1a5194,"
                "                              stop:0.46 #2c76c6, stop:0.52 #1a5194,"
                "                              stop:1 #0b2448);"
                "}"));
    } else {
        m_dataFreezeBtn->setText(QStringLiteral("冻结数据"));
        m_dataFreezeBtn->setToolTip(QStringLiteral("点击发送 datafreeze on，冻结 DataSpont 更新内部值"));
        m_dataFreezeBtn->setStyleSheet(QString());
    }
}

bool MainWindow::handleControlResponseLogLine(const QString &line)
{
    const int eventIndex = line.indexOf(QStringLiteral("CTRLRESP "));
    if (eventIndex < 0) {
        return false;
    }

    const QString eventText = line.mid(eventIndex).trimmed();
    QHash<QString, QString> fields;
    static const QRegularExpression fieldPattern(R"((\w+)=([^\s]+))");
    QRegularExpressionMatchIterator it = fieldPattern.globalMatch(eventText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        fields.insert(match.captured(1), match.captured(2));
    }

    const QString deviceId = fields.value(QStringLiteral("DeviceId")).trimmed();
    const QString dataRef = fields.value(QStringLiteral("DataRefer")).trimmed();
    const QString ctrlType = fields.value(QStringLiteral("CtrlType")).trimmed();
    const QString ctrlVal = fields.value(QStringLiteral("CtrlVal")).trimmed();
    const QString result = fields.value(QStringLiteral("Result")).trimmed();
    const QString errorCode = fields.value(QStringLiteral("ErrorCode")).trimmed();

    if (deviceId.isEmpty() || dataRef.isEmpty()) {
        appendLog(line);
        return true;
    }

    appendLog(line);

    const QString normalizedResult = result.toLower();
    const bool resultOk = normalizedResult.isEmpty() ||
                          normalizedResult == QStringLiteral("0") ||
                          normalizedResult == QStringLiteral("true") ||
                          normalizedResult == QStringLiteral("ok") ||
                          normalizedResult == QStringLiteral("success");
    const bool errorOk = errorCode.isEmpty() || errorCode == QStringLiteral("0");
    const bool success = resultOk && errorOk;

    const bool matchesPending = m_waitingControlResponse &&
                                m_pendingControlDeviceId == deviceId &&
                                m_pendingControlDataRef == dataRef &&
                                (m_pendingControlType < 0 || QString::number(m_pendingControlType) == ctrlType);

    const QString statusText = success
        ? QStringLiteral("成功 CtrlType=%1 Value=%2").arg(ctrlType, ctrlVal)
        : QStringLiteral("失败 Result=%1 Error=%2").arg(result.isEmpty() ? QStringLiteral("-") : result,
                                                       errorCode.isEmpty() ? QStringLiteral("-") : errorCode);
    setControlStatus(deviceId,
                     dataRef,
                     statusText,
                     success ? QColor(QStringLiteral("#32cd32")) : QColor(QStringLiteral("#ff4444")));

    if (matchesPending) {
        m_controlResponseTimer->stop();
        m_waitingControlResponse = false;
        appendSystem(QStringLiteral("控制响应%1：%2#%3，CtrlType=%4，CtrlVal=%5，Result=%6，ErrorCode=%7")
                         .arg(success ? QStringLiteral("成功") : QStringLiteral("失败"),
                              deviceId,
                              dataRef,
                              ctrlType,
                              ctrlVal,
                              result.isEmpty() ? QStringLiteral("-") : result,
                              errorCode.isEmpty() ? QStringLiteral("-") : errorCode),
                     success ? "#32cd32" : "#ff4444");
        requestServiceChannelData(false);
    } else {
        appendSystem(QStringLiteral("收到控制响应：%1#%2，%3").arg(deviceId, dataRef, statusText),
                     success ? "#32cd32" : "#ff4444");
    }

    return true;
}

void MainWindow::handleControlResponseTimeout()
{
    if (!m_waitingControlResponse) {
        return;
    }

    const QString deviceId = m_pendingControlDeviceId;
    const QString dataRef = m_pendingControlDataRef;
    m_waitingControlResponse = false;
    setControlStatus(deviceId,
                     dataRef,
                     QStringLiteral("响应超时 CtrlType=%1 Value=%2")
                         .arg(m_pendingControlType)
                         .arg(m_pendingControlValue),
                     QColor(QStringLiteral("#ffcc66")));
    appendSystem(QStringLiteral("控制命令已发送，但 15 秒内未收到 CTRLRESP：%1#%2").arg(deviceId, dataRef), "#ffcc66");
}

QString MainWindow::controlCommandKey(const QString &deviceId, const QString &dataRef) const
{
    return deviceId + QLatin1Char('#') + dataRef;
}

void MainWindow::setControlStatus(const QString &deviceId,
                                  const QString &dataRef,
                                  const QString &statusText,
                                  const QColor &color)
{
    const QString key = controlCommandKey(deviceId, dataRef);
    m_controlStatusTextMap.insert(key, statusText);
    m_controlStatusColorMap.insert(key, color);

    if (m_controlStatusLabel) {
        m_controlStatusLabel->setText(QStringLiteral("状态: %1").arg(statusText));
        m_controlStatusLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(color.name()));
    }

    applyServiceChannelFilter();
}

void MainWindow::openControlCommandDialog(int row)
{
    if (!isServiceChannelControlRow(row)) {
        return;
    }

    ServiceChannelDataItem item;
    item.deviceId = m_dataTable->item(row, 0)->text().trimmed();
    item.dataRef = m_dataTable->item(row, 1)->text().trimmed();
    item.serviceId = m_dataTable->item(row, 2)->text().trimmed().toLower();
    item.description = m_dataTable->item(row, 3)->text().trimmed();
    item.dataTime = m_dataTable->item(row, 4)->text().trimmed();
    item.value = m_dataTable->item(row, 5)->text().trimmed();

    configtool::ControlKind controlKind = configtool::ControlKind::None;
    isServiceChannelControlPoint(item, &controlKind);

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("发送控制命令"));
    dialog.resize(640, dialog.height());
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();

    auto *deviceEdit = new QLineEdit(item.deviceId, &dialog);
    deviceEdit->setReadOnly(true);
    form->addRow(QStringLiteral("DeviceId:"), deviceEdit);

    auto *dataRefEdit = new QLineEdit(item.dataRef, &dialog);
    dataRefEdit->setReadOnly(true);
    dataRefEdit->setMinimumWidth(480);
    form->addRow(QStringLiteral("DataRef:"), dataRefEdit);

    auto *serviceIdEdit = new QLineEdit(item.serviceId, &dialog);
    serviceIdEdit->setReadOnly(true);
    form->addRow(QStringLiteral("ServiceId:"), serviceIdEdit);

    auto *descriptionEdit = new QLineEdit(item.description, &dialog);
    descriptionEdit->setReadOnly(true);
    descriptionEdit->setMinimumWidth(480);
    form->addRow(QStringLiteral("描述:"), descriptionEdit);

    auto *ctrlValEdit = new QLineEdit(&dialog);
    ctrlValEdit->setPlaceholderText(QStringLiteral("输入 CtrlVal"));
    ctrlValEdit->setText(item.value);
    ctrlValEdit->selectAll();
    form->addRow(QStringLiteral("CtrlVal:"), ctrlValEdit);

    auto *ctrlTypeCombo = new QComboBox(&dialog);
    for (int ctrlType = 0; ctrlType <= 7; ++ctrlType) {
        ctrlTypeCombo->addItem(controlTypeText(ctrlType), ctrlType);
    }
    const int defaultCtrlType = controlKind == configtool::ControlKind::RemoteAdjust ? 4 : 1;
    ctrlTypeCombo->setCurrentIndex(ctrlTypeCombo->findData(defaultCtrlType));
    form->addRow(QStringLiteral("CtrlType:"), ctrlTypeCombo);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("发送"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    ctrlValEdit->setFocus();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString ctrlVal = ctrlValEdit->text().trimmed();
    if (ctrlVal.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("CtrlVal 不能为空"));
        return;
    }

    sendServiceChannelControlCommand(item, ctrlVal, ctrlTypeCombo->currentData().toInt());
}

void MainWindow::openDataWriteDialog(int row)
{
    if (!isServiceChannelDataWriteRow(row)) {
        return;
    }

    ServiceChannelDataItem item;
    item.deviceId = m_dataTable->item(row, 0)->text().trimmed();
    item.dataRef = m_dataTable->item(row, 1)->text().trimmed();
    item.serviceId = m_dataTable->item(row, 2)->text().trimmed().toLower();
    item.description = m_dataTable->item(row, 3)->text().trimmed();
    item.dataTime = m_dataTable->item(row, 4)->text().trimmed();
    item.value = m_dataTable->item(row, 5)->text().trimmed();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("发送 datawrite"));
    dialog.resize(640, dialog.height());
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();

    auto *deviceEdit = new QLineEdit(item.deviceId, &dialog);
    deviceEdit->setReadOnly(true);
    form->addRow(QStringLiteral("DeviceId:"), deviceEdit);

    auto *dataRefEdit = new QLineEdit(item.dataRef, &dialog);
    dataRefEdit->setReadOnly(true);
    dataRefEdit->setMinimumWidth(480);
    form->addRow(QStringLiteral("DataRef:"), dataRefEdit);

    auto *serviceIdEdit = new QLineEdit(item.serviceId, &dialog);
    serviceIdEdit->setReadOnly(true);
    form->addRow(QStringLiteral("ServiceId:"), serviceIdEdit);

    auto *descriptionEdit = new QLineEdit(item.description, &dialog);
    descriptionEdit->setReadOnly(true);
    descriptionEdit->setMinimumWidth(480);
    form->addRow(QStringLiteral("描述:"), descriptionEdit);

    auto *valueEdit = new QLineEdit(&dialog);
    valueEdit->setPlaceholderText(QStringLiteral("输入 Value"));
    valueEdit->setText(item.value);
    valueEdit->selectAll();
    form->addRow(QStringLiteral("Value:"), valueEdit);

    auto *qualityEdit = new QLineEdit(QStringLiteral("00"), &dialog);
    qualityEdit->setPlaceholderText(QStringLiteral("默认 00"));
    form->addRow(QStringLiteral("Quality:"), qualityEdit);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("发送"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    valueEdit->setFocus();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString value = valueEdit->text().trimmed();
    const QString quality = qualityEdit->text().trimmed();
    if (value.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("Value 不能为空"));
        return;
    }
    if (quality.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("Quality 不能为空"));
        return;
    }

    bool valueOk = false;
    value.toDouble(&valueOk);
    if (!valueOk) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("Value 必须是数字"));
        return;
    }

    sendServiceChannelDataWriteCommand(item, value, quality);
}

bool MainWindow::isServiceChannelControlRow(int row) const
{
    if (!m_dataTable || row < 0 || row >= m_dataTable->rowCount()) {
        return false;
    }

    if (!isServiceChannelApp(currentAppConfig())) {
        return false;
    }

    const QTableWidgetItem *deviceItem = m_dataTable->item(row, 0);
    const QTableWidgetItem *dataRefItem = m_dataTable->item(row, 1);
    const QTableWidgetItem *serviceIdItem = m_dataTable->item(row, 2);
    const QTableWidgetItem *descriptionItem = m_dataTable->item(row, 3);
    if (!deviceItem || !dataRefItem || !serviceIdItem || !descriptionItem) {
        return false;
    }

    ServiceChannelDataItem item;
    item.deviceId = deviceItem->text().trimmed();
    item.dataRef = dataRefItem->text().trimmed();
    item.serviceId = serviceIdItem->text().trimmed().toLower();
    item.description = descriptionItem->text().trimmed();
    return isServiceChannelControlPoint(item);
}

bool MainWindow::isServiceChannelControlPoint(const ServiceChannelDataItem &item,
                                              configtool::ControlKind *controlKind) const
{
    if (controlKind) {
        *controlKind = configtool::ControlKind::None;
    }

    const QString serviceId = item.serviceId.trimmed().toLower();
    const bool hasExplicitControlService = serviceId == QStringLiteral("control");
    if (serviceId == QStringLiteral("analog") || serviceId == QStringLiteral("discrete")) {
        return false;
    }

    const configtool::ConfigProject &project = m_configProjectManager.project();
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.deviceId.trimmed() != item.deviceId) {
            continue;
        }

        const configtool::ModelTemplate *matchedModel = nullptr;
        for (const configtool::ModelTemplate &model : project.models) {
            if (model.modelId == device.modelId) {
                matchedModel = &model;
                break;
            }
        }

        if (!matchedModel) {
            break;
        }

        for (const configtool::ServiceTemplate &service : matchedModel->services) {
            if (service.type != configtool::ModelServiceType::Control) {
                continue;
            }

            for (const configtool::PointTemplate &point : service.points) {
                if (point.dataRef() == item.dataRef ||
                    point.pointRef(matchedModel->modelId) == item.dataRef) {
                    if (controlKind) {
                        *controlKind = point.controlKind;
                    }
                    return true;
                }
            }
        }
    }

    if (hasExplicitControlService) {
        if (controlKind) {
            *controlKind = inferControlKindFromText(item);
        }
        return true;
    }

    const QString dataRef = item.dataRef.trimmed();
    const QString lowerDataRef = dataRef.toLower();
    const QString description = item.description.trimmed();
    const bool looksLikeControlSuffix = lowerDataRef.endsWith(QStringLiteral("_ctrl"));
    const bool looksLikeControlDescription = description.contains(QStringLiteral("控制")) ||
                                             description.contains(QStringLiteral("遥控")) ||
                                             description.contains(QStringLiteral("遥调"));

    if (looksLikeControlSuffix || looksLikeControlDescription) {
        if (controlKind) {
            *controlKind = inferControlKindFromText(item);
        }
        return true;
    }

    return false;
}

bool MainWindow::isServiceChannelDataWriteRow(int row) const
{
    if (!m_dataTable || row < 0 || row >= m_dataTable->rowCount()) {
        return false;
    }

    if (!isServiceChannelApp(currentAppConfig())) {
        return false;
    }

    const QTableWidgetItem *deviceItem = m_dataTable->item(row, 0);
    const QTableWidgetItem *dataRefItem = m_dataTable->item(row, 1);
    const QTableWidgetItem *serviceIdItem = m_dataTable->item(row, 2);
    if (!deviceItem || !dataRefItem || !serviceIdItem) {
        return false;
    }

    ServiceChannelDataItem item;
    item.deviceId = deviceItem->text().trimmed();
    item.dataRef = dataRefItem->text().trimmed();
    item.serviceId = serviceIdItem->text().trimmed().toLower();
    return isServiceChannelDataWritePoint(item);
}

bool MainWindow::isServiceChannelDataWritePoint(const ServiceChannelDataItem &item) const
{
    const QString serviceId = item.serviceId.trimmed().toLower();
    return serviceId == QStringLiteral("analog") || serviceId == QStringLiteral("discrete");
}

configtool::ControlKind MainWindow::inferControlKindFromText(const ServiceChannelDataItem &item) const
{
    const QString lowerText = (item.dataRef + QLatin1Char(' ') + item.description).toLower();
    if (item.description.contains(QStringLiteral("遥调")) ||
        item.description.contains(QStringLiteral("设定")) ||
        lowerText.contains(QStringLiteral("totalp_ctrl")) ||
        lowerText.contains(QStringLiteral("totalq_ctrl")) ||
        lowerText.contains(QStringLiteral("voltage_ctrl")) ||
        lowerText.contains(QStringLiteral("p_ctrl")) ||
        lowerText.contains(QStringLiteral("q_ctrl"))) {
        return configtool::ControlKind::RemoteAdjust;
    }

    return configtool::ControlKind::RemoteControl;
}

void MainWindow::sendServiceChannelControlCommand(const ServiceChannelDataItem &item,
                                                  const QString &ctrlVal,
                                                  int ctrlType)
{
    const QString appName = currentAppConfig().name;
    if (!m_client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送控制命令").arg(appName), "#ffcc66");
        return;
    }

    if (m_client->isExecutingCommand()) {
        appendSystem(QStringLiteral("上一条命令尚未返回，暂不能发送控制命令"), "#ffcc66");
        return;
    }

    const QString command = QStringLiteral("ctrlcmd %1 %2 %3 %4")
        .arg(item.deviceId, item.dataRef, ctrlVal, QString::number(ctrlType));
    appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    m_waitingControlResponse = true;
    m_pendingControlDeviceId = item.deviceId;
    m_pendingControlDataRef = item.dataRef;
    m_pendingControlValue = ctrlVal;
    m_pendingControlType = ctrlType;
    setControlStatus(item.deviceId,
                     item.dataRef,
                     QStringLiteral("等待响应 CtrlType=%1 Value=%2").arg(ctrlType).arg(ctrlVal),
                     QColor(QStringLiteral("#ffcc66")));
    m_controlResponseTimer->start();
    m_pendingDataTableCommand = QStringLiteral("ctrlcmd");
    m_client->sendCommand(command);
    updateControlCommandUi();
}

void MainWindow::sendServiceChannelDataWriteCommand(const ServiceChannelDataItem &item,
                                                    const QString &value,
                                                    const QString &quality)
{
    const QString appName = currentAppConfig().name;
    if (!m_client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送 datawrite").arg(appName), "#ffcc66");
        return;
    }

    if (m_client->isExecutingCommand()) {
        appendSystem(QStringLiteral("上一条命令尚未返回，暂不能发送 datawrite"), "#ffcc66");
        return;
    }

    if (!isServiceChannelDataWritePoint(item)) {
        appendSystem(QStringLiteral("datawrite 仅支持遥测/遥信点位: %1#%2")
                         .arg(item.deviceId, item.dataRef),
                     "#ffcc66");
        return;
    }

    bool valueOk = false;
    value.toDouble(&valueOk);
    if (!valueOk) {
        appendSystem(QStringLiteral("datawrite Value 必须是数字: %1").arg(value), "#ffcc66");
        return;
    }

    const QString command = QStringLiteral("datawrite %1 %2 %3 %4")
        .arg(item.deviceId, item.dataRef, value, quality);
    appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    m_pendingDataWriteDeviceId = item.deviceId;
    m_pendingDataWriteDataRef = item.dataRef;
    m_pendingDataWriteValue = value;
    m_pendingDataWriteQuality = quality;
    setControlStatus(item.deviceId,
                     item.dataRef,
                     QStringLiteral("datawrite 发送中 Value=%1 Quality=%2").arg(value, quality),
                     QColor(QStringLiteral("#ffcc66")));
    m_pendingDataTableCommand = QStringLiteral("datawrite");
    m_client->sendCommand(command);
    updateControlCommandUi();
}

void MainWindow::sendServiceChannelDataFreezeCommand(const QString &mode)
{
    const QString appName = currentAppConfig().name;
    if (!m_client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送 datafreeze").arg(appName), "#ffcc66");
        return;
    }

    if (m_client->isExecutingCommand()) {
        appendSystem(QStringLiteral("上一条命令尚未返回，暂不能发送 datafreeze"), "#ffcc66");
        return;
    }

    const QString normalizedMode = mode.trimmed().toLower();
    if (normalizedMode != QStringLiteral("on") &&
        normalizedMode != QStringLiteral("off") &&
        normalizedMode != QStringLiteral("status")) {
        appendSystem(QStringLiteral("datafreeze 参数无效: %1").arg(mode), "#ffcc66");
        return;
    }

    const QString command = QStringLiteral("datafreeze %1").arg(normalizedMode);
    appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    m_pendingDataFreezeMode = normalizedMode;
    m_pendingDataTableCommand = QStringLiteral("datafreeze");
    m_client->sendCommand(command);
    updateControlCommandUi();
}

QString MainWindow::serviceChannelItemKey(const ServiceChannelDataItem &item) const
{
    return item.deviceId + "#" + item.dataRef;
}
