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
#include <QSplitter>
#include <QGroupBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSet>
#include <QTimer>
#include <QUuid>
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

    m_configPage = new QWidget(this);
    auto *configLayout = new QVBoxLayout(m_configPage);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(8);

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
    summaryFrame->setMaximumHeight(88);
    auto *summaryLayout = new QFormLayout(summaryFrame);
    summaryLayout->setContentsMargins(8, 6, 8, 6);
    summaryLayout->setHorizontalSpacing(24);
    summaryLayout->setVerticalSpacing(2);
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

    auto *configWorkspaceSplitter = new QSplitter(Qt::Horizontal, this);

    auto *objectPanel = new QWidget(this);
    auto *objectPanelLayout = new QVBoxLayout(objectPanel);
    objectPanelLayout->setContentsMargins(0, 0, 0, 0);
    objectPanelLayout->setSpacing(10);

    auto *modelGroup = new QGroupBox("模型列表", this);
    auto *modelGroupLayout = new QVBoxLayout(modelGroup);
    auto *modelToolbar = new QHBoxLayout();
    m_newModelBtn = new QPushButton("新建模型");
    modelToolbar->addWidget(m_newModelBtn);
    m_createDeviceFromModelBtn = new QPushButton("由模型创建设备");
    modelToolbar->addWidget(m_createDeviceFromModelBtn);
    modelToolbar->addStretch();
    modelGroupLayout->addLayout(modelToolbar);
    m_configModelTable = new QTableWidget(0, 4, this);
    m_configModelTable->setHorizontalHeaderLabels({"模型", "展示名", "设备类型", "点位数"});
    m_configModelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_configModelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configModelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_configModelTable->verticalHeader()->setVisible(false);
    m_configModelTable->horizontalHeader()->setStretchLastSection(true);
    m_configModelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    modelGroupLayout->addWidget(m_configModelTable, 1);
    objectPanelLayout->addWidget(modelGroup, 1);

    auto *deviceGroup = new QGroupBox("设备列表", this);
    auto *deviceGroupLayout = new QVBoxLayout(deviceGroup);
    m_configDeviceTable = new QTableWidget(0, 5, this);
    m_configDeviceTable->setHorizontalHeaderLabels({"DeviceId", "描述", "模型", "站地址", "点位数"});
    m_configDeviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_configDeviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configDeviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_configDeviceTable->verticalHeader()->setVisible(false);
    m_configDeviceTable->horizontalHeader()->setStretchLastSection(true);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    deviceGroupLayout->addWidget(m_configDeviceTable, 1);
    objectPanelLayout->addWidget(deviceGroup, 1);

    configWorkspaceSplitter->addWidget(objectPanel);

    auto *detailPanel = new QWidget(this);
    auto *detailPanelLayout = new QVBoxLayout(detailPanel);
    detailPanelLayout->setContentsMargins(0, 0, 0, 0);
    detailPanelLayout->setSpacing(8);

    m_modelEditorPage = new QWidget(this);
    auto *modelDetailLayout = new QVBoxLayout(m_modelEditorPage);
    modelDetailLayout->setContentsMargins(0, 0, 0, 0);
    modelDetailLayout->setSpacing(8);
    auto *modelFormFrame = new QFrame(this);
    modelFormFrame->setFrameShape(QFrame::StyledPanel);
    modelFormFrame->setMaximumHeight(180);
    auto *modelFormLayout = new QFormLayout(modelFormFrame);
    modelFormLayout->setContentsMargins(10, 8, 10, 8);
    modelFormLayout->setVerticalSpacing(4);
    m_modelIdEdit = new QLineEdit(this);
    m_modelDisplayNameEdit = new QLineEdit(this);
    m_modelDeviceTypeEdit = new QLineEdit(this);
    m_modelVersionEdit = new QLineEdit(this);
    m_modelManufacturerIdEdit = new QLineEdit(this);
    m_modelManufacturerDescEdit = new QLineEdit(this);
    m_modelSchemaEdit = new QLineEdit(this);
    modelFormLayout->addRow("模型ID:", m_modelIdEdit);
    modelFormLayout->addRow("展示名称:", m_modelDisplayNameEdit);
    modelFormLayout->addRow("设备类型:", m_modelDeviceTypeEdit);
    modelFormLayout->addRow("版本:", m_modelVersionEdit);
    modelFormLayout->addRow("厂家ID:", m_modelManufacturerIdEdit);
    modelFormLayout->addRow("厂家描述:", m_modelManufacturerDescEdit);
    modelFormLayout->addRow("Schema:", m_modelSchemaEdit);
    modelDetailLayout->addWidget(modelFormFrame);
    auto *pointToolbar = new QHBoxLayout();
    pointToolbar->addWidget(new QLabel("模型点位:"));
    m_addPointBtn = new QPushButton("新增点位");
    m_copyPointBtn = new QPushButton("复制点位");
    m_deletePointBtn = new QPushButton("删除点位");
    pointToolbar->addWidget(m_addPointBtn);
    pointToolbar->addWidget(m_copyPointBtn);
    pointToolbar->addWidget(m_deletePointBtn);
    pointToolbar->addStretch();
    modelDetailLayout->addLayout(pointToolbar);
    m_modelValidationLabel = new QLabel(this);
    m_modelValidationLabel->setWordWrap(true);
    m_modelValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
    modelDetailLayout->addWidget(m_modelValidationLabel);
    m_modelPointsTable = new QTableWidget(0, 6, this);
    m_modelPointsTable->setColumnCount(9);
    m_modelPointsTable->setHorizontalHeaderLabels({"类别", "DOname", "描述", "LDname", "LNtype", "LNinst", "DataRef", "数据类型", "单位"});
    m_modelPointsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_modelPointsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modelPointsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modelPointsTable->verticalHeader()->setVisible(false);
    m_modelPointsTable->horizontalHeader()->setStretchLastSection(true);
    m_modelPointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_modelPointsTable->setColumnWidth(0, 70);
    m_modelPointsTable->setColumnWidth(1, 130);
    m_modelPointsTable->setColumnWidth(2, 220);
    m_modelPointsTable->setColumnWidth(3, 90);
    m_modelPointsTable->setColumnWidth(4, 120);
    m_modelPointsTable->setColumnWidth(5, 70);
    m_modelPointsTable->setColumnWidth(6, 260);
    m_modelPointsTable->setColumnWidth(7, 90);
    m_modelPointsTable->setColumnWidth(8, 70);
    modelDetailLayout->addWidget(m_modelPointsTable, 1);
    modelDetailLayout->setStretch(2, 1);
    modelDetailLayout->setStretch(3, 8);

    m_deviceEditorPage = new QWidget(this);
    auto *deviceEditorLayout = new QVBoxLayout(m_deviceEditorPage);
    deviceEditorLayout->setContentsMargins(0, 0, 0, 0);
    deviceEditorLayout->setSpacing(8);
    auto *deviceFormFrame = new QFrame(this);
    deviceFormFrame->setFrameShape(QFrame::StyledPanel);
    deviceFormFrame->setMaximumHeight(220);
    auto *deviceFormLayout = new QFormLayout(deviceFormFrame);
    deviceFormLayout->setContentsMargins(10, 8, 10, 8);
    deviceFormLayout->setVerticalSpacing(4);
    m_deviceIdEdit = new QLineEdit(this);
    m_deviceDescEdit = new QLineEdit(this);
    m_deviceModelEdit = new QLineEdit(this);
    m_deviceModelEdit->setReadOnly(true);
    m_deviceStationAddressEdit = new QLineEdit(this);
    m_deviceIpEdit = new QLineEdit(this);
    m_devicePortEdit = new QLineEdit(this);
    m_deviceChannelEdit = new QLineEdit(this);
    m_deviceCompatIpbLabel = new QLabel(QStringLiteral("-"), this);
    deviceFormLayout->addRow("DeviceId:", m_deviceIdEdit);
    deviceFormLayout->addRow("设备描述:", m_deviceDescEdit);
    deviceFormLayout->addRow("模型:", m_deviceModelEdit);
    deviceFormLayout->addRow("站地址:", m_deviceStationAddressEdit);
    deviceFormLayout->addRow("IP:", m_deviceIpEdit);
    deviceFormLayout->addRow("端口:", m_devicePortEdit);
    deviceFormLayout->addRow("通道:", m_deviceChannelEdit);
    deviceFormLayout->addRow("兼容 ipb:", m_deviceCompatIpbLabel);
    deviceEditorLayout->addWidget(deviceFormFrame);
    m_deviceValidationLabel = new QLabel(this);
    m_deviceValidationLabel->setWordWrap(true);
    deviceEditorLayout->addWidget(m_deviceValidationLabel);
    auto *bindingToolbar = new QHBoxLayout();
    bindingToolbar->addWidget(new QLabel("104 点位地址绑定:", this));
    bindingToolbar->addStretch();
    deviceEditorLayout->addLayout(bindingToolbar);
    m_deviceBindingsTable = new QTableWidget(0, 7, this);
    m_deviceBindingsTable->setHorizontalHeaderLabels({"启用", "DataRef", "描述", "地址", "初值", "自发标志", "PointRef"});
    m_deviceBindingsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_deviceBindingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceBindingsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deviceBindingsTable->verticalHeader()->setVisible(false);
    m_deviceBindingsTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceBindingsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceBindingsTable->setColumnWidth(0, 56);
    m_deviceBindingsTable->setColumnWidth(1, 260);
    m_deviceBindingsTable->setColumnWidth(2, 220);
    m_deviceBindingsTable->setColumnWidth(3, 90);
    m_deviceBindingsTable->setColumnWidth(4, 90);
    m_deviceBindingsTable->setColumnWidth(5, 90);
    m_deviceBindingsTable->setColumnWidth(6, 280);
    deviceEditorLayout->addWidget(m_deviceBindingsTable, 1);
    deviceEditorLayout->setStretch(2, 1);
    deviceEditorLayout->setStretch(3, 8);

    auto *deviceDetailPage = new QWidget(this);
    auto *deviceDetailLayout = new QFormLayout(deviceDetailPage);
    deviceDetailLayout->setContentsMargins(12, 12, 12, 12);
    deviceDetailLayout->setHorizontalSpacing(24);
    deviceDetailLayout->setVerticalSpacing(10);
    m_deviceDetailTitleLabel = new QLabel("-");
    m_deviceDetailModelLabel = new QLabel("-");
    m_deviceDetailAddressLabel = new QLabel("-");
    m_deviceDetailIpLabel = new QLabel("-");
    m_deviceDetailPortLabel = new QLabel("-");
    m_deviceDetailBindingCountLabel = new QLabel("0");
    auto *deviceDetailTitle = new QLabel("设备概览", this);
    QFont titleFont = deviceDetailTitle->font();
    titleFont.setBold(true);
    deviceDetailTitle->setFont(titleFont);
    detailPanelLayout->addWidget(deviceDetailTitle);
    deviceDetailLayout->addRow("设备:", m_deviceDetailTitleLabel);
    deviceDetailLayout->addRow("模型:", m_deviceDetailModelLabel);
    deviceDetailLayout->addRow("站地址:", m_deviceDetailAddressLabel);
    deviceDetailLayout->addRow("IP:", m_deviceDetailIpLabel);
    deviceDetailLayout->addRow("端口:", m_deviceDetailPortLabel);
    deviceDetailLayout->addRow("绑定点位数:", m_deviceDetailBindingCountLabel);
    detailPanelLayout->addWidget(deviceDetailPage, 1);
    configWorkspaceSplitter->addWidget(detailPanel);
    configWorkspaceSplitter->setStretchFactor(0, 0);
    configWorkspaceSplitter->setStretchFactor(1, 1);
    configWorkspaceSplitter->setSizes({420, 420});
    configLayout->addWidget(configWorkspaceSplitter, 1);

    m_mainTabWidget->addTab(debugPage, "调试控制");
    m_mainTabWidget->addTab(m_configPage, "配置概览");
    m_mainTabWidget->addTab(m_modelEditorPage, "模型编辑器");
    m_mainTabWidget->addTab(m_deviceEditorPage, "104设备编辑器");

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
                connect(m_newModelBtn, &QPushButton::clicked,
                    this, &MainWindow::onNewModelClicked);
                connect(m_createDeviceFromModelBtn, &QPushButton::clicked,
                    this, &MainWindow::onCreateDeviceFromModelClicked);
                connect(m_configModelTable, &QTableWidget::itemSelectionChanged,
                    this, &MainWindow::onConfigModelSelectionChanged);
                connect(m_configDeviceTable, &QTableWidget::itemSelectionChanged,
                    this, &MainWindow::onConfigDeviceSelectionChanged);
                connect(m_modelIdEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_modelDisplayNameEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_modelDeviceTypeEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_modelVersionEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_modelManufacturerIdEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_modelManufacturerDescEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_modelSchemaEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onModelFieldEdited);
                connect(m_addPointBtn, &QPushButton::clicked,
                    this, &MainWindow::onAddPointClicked);
                connect(m_copyPointBtn, &QPushButton::clicked,
                    this, &MainWindow::onCopyPointClicked);
                connect(m_deletePointBtn, &QPushButton::clicked,
                    this, &MainWindow::onDeletePointClicked);
                connect(m_modelPointsTable, &QTableWidget::itemChanged,
                    this, &MainWindow::onModelPointItemChanged);
                connect(m_deviceIdEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onDeviceFieldEdited);
                connect(m_deviceDescEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onDeviceFieldEdited);
                connect(m_deviceStationAddressEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onDeviceFieldEdited);
                connect(m_deviceIpEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onDeviceFieldEdited);
                connect(m_devicePortEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onDeviceFieldEdited);
                connect(m_deviceChannelEdit, &QLineEdit::textEdited,
                    this, &MainWindow::onDeviceFieldEdited);
                connect(m_deviceBindingsTable, &QTableWidget::itemChanged,
                    this, &MainWindow::onDeviceBindingItemChanged);
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

void MainWindow::onConfigModelSelectionChanged()
{
    refreshModelDetail(currentConfigModelIndex());
    if (currentConfigModelIndex() >= 0) {
        m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    }
}

void MainWindow::onConfigDeviceSelectionChanged()
{
    refreshDeviceDetail(currentConfigDeviceIndex());
    if (currentConfigDeviceIndex() >= 0) {
        refreshDeviceEditor(currentConfigDeviceIndex());
        m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    } else {
        refreshDeviceEditor(-1);
    }
}

void MainWindow::onNewModelClicked()
{
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (project.projectId.isEmpty()) {
        m_configProjectManager.createEmptyProject(QStringLiteral("本地配置工程"), QString());
    }

    configtool::ModelTemplate model;
    model.modelId = QStringLiteral("model_new_%1").arg(project.models.size() + 1);
    model.name = model.modelId;
    model.displayName = QStringLiteral("新模型%1").arg(project.models.size() + 1);
    model.deviceType = QStringLiteral("未定义设备");
    model.version = QStringLiteral("1.0");
    model.ensureDefaultServices();
    project.models.append(model);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int row = m_configModelTable->rowCount() - 1;
    if (row >= 0) {
        m_configModelTable->selectRow(row);
    }
    m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    statusBar()->showMessage(QStringLiteral("已创建模型骨架"), 4000);
}

void MainWindow::onModelFieldEdited()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    model.modelId = m_modelIdEdit->text().trimmed();
    model.name = model.modelId;
    model.displayName = m_modelDisplayNameEdit->text().trimmed();
    model.deviceType = m_modelDeviceTypeEdit->text().trimmed();
    model.version = m_modelVersionEdit->text().trimmed();
    model.manufacturerId = m_modelManufacturerIdEdit->text().trimmed();
    model.manufacturerDesc = m_modelManufacturerDescEdit->text().trimmed();
    model.schema = m_modelSchemaEdit->text().trimmed();

    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
}

void MainWindow::onAddPointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个模型。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    model.ensureDefaultServices();
    configtool::ServiceTemplate *service = model.findService(configtool::ModelServiceType::Measurement);
    if (!service) {
        return;
    }

    configtool::PointTemplate point;
    point.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    point.category = configtool::ModelServiceType::Measurement;
    point.signalType = configtool::PointSignalType::Yc;
    const int nextIndex = service->points.size() + 1;
    point.name = QStringLiteral("NewPoint%1").arg(nextIndex);
    point.description = QStringLiteral("新建点位%1").arg(nextIndex);
    point.ldName = QStringLiteral("PROT");
    point.lnType = QStringLiteral("CustomGGIO");
    point.lnInst = QStringLiteral("1");
    point.doName = point.name;
    point.doType = QStringLiteral("MV");
    point.dataType = QStringLiteral("Float");
    service->points.append(point);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    statusBar()->showMessage(QStringLiteral("已新增模型点位"), 3000);
}

void MainWindow::onCopyPointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    const QPair<int, int> pointLocation = currentModelPointLocation();
    if (modelIndex < 0 || pointLocation.first < 0 || pointLocation.second < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (pointLocation.first >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[pointLocation.first];
    if (pointLocation.second >= service.points.size()) {
        return;
    }

    configtool::PointTemplate copied = service.points.at(pointLocation.second);
    copied.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copied.name += QStringLiteral("_copy");
    copied.doName = copied.name;
    copied.description += QStringLiteral("-副本");
    service.points.insert(pointLocation.second + 1, copied);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    if (pointLocation.second + 1 < m_modelPointsTable->rowCount()) {
        m_modelPointsTable->selectRow(pointLocation.second + 1);
    }
    statusBar()->showMessage(QStringLiteral("已复制模型点位"), 3000);
}

void MainWindow::onDeletePointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    const QPair<int, int> pointLocation = currentModelPointLocation();
    if (modelIndex < 0 || pointLocation.first < 0 || pointLocation.second < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除点位"),
                              QStringLiteral("确定删除当前选中的模型点位吗？")) != QMessageBox::Yes) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (pointLocation.first >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[pointLocation.first];
    if (pointLocation.second >= service.points.size()) {
        return;
    }

    service.points.removeAt(pointLocation.second);
    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    statusBar()->showMessage(QStringLiteral("已删除模型点位"), 3000);
}

void MainWindow::onCreateDeviceFromModelClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个模型。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    const QString suggestedDeviceId = QStringLiteral("DEV_%1_%2")
        .arg(model.modelId.isEmpty() ? QStringLiteral("new") : model.modelId)
        .arg(project.devices.size() + 1);
    bool accepted = false;
    const QString deviceId = QInputDialog::getText(
        this,
        QStringLiteral("创建设备骨架"),
        QStringLiteral("请输入 DeviceId:"),
        QLineEdit::Normal,
        suggestedDeviceId,
        &accepted).trimmed();
    if (!accepted || deviceId.isEmpty()) {
        return;
    }

    configtool::ProtocolDeviceInstance device;
    device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    device.appType = QStringLiteral("cepiec104");
    device.protocol = configtool::ProtocolType::Iec104;
    device.deviceId = deviceId;
    device.deviceDesc = model.displayName.isEmpty() ? model.modelId : model.displayName;
    device.modelId = model.modelId;

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            configtool::PointBinding binding;
            binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            binding.pointRef = point.pointRef(model.modelId);
            binding.dataRef = point.dataRef();
            binding.descriptionOverride = point.description;
            binding.enabled = true;
            device.bindings.append(binding);
        }
    }

    project.devices.append(device);
    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int row = m_configDeviceTable->rowCount() - 1;
    if (row >= 0) {
        m_configDeviceTable->selectRow(row);
    }
    m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    statusBar()->showMessage(QStringLiteral("已根据模型生成 104 设备绑定骨架"), 4000);
}

void MainWindow::onDeviceFieldEdited()
{
    const int deviceIndex = currentConfigDeviceIndex();
    if (deviceIndex < 0) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    device.deviceId = m_deviceIdEdit->text().trimmed();
    device.deviceDesc = m_deviceDescEdit->text().trimmed();
    device.transport.stationAddress = m_deviceStationAddressEdit->text().trimmed();
    device.transport.ip = m_deviceIpEdit->text().trimmed();
    device.transport.port = m_devicePortEdit->text().trimmed();
    device.transport.channel = m_deviceChannelEdit->text().trimmed();

    refreshConfigObjectViews();
    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    if (deviceIndex < m_configDeviceTable->rowCount()) {
        m_configDeviceTable->selectRow(deviceIndex);
    }
}

void MainWindow::onDeviceBindingItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingDeviceBindingsTable) {
        return;
    }

    const int deviceIndex = currentConfigDeviceIndex();
    if (deviceIndex < 0) {
        return;
    }

    QTableWidgetItem *enabledItem = m_deviceBindingsTable->item(item->row(), 0);
    if (!enabledItem) {
        return;
    }

    const int bindingIndex = enabledItem->data(Qt::UserRole).toInt();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
        return;
    }

    configtool::PointBinding &binding = device.bindings[bindingIndex];
    switch (item->column()) {
    case 0:
        binding.enabled = item->checkState() == Qt::Checked;
        break;
    case 2:
        binding.descriptionOverride = item->text().trimmed();
        break;
    case 3:
        binding.address = item->text().trimmed();
        break;
    case 4:
        binding.initValue = item->text().trimmed();
        break;
    case 5:
        binding.selfSignalFlag = item->text().trimmed();
        break;
    default:
        break;
    }

    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    m_deviceBindingsTable->selectRow(item->row());
}

void MainWindow::onModelPointItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingModelPointsTable) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    QTableWidgetItem *categoryItem = m_modelPointsTable->item(item->row(), 0);
    if (!categoryItem) {
        return;
    }

    const int serviceIndex = categoryItem->data(Qt::UserRole).toInt();
    const int pointIndex = categoryItem->data(Qt::UserRole + 1).toInt();

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= service.points.size()) {
        return;
    }

    configtool::PointTemplate &point = service.points[pointIndex];
    switch (item->column()) {
    case 1:
        point.name = item->text().trimmed();
        point.doName = point.name;
        break;
    case 2:
        point.description = item->text().trimmed();
        break;
    case 3:
        point.ldName = item->text().trimmed();
        break;
    case 4:
        point.lnType = item->text().trimmed();
        break;
    case 5:
        point.lnInst = item->text().trimmed();
        break;
    case 7:
        point.dataType = item->text().trimmed();
        break;
    case 8:
        point.unit = item->text().trimmed();
        break;
    default:
        break;
    }

    m_updatingModelPointsTable = true;
    if (QTableWidgetItem *nameItem = m_modelPointsTable->item(item->row(), 1)) {
        nameItem->setText(point.doName);
    }
    if (QTableWidgetItem *dataRefItem = m_modelPointsTable->item(item->row(), 6)) {
        dataRefItem->setText(point.dataRef());
    }
    m_updatingModelPointsTable = false;

    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    m_modelPointsTable->selectRow(item->row());
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

    QString statusMessage = QStringLiteral("导入结果: 模型 %1，设备 %2")
        .arg(report.importedModelCount)
        .arg(report.importedDeviceCount);
    if (report.issues.isEmpty()) {
        statusMessage += QStringLiteral("，未发现错误或警告。");
    } else {
        statusMessage += QStringLiteral("，问题数 %1。")
            .arg(report.issues.size());
    }
    statusBar()->showMessage(statusMessage, 8000);
    refreshConfigObjectViews();
}

void MainWindow::refreshConfigObjectViews()
{
    const configtool::ConfigProject &project = m_configProjectManager.project();

    const int previousModelIndex = currentConfigModelIndex();
    const int previousDeviceIndex = currentConfigDeviceIndex();

    m_configModelTable->setRowCount(project.models.size());
    for (int row = 0; row < project.models.size(); ++row) {
        const configtool::ModelTemplate &model = project.models.at(row);
        int pointCount = 0;
        for (const configtool::ServiceTemplate &service : model.services) {
            pointCount += service.points.size();
        }

        m_configModelTable->setItem(row, 0, new QTableWidgetItem(model.modelId));
        m_configModelTable->setItem(row, 1, new QTableWidgetItem(model.displayName));
        m_configModelTable->setItem(row, 2, new QTableWidgetItem(model.deviceType));
        m_configModelTable->setItem(row, 3, new QTableWidgetItem(QString::number(pointCount)));
    }

    m_configDeviceTable->setRowCount(project.devices.size());
    for (int row = 0; row < project.devices.size(); ++row) {
        const configtool::ProtocolDeviceInstance &device = project.devices.at(row);
        m_configDeviceTable->setItem(row, 0, new QTableWidgetItem(device.deviceId));
        m_configDeviceTable->setItem(row, 1, new QTableWidgetItem(device.deviceDesc));
        m_configDeviceTable->setItem(row, 2, new QTableWidgetItem(device.modelId));
        m_configDeviceTable->setItem(row, 3, new QTableWidgetItem(device.transport.stationAddress));
        m_configDeviceTable->setItem(row, 4, new QTableWidgetItem(QString::number(device.bindings.size())));
    }

    if (previousModelIndex >= 0 && previousModelIndex < project.models.size()) {
        m_configModelTable->selectRow(previousModelIndex);
    } else if (!project.models.isEmpty()) {
        m_configModelTable->selectRow(0);
    } else {
        refreshModelDetail(-1);
    }

    if (previousDeviceIndex >= 0 && previousDeviceIndex < project.devices.size()) {
        m_configDeviceTable->selectRow(previousDeviceIndex);
    } else if (!project.devices.isEmpty()) {
        m_configDeviceTable->selectRow(0);
    } else {
        refreshDeviceDetail(-1);
        refreshDeviceEditor(-1);
    }
}

void MainWindow::refreshModelDetail(int modelIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        for (QLineEdit *edit : {m_modelIdEdit, m_modelDisplayNameEdit, m_modelDeviceTypeEdit,
                                m_modelVersionEdit, m_modelManufacturerIdEdit,
                                m_modelManufacturerDescEdit, m_modelSchemaEdit}) {
            edit->clear();
        }
        m_modelValidationLabel->setText(QStringLiteral("请选择一个模型。"));
        m_modelPointsTable->setRowCount(0);
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
    for (auto pair : {qMakePair(m_modelIdEdit, model.modelId),
                      qMakePair(m_modelDisplayNameEdit, model.displayName),
                      qMakePair(m_modelDeviceTypeEdit, model.deviceType),
                      qMakePair(m_modelVersionEdit, model.version),
                      qMakePair(m_modelManufacturerIdEdit, model.manufacturerId),
                      qMakePair(m_modelManufacturerDescEdit, model.manufacturerDesc),
                      qMakePair(m_modelSchemaEdit, model.schema)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }

    int totalPointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        totalPointCount += service.points.size();
    }

    m_modelPointsTable->setRowCount(totalPointCount);
    m_updatingModelPointsTable = true;
    int row = 0;
    for (int serviceIndex = 0; serviceIndex < model.services.size(); ++serviceIndex) {
        const configtool::ServiceTemplate &service = model.services.at(serviceIndex);
        for (int pointIndex = 0; pointIndex < service.points.size(); ++pointIndex, ++row) {
            const configtool::PointTemplate &point = service.points.at(pointIndex);
        auto *categoryItem = new QTableWidgetItem(configtool::modelServiceTypeDisplayName(point.category));
        auto *nameItem = new QTableWidgetItem(point.doName);
        auto *descriptionItem = new QTableWidgetItem(point.description);
        auto *ldNameItem = new QTableWidgetItem(point.ldName);
        auto *lnTypeItem = new QTableWidgetItem(point.lnType);
        auto *lnInstItem = new QTableWidgetItem(point.lnInst);
        auto *dataRefItem = new QTableWidgetItem(point.dataRef());
        auto *dataTypeItem = new QTableWidgetItem(point.dataType);
        auto *unitItem = new QTableWidgetItem(point.unit);

        categoryItem->setData(Qt::UserRole, serviceIndex);
        categoryItem->setData(Qt::UserRole + 1, pointIndex);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsEditable);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);

        if (duplicateRefs.contains(point.dataRef())) {
            const QColor duplicateColor(QStringLiteral("#c0392b"));
            categoryItem->setForeground(duplicateColor);
            nameItem->setForeground(duplicateColor);
            descriptionItem->setForeground(duplicateColor);
            ldNameItem->setForeground(duplicateColor);
            lnTypeItem->setForeground(duplicateColor);
            lnInstItem->setForeground(duplicateColor);
            dataRefItem->setForeground(duplicateColor);
            dataTypeItem->setForeground(duplicateColor);
            unitItem->setForeground(duplicateColor);
        }

        m_modelPointsTable->setItem(row, 0, categoryItem);
        m_modelPointsTable->setItem(row, 1, nameItem);
        m_modelPointsTable->setItem(row, 2, descriptionItem);
        m_modelPointsTable->setItem(row, 3, ldNameItem);
        m_modelPointsTable->setItem(row, 4, lnTypeItem);
        m_modelPointsTable->setItem(row, 5, lnInstItem);
        m_modelPointsTable->setItem(row, 6, dataRefItem);
        m_modelPointsTable->setItem(row, 7, dataTypeItem);
        m_modelPointsTable->setItem(row, 8, unitItem);
        }
    }
    m_updatingModelPointsTable = false;

    if (duplicateRefs.isEmpty()) {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        m_modelValidationLabel->setText(QStringLiteral("当前模型点位的 DataRef 唯一。"));
    } else {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
        m_modelValidationLabel->setText(
            QStringLiteral("检测到重复 DataRef：%1。请调整 LDname/LNtype/LNinst/DOname 组合。")
                .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
    }
}

void MainWindow::refreshDeviceDetail(int deviceIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        m_deviceDetailTitleLabel->setText(QStringLiteral("-"));
        m_deviceDetailModelLabel->setText(QStringLiteral("-"));
        m_deviceDetailAddressLabel->setText(QStringLiteral("-"));
        m_deviceDetailIpLabel->setText(QStringLiteral("-"));
        m_deviceDetailPortLabel->setText(QStringLiteral("-"));
        m_deviceDetailBindingCountLabel->setText(QStringLiteral("0"));
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    m_deviceDetailTitleLabel->setText(device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc);
    m_deviceDetailModelLabel->setText(device.modelId);
    m_deviceDetailAddressLabel->setText(device.transport.stationAddress);
    m_deviceDetailIpLabel->setText(device.transport.ip);
    m_deviceDetailPortLabel->setText(device.transport.port);
    m_deviceDetailBindingCountLabel->setText(QString::number(device.bindings.size()));
}

void MainWindow::refreshDeviceEditor(int deviceIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        for (QLineEdit *edit : {m_deviceIdEdit, m_deviceDescEdit, m_deviceModelEdit,
                                m_deviceStationAddressEdit, m_deviceIpEdit,
                                m_devicePortEdit, m_deviceChannelEdit}) {
            if (edit) {
                edit->clear();
            }
        }
        m_deviceCompatIpbLabel->setText(QStringLiteral("-"));
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #666666; }");
        m_deviceValidationLabel->setText(QStringLiteral("请选择一个 104 设备。"));
        m_deviceBindingsTable->setRowCount(0);
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    for (auto pair : {qMakePair(m_deviceIdEdit, device.deviceId),
                      qMakePair(m_deviceDescEdit, device.deviceDesc),
                      qMakePair(m_deviceModelEdit, device.modelId),
                      qMakePair(m_deviceStationAddressEdit, device.transport.stationAddress),
                      qMakePair(m_deviceIpEdit, device.transport.ip),
                      qMakePair(m_devicePortEdit, device.transport.port),
                      qMakePair(m_deviceChannelEdit, device.transport.channel)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }

    const QJsonValue ipbValue = device.transport.source.rawExtra.value(QStringLiteral("ipb"));
    if (ipbValue.isUndefined() || ipbValue.isNull()) {
        m_deviceCompatIpbLabel->setText(QStringLiteral("-"));
    } else if (ipbValue.isString()) {
        m_deviceCompatIpbLabel->setText(ipbValue.toString());
    } else if (ipbValue.isDouble()) {
        m_deviceCompatIpbLabel->setText(QString::number(ipbValue.toInt()));
    } else {
        m_deviceCompatIpbLabel->setText(QString::fromUtf8(QJsonDocument(ipbValue.toObject()).toJson(QJsonDocument::Compact)));
    }

    const QSet<QString> duplicateAddresses = duplicateBindingAddresses(device);
    int emptyAddressCount = 0;
    for (const configtool::PointBinding &binding : device.bindings) {
        if (binding.enabled && binding.address.trimmed().isEmpty()) {
            ++emptyAddressCount;
        }
    }

    m_updatingDeviceBindingsTable = true;
    m_deviceBindingsTable->setRowCount(device.bindings.size());
    for (int row = 0; row < device.bindings.size(); ++row) {
        const configtool::PointBinding &binding = device.bindings.at(row);
        auto *enabledItem = new QTableWidgetItem();
        enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, row);
        auto *dataRefItem = new QTableWidgetItem(binding.dataRef);
        auto *descriptionItem = new QTableWidgetItem(binding.descriptionOverride);
        auto *addressItem = new QTableWidgetItem(binding.address);
        auto *initValueItem = new QTableWidgetItem(binding.initValue);
        auto *selfSignalItem = new QTableWidgetItem(binding.selfSignalFlag);
        auto *pointRefItem = new QTableWidgetItem(binding.pointRef);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
        pointRefItem->setFlags(pointRefItem->flags() & ~Qt::ItemIsEditable);

        if (binding.enabled && duplicateAddresses.contains(binding.address.trimmed())) {
            const QColor duplicateColor(QStringLiteral("#c0392b"));
            enabledItem->setForeground(duplicateColor);
            dataRefItem->setForeground(duplicateColor);
            descriptionItem->setForeground(duplicateColor);
            addressItem->setForeground(duplicateColor);
            initValueItem->setForeground(duplicateColor);
            selfSignalItem->setForeground(duplicateColor);
            pointRefItem->setForeground(duplicateColor);
        } else if (binding.enabled && binding.address.trimmed().isEmpty()) {
            const QColor warningColor(QStringLiteral("#b9770e"));
            dataRefItem->setForeground(warningColor);
            addressItem->setForeground(warningColor);
        }

        m_deviceBindingsTable->setItem(row, 0, enabledItem);
        m_deviceBindingsTable->setItem(row, 1, dataRefItem);
        m_deviceBindingsTable->setItem(row, 2, descriptionItem);
        m_deviceBindingsTable->setItem(row, 3, addressItem);
        m_deviceBindingsTable->setItem(row, 4, initValueItem);
        m_deviceBindingsTable->setItem(row, 5, selfSignalItem);
        m_deviceBindingsTable->setItem(row, 6, pointRefItem);
    }
    m_updatingDeviceBindingsTable = false;

    if (!duplicateAddresses.isEmpty()) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("检测到重复 104 地址：%1。请调整地址列，避免启用点位地址冲突。")
                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，"))));
    } else if (emptyAddressCount > 0) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #b9770e; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("当前仍有 %1 个启用点位未填写 104 地址，导出前需要补齐。")
                .arg(emptyAddressCount));
    } else {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        m_deviceValidationLabel->setText(QStringLiteral("当前设备地址分配未发现重复。"));
    }
}

int MainWindow::currentConfigModelIndex() const
{
    if (!m_configModelTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_configModelTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

int MainWindow::currentConfigDeviceIndex() const
{
    if (!m_configDeviceTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_configDeviceTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QPair<int, int> MainWindow::currentModelPointLocation() const
{
    if (!m_modelPointsTable->selectionModel()) {
        return qMakePair(-1, -1);
    }

    const QModelIndexList rows = m_modelPointsTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return qMakePair(-1, -1);
    }

    QTableWidgetItem *categoryItem = m_modelPointsTable->item(rows.first().row(), 0);
    if (!categoryItem) {
        return qMakePair(-1, -1);
    }

    return qMakePair(categoryItem->data(Qt::UserRole).toInt(),
                     categoryItem->data(Qt::UserRole + 1).toInt());
}

QSet<QString> MainWindow::duplicateDataRefsForModel(const configtool::ModelTemplate &model) const
{
    QSet<QString> seenRefs;
    QSet<QString> duplicateRefs;

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            const QString ref = point.dataRef();
            if (ref.isEmpty()) {
                continue;
            }

            if (seenRefs.contains(ref)) {
                duplicateRefs.insert(ref);
            } else {
                seenRefs.insert(ref);
            }
        }
    }

    return duplicateRefs;
}

QSet<QString> MainWindow::duplicateBindingAddresses(const configtool::ProtocolDeviceInstance &device) const
{
    QSet<QString> seenAddresses;
    QSet<QString> duplicateAddresses;

    for (const configtool::PointBinding &binding : device.bindings) {
        if (!binding.enabled) {
            continue;
        }

        const QString address = binding.address.trimmed();
        if (address.isEmpty()) {
            continue;
        }

        if (seenAddresses.contains(address)) {
            duplicateAddresses.insert(address);
        } else {
            seenAddresses.insert(address);
        }
    }

    return duplicateAddresses;
}
