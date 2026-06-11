#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabWidget>
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

bool isModbusApp(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0;
}

bool isDataViewApp(const AppConfig &appConfig)
{
    return appConfig.viewMode == AppViewMode::DataTable ||
           appConfig.viewMode == AppViewMode::LogicAgcAvcTable;
}

QHash<QString, QString> parseAgcAvcParams(const QString &params)
{
    QHash<QString, QString> fields;
    static const QRegularExpression fieldPattern(R"((enable|distant|openloop|lock|upLock|downLock|target)=([^\s]+))");
    QRegularExpressionMatchIterator it = fieldPattern.globalMatch(params);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        fields.insert(match.captured(1), match.captured(2));
    }
    return fields;
}

bool isAgcAvcGatePassing(const QString &gateName, const QString &value)
{
    bool ok = false;
    const int intValue = value.toInt(&ok);
    if (!ok) {
        return false;
    }

    if (gateName == QStringLiteral("enable") ||
        gateName == QStringLiteral("distant")) {
        return intValue == 1;
    }

    return intValue == 0;
}

void clearTabWidgetPages(QTabWidget *tabs)
{
    if (!tabs) {
        return;
    }

    while (tabs->count() > 0) {
        QWidget *page = tabs->widget(0);
        tabs->removeTab(0);
        delete page;
    }
}

QFrame *createLogicMetricPanel(const QString &title,
                               const QString &value,
                               const QString &unit,
                               const QString &accentColor,
                               QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("logicMetricPanel"));
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setStyleSheet(QStringLiteral(
        "QFrame#logicMetricPanel {"
        "  border: 1px solid #c6d3df;"
        "  border-radius: 6px;"
        "  background: #ffffff;"
        "}"
    ));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    auto *titleLabel = new QLabel(title, panel);
    titleLabel->setStyleSheet(QStringLiteral("color: #5d6b78; font-size: 12px;"));
    layout->addWidget(titleLabel);

    auto *valueLabel = new QLabel(value.isEmpty() ? QStringLiteral("-") : value, panel);
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 26px; font-weight: 700;").arg(accentColor));
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(valueLabel);

    if (!unit.isEmpty()) {
        auto *unitLabel = new QLabel(unit, panel);
        unitLabel->setStyleSheet(QStringLiteral("color: #7c8792; font-size: 12px;"));
        layout->addWidget(unitLabel);
    }

    panel->setMinimumHeight(104);
    return panel;
}

QFrame *createLogicGateLamp(const QString &label,
                            const QString &value,
                            bool passing,
                            QWidget *parent)
{
    const QString color = passing ? QStringLiteral("#1f9d55") : QStringLiteral("#d64545");
    const QString softColor = passing ? QStringLiteral("#e7f6ed") : QStringLiteral("#fdeaea");
    const QString text = passing ? QStringLiteral("放行") : QStringLiteral("闭锁");

    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("logicGateLampPanel"));
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setStyleSheet(QStringLiteral(
        "QFrame#logicGateLampPanel {"
        "  border: 1px solid #c6d3df;"
        "  border-radius: 6px;"
        "  background: #ffffff;"
        "}"
    ));
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    auto *lamp = new QLabel(panel);
    lamp->setFixedSize(28, 28);
    lamp->setStyleSheet(QStringLiteral(
        "border-radius: 14px;"
        "background: %1;"
        "border: 3px solid %2;"
    ).arg(color, softColor));
    layout->addWidget(lamp);

    auto *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    auto *nameLabel = new QLabel(label, panel);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700;"));
    textLayout->addWidget(nameLabel);

    auto *statusLabel = new QLabel(QStringLiteral("%1  value=%2").arg(text, value.isEmpty() ? QStringLiteral("-") : value), panel);
    statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(color));
    textLayout->addWidget(statusLabel);

    layout->addLayout(textLayout, 1);
    return panel;
}

QWidget *createLogicGateSection(const QString &title,
                                const QString &params,
                                QWidget *parent)
{
    const QHash<QString, QString> fields = parseAgcAvcParams(params);
    auto *section = new QFrame(parent);
    section->setObjectName(QStringLiteral("logicGateSection"));
    section->setStyleSheet(QStringLiteral(
        "QFrame#logicGateSection {"
        "  border: 1px solid #d6e0ea;"
        "  border-radius: 6px;"
        "  background: #f8fafc;"
        "}"
    ));

    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title, section);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700;"));
    layout->addWidget(titleLabel);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QStringList gates = {
        QStringLiteral("enable"),
        QStringLiteral("distant"),
        QStringLiteral("openloop"),
        QStringLiteral("lock"),
        QStringLiteral("upLock"),
        QStringLiteral("downLock")
    };

    for (int index = 0; index < gates.size(); ++index) {
        const QString gateName = gates.at(index);
        const QString value = fields.value(gateName);
        grid->addWidget(createLogicGateLamp(gateName, value, isAgcAvcGatePassing(gateName, value), section),
                        index / 3,
                        index % 3);
    }

    layout->addLayout(grid);
    return section;
}

bool isKnownServiceChannelServiceId(const QString &serviceId)
{
    return serviceId == QStringLiteral("analog") ||
           serviceId == QStringLiteral("discrete") ||
           serviceId == QStringLiteral("accumulator") ||
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

    if (isDataViewApp(appConfig)) {
        const bool dataTableApp = appConfig.viewMode == AppViewMode::DataTable;
        m_deviceFilterCombo->setEnabled(dataTableApp);
        m_serviceTypeFilterCombo->setEnabled(dataTableApp);
        m_dataRefFilterEdit->setEnabled(dataTableApp);
        m_descriptionFilterEdit->setEnabled(dataTableApp);
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

    if (isDataViewApp(currentAppConfig())) {
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
        if (m_logicAgcAvcStatusTabs) {
            clearTabWidgetPages(m_logicAgcAvcStatusTabs);
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
    if (isDataViewApp(currentAppConfig())) {
        const QString appName = currentAppConfig().name;
        const QString pendingCommand = m_pendingDataTableCommand;
        m_pendingDataTableCommand.clear();

        if (currentAppConfig().viewMode == AppViewMode::LogicAgcAvcTable) {
            const QList<LogicAgcAvcStatusItem> items = parseLogicAgcAvcReply(reply);
            if (items.isEmpty()) {
                appendReply(reply);
            } else {
                populateLogicAgcAvcTable(items);
                appendSystem(QStringLiteral("%1 AGC/AVC 状态已加载，共 %2 组").arg(appName).arg(items.size()), "#87ceeb");
            }
            updateControlCommandUi();
            return;
        }

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
    const bool modbusMode = isModbusApp(appConfig);
    const bool dataViewMode = isDataViewApp(appConfig);
    const bool dataTableMode = appConfig.viewMode == AppViewMode::DataTable;

    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_sendBtn->setEnabled(connected && terminalMode);
    m_ipEdit->setEnabled(!connected);
    m_appCombo->setEnabled(!connected);
    m_cmdEdit->setEnabled(connected && terminalMode);
    m_refreshDataBtn->setEnabled(connected && dataViewMode);
    m_refreshDataBtn->setVisible(dataViewMode);
    if (m_modbusRawLogBtn) {
        m_modbusRawLogBtn->setVisible(modbusMode);
        m_modbusRawLogBtn->setEnabled(modbusMode);
    }
    updateControlCommandUi();
    m_deviceFilterCombo->setEnabled(connected && dataTableMode);
    m_serviceTypeFilterCombo->setEnabled(connected && dataTableMode);
    m_dataRefFilterEdit->setEnabled(connected && dataTableMode);
    m_descriptionFilterEdit->setEnabled(connected && dataTableMode);
    m_autoRefreshCombo->setEnabled(connected && dataViewMode);

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
    const bool serviceChannelMode = isServiceChannelApp(appConfig);
    const bool modbusMode = isModbusApp(appConfig);

    m_contentStack->setCurrentIndex(terminalMode ? 0 : 1);
    m_cmdEdit->setPlaceholderText(terminalMode ? "输入命令后按回车..." : "当前 APP 使用数据展示视图");
    configureDataTableForCurrentApp();
    if (m_modbusRawLogBtn) {
        m_modbusRawLogBtn->setVisible(modbusMode);
        m_modbusRawLogBtn->setEnabled(modbusMode);
    }
    if (m_dataFreezeBtn) {
        m_dataFreezeBtn->setVisible(serviceChannelMode);
    }
    if (m_sendControlBtn) {
        m_sendControlBtn->setVisible(serviceChannelMode);
    }
    if (m_refreshDataBtn) {
        m_refreshDataBtn->setVisible(!terminalMode);
    }

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
    if (!isDataViewApp(currentAppConfig())) {
        return;
    }

    if (!m_client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法刷新数据").arg(currentAppConfig().name), "#ffcc66");
        return;
    }

    if (m_client->isExecutingCommand()) {
        return;
    }

    const bool logicCenterMode = currentAppConfig().viewMode == AppViewMode::LogicAgcAvcTable;
    const QString command = logicCenterMode ? QStringLiteral("agcavc") : QStringLiteral("dataread all");
    if (logRequest) {
        appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    }
    m_pendingDataTableCommand = logicCenterMode ? QStringLiteral("agcavc") : QStringLiteral("dataread");
    m_client->sendCommand(command);
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

void MainWindow::onOpenModbusRawLogClicked()
{
    const AppConfig appConfig = currentAppConfig();
    if (!isModbusApp(appConfig)) {
        return;
    }

    const QString host = m_ipEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("IP地址不能为空"));
        return;
    }

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Modbus 原始 debugconsole 日志"));
    dialog->resize(900, 560);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *logView = new QTextEdit(dialog);
    logView->setReadOnly(true);
    logView->setLineWrapMode(QTextEdit::NoWrap);
    logView->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 12px;"
        "  background-color: #101820;"
        "  color: #d7e2ee;"
        "}"
    ));
    layout->addWidget(logView, 1);

    auto *buttonRow = new QHBoxLayout();
    auto *toggleModbusFrameBtn = new QPushButton(QStringLiteral("开启原始帧"), dialog);
    toggleModbusFrameBtn->setToolTip(QStringLiteral("发送 debug modbus on/off，切换 Modbus 原始轮询帧打印"));
    toggleModbusFrameBtn->setProperty("modbusDebugEnabled", false);
    auto *clearBtn = new QPushButton(QStringLiteral("清空"), dialog);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), dialog);
    buttonRow->addWidget(toggleModbusFrameBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(clearBtn);
    buttonRow->addWidget(closeBtn);
    layout->addLayout(buttonRow);

    auto appendRawLog = [logView](const QString &text, const QString &color = QStringLiteral("#d7e2ee")) {
        const QString time = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
        logView->append(QStringLiteral("<span style='color:%1'>[%2] %3</span>")
                            .arg(color, time, text.toHtmlEscaped()));
    };

    auto *client = new DebugConsoleClient(dialog);
    client->setPromptPattern(appConfig.prompt);

    connect(toggleModbusFrameBtn, &QPushButton::clicked, dialog, [client, toggleModbusFrameBtn, appendRawLog]() {
        if (!client->isConnected()) {
            appendRawLog(QStringLiteral("未连接，无法切换 Modbus 原始帧打印"), QStringLiteral("#ffcc66"));
            return;
        }
        if (client->isExecutingCommand()) {
            appendRawLog(QStringLiteral("上一条 debug 命令尚未返回"), QStringLiteral("#ffcc66"));
            return;
        }

        const bool enabled = toggleModbusFrameBtn->property("modbusDebugEnabled").toBool();
        const QString command = enabled ? QStringLiteral("debug modbus off") : QStringLiteral("debug modbus on");
        appendRawLog(QStringLiteral("=> %1").arg(command), QStringLiteral("#aaaaaa"));
        toggleModbusFrameBtn->setEnabled(false);
        client->sendCommand(command);
    });
    connect(clearBtn, &QPushButton::clicked, logView, &QTextEdit::clear);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);
    connect(dialog, &QDialog::finished, client, &DebugConsoleClient::disconnectFromHost);
    connect(client, &DebugConsoleClient::connected, dialog, [appendRawLog, host, appConfig]() {
        appendRawLog(QStringLiteral("已连接 %1:%2").arg(host).arg(appConfig.port), QStringLiteral("#32cd32"));
    });
    connect(client, &DebugConsoleClient::disconnected, dialog, [appendRawLog]() {
        appendRawLog(QStringLiteral("已断开"), QStringLiteral("#ffcc66"));
    });
    connect(client, &DebugConsoleClient::errorOccurred, dialog, [appendRawLog](const QString &error) {
        appendRawLog(QStringLiteral("错误: %1").arg(error), QStringLiteral("#ff4444"));
    });
    connect(client, &DebugConsoleClient::logLineReceived, dialog, [appendRawLog](const QString &line) {
        appendRawLog(line);
    });
    connect(client, &DebugConsoleClient::commandReplyReceived, dialog, [toggleModbusFrameBtn, appendRawLog](const QString &reply) {
        appendRawLog(reply, QStringLiteral("#87ceeb"));
        const QString normalizedReply = reply.trimmed().toLower();
        if (normalizedReply.contains(QStringLiteral("modbus=on"))) {
            toggleModbusFrameBtn->setProperty("modbusDebugEnabled", true);
            toggleModbusFrameBtn->setText(QStringLiteral("关闭原始帧"));
        } else if (normalizedReply.contains(QStringLiteral("modbus=off"))) {
            toggleModbusFrameBtn->setProperty("modbusDebugEnabled", false);
            toggleModbusFrameBtn->setText(QStringLiteral("开启原始帧"));
        }
        toggleModbusFrameBtn->setEnabled(true);
    });

    appendRawLog(QStringLiteral("正在连接 %1:%2 ...").arg(host).arg(appConfig.port), QStringLiteral("#87ceeb"));
    client->connectToHost(host, appConfig.port);
    dialog->show();
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

QList<LogicAgcAvcStatusItem> MainWindow::parseLogicAgcAvcReply(const QString &reply) const
{
    static const QRegularExpression devicesPattern(
        R"(^AGCAVC\[([^/\]]+)(?:/([^\]]+))?\]\s+devices:\s+total=([^\s]+)\s+online=([^\s]+)\s+offline=([^\s]+))"
    );
    static const QRegularExpression agcPattern(R"(^AGC\[([^\]]+)\]\s+params:\s+(.+)$)");
    static const QRegularExpression avcPattern(R"(^AVC\[([^\]]+)\]\s+params:\s+(.+)$)");
    static const QRegularExpression totalPPattern(R"(^(.+)的实时总有功[:：]\s*(.+)$)");
    static const QRegularExpression totalQPattern(R"(^(.+)的实时总无功[:：]\s*(.+)$)");
    static const QRegularExpression offlinePattern(R"(^AGCAVC\[([^\]]+)\]\s+offline=\(([^)]*)\):\s*(.*)$)");

    QList<LogicAgcAvcStatusItem> items;
    QHash<QString, int> groupRows;

    const auto ensureItem = [&items, &groupRows](const QString &groupId) -> LogicAgcAvcStatusItem& {
        const QString key = groupId.trimmed();
        if (!groupRows.contains(key)) {
            LogicAgcAvcStatusItem item;
            item.groupId = key;
            groupRows.insert(key, items.size());
            items.append(item);
        }
        return items[groupRows.value(key)];
    };

    for (const QString &rawLine : reply.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        line.remove(QRegularExpression(QStringLiteral(R"(\x1B\[[0-9;]*[A-Za-z])")));

        QRegularExpressionMatch match = devicesPattern.match(line);
        if (match.hasMatch()) {
            LogicAgcAvcStatusItem &item = ensureItem(match.captured(1));
            item.virtualDeviceId = match.captured(2).trimmed();
            item.totalDevices = match.captured(3).trimmed();
            item.onlineDevices = match.captured(4).trimmed();
            item.offlineDevices = match.captured(5).trimmed();
            continue;
        }

        match = agcPattern.match(line);
        if (match.hasMatch()) {
            LogicAgcAvcStatusItem &item = ensureItem(match.captured(1));
            item.agcParams = match.captured(2).trimmed();
            item.agcTarget = parseAgcAvcParams(item.agcParams).value(QStringLiteral("target"));
            continue;
        }

        match = avcPattern.match(line);
        if (match.hasMatch()) {
            LogicAgcAvcStatusItem &item = ensureItem(match.captured(1));
            item.avcParams = match.captured(2).trimmed();
            item.avcTarget = parseAgcAvcParams(item.avcParams).value(QStringLiteral("target"));
            continue;
        }

        match = totalPPattern.match(line);
        if (match.hasMatch()) {
            ensureItem(match.captured(1)).totalP = match.captured(2).trimmed();
            continue;
        }

        match = totalQPattern.match(line);
        if (match.hasMatch()) {
            ensureItem(match.captured(1)).totalQ = match.captured(2).trimmed();
            continue;
        }

        match = offlinePattern.match(line);
        if (match.hasMatch()) {
            LogicAgcAvcStatusItem &item = ensureItem(match.captured(1));
            if (item.offlineDevices.isEmpty()) {
                item.offlineDevices = match.captured(2).trimmed();
            }
            item.offlineList = match.captured(3).trimmed();
            continue;
        }
    }

    return items;
}

void MainWindow::populateLogicAgcAvcTable(const QList<LogicAgcAvcStatusItem> &items)
{
    if (!m_logicAgcAvcStatusTabs) {
        return;
    }

    clearTabWidgetPages(m_logicAgcAvcStatusTabs);

    for (const LogicAgcAvcStatusItem &item : items) {
        auto *page = new QWidget(m_logicAgcAvcStatusTabs);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(12);

        auto *summary = new QFrame(page);
        summary->setObjectName(QStringLiteral("logicSummaryPanel"));
        summary->setStyleSheet(QStringLiteral(
            "QFrame#logicSummaryPanel {"
            "  border: 1px solid #c6d3df;"
            "  border-radius: 6px;"
            "  background: #ffffff;"
            "}"
        ));
        auto *summaryLayout = new QGridLayout(summary);
        summaryLayout->setContentsMargins(14, 12, 14, 12);
        summaryLayout->setHorizontalSpacing(22);
        summaryLayout->setVerticalSpacing(6);

        const QString offlineList = item.offlineList.isEmpty() ? QStringLiteral("-") : item.offlineList;
        const QList<QPair<QString, QString>> summaryFields = {
            {QStringLiteral("Group"), item.groupId},
            {QStringLiteral("VirtualDevice"), item.virtualDeviceId},
            {QStringLiteral("设备总数"), item.totalDevices},
            {QStringLiteral("在线"), item.onlineDevices},
            {QStringLiteral("离线"), item.offlineDevices},
            {QStringLiteral("离线设备"), offlineList}
        };
        for (int index = 0; index < summaryFields.size(); ++index) {
            auto *nameLabel = new QLabel(summaryFields.at(index).first, summary);
            nameLabel->setStyleSheet(QStringLiteral("color: #607080; font-size: 12px;"));
            auto *valueLabel = new QLabel(summaryFields.at(index).second.isEmpty()
                                              ? QStringLiteral("-")
                                              : summaryFields.at(index).second,
                                          summary);
            valueLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700;"));
            valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            const int row = index / 3;
            const int column = (index % 3) * 2;
            summaryLayout->addWidget(nameLabel, row, column);
            summaryLayout->addWidget(valueLabel, row, column + 1);
        }
        layout->addWidget(summary);

        auto *metricGrid = new QGridLayout();
        metricGrid->setHorizontalSpacing(12);
        metricGrid->setVerticalSpacing(12);
        metricGrid->addWidget(createLogicMetricPanel(QStringLiteral("AGC Target"),
                                                     item.agcTarget,
                                                     QStringLiteral("目标有功"),
                                                     QStringLiteral("#1b5fa7"),
                                                     page),
                              0,
                              0);
        metricGrid->addWidget(createLogicMetricPanel(QStringLiteral("实时总有功"),
                                                     item.totalP,
                                                     QStringLiteral("AGC 实时值"),
                                                     QStringLiteral("#188038"),
                                                     page),
                              0,
                              1);
        metricGrid->addWidget(createLogicMetricPanel(QStringLiteral("AVC Target"),
                                                     item.avcTarget,
                                                     QStringLiteral("目标无功"),
                                                     QStringLiteral("#6d4bc2"),
                                                     page),
                              0,
                              2);
        metricGrid->addWidget(createLogicMetricPanel(QStringLiteral("实时总无功"),
                                                     item.totalQ,
                                                     QStringLiteral("AVC 实时值"),
                                                     QStringLiteral("#188038"),
                                                     page),
                              0,
                              3);
        layout->addLayout(metricGrid);

        auto *gateGrid = new QGridLayout();
        gateGrid->setHorizontalSpacing(12);
        gateGrid->setVerticalSpacing(12);
        gateGrid->addWidget(createLogicGateSection(QStringLiteral("AGC 门禁状态"), item.agcParams, page), 0, 0);
        gateGrid->addWidget(createLogicGateSection(QStringLiteral("AVC 门禁状态"), item.avcParams, page), 0, 1);
        layout->addLayout(gateGrid);
        layout->addStretch();

        m_logicAgcAvcStatusTabs->addTab(page, item.groupId.isEmpty() ? QStringLiteral("AGC/AVC") : item.groupId);
    }
}

void MainWindow::configureDataTableForCurrentApp()
{
    if (!m_dataTable || !m_dataViewStack) {
        return;
    }

    const bool logicMode = currentAppConfig().viewMode == AppViewMode::LogicAgcAvcTable;
    m_dataViewStack->setCurrentIndex(logicMode ? 1 : 0);
    if (m_serviceDataFilterWidget) {
        m_serviceDataFilterWidget->setVisible(!logicMode);
    }

    if (logicMode) {
        if (m_logicAgcAvcStatusTabs) {
            clearTabWidgetPages(m_logicAgcAvcStatusTabs);
        }
        return;
    }

    m_dataTable->clear();
    m_dataTable->setRowCount(0);
    m_dataTable->setColumnCount(7);
    m_dataTable->setHorizontalHeaderLabels({"DeviceId", "DataRef", "ServiceId", "Description", "DataTime", "Value", "Status"});
    const QList<int> widths = {140, 280, 90, 360, 170, 90, 180};
    for (int column = 0; column < widths.size(); ++column) {
        m_dataTable->setColumnWidth(column, widths.at(column));
    }
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
    if (!m_client->isConnected() || !isDataViewApp(currentAppConfig())) {
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
