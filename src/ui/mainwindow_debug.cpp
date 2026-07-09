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
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

class ClickableFrame : public QFrame
{
public:
    using QFrame::QFrame;

    std::function<void()> onClicked;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton &&
            rect().contains(event->position().toPoint()) &&
            isEnabled() &&
            onClicked) {
            onClicked();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }
};

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
    return appConfig.name.compare(QStringLiteral("North_CEP"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("North_101"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("North_104"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("North_Mqtt"), Qt::CaseInsensitive) == 0;
}

bool isModbusApp(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("South_Modbus"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0;
}

bool isIec104App(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("South_104"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("North_104"), Qt::CaseInsensitive) == 0;
}

bool isDlt645App(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("South_645"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("cepdlt645"), Qt::CaseInsensitive) == 0;
}

bool isIec101App(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("North_101"), Qt::CaseInsensitive) == 0;
}

bool isLogicCenterApp(const AppConfig &appConfig)
{
    return appConfig.name.compare(QStringLiteral("LogicCenter"), Qt::CaseInsensitive) == 0 ||
           appConfig.name.compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0;
}

bool supportsRawFrameLogWindow(const AppConfig &appConfig)
{
    return isServiceChannelApp(appConfig) ||
           isModbusApp(appConfig) ||
           isIec104App(appConfig) ||
           isDlt645App(appConfig) ||
           isIec101App(appConfig) ||
           isLogicCenterApp(appConfig);
}

QString rawFrameDebugCategory(const AppConfig &appConfig)
{
    if (appConfig.name.compare(QStringLiteral("North_CEP"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("north");
    }
    if (isIec104App(appConfig)) {
        return QStringLiteral("104");
    }
    if (isDlt645App(appConfig)) {
        return QStringLiteral("dlt645");
    }
    if (isIec101App(appConfig)) {
        return QStringLiteral("101");
    }
    if (isLogicCenterApp(appConfig)) {
        return QStringLiteral("compute");
    }
    return QStringLiteral("modbus");
}

QStringList rawFrameDebugCategories(const AppConfig &appConfig)
{
    if (isLogicCenterApp(appConfig)) {
        return {
            QStringLiteral("compute"),
            QStringLiteral("misc"),
            QStringLiteral("mqtt"),
            QStringLiteral("agcavc")
        };
    }
    return {rawFrameDebugCategory(appConfig)};
}

QString rawFrameDebugCategoryDisplayName(const QString &category)
{
    if (category == QStringLiteral("agcavc")) {
        return QStringLiteral("AGC/AVC");
    }
    return category;
}

QString rawFrameAppDisplayName(const AppConfig &appConfig)
{
    if (appConfig.name.compare(QStringLiteral("North_CEP"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("North_CEP");
    }
    if (appConfig.name.compare(QStringLiteral("North_104"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("North_104");
    }
    if (appConfig.name.compare(QStringLiteral("North_101"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("North_101");
    }
    if (appConfig.name.compare(QStringLiteral("North_Mqtt"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("North_Mqtt");
    }
    if (isIec104App(appConfig)) {
        return QStringLiteral("IEC104");
    }
    if (isDlt645App(appConfig)) {
        return QStringLiteral("DLT645");
    }
    if (isIec101App(appConfig)) {
        return QStringLiteral("IEC101");
    }
    if (isLogicCenterApp(appConfig)) {
        return QStringLiteral("LogicCenter");
    }
    return QStringLiteral("Modbus");
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

QString serviceChannelValueText(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2
        && value.startsWith(QLatin1Char('"'))
        && value.endsWith(QLatin1Char('"'))) {
        value = value.mid(1, value.size() - 2);
        value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    }
    return value;
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

QString logicGateDataRefName(const QString &domain, const QString &gateName)
{
    const QString normalizedDomain = domain.trimmed().toUpper();
    QString normalizedGate = gateName.trimmed();
    if (normalizedGate == QStringLiteral("upLock")) {
        normalizedGate = QStringLiteral("uplock");
    } else if (normalizedGate == QStringLiteral("downLock")) {
        normalizedGate = QStringLiteral("downlock");
    }

    if (normalizedDomain.isEmpty() || normalizedGate.isEmpty()) {
        return QString();
    }

    return QStringLiteral("PROT.SlrInvAlmGGIO.1.%1_%2").arg(normalizedDomain, normalizedGate);
}

QString logicGateDisplayName(const QString &gateName)
{
    if (gateName == QStringLiteral("enable")) {
        return QStringLiteral("投退");
    }
    if (gateName == QStringLiteral("distant")) {
        return QStringLiteral("远方");
    }
    if (gateName == QStringLiteral("openloop")) {
        return QStringLiteral("开闭环");
    }
    if (gateName == QStringLiteral("lock")) {
        return QStringLiteral("闭锁");
    }
    if (gateName == QStringLiteral("upLock")) {
        return QStringLiteral("增闭锁");
    }
    if (gateName == QStringLiteral("downLock")) {
        return QStringLiteral("减闭锁");
    }
    return gateName;
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

ClickableFrame *createLogicGateButton(const QString &label,
                                      const QString &value,
                                      bool passing,
                                      QWidget *parent)
{
    const QString color = passing ? QStringLiteral("#1f9d55") : QStringLiteral("#d64545");
    const QString softColor = passing ? QStringLiteral("#e7f6ed") : QStringLiteral("#fdeaea");
    const QString text = passing ? QStringLiteral("放行") : QStringLiteral("闭锁");

    auto *panel = new ClickableFrame(parent);
    panel->setObjectName(QStringLiteral("logicGateLampPanel"));
    panel->setCursor(Qt::PointingHandCursor);
    panel->setStyleSheet(QStringLiteral(
        "QFrame#logicGateLampPanel {"
        "  border: 1px solid #c6d3df;"
        "  border-radius: 6px;"
        "  background: #ffffff;"
        "}"
        "QFrame#logicGateLampPanel:hover {"
        "  border-color: #4d8fd5;"
        "  background: #f5faff;"
        "}"
    ));
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panel->setMinimumHeight(76);
    panel->setToolTip(QStringLiteral("点击发送 datawrite 翻转 %1").arg(label));

    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
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
    textLayout->setSpacing(4);

    auto *nameLabel = new QLabel(label, panel);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #111827;"));
    textLayout->addWidget(nameLabel);

    auto *statusLabel = new QLabel(QStringLiteral("%1  value=%2").arg(text, value.isEmpty() ? QStringLiteral("-") : value), panel);
    statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(color));
    textLayout->addWidget(statusLabel);
    textLayout->addStretch();

    layout->addLayout(textLayout, 1);
    return panel;
}

QWidget *createLogicGateSection(const QString &title,
                                const QString &domain,
                                const QString &params,
                                const QString &virtualDeviceId,
                                const std::function<void(const QString &, const QString &, const QString &)> &onToggle,
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
    grid->setVerticalSpacing(12);
    grid->setRowMinimumHeight(0, 76);
    grid->setRowMinimumHeight(1, 76);

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
        const QString displayName = logicGateDisplayName(gateName);
        const QString value = fields.value(gateName);
        auto *button = createLogicGateButton(displayName, value, isAgcAvcGatePassing(gateName, value), section);
        button->setEnabled(!virtualDeviceId.trimmed().isEmpty() && !value.trimmed().isEmpty());
        button->onClicked = [domain, gateName, value, onToggle]() {
            onToggle(domain, gateName, value);
        };
        grid->addWidget(button,
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

AppConfig MainWindow::appConfigForSession(const DebugAppSession *session) const
{
    if (session && session->appIndex >= 0 && session->appIndex < m_appConfigs.size()) {
        return m_appConfigs.at(session->appIndex);
    }
    return AppConfig{};
}

DebugAppSession *MainWindow::currentDebugSession() const
{
    if (!m_appTabBar) {
        return nullptr;
    }

    const int configIndex = m_appTabBar->tabData(m_appTabBar->currentIndex()).toInt();
    if (configIndex < 0 || configIndex >= m_appConfigs.size()) {
        return nullptr;
    }

    return m_debugSessions.value(m_appConfigs.at(configIndex).name, nullptr);
}

DebugAppSession *MainWindow::debugSessionForClient(QObject *client) const
{
    for (DebugAppSession *session : m_debugSessions) {
        if (session && session->client == client) {
            return session;
        }
    }
    return nullptr;
}

DebugAppSession *MainWindow::debugSessionForAppName(const QString &appName) const
{
    return m_debugSessions.value(appName, nullptr);
}

DebugConsoleClient *MainWindow::currentDebugClient() const
{
    DebugAppSession *session = currentDebugSession();
    return session ? session->client : nullptr;
}

bool MainWindow::anyDebugClientConnected() const
{
    for (DebugAppSession *session : m_debugSessions) {
        if (session && session->client && session->client->isConnected()) {
            return true;
        }
    }
    return false;
}

void MainWindow::updateDebugAppTabText(DebugAppSession *session)
{
    if (!m_appTabBar || !session) {
        return;
    }

    const AppConfig appConfig = appConfigForSession(session);
    if (appConfig.name.isEmpty()) {
        return;
    }

    for (int tab = 0; tab < m_appTabBar->count(); ++tab) {
        if (m_appTabBar->tabData(tab).toInt() == session->appIndex) {
            const bool connected = session->client && session->client->isConnected();
            m_appTabBar->setTabText(tab, connected
                ? QStringLiteral("%1 (on)").arg(appConfig.name)
                : appConfig.name);
            return;
        }
    }
}

void MainWindow::onConnectClicked()
{
    QString host = deviceHost();
    const AppConfig appConfig = currentAppConfig();
    DebugConsoleClient *client = currentDebugClient();
    quint16 port = appConfig.port;
    if (!client) {
        return;
    }
    if (host.isEmpty()) {
        QMessageBox::warning(this, "警告", "IP地址不能为空");
        return;
    }

    client->setPromptPattern(appConfig.prompt);

    appendSystem("正在连接 " + host + ":" + QString::number(port) + " ...");
    client->connectToHost(host, port);
    m_connectBtn->setEnabled(false);
}

void MainWindow::onDisconnectClicked()
{
    if (DebugConsoleClient *client = currentDebugClient()) {
        client->disconnectFromHost();
    }
}

void MainWindow::onSendClicked()
{
    QString cmd = m_cmdEdit->text().trimmed();
    if (cmd.isEmpty())
        return;
    DebugConsoleClient *client = currentDebugClient();
    if (!client) {
        return;
    }
    appendSystem("=> " + cmd, "#aaaaaa");
    client->sendCommand(cmd);
    m_cmdEdit->clear();
}

void MainWindow::onQuickCommandClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn)
        return;
    DebugConsoleClient *client = currentDebugClient();
    if (!client) {
        return;
    }
    QString cmd = btn->property("command").toString();
    appendSystem("=> " + cmd, "#aaaaaa");
    client->sendCommand(cmd);
}

void MainWindow::onAppSelectionChanged(int /*index*/)
{
    applyCurrentAppView();
}

void MainWindow::onConnected()
{
    DebugAppSession *session = debugSessionForClient(sender());
    if (!session) {
        return;
    }

    const AppConfig appConfig = appConfigForSession(session);
    updateDebugAppTabText(session);
    if (session != currentDebugSession()) {
        appendSystem(QStringLiteral("%1 connected").arg(appConfig.name), "#32cd32");
        return;
    }

    updateUIState(session->client->isConnected());
    appendSystem(QStringLiteral("已连接"), "#32cd32");
    m_statusLabel->setText(QStringLiteral("已连接 | %1:%2").arg(m_ipEdit->text()).arg(appConfig.port));

    if (isDataViewApp(appConfig)) {
        const bool dataTableApp = appConfig.viewMode == AppViewMode::DataTable;
        m_deviceFilterCombo->setEnabled(dataTableApp);
        m_serviceTypeFilterCombo->setEnabled(dataTableApp);
        m_dataRefFilterEdit->setEnabled(dataTableApp);
        m_autoRefreshCombo->setEnabled(true);
        updateControlCommandUi();
        requestServiceChannelData(false);
        updateAutoRefreshTimer();
    }
}

void MainWindow::onDisconnected()
{
    DebugAppSession *session = debugSessionForClient(sender());
    if (!session) {
        return;
    }

    const AppConfig appConfig = appConfigForSession(session);
    updateDebugAppTabText(session);
    session->waitingControlResponse = false;
    session->serviceChannelItems.clear();
    session->previousServiceChannelItemMap.clear();
    session->timeHighlightUntilMap.clear();
    session->valueHighlightUntilMap.clear();
    session->controlStatusTextMap.clear();
    session->controlStatusColorMap.clear();
    session->logicAgcAvcItems.clear();
    session->serviceChannelDataFrozen = false;
    session->pendingDataFreezeMode.clear();

    if (session != currentDebugSession()) {
        appendSystem(QStringLiteral("%1 disconnected").arg(appConfig.name), "#ff4500");
        return;
    }

    m_autoRefreshTimer->stop();
    m_highlightRefreshTimer->stop();
    m_controlResponseTimer->stop();
    updateUIState(false);
    appendSystem(QStringLiteral("已断开"), "#ff4500");
    m_statusLabel->setText(QStringLiteral("未连接"));

    if (isDataViewApp(currentAppConfig())) {
        refreshDeviceFilterOptions();
        m_dataTable->setRowCount(0);
        m_deviceFilterCombo->setEnabled(false);
        m_serviceTypeFilterCombo->setCurrentIndex(0);
        m_serviceTypeFilterCombo->setEnabled(false);
        m_dataRefFilterEdit->clear();
        m_dataRefFilterEdit->setEnabled(false);
        m_autoRefreshCombo->setEnabled(false);
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
    DebugAppSession *session = debugSessionForClient(sender());
    if (session) {
        updateDebugAppTabText(session);
        session->waitingControlResponse = false;
    }

    appendSystem(QStringLiteral("错误: ") + err, "#ff4444");
    if (session == currentDebugSession()) {
        m_autoRefreshTimer->stop();
        m_highlightRefreshTimer->stop();
        m_controlResponseTimer->stop();
        updateUIState(false);
        m_statusLabel->setText(QStringLiteral("连接错误"));
    }
}

void MainWindow::onLogLine(const QString &line)
{
    DebugAppSession *session = debugSessionForClient(sender());
    if (handleControlResponseLogLine(line, session)) {
        return;
    }

    appendLog(line);
}

void MainWindow::onCommandReply(const QString &reply)
{
    DebugAppSession *session = debugSessionForClient(sender());
    if (!session) {
        appendReply(reply);
        return;
    }

    const AppConfig appConfig = appConfigForSession(session);
    const bool isCurrentSession = session == currentDebugSession();
    if (!isDataViewApp(appConfig)) {
        if (isCurrentSession) {
            appendReply(reply);
        }
        return;
    }

    const QString appName = appConfig.name;
    const QString pendingCommand = session->pendingDataTableCommand;
    session->pendingDataTableCommand.clear();

    if (appConfig.viewMode == AppViewMode::LogicAgcAvcTable) {
        if (pendingCommand == QStringLiteral("datawrite")) {
            if (isCurrentSession) {
                appendReply(reply);
            }
            const QString replyText = reply.trimmed();
            const bool sent = replyText.startsWith(QStringLiteral("datawrite sent:"), Qt::CaseInsensitive);
            const bool rejected = replyText.startsWith(QStringLiteral("datawrite rejected:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("invalid value:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("usage:"), Qt::CaseInsensitive) ||
                                  replyText.startsWith(QStringLiteral("mqtt client unavailable"), Qt::CaseInsensitive);
            if (isCurrentSession) {
                if (sent) {
                    appendSystem(QStringLiteral("LogicCenter datawrite 已发送 %1#%2=%3，正在刷新 AGC/AVC 状态...")
                                     .arg(session->pendingDataWriteDeviceId,
                                          session->pendingDataWriteDataRef,
                                          session->pendingDataWriteValue),
                                 "#32cd32");
                    requestServiceChannelData(false);
                } else if (rejected) {
                    appendSystem(QStringLiteral("LogicCenter datawrite 发送失败: %1").arg(replyText), "#ff4444");
                }
            }
            session->pendingDataWriteDeviceId.clear();
            session->pendingDataWriteDataRef.clear();
            session->pendingDataWriteValue.clear();
            session->pendingDataWriteQuality.clear();
            if (isCurrentSession) {
                updateControlCommandUi();
            }
            return;
        }

        const QList<LogicAgcAvcStatusItem> items = parseLogicAgcAvcReply(reply);
        if (items.isEmpty()) {
            if (isCurrentSession) {
                appendReply(reply);
            }
        } else {
            session->logicAgcAvcItems = items;
            if (isCurrentSession) {
                populateLogicAgcAvcTable(items);
                appendSystem(QStringLiteral("%1 AGC/AVC 状态已加载，共 %2 组").arg(appName).arg(items.size()), "#87ceeb");
            }
        }
        if (isCurrentSession) {
            updateControlCommandUi();
        }
        return;
    }

    if (pendingCommand == QStringLiteral("ctrlcmd")) {
        if (isCurrentSession) {
            appendReply(reply);
            if (reply.startsWith(QStringLiteral("ctrlcmd sent:"), Qt::CaseInsensitive)) {
                appendSystem(QStringLiteral("控制命令已发送，等待 CTRLRESP..."), "#ffcc66");
            }
            updateControlCommandUi();
        }
        return;
    }

    if (pendingCommand == QStringLiteral("datawrite")) {
        if (isCurrentSession) {
            appendReply(reply);
        }
        const QString replyText = reply.trimmed();
        const bool sent = replyText.startsWith(QStringLiteral("datawrite sent:"), Qt::CaseInsensitive);
        const bool rejected = replyText.startsWith(QStringLiteral("datawrite rejected:"), Qt::CaseInsensitive) ||
                              replyText.startsWith(QStringLiteral("invalid value:"), Qt::CaseInsensitive) ||
                              replyText.startsWith(QStringLiteral("usage:"), Qt::CaseInsensitive) ||
                              replyText.startsWith(QStringLiteral("mqtt client unavailable"), Qt::CaseInsensitive);
        if (isCurrentSession) {
            if (sent) {
                setControlStatus(session->pendingDataWriteDeviceId,
                                 session->pendingDataWriteDataRef,
                                 QStringLiteral("datawrite 成功 Value=%1 Quality=%2")
                                     .arg(session->pendingDataWriteValue, session->pendingDataWriteQuality),
                                 QColor(QStringLiteral("#32cd32")));
                appendSystem(QStringLiteral("datawrite 已发送，正在刷新数据..."), "#32cd32");
                requestServiceChannelData(false);
            } else if (rejected) {
                setControlStatus(session->pendingDataWriteDeviceId,
                                 session->pendingDataWriteDataRef,
                                 QStringLiteral("datawrite 失败: %1").arg(replyText),
                                 QColor(QStringLiteral("#ff4444")));
                appendSystem(QStringLiteral("datawrite 发送失败: %1").arg(replyText), "#ff4444");
            }
        }
        session->pendingDataWriteDeviceId.clear();
        session->pendingDataWriteDataRef.clear();
        session->pendingDataWriteValue.clear();
        session->pendingDataWriteQuality.clear();
        if (isCurrentSession) {
            updateControlCommandUi();
        }
        return;
    }

    if (pendingCommand == QStringLiteral("datafreeze")) {
        if (isCurrentSession) {
            appendReply(reply);
        }
        const QString replyText = reply.trimmed();
        const bool rejected = replyText.startsWith(QStringLiteral("datafreeze rejected:"), Qt::CaseInsensitive) ||
                              replyText.startsWith(QStringLiteral("invalid value:"), Qt::CaseInsensitive) ||
                              replyText.startsWith(QStringLiteral("usage:"), Qt::CaseInsensitive) ||
                              replyText.contains(QStringLiteral("error"), Qt::CaseInsensitive);
        if (!rejected) {
            if (session->pendingDataFreezeMode == QStringLiteral("on")) {
                session->serviceChannelDataFrozen = true;
                if (isCurrentSession) {
                    appendSystem(QStringLiteral("DataSpont 更新已冻结"), "#62a8ee");
                }
            } else if (session->pendingDataFreezeMode == QStringLiteral("off")) {
                session->serviceChannelDataFrozen = false;
                if (isCurrentSession) {
                    appendSystem(QStringLiteral("DataSpont 更新已恢复"), "#32cd32");
                }
            }
        } else if (isCurrentSession) {
            appendSystem(QStringLiteral("datafreeze 执行失败: %1").arg(replyText), "#ff4444");
        }
        session->pendingDataFreezeMode.clear();
        if (isCurrentSession) {
            updateControlCommandUi();
        }
        return;
    }

    QStringList dataReplyLines;
    for (const QString &line : reply.split('\n')) {
        if (!handleControlResponseLogLine(line, session)) {
            dataReplyLines.append(line);
        }
    }

    const QList<ServiceChannelDataItem> items = parseServiceChannelDataReply(dataReplyLines.join('\n'));
    if (items.isEmpty()) {
        if (isCurrentSession) {
            if (pendingCommand == QStringLiteral("dataread") && !dataReplyLines.isEmpty()) {
                appendSystem(QStringLiteral("未解析到 %1 数据: %2").arg(appName, reply), "#ffcc66");
            } else {
                appendReply(reply);
            }
            updateControlCommandUi();
        }
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime highlightUntil = now.addSecs(3);
    for (const ServiceChannelDataItem &item : items) {
        const QString itemKey = serviceChannelItemKey(item);
        const ServiceChannelDataItem previousItem = session->previousServiceChannelItemMap.value(itemKey);
        const bool hasPreviousItem = !previousItem.deviceId.isEmpty() || !previousItem.dataRef.isEmpty();

        if (hasPreviousItem && previousItem.dataTime != item.dataTime) {
            session->timeHighlightUntilMap.insert(itemKey, highlightUntil);
        }
        if (hasPreviousItem && previousItem.value != item.value) {
            session->valueHighlightUntilMap.insert(itemKey, highlightUntil);
        }
    }

    session->serviceChannelItems = items;
    session->previousServiceChannelItemMap.clear();
    for (const ServiceChannelDataItem &item : session->serviceChannelItems) {
        session->previousServiceChannelItemMap.insert(serviceChannelItemKey(item), item);
    }

    if (isCurrentSession) {
        refreshDeviceFilterOptions();
        applyServiceChannelFilter();
        updateHighlightRefreshTimer();
        appendSystem(QStringLiteral("%1 数据已加载，共 %2 条").arg(appName).arg(items.size()), "#87ceeb");
        updateControlCommandUi();
    }
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
    const bool rawFrameLogMode = supportsRawFrameLogWindow(appConfig);
    const bool dataViewMode = isDataViewApp(appConfig);
    const bool dataTableMode = appConfig.viewMode == AppViewMode::DataTable;

    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_sendBtn->setEnabled(connected && terminalMode);
    m_ipEdit->setEnabled(!anyDebugClientConnected() && !m_programControlConnected);
    m_cmdEdit->setEnabled(connected && terminalMode);
    m_refreshDataBtn->setEnabled(connected && dataViewMode);
    m_refreshDataBtn->setVisible(dataViewMode);
    if (m_rawFrameLogBtn) {
        m_rawFrameLogBtn->setVisible(rawFrameLogMode);
        m_rawFrameLogBtn->setEnabled(rawFrameLogMode);
    }
    updateControlCommandUi();
    m_deviceFilterCombo->setEnabled(connected && dataTableMode);
    m_serviceTypeFilterCombo->setEnabled(connected && dataTableMode);
    m_dataRefFilterEdit->setEnabled(connected && dataTableMode);
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
    const bool rawFrameLogMode = supportsRawFrameLogWindow(appConfig);

    m_contentStack->setCurrentIndex(terminalMode ? 0 : 1);
    m_cmdEdit->setPlaceholderText(terminalMode ? "输入命令后按回车..." : "当前 APP 使用数据展示视图");
    configureDataTableForCurrentApp();
    DebugAppSession *session = currentDebugSession();
    if (session && !terminalMode) {
        if (appConfig.viewMode == AppViewMode::LogicAgcAvcTable) {
            populateLogicAgcAvcTable(session->logicAgcAvcItems);
        } else {
            refreshDeviceFilterOptions();
            applyServiceChannelFilter();
        }
    }
    if (m_rawFrameLogBtn) {
        m_rawFrameLogBtn->setVisible(rawFrameLogMode);
        m_rawFrameLogBtn->setEnabled(rawFrameLogMode);
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

    DebugConsoleClient *client = currentDebugClient();
    const bool connected = client && client->isConnected();
    if (!connected) {
        updateUIState(false);
        m_autoRefreshTimer->stop();
        m_highlightRefreshTimer->stop();
    } else {
        updateUIState(true);
        updateAutoRefreshTimer();
        updateHighlightRefreshTimer();
    }
    updateControlCommandUi();
}

AppConfig MainWindow::currentAppConfig() const
{
    if (!m_appTabBar) {
        return AppConfig{};
    }

    const int configIndex = m_appTabBar->tabData(m_appTabBar->currentIndex()).toInt();
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

    DebugAppSession *session = currentDebugSession();
    DebugConsoleClient *client = session ? session->client : nullptr;
    if (!client || !client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法刷新数据").arg(currentAppConfig().name), "#ffcc66");
        return;
    }

    if (client->isExecutingCommand()) {
        return;
    }

    const bool logicCenterMode = currentAppConfig().viewMode == AppViewMode::LogicAgcAvcTable;
    const QString command = logicCenterMode ? QStringLiteral("agcavc") : QStringLiteral("dataread all");
    if (logRequest) {
        appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    }
    session->pendingDataTableCommand = logicCenterMode ? QStringLiteral("agcavc") : QStringLiteral("dataread");
    client->sendCommand(command);
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

void MainWindow::onOpenRawFrameLogClicked()
{
    const AppConfig appConfig = currentAppConfig();
    if (!supportsRawFrameLogWindow(appConfig)) {
        return;
    }

    const QString host = deviceHost();
    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("IP地址不能为空"));
        return;
    }

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const QStringList debugCategories = rawFrameDebugCategories(appConfig);
    const QString appDisplayName = rawFrameAppDisplayName(appConfig);

    dialog->setWindowTitle(QStringLiteral("%1 原始 debugconsole 日志").arg(appDisplayName));
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
    QList<QPushButton *> toggleRawFrameBtns;
    for (const QString &debugCategory : debugCategories) {
        const QString categoryDisplayName = rawFrameDebugCategoryDisplayName(debugCategory);
        const bool singleCategory = debugCategories.size() == 1;
        auto *toggleRawFrameBtn = new QPushButton(
            singleCategory
                ? QStringLiteral("开启原始帧")
                : QStringLiteral("开启 %1").arg(categoryDisplayName),
            dialog);
        toggleRawFrameBtn->setToolTip(QStringLiteral("发送 debug %1 on/off，切换 %2 %3 日志打印")
                                          .arg(debugCategory, appDisplayName, categoryDisplayName));
        toggleRawFrameBtn->setProperty("rawFrameDebugCategory", debugCategory);
        toggleRawFrameBtn->setProperty("rawFrameDebugCategoryDisplayName", categoryDisplayName);
        toggleRawFrameBtn->setProperty("rawFrameDebugEnabled", false);
        toggleRawFrameBtns.append(toggleRawFrameBtn);
        buttonRow->addWidget(toggleRawFrameBtn);
    }
    auto *clearBtn = new QPushButton(QStringLiteral("清空"), dialog);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), dialog);
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

    for (QPushButton *toggleRawFrameBtn : toggleRawFrameBtns) {
        connect(toggleRawFrameBtn, &QPushButton::clicked, dialog, [client, toggleRawFrameBtn, appDisplayName, appendRawLog]() {
            const QString debugCategory = toggleRawFrameBtn->property("rawFrameDebugCategory").toString();
            const QString categoryDisplayName = toggleRawFrameBtn->property("rawFrameDebugCategoryDisplayName").toString();
            if (!client->isConnected()) {
                appendRawLog(QStringLiteral("未连接，无法切换 %1 %2 日志打印")
                                 .arg(appDisplayName, categoryDisplayName),
                             QStringLiteral("#ffcc66"));
                return;
            }
            if (client->isExecutingCommand()) {
                appendRawLog(QStringLiteral("上一条 debug 命令尚未返回"), QStringLiteral("#ffcc66"));
                return;
            }

            const bool enabled = toggleRawFrameBtn->property("rawFrameDebugEnabled").toBool();
            const QString command = QStringLiteral("debug %1 %2")
                                        .arg(debugCategory, enabled ? QStringLiteral("off") : QStringLiteral("on"));
            appendRawLog(QStringLiteral("=> %1").arg(command), QStringLiteral("#aaaaaa"));
            toggleRawFrameBtn->setEnabled(false);
            client->sendCommand(command);
        });
    }
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
    connect(client, &DebugConsoleClient::commandReplyReceived, dialog, [toggleRawFrameBtns, appendRawLog](const QString &reply) {
        appendRawLog(reply, QStringLiteral("#87ceeb"));
        const QString normalizedReply = reply.trimmed().toLower();
        for (QPushButton *toggleRawFrameBtn : toggleRawFrameBtns) {
            const QString debugCategory = toggleRawFrameBtn->property("rawFrameDebugCategory").toString();
            const QString categoryDisplayName = toggleRawFrameBtn->property("rawFrameDebugCategoryDisplayName").toString();
            const bool singleCategory = toggleRawFrameBtns.size() == 1;
            if (normalizedReply.contains(QStringLiteral("%1=on").arg(debugCategory))) {
                toggleRawFrameBtn->setProperty("rawFrameDebugEnabled", true);
                toggleRawFrameBtn->setText(singleCategory
                    ? QStringLiteral("关闭原始帧")
                    : QStringLiteral("关闭 %1").arg(categoryDisplayName));
            } else if (normalizedReply.contains(QStringLiteral("%1=off").arg(debugCategory))) {
                toggleRawFrameBtn->setProperty("rawFrameDebugEnabled", false);
                toggleRawFrameBtn->setText(singleCategory
                    ? QStringLiteral("开启原始帧")
                    : QStringLiteral("开启 %1").arg(categoryDisplayName));
            }
            toggleRawFrameBtn->setEnabled(true);
        }
    });

    appendRawLog(QStringLiteral("正在连接 %1:%2 ...").arg(host).arg(appConfig.port), QStringLiteral("#87ceeb"));
    client->connectToHost(host, appConfig.port);
    dialog->show();
}

QList<ServiceChannelDataItem> MainWindow::parseServiceChannelDataReply(const QString &reply) const
{
    static const QRegularExpression newLinePattern(
        R"(^([^\s]+)\s+([^\s]+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?)(?:\s+(.*))?$)"
    );
    static const QRegularExpression legacyLinePattern(
        R"(^([^\s]+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?)(?:\s+(.*))?$)"
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
            item.value = serviceChannelValueText(activeMatch.captured(5));
        } else {
            item.description = activeMatch.captured(2).trimmed();
            item.dataTime = activeMatch.captured(3).trimmed();
            item.value = serviceChannelValueText(activeMatch.captured(4));
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
        const auto sendToggle = [this, virtualDeviceId = item.virtualDeviceId](const QString &domain,
                                                                               const QString &gateName,
                                                                               const QString &currentValue) {
            bool ok = false;
            const int intValue = currentValue.trimmed().toInt(&ok);
            if (!ok) {
                appendSystem(QStringLiteral("无法翻转 %1，当前值不是数字: %2")
                                 .arg(logicGateDisplayName(gateName), currentValue),
                             "#ffcc66");
                return;
            }

            const QString dataRef = logicGateDataRefName(domain, gateName);
            if (sendLogicCenterDataWriteCommand(QStringLiteral("South_104"),
                                                virtualDeviceId,
                                                dataRef,
                                                QString::number(intValue == 0 ? 1 : 0))) {
                const QString message = QStringLiteral("%1%2门禁状态翻转已发送")
                    .arg(domain, logicGateDisplayName(gateName));
                QTimer::singleShot(0, this, [this, message]() {
                    QMessageBox::information(this, QStringLiteral("已发送"), message);
                });
            }
        };
        gateGrid->addWidget(createLogicGateSection(QStringLiteral("AGC 门禁状态"),
                                                   QStringLiteral("AGC"),
                                                   item.agcParams,
                                                   item.virtualDeviceId,
                                                   sendToggle,
                                                   page),
                            0,
                            0);
        gateGrid->addWidget(createLogicGateSection(QStringLiteral("AVC 门禁状态"),
                                                   QStringLiteral("AVC"),
                                                   item.avcParams,
                                                   item.virtualDeviceId,
                                                   sendToggle,
                                                   page),
                            0,
                            1);
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
        const bool timeChanged = currentDebugSession()->timeHighlightUntilMap.value(itemKey).isValid() &&
                                 currentDebugSession()->timeHighlightUntilMap.value(itemKey) > now;
        const bool valueChanged = currentDebugSession()->valueHighlightUntilMap.value(itemKey).isValid() &&
                                  currentDebugSession()->valueHighlightUntilMap.value(itemKey) > now;

        auto *deviceIdItem = new QTableWidgetItem(item.deviceId);
        auto *dataRefItem = new QTableWidgetItem(item.dataRef);
        auto *serviceIdItem = new QTableWidgetItem(item.serviceId);
        auto *descriptionItem = new QTableWidgetItem(item.description);
        auto *dataTimeItem = new QTableWidgetItem(item.dataTime);
        auto *valueItem = new QTableWidgetItem(item.value);
        auto *controlStatusItem = new QTableWidgetItem(currentDebugSession()->controlStatusTextMap.value(itemKey));

        deviceIdItem->setForeground(defaultTextColor);
        dataRefItem->setForeground(defaultTextColor);
        serviceIdItem->setForeground(defaultTextColor);
        descriptionItem->setForeground(defaultTextColor);
        dataTimeItem->setForeground(timeChanged ? changedTextColor : defaultTextColor);
        valueItem->setForeground(valueChanged ? changedTextColor : defaultTextColor);
        controlStatusItem->setForeground(currentDebugSession()->controlStatusColorMap.value(itemKey, defaultTextColor));

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

    for (const ServiceChannelDataItem &item : currentDebugSession()->serviceChannelItems) {
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
    const QString keyword = m_dataRefFilterEdit->text().trimmed();
    QList<ServiceChannelDataItem> filteredItems;

    for (const ServiceChannelDataItem &item : currentDebugSession()->serviceChannelItems) {
        const bool matchesDeviceId = selectedDeviceId.isEmpty() || item.deviceId == selectedDeviceId;
        const bool matchesServiceId = selectedServiceId.isEmpty() || item.serviceId == selectedServiceId;
        const bool matchesKeyword = keyword.isEmpty() ||
                                    item.dataRef.contains(keyword, Qt::CaseInsensitive) ||
                                    item.description.contains(keyword, Qt::CaseInsensitive);

        if (matchesDeviceId && matchesServiceId && matchesKeyword) {
            filteredItems.append(item);
        }
    }

    populateServiceChannelTable(filteredItems);
}

void MainWindow::updateAutoRefreshTimer()
{
    DebugConsoleClient *client = currentDebugClient();
    if (!client || !client->isConnected() || !isDataViewApp(currentAppConfig())) {
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
    DebugAppSession *session = currentDebugSession();
    if (!session) {
        m_highlightRefreshTimer->stop();
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();

    for (auto it = session->timeHighlightUntilMap.begin(); it != session->timeHighlightUntilMap.end(); ) {
        if (!it.value().isValid() || it.value() <= now) {
            it = session->timeHighlightUntilMap.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = session->valueHighlightUntilMap.begin(); it != session->valueHighlightUntilMap.end(); ) {
        if (!it.value().isValid() || it.value() <= now) {
            it = session->valueHighlightUntilMap.erase(it);
        } else {
            ++it;
        }
    }

    if (!session->client || !session->client->isConnected() || currentAppConfig().viewMode != AppViewMode::DataTable) {
        m_highlightRefreshTimer->stop();
        return;
    }

    if (session->timeHighlightUntilMap.isEmpty() && session->valueHighlightUntilMap.isEmpty()) {
        m_highlightRefreshTimer->stop();
        return;
    }

    if (!m_highlightRefreshTimer->isActive()) {
        m_highlightRefreshTimer->start();
    }
}

void MainWindow::copySelectedTableCells(QTableWidget *table)
{
    if (!table) {
        table = m_dataTable;
    }
    if (!table || !table->selectionModel()) {
        return;
    }

    const QModelIndexList indexes = table->selectionModel()->selectedIndexes();
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

void MainWindow::clearSelectedEditableTableCells(QTableWidget *table)
{
    if (!table || !table->selectionModel()) {
        return;
    }

    QModelIndexList targets = table->selectionModel()->selectedIndexes();
    std::sort(targets.begin(), targets.end(), [](const QModelIndex &left, const QModelIndex &right) {
        if (left.row() != right.row()) {
            return left.row() < right.row();
        }
        return left.column() < right.column();
    });
    bool changed = false;
    for (const QModelIndex &target : targets) {
        QTableWidgetItem *item = table->item(target.row(), target.column());
        if (!item || !(item->flags() & Qt::ItemIsEditable) || item->text().isEmpty()) {
            continue;
        }
        item->setText(QString());
        changed = true;
    }

    if (changed) {
        statusBar()->showMessage(QStringLiteral("已清空选中单元格"), 2000);
    }
}

void MainWindow::updateControlCommandUi()
{
    if (!m_sendControlBtn || !m_dataTable) {
        return;
    }

    DebugConsoleClient *client = currentDebugClient();
    const bool canSendDataTableCommand = client &&
                                         client->isConnected() &&
                                         !client->isExecutingCommand() &&
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

    DebugAppSession *session = currentDebugSession();
    DebugConsoleClient *client = session ? session->client : nullptr;
    if (!client || !client->isConnected() || !isServiceChannelApp(currentAppConfig())) {
        m_dataFreezeBtn->setText(QStringLiteral("冻结数据"));
        m_dataFreezeBtn->setToolTip(QStringLiteral("发送 datafreeze on/off，冻结或恢复 DataSpont 更新内部值"));
        m_dataFreezeBtn->setStyleSheet(QString());
        return;
    }

    if (client->isExecutingCommand() && session->pendingDataTableCommand == QStringLiteral("datafreeze")) {
        m_dataFreezeBtn->setText(session->pendingDataFreezeMode == QStringLiteral("off")
            ? QStringLiteral("恢复中...")
            : QStringLiteral("冻结中..."));
        m_dataFreezeBtn->setStyleSheet(
            QStringLiteral("QPushButton { background-color: #4a5568; color: #dbe7f6; border-color: #7f8fa6; }"));
        return;
    }

    if (session->serviceChannelDataFrozen) {
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
    return handleControlResponseLogLine(line, currentDebugSession());
}

bool MainWindow::handleControlResponseLogLine(const QString &line, DebugAppSession *session)
{
    const int eventIndex = line.indexOf(QStringLiteral("CTRLRESP "));
    if (eventIndex < 0) {
        return false;
    }
    if (!session) {
        appendLog(line);
        return true;
    }

    const bool isCurrentSession = session == currentDebugSession();
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
        if (isCurrentSession) {
            appendLog(line);
        }
        return true;
    }

    if (isCurrentSession) {
        appendLog(line);
    }

    const QString normalizedResult = result.toLower();
    const bool resultOk = normalizedResult.isEmpty() ||
                          normalizedResult == QStringLiteral("0") ||
                          normalizedResult == QStringLiteral("true") ||
                          normalizedResult == QStringLiteral("ok") ||
                          normalizedResult == QStringLiteral("success");
    const bool errorOk = errorCode.isEmpty() || errorCode == QStringLiteral("0");
    const bool success = resultOk && errorOk;

    const bool matchesPending = session->waitingControlResponse &&
                                session->pendingControlDeviceId == deviceId &&
                                session->pendingControlDataRef == dataRef &&
                                (session->pendingControlType < 0 || QString::number(session->pendingControlType) == ctrlType);

    const QString statusText = success
        ? QStringLiteral("成功 CtrlType=%1 Value=%2").arg(ctrlType, ctrlVal)
        : QStringLiteral("失败 Result=%1 Error=%2").arg(result.isEmpty() ? QStringLiteral("-") : result,
                                                       errorCode.isEmpty() ? QStringLiteral("-") : errorCode);

    const QString key = controlCommandKey(deviceId, dataRef);
    session->controlStatusTextMap.insert(key, statusText);
    session->controlStatusColorMap.insert(key, success ? QColor(QStringLiteral("#32cd32")) : QColor(QStringLiteral("#ff4444")));
    if (isCurrentSession) {
        setControlStatus(deviceId,
                         dataRef,
                         statusText,
                         success ? QColor(QStringLiteral("#32cd32")) : QColor(QStringLiteral("#ff4444")));
    }

    if (matchesPending) {
        if (isCurrentSession) {
            m_controlResponseTimer->stop();
        }
        session->waitingControlResponse = false;
        if (isCurrentSession) {
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
        }
    } else if (isCurrentSession) {
        appendSystem(QStringLiteral("收到控制响应：%1#%2，%3").arg(deviceId, dataRef, statusText),
                     success ? "#32cd32" : "#ff4444");
    }

    return true;
}

void MainWindow::handleControlResponseTimeout()
{
    const QString appName = m_controlResponseTimer->property("appName").toString();
    DebugAppSession *session = debugSessionForAppName(appName);
    if (!session || !session->waitingControlResponse) {
        return;
    }

    const QString deviceId = session->pendingControlDeviceId;
    const QString dataRef = session->pendingControlDataRef;
    session->waitingControlResponse = false;
    if (session == currentDebugSession()) {
        setControlStatus(deviceId,
                         dataRef,
                         QStringLiteral("响应超时 CtrlType=%1 Value=%2")
                             .arg(session->pendingControlType)
                             .arg(session->pendingControlValue),
                         QColor(QStringLiteral("#ffcc66")));
        appendSystem(QStringLiteral("控制命令已发送，但 15 秒内未收到 CTRLRESP：%1#%2").arg(deviceId, dataRef), "#ffcc66");
    } else {
        const QString key = controlCommandKey(deviceId, dataRef);
        session->controlStatusTextMap.insert(key,
                                             QStringLiteral("响应超时 CtrlType=%1 Value=%2")
                                                 .arg(session->pendingControlType)
                                                 .arg(session->pendingControlValue));
        session->controlStatusColorMap.insert(key, QColor(QStringLiteral("#ffcc66")));
    }
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
    currentDebugSession()->controlStatusTextMap.insert(key, statusText);
    currentDebugSession()->controlStatusColorMap.insert(key, color);

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
    DebugAppSession *session = currentDebugSession();
    DebugConsoleClient *client = session ? session->client : nullptr;
    if (!client || !client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送控制命令").arg(appName), "#ffcc66");
        return;
    }

    if (client->isExecutingCommand()) {
        appendSystem(QStringLiteral("上一条命令尚未返回，暂不能发送控制命令"), "#ffcc66");
        return;
    }

    const QString command = QStringLiteral("ctrlcmd %1 %2 %3 %4")
        .arg(item.deviceId, item.dataRef, ctrlVal, QString::number(ctrlType));
    appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    session->waitingControlResponse = true;
    session->pendingControlDeviceId = item.deviceId;
    session->pendingControlDataRef = item.dataRef;
    session->pendingControlValue = ctrlVal;
    session->pendingControlType = ctrlType;
    setControlStatus(item.deviceId,
                     item.dataRef,
                     QStringLiteral("等待响应 CtrlType=%1 Value=%2").arg(ctrlType).arg(ctrlVal),
                     QColor(QStringLiteral("#ffcc66")));
    m_controlResponseTimer->setProperty("appName", appName);
    m_controlResponseTimer->start();
    session->pendingDataTableCommand = QStringLiteral("ctrlcmd");
    client->sendCommand(command);
    updateControlCommandUi();
}

void MainWindow::sendServiceChannelDataWriteCommand(const ServiceChannelDataItem &item,
                                                    const QString &value,
                                                    const QString &quality)
{
    const QString appName = currentAppConfig().name;
    DebugAppSession *session = currentDebugSession();
    DebugConsoleClient *client = session ? session->client : nullptr;
    if (!client || !client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送 datawrite").arg(appName), "#ffcc66");
        return;
    }

    if (client->isExecutingCommand()) {
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
    session->pendingDataWriteDeviceId = item.deviceId;
    session->pendingDataWriteDataRef = item.dataRef;
    session->pendingDataWriteValue = value;
    session->pendingDataWriteQuality = quality;
    setControlStatus(item.deviceId,
                     item.dataRef,
                     QStringLiteral("datawrite 发送中 Value=%1 Quality=%2").arg(value, quality),
                     QColor(QStringLiteral("#ffcc66")));
    session->pendingDataTableCommand = QStringLiteral("datawrite");
    client->sendCommand(command);
    updateControlCommandUi();
}

bool MainWindow::sendLogicCenterDataWriteCommand(const QString &appName,
                                                 const QString &deviceId,
                                                 const QString &dataRef,
                                                 const QString &value)
{
    const QString displayAppName = currentAppConfig().name;
    DebugAppSession *session = currentDebugSession();
    DebugConsoleClient *client = session ? session->client : nullptr;
    if (!client || !client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送 datawrite").arg(displayAppName), "#ffcc66");
        return false;
    }

    if (client->isExecutingCommand()) {
        appendSystem(QStringLiteral("上一条命令尚未返回，暂不能发送 datawrite"), "#ffcc66");
        return false;
    }

    const QString trimmedAppName = appName.trimmed();
    const QString trimmedDeviceId = deviceId.trimmed();
    const QString trimmedDataRef = dataRef.trimmed();
    if (trimmedAppName.isEmpty() || trimmedDeviceId.isEmpty() || trimmedDataRef.isEmpty()) {
        appendSystem(QStringLiteral("LogicCenter datawrite 参数不完整: app=%1 deviceId=%2 dataRef=%3")
                         .arg(trimmedAppName, trimmedDeviceId, trimmedDataRef),
                     "#ffcc66");
        return false;
    }

    bool valueOk = false;
    value.toDouble(&valueOk);
    if (!valueOk) {
        appendSystem(QStringLiteral("datawrite Value 必须是数字: %1").arg(value), "#ffcc66");
        return false;
    }

    const QString command = QStringLiteral("datawrite %1 %2 %3 %4")
        .arg(trimmedAppName, trimmedDeviceId, trimmedDataRef, value);
    appendSystem(QStringLiteral("=> %1").arg(command), "#aaaaaa");
    session->pendingDataWriteDeviceId = trimmedDeviceId;
    session->pendingDataWriteDataRef = trimmedDataRef;
    session->pendingDataWriteValue = value;
    session->pendingDataWriteQuality.clear();
    session->pendingDataTableCommand = QStringLiteral("datawrite");
    client->sendCommand(command);
    updateControlCommandUi();
    return true;
}

void MainWindow::sendServiceChannelDataFreezeCommand(const QString &mode)
{
    const QString appName = currentAppConfig().name;
    DebugAppSession *session = currentDebugSession();
    DebugConsoleClient *client = session ? session->client : nullptr;
    if (!client || !client->isConnected()) {
        appendSystem(QStringLiteral("未连接 %1，无法发送 datafreeze").arg(appName), "#ffcc66");
        return;
    }

    if (client->isExecutingCommand()) {
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
    session->pendingDataFreezeMode = normalizedMode;
    session->pendingDataTableCommand = QStringLiteral("datafreeze");
    client->sendCommand(command);
    updateControlCommandUi();
}

QString MainWindow::serviceChannelItemKey(const ServiceChannelDataItem &item) const
{
    return item.deviceId + "#" + item.dataRef;
}
