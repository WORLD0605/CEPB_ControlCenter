#include "mainwindow.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QStatusBar>
#include <QTabWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSet>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QMenu>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_client(new DebugConsoleClient(this))
    , m_autoRefreshTimer(new QTimer(this))
    , m_highlightRefreshTimer(new QTimer(this))
{
    m_highlightRefreshTimer->setInterval(500);

    m_appConfigs = {
        {"ServiceChannel", 4444, "ServiceChannel>", AppViewMode::DataTable},
        {"cepiec104", 6666, "cepiec104>", AppViewMode::Terminal}
    };

    setWindowTitle("CEPB Control Center");
    resize(1080, 720);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    m_mainTabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_mainTabWidget, 1);

    auto *debugPage = new QWidget(this);
    auto *debugLayout = new QVBoxLayout(debugPage);
    debugLayout->setContentsMargins(0, 0, 0, 0);
    debugLayout->setSpacing(10);

    // === 顶部连接配置 ===
    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("IP地址:"));
    m_ipEdit = new QLineEdit("192.168.7.10");
    m_ipEdit->setMinimumWidth(140);
    topLayout->addWidget(m_ipEdit);

    topLayout->addWidget(new QLabel("APP:"));
    m_appCombo = new QComboBox();
    for (int index = 0; index < m_appConfigs.size(); ++index) {
        m_appCombo->addItem(m_appConfigs.at(index).name, index);
    }
    topLayout->addWidget(m_appCombo);

    m_connectBtn = new QPushButton("连接");
    m_disconnectBtn = new QPushButton("断开");
    m_disconnectBtn->setEnabled(false);
    topLayout->addWidget(m_connectBtn);
    topLayout->addWidget(m_disconnectBtn);
    topLayout->addStretch();

    debugLayout->addLayout(topLayout);

    m_contentStack = new QStackedWidget();

    auto *terminalPage = new QWidget();
    auto *terminalLayout = new QVBoxLayout(terminalPage);
    terminalLayout->setContentsMargins(0, 0, 0, 0);
    terminalLayout->setSpacing(10);

    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setLineWrapMode(QTextEdit::WidgetWidth);
    m_logView->setStyleSheet(
        "QTextEdit {"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 12px;"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "}"
    );
    terminalLayout->addWidget(m_logView, 1);

    auto *bottomLayout = new QHBoxLayout();
    m_cmdEdit = new QLineEdit();
    m_cmdEdit->setPlaceholderText("输入命令后按回车...");
    bottomLayout->addWidget(m_cmdEdit, 1);

    m_sendBtn = new QPushButton("发送");
    m_sendBtn->setEnabled(false);
    bottomLayout->addWidget(m_sendBtn);
    terminalLayout->addLayout(bottomLayout);

    auto *quickLayout = new QHBoxLayout();
    const QStringList quickCmds = {
        "help", "ping", "uptime", "mqtt",
        "debug data on", "debug data off", "debug list"
    };
    for (const QString &cmd : quickCmds) {
        auto *btn = new QPushButton(cmd);
        btn->setProperty("command", cmd);
        btn->setEnabled(false);
        connect(btn, &QPushButton::clicked,
                this, &MainWindow::onQuickCommandClicked);
        quickLayout->addWidget(btn);
    }
    quickLayout->addStretch();
    terminalLayout->addLayout(quickLayout);

    auto *dataPage = new QWidget();
    auto *dataLayout = new QVBoxLayout(dataPage);
    dataLayout->setContentsMargins(0, 0, 0, 0);
    dataLayout->setSpacing(10);

    auto *dataToolbar = new QHBoxLayout();
    dataToolbar->addWidget(new QLabel("DeviceId:"));
    m_deviceFilterCombo = new QComboBox();
    m_deviceFilterCombo->addItem("all", QString());
    m_deviceFilterCombo->setMinimumContentsLength(18);
    m_deviceFilterCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_deviceFilterCombo->setMinimumWidth(220);
    m_deviceFilterCombo->setEnabled(false);
    dataToolbar->addWidget(m_deviceFilterCombo);
    dataToolbar->addWidget(new QLabel("DataRef:"));
    m_dataRefFilterEdit = new QLineEdit();
    m_dataRefFilterEdit->setPlaceholderText("输入 DataRef 关键字实时筛选...");
    m_dataRefFilterEdit->setClearButtonEnabled(true);
    m_dataRefFilterEdit->setMinimumWidth(260);
    m_dataRefFilterEdit->setEnabled(false);
    dataToolbar->addWidget(m_dataRefFilterEdit);
    dataToolbar->addWidget(new QLabel("Description:"));
    m_descriptionFilterEdit = new QLineEdit();
    m_descriptionFilterEdit->setPlaceholderText("输入描述关键字实时筛选...");
    m_descriptionFilterEdit->setClearButtonEnabled(true);
    m_descriptionFilterEdit->setMinimumWidth(240);
    m_descriptionFilterEdit->setEnabled(false);
    dataToolbar->addWidget(m_descriptionFilterEdit);
    dataToolbar->addWidget(new QLabel("自动刷新:"));
    m_autoRefreshCombo = new QComboBox();
    m_autoRefreshCombo->addItem("关闭", 0);
    m_autoRefreshCombo->addItem("1 秒", 1000);
    m_autoRefreshCombo->addItem("2 秒", 2000);
    m_autoRefreshCombo->addItem("5 秒", 5000);
    m_autoRefreshCombo->addItem("10 秒", 10000);
    m_autoRefreshCombo->setCurrentIndex(0);
    m_autoRefreshCombo->setEnabled(false);
    dataToolbar->addWidget(m_autoRefreshCombo);
    dataToolbar->addStretch();
    m_refreshDataBtn = new QPushButton("刷新数据");
    m_refreshDataBtn->setEnabled(false);
    dataToolbar->addWidget(m_refreshDataBtn);
    dataLayout->addLayout(dataToolbar);

    m_dataTable = new QTableWidget(0, 5);
    m_dataTable->setHorizontalHeaderLabels({"DeviceId", "DataRef", "Description", "DataTime", "Value"});
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_dataTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_dataTable->verticalHeader()->setVisible(false);
    m_dataTable->horizontalHeader()->setSectionsClickable(true);
    m_dataTable->horizontalHeader()->setSectionsMovable(false);
    m_dataTable->horizontalHeader()->setStretchLastSection(false);
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_dataTable->setColumnWidth(0, 140);
    m_dataTable->setColumnWidth(1, 280);
    m_dataTable->setColumnWidth(2, 380);
    m_dataTable->setColumnWidth(3, 170);
    m_dataTable->setColumnWidth(4, 90);
    dataLayout->addWidget(m_dataTable, 1);

    m_contentStack->addWidget(terminalPage);
    m_contentStack->addWidget(dataPage);
    debugLayout->addWidget(m_contentStack, 1);

    auto *configPage = new QWidget(this);
    auto *configLayout = new QVBoxLayout(configPage);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(10);

    auto *importRow = new QHBoxLayout();
    importRow->addWidget(new QLabel("104 APP目录:"));
    m_configImportDirEdit = new QLineEdit();
    m_configImportDirEdit->setPlaceholderText("选择 cepiec104 目录，例如 /home/cepgateway/app/cepiec104 的本地镜像路径");
    importRow->addWidget(m_configImportDirEdit, 1);
    m_browseConfigImportDirBtn = new QPushButton("浏览...");
    importRow->addWidget(m_browseConfigImportDirBtn);
    m_importIec104ConfigBtn = new QPushButton("导入104配置");
    importRow->addWidget(m_importIec104ConfigBtn);
    configLayout->addLayout(importRow);

    auto *summaryFrame = new QFrame(this);
    summaryFrame->setFrameShape(QFrame::StyledPanel);
    auto *summaryLayout = new QFormLayout(summaryFrame);
    summaryLayout->setContentsMargins(12, 12, 12, 12);
    summaryLayout->setHorizontalSpacing(24);
    summaryLayout->setVerticalSpacing(10);
    m_configProjectNameValueLabel = new QLabel("-");
    m_configSourceRootValueLabel = new QLabel("-");
    m_configModelCountValueLabel = new QLabel("0");
    m_configDeviceCountValueLabel = new QLabel("0");
    m_configIssueCountValueLabel = new QLabel("0");
    summaryLayout->addRow("工程名称:", m_configProjectNameValueLabel);
    summaryLayout->addRow("来源目录:", m_configSourceRootValueLabel);
    summaryLayout->addRow("模型数量:", m_configModelCountValueLabel);
    summaryLayout->addRow("设备数量:", m_configDeviceCountValueLabel);
    summaryLayout->addRow("导入问题数:", m_configIssueCountValueLabel);
    configLayout->addWidget(summaryFrame);

    configLayout->addWidget(new QLabel("导入报告:"));
    m_configImportReportView = new QTextEdit();
    m_configImportReportView->setReadOnly(true);
    m_configImportReportView->setPlaceholderText("导入 104 目录后，这里会显示模型/设备统计和错误、警告信息。");
    configLayout->addWidget(m_configImportReportView, 1);

    m_mainTabWidget->addTab(debugPage, "调试控制");
    m_mainTabWidget->addTab(configPage, "配置工具");

    setCentralWidget(central);

    // === 状态栏 ===
    m_statusLabel = new QLabel("未连接");
    statusBar()->addWidget(m_statusLabel);

    // === 信号连接 ===
    connect(m_connectBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectClicked);
    connect(m_appCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onAppSelectionChanged);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &MainWindow::onSendClicked);
    connect(m_cmdEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onSendClicked);

    connect(m_refreshDataBtn, &QPushButton::clicked,
            this, [this]() { requestServiceChannelData(); });
    connect(m_deviceFilterCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onDeviceFilterChanged);
    connect(m_dataRefFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onDataRefFilterTextChanged);
        connect(m_descriptionFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onDescriptionFilterTextChanged);
        connect(m_autoRefreshCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onAutoRefreshIntervalChanged);
            connect(m_browseConfigImportDirBtn, &QPushButton::clicked,
                this, &MainWindow::onBrowseConfigImportDirClicked);
            connect(m_importIec104ConfigBtn, &QPushButton::clicked,
                this, &MainWindow::onImportIec104ConfigClicked);
        connect(m_autoRefreshTimer, &QTimer::timeout,
            this, [this]() { requestServiceChannelData(false); });
            connect(m_highlightRefreshTimer, &QTimer::timeout,
                this, [this]() {
                applyServiceChannelFilter();
                updateHighlightRefreshTimer();
                });
            connect(new QShortcut(QKeySequence::Copy, m_dataTable), &QShortcut::activated,
                this, [this]() { copySelectedTableCells(); });
            connect(m_dataTable, &QWidget::customContextMenuRequested, this,
                [this](const QPoint &position) {
                QMenu menu(this);
                QAction *copyAction = menu.addAction("复制");
                        copyAction->setEnabled(m_dataTable->selectionModel() &&
                                               !m_dataTable->selectionModel()->selectedIndexes().isEmpty());
                QAction *selectedAction = menu.exec(m_dataTable->viewport()->mapToGlobal(position));
                if (selectedAction == copyAction) {
                    copySelectedTableCells();
                }
                });

    connect(m_client, &DebugConsoleClient::connected,
            this, &MainWindow::onConnected);
    connect(m_client, &DebugConsoleClient::disconnected,
            this, &MainWindow::onDisconnected);
    connect(m_client, &DebugConsoleClient::errorOccurred,
            this, &MainWindow::onError);
    connect(m_client, &DebugConsoleClient::logLineReceived,
            this, &MainWindow::onLogLine);
    connect(m_client, &DebugConsoleClient::commandReplyReceived,
            this, &MainWindow::onCommandReply);

    applyCurrentAppView();
}

MainWindow::~MainWindow() = default;

void MainWindow::onBrowseConfigImportDirClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择 104 APP 目录"),
        m_configImportDirEdit->text().trimmed());
    if (!dir.isEmpty()) {
        m_configImportDirEdit->setText(dir);
    }
}

void MainWindow::onImportIec104ConfigClicked()
{
    const QString appDir = m_configImportDirEdit->text().trimmed();
    if (appDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择 104 APP 目录"));
        return;
    }

    configtool::ImportReport report;
    m_configProjectManager.createEmptyProject(QStringLiteral("104导入工程"), appDir);
    const bool ok = m_configProjectManager.importIec104AppDirectory(appDir, report);
    refreshConfigImportSummary(report);

    if (!ok && report.hasErrors()) {
        statusBar()->showMessage(QStringLiteral("104 配置导入失败"), 5000);
        return;
    }

    statusBar()->showMessage(QStringLiteral("104 配置导入完成"), 5000);
}

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
    // 回复可能有多行，每行都带时间戳比较清晰
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
    for (const QString &deviceId : deviceIds) {
        m_deviceFilterCombo->addItem(deviceId, deviceId);
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

void MainWindow::refreshConfigImportSummary(const configtool::ImportReport &report)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_configProjectNameValueLabel->setText(project.projectName.isEmpty() ? QStringLiteral("-") : project.projectName);
    m_configSourceRootValueLabel->setText(project.sourceRoot.isEmpty() ? QStringLiteral("-") : project.sourceRoot);
    m_configModelCountValueLabel->setText(QString::number(project.models.size()));
    m_configDeviceCountValueLabel->setText(QString::number(project.devices.size()));
    m_configIssueCountValueLabel->setText(QString::number(report.issues.size()));

    QStringList lines;
    lines << QStringLiteral("导入结果:")
          << QStringLiteral("- 模型: %1").arg(report.importedModelCount)
          << QStringLiteral("- 设备: %1").arg(report.importedDeviceCount);

    if (report.issues.isEmpty()) {
        lines << QStringLiteral("")
              << QStringLiteral("未发现错误或警告。");
    } else {
        lines << QStringLiteral("")
              << QStringLiteral("问题列表:");

        for (const configtool::ImportIssue &issue : report.issues) {
            const QString severity = issue.severity == configtool::ImportIssueSeverity::Error
                ? QStringLiteral("错误")
                : QStringLiteral("警告");
            lines << QStringLiteral("[%1] %2").arg(severity, issue.filePath);
            lines << QStringLiteral("  %1").arg(issue.message);
        }
    }

    m_configImportReportView->setPlainText(lines.join('\n'));
}
