#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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
    importRow->addWidget(new QLabel("工程目录:"));
    m_configImportDirEdit = new QLineEdit();
    m_configImportDirEdit->setPlaceholderText("选择工程根目录，例如包含 cepiec104、cepmodbus、cepdlt645、cepLogicCenter 的目录");
    importRow->addWidget(m_configImportDirEdit, 1);
    m_browseConfigImportDirBtn = new QPushButton("浏览...");
    importRow->addWidget(m_browseConfigImportDirBtn);
    m_importIec104ConfigBtn = new QPushButton("导入配置");
    importRow->addWidget(m_importIec104ConfigBtn);
    m_exportIec104ConfigBtn = new QPushButton("导出配置");
    importRow->addWidget(m_exportIec104ConfigBtn);
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
    summaryLayout->addRow("工程目录:", m_configSourceRootValueLabel);
    summaryLayout->addRow("模型数量:", m_configModelCountValueLabel);
    summaryLayout->addRow("设备数量:", m_configDeviceCountValueLabel);
    summaryLayout->addRow("导入问题数:", m_configIssueCountValueLabel);
    configLayout->addWidget(summaryFrame);

    auto *configWorkspaceSplitter = new QSplitter(Qt::Horizontal, this);

    auto *objectPanel = new QWidget(this);
    auto *objectPanelLayout = new QVBoxLayout(objectPanel);
    objectPanelLayout->setContentsMargins(0, 0, 0, 0);
    objectPanelLayout->setSpacing(10);

    m_modelGroupBox = new QGroupBox("模型列表", this);
    auto *modelGroupLayout = new QVBoxLayout(m_modelGroupBox);
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
    m_configModelTable->setStyleSheet(
        "QTableWidget::item:selected {"
        "  background-color: #355c7d;"
        "  color: #ffffff;"
        "}"
    );
    modelGroupLayout->addWidget(m_configModelTable, 1);
    objectPanelLayout->addWidget(m_modelGroupBox, 1);

    m_deviceGroupBox = new QGroupBox("设备列表", this);
    auto *deviceGroupLayout = new QVBoxLayout(m_deviceGroupBox);
    m_configDeviceTable = new QTableWidget(0, 5, this);
    m_configDeviceTable->setHorizontalHeaderLabels({"DeviceId", "描述", "模型", "站地址", "点位数"});
    m_configDeviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_configDeviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configDeviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_configDeviceTable->verticalHeader()->setVisible(false);
    m_configDeviceTable->horizontalHeader()->setStretchLastSection(true);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_configDeviceTable->setStyleSheet(
        "QTableWidget::item:selected {"
        "  background-color: #7d4f50;"
        "  color: #ffffff;"
        "}"
    );
    deviceGroupLayout->addWidget(m_configDeviceTable, 1);
    objectPanelLayout->addWidget(m_deviceGroupBox, 1);

    configWorkspaceSplitter->addWidget(objectPanel);

    auto *detailPanel = new QWidget(this);
    auto *detailPanelLayout = new QVBoxLayout(detailPanel);
    detailPanelLayout->setContentsMargins(0, 0, 0, 0);
    detailPanelLayout->setSpacing(8);

    auto *modelOverviewPage = new QWidget(this);
    auto *modelOverviewLayout = new QFormLayout(modelOverviewPage);
    modelOverviewLayout->setContentsMargins(12, 12, 12, 12);
    modelOverviewLayout->setHorizontalSpacing(24);
    modelOverviewLayout->setVerticalSpacing(10);
    m_modelOverviewIdLabel = new QLabel("-");
    m_modelOverviewDisplayNameLabel = new QLabel("-");
    m_modelOverviewDeviceTypeLabel = new QLabel("-");
    m_modelOverviewVersionLabel = new QLabel("-");
    m_modelOverviewPointCountLabel = new QLabel("0");
    auto *modelOverviewTitle = new QLabel("模型概览", this);
    QFont modelTitleFont = modelOverviewTitle->font();
    modelTitleFont.setBold(true);
    modelOverviewTitle->setFont(modelTitleFont);
    detailPanelLayout->addWidget(modelOverviewTitle);
    modelOverviewLayout->addRow("模型:", m_modelOverviewIdLabel);
    modelOverviewLayout->addRow("展示名:", m_modelOverviewDisplayNameLabel);
    modelOverviewLayout->addRow("设备类型:", m_modelOverviewDeviceTypeLabel);
    modelOverviewLayout->addRow("版本:", m_modelOverviewVersionLabel);
    modelOverviewLayout->addRow("点位数:", m_modelOverviewPointCountLabel);
    detailPanelLayout->addWidget(modelOverviewPage);

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
    m_modelPointFilterTabBar = new QTabBar(this);
    m_modelPointFilterTabBar->addTab(QStringLiteral("全部"));
    m_modelPointFilterTabBar->addTab(QStringLiteral("遥测"));
    m_modelPointFilterTabBar->addTab(QStringLiteral("遥信"));
    m_modelPointFilterTabBar->addTab(QStringLiteral("控制"));
    m_modelPointFilterTabBar->setExpanding(false);
    m_modelPointFilterTabBar->setCurrentIndex(0);
    pointToolbar->addWidget(m_modelPointFilterTabBar);
    pointToolbar->addSpacing(12);
    pointToolbar->addWidget(new QLabel("新建到:"));
    m_newPointCategoryCombo = new QComboBox(this);
    m_newPointCategoryCombo->addItem(QStringLiteral("遥测"), static_cast<int>(configtool::ModelServiceType::Measurement));
    m_newPointCategoryCombo->addItem(QStringLiteral("遥信"), static_cast<int>(configtool::ModelServiceType::Status));
    m_newPointCategoryCombo->addItem(QStringLiteral("控制"), static_cast<int>(configtool::ModelServiceType::Control));
    pointToolbar->addWidget(m_newPointCategoryCombo);
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

    m_statusLabel = new QLabel("未连接");
    statusBar()->addWidget(m_statusLabel);

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
    connect(m_exportIec104ConfigBtn, &QPushButton::clicked,
            this, &MainWindow::onExportIec104ConfigClicked);
    connect(m_newModelBtn, &QPushButton::clicked,
            this, &MainWindow::onNewModelClicked);
    connect(m_createDeviceFromModelBtn, &QPushButton::clicked,
            this, &MainWindow::onCreateDeviceFromModelClicked);
    connect(m_configModelTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onConfigModelSelectionChanged);
    connect(m_configDeviceTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onConfigDeviceSelectionChanged);
    connect(m_configModelTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onConfigModelActivated);
    connect(m_configDeviceTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onConfigDeviceActivated);
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
    connect(m_modelPointFilterTabBar, &QTabBar::currentChanged,
            this, &MainWindow::onModelPointFilterChanged);
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
