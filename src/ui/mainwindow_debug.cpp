#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
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

#include <algorithm>

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
        m_dataRefFilterEdit->setEnabled(true);
        m_descriptionFilterEdit->setEnabled(true);
        m_autoRefreshCombo->setEnabled(true);
        requestServiceChannelData(false);
        updateAutoRefreshTimer();
    }
}

void MainWindow::onDisconnected()
{
    m_autoRefreshTimer->stop();
    m_highlightRefreshTimer->stop();
    updateUIState(false);
    appendSystem("已断开", "#ff4500");
    m_statusLabel->setText("未连接");

    if (currentAppConfig().viewMode == AppViewMode::DataTable) {
        m_serviceChannelItems.clear();
        m_previousServiceChannelItemMap.clear();
        m_timeHighlightUntilMap.clear();
        m_valueHighlightUntilMap.clear();
        refreshDeviceFilterOptions();
        m_dataTable->setRowCount(0);
        m_deviceFilterCombo->setEnabled(false);
        m_dataRefFilterEdit->clear();
        m_dataRefFilterEdit->setEnabled(false);
        m_descriptionFilterEdit->clear();
        m_descriptionFilterEdit->setEnabled(false);
        m_autoRefreshCombo->setEnabled(false);
    }
}

void MainWindow::onError(const QString &err)
{
    m_autoRefreshTimer->stop();
    m_highlightRefreshTimer->stop();
    appendSystem("错误: " + err, "#ff4444");
    updateUIState(false);
    m_statusLabel->setText("连接错误");
}

void MainWindow::onLogLine(const QString &line)
{
    appendLog(line);
}

void MainWindow::onCommandReply(const QString &reply)
{
    if (currentAppConfig().viewMode == AppViewMode::DataTable) {
        const QList<ServiceChannelDataItem> items = parseServiceChannelDataReply(reply);
        if (items.isEmpty()) {
            appendSystem("未解析到 ServiceChannel 数据: " + reply, "#ffcc66");
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
        appendSystem(QString("ServiceChannel 数据已加载，共 %1 条").arg(items.size()), "#87ceeb");
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
    m_deviceFilterCombo->setEnabled(connected && !terminalMode);
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
        appendSystem("未连接 ServiceChannel，无法刷新数据", "#ffcc66");
        return;
    }

    if (m_client->isExecutingCommand()) {
        return;
    }

    if (logRequest) {
        appendSystem("=> dataread all", "#aaaaaa");
    }
    m_client->sendCommand("dataread all");
}

void MainWindow::onDeviceFilterChanged(int /*index*/)
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

QList<ServiceChannelDataItem> MainWindow::parseServiceChannelDataReply(const QString &reply) const
{
    static const QRegularExpression linePattern(
        R"(^([^\s]+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?)\s+([^\s]+)$)"
    );

    QList<ServiceChannelDataItem> items;
    for (const QString &rawLine : reply.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QRegularExpressionMatch match = linePattern.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const QString key = match.captured(1).trimmed();
        const int keySeparator = key.indexOf('#');
        if (keySeparator <= 0 || keySeparator >= key.size() - 1) {
            continue;
        }

        ServiceChannelDataItem item;
        item.deviceId = key.left(keySeparator);
        item.dataRef = key.mid(keySeparator + 1);
        item.description = match.captured(2).trimmed();
        item.dataTime = match.captured(3).trimmed();
        item.value = match.captured(4).trimmed();
        items.append(item);
    }

    return items;
}

void MainWindow::populateServiceChannelTable(const QList<ServiceChannelDataItem> &items)
{
    const QColor defaultTextColor("#ffffff");
    const QColor changedTextColor("#32cd32");
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
        auto *descriptionItem = new QTableWidgetItem(item.description);
        auto *dataTimeItem = new QTableWidgetItem(item.dataTime);
        auto *valueItem = new QTableWidgetItem(item.value);

        deviceIdItem->setForeground(defaultTextColor);
        dataRefItem->setForeground(defaultTextColor);
        descriptionItem->setForeground(defaultTextColor);
        dataTimeItem->setForeground(timeChanged ? changedTextColor : defaultTextColor);
        valueItem->setForeground(valueChanged ? changedTextColor : defaultTextColor);

        m_dataTable->setItem(row, 0, deviceIdItem);
        m_dataTable->setItem(row, 1, dataRefItem);
        m_dataTable->setItem(row, 2, descriptionItem);
        m_dataTable->setItem(row, 3, dataTimeItem);
        m_dataTable->setItem(row, 4, valueItem);
    }

    m_dataTable->resizeRowsToContents();
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
    const QString dataRefKeyword = m_dataRefFilterEdit->text().trimmed();
    const QString descriptionKeyword = m_descriptionFilterEdit->text().trimmed();
    QList<ServiceChannelDataItem> filteredItems;

    for (const ServiceChannelDataItem &item : m_serviceChannelItems) {
        const bool matchesDeviceId = selectedDeviceId.isEmpty() || item.deviceId == selectedDeviceId;
        const bool matchesDataRef = dataRefKeyword.isEmpty() ||
                                    item.dataRef.contains(dataRefKeyword, Qt::CaseInsensitive);
        const bool matchesDescription = descriptionKeyword.isEmpty() ||
                                        item.description.contains(descriptionKeyword, Qt::CaseInsensitive);

        if (matchesDeviceId && matchesDataRef && matchesDescription) {
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

QString MainWindow::serviceChannelItemKey(const ServiceChannelDataItem &item) const
{
    return item.deviceId + "#" + item.dataRef;
}
