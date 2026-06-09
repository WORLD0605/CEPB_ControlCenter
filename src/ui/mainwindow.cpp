#include "mainwindow.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
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
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
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
    m_browseConfigImportDirBtn = new QPushButton("打开配置工作区...");
    importRow->addWidget(m_browseConfigImportDirBtn);
    m_exportIec104ConfigBtn = new QPushButton("保存配置");
    importRow->addWidget(m_exportIec104ConfigBtn);
    configLayout->addLayout(importRow);

    auto *transferRow = new QHBoxLayout();
    transferRow->addWidget(new QLabel(QStringLiteral("设备:")));
    m_configRemoteHostEdit = new QLineEdit(QStringLiteral("192.168.7.10"), this);
    m_configRemoteHostEdit->setMinimumWidth(130);
    transferRow->addWidget(m_configRemoteHostEdit);
    transferRow->addWidget(new QLabel(QStringLiteral("用户:")));
    m_configRemoteUserEdit = new QLineEdit(QStringLiteral("root"), this);
    m_configRemoteUserEdit->setMaximumWidth(90);
    transferRow->addWidget(m_configRemoteUserEdit);
    transferRow->addWidget(new QLabel(QStringLiteral("密码:")));
    m_configRemotePasswordEdit = new QLineEdit(QStringLiteral("root"), this);
    m_configRemotePasswordEdit->setEchoMode(QLineEdit::Password);
    m_configRemotePasswordEdit->setMaximumWidth(90);
    transferRow->addWidget(m_configRemotePasswordEdit);
    transferRow->addWidget(new QLabel(QStringLiteral("端口:")));
    m_configRemotePortEdit = new QSpinBox(this);
    m_configRemotePortEdit->setRange(1, 65535);
    m_configRemotePortEdit->setValue(10022);
    m_configRemotePortEdit->setMaximumWidth(84);
    transferRow->addWidget(m_configRemotePortEdit);
    transferRow->addWidget(new QLabel(QStringLiteral("APP目录:")));
    m_configRemoteBaseDirEdit = new QLineEdit(QStringLiteral("/home/cepgateway/app"), this);
    m_configRemoteBaseDirEdit->setMinimumWidth(220);
    transferRow->addWidget(m_configRemoteBaseDirEdit, 1);
    m_uploadConfigBtn = new QPushButton(QStringLiteral("上传到设备"), this);
    m_downloadConfigBtn = new QPushButton(QStringLiteral("从设备下载"), this);
    transferRow->addWidget(m_uploadConfigBtn);
    transferRow->addWidget(m_downloadConfigBtn);
    configLayout->addLayout(transferRow);

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
    summaryLayout->addRow("问题数:", m_configIssueCountValueLabel);
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
    m_deleteModelBtn = new QPushButton("删除模型");
    m_deleteModelBtn->setEnabled(false);
    modelToolbar->addWidget(m_deleteModelBtn);
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
    auto *deviceToolbar = new QHBoxLayout();
    m_deleteDeviceBtn = new QPushButton("删除设备");
    m_deleteDeviceBtn->setEnabled(false);
    deviceToolbar->addWidget(m_deleteDeviceBtn);
    deviceToolbar->addStretch();
    deviceGroupLayout->addLayout(deviceToolbar);
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
    modelFormFrame->setMaximumHeight(210);
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
    m_modelNorthVisibleCheck = new QCheckBox(QStringLiteral("北向可见"), this);
    m_modelNorthVisibleCheck->setChecked(true);
    modelFormLayout->addRow("模型ID:", m_modelIdEdit);
    modelFormLayout->addRow("展示名称:", m_modelDisplayNameEdit);
    modelFormLayout->addRow("设备类型:", m_modelDeviceTypeEdit);
    modelFormLayout->addRow("版本:", m_modelVersionEdit);
    modelFormLayout->addRow("厂家ID:", m_modelManufacturerIdEdit);
    modelFormLayout->addRow("厂家描述:", m_modelManufacturerDescEdit);
    modelFormLayout->addRow("Schema:", m_modelSchemaEdit);
    modelFormLayout->addRow(QStringLiteral("北向:"), m_modelNorthVisibleCheck);
    modelDetailLayout->addWidget(modelFormFrame);
    auto *pointToolbar = new QHBoxLayout();
    pointToolbar->setSpacing(8);
    pointToolbar->addWidget(new QLabel("模型点位:"));
    m_modelPointFilterTabBar = new QTabBar(this);
    m_modelPointFilterTabBar->addTab(QStringLiteral("全部"));
    m_modelPointFilterTabBar->addTab(QStringLiteral("遥测"));
    m_modelPointFilterTabBar->addTab(QStringLiteral("遥信"));
    m_modelPointFilterTabBar->addTab(QStringLiteral("控制"));
    m_modelPointFilterTabBar->setExpanding(false);
    m_modelPointFilterTabBar->setCurrentIndex(0);
    pointToolbar->addWidget(m_modelPointFilterTabBar);
    pointToolbar->addStretch();
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
    modelDetailLayout->addLayout(pointToolbar);

    auto *pointFilterToolbar = new QHBoxLayout();
    pointFilterToolbar->setSpacing(8);
    pointFilterToolbar->addSpacing(2);
    pointFilterToolbar->addWidget(new QLabel("DataRef:"));
    m_modelPointDataRefFilterEdit = new QLineEdit(this);
    m_modelPointDataRefFilterEdit->setPlaceholderText("输入 DataRef 关键字实时筛选...");
    m_modelPointDataRefFilterEdit->setClearButtonEnabled(true);
    m_modelPointDataRefFilterEdit->setMinimumWidth(320);
    pointFilterToolbar->addWidget(m_modelPointDataRefFilterEdit, 1);
    pointFilterToolbar->addWidget(new QLabel("Description:"));
    m_modelPointDescriptionFilterEdit = new QLineEdit(this);
    m_modelPointDescriptionFilterEdit->setPlaceholderText("输入描述关键字实时筛选...");
    m_modelPointDescriptionFilterEdit->setClearButtonEnabled(true);
    m_modelPointDescriptionFilterEdit->setMinimumWidth(320);
    pointFilterToolbar->addWidget(m_modelPointDescriptionFilterEdit, 1);
    modelDetailLayout->addLayout(pointFilterToolbar);

    m_modelValidationLabel = new QLabel(this);
    m_modelValidationLabel->setWordWrap(false);
    m_modelValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
    m_modelPointsTable = new QTableWidget(0, 10, this);
    m_modelPointsTable->setColumnCount(10);
    m_modelPointsTable->setHorizontalHeaderLabels({"北向可见", "类别", "DOname", "描述", "LDname", "LNtype", "LNinst", "DataRef", "数据类型", "单位"});
    m_modelPointsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_modelPointsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_modelPointsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_modelPointsTable->setDragEnabled(true);
    m_modelPointsTable->setAcceptDrops(true);
    m_modelPointsTable->setDropIndicatorShown(true);
    m_modelPointsTable->setDragDropMode(QAbstractItemView::DragDrop);
    m_modelPointsTable->setDragDropOverwriteMode(false);
    m_modelPointsTable->setDefaultDropAction(Qt::CopyAction);
    m_modelPointsTable->viewport()->installEventFilter(this);
    m_modelPointDropLine = new QFrame(m_modelPointsTable->viewport());
    m_modelPointDropLine->setFixedHeight(3);
    m_modelPointDropLine->setStyleSheet(QStringLiteral("background-color: #ff8c00; border-radius: 1px;"));
    m_modelPointDropLine->hide();
    m_modelPointsTable->verticalHeader()->setVisible(false);
    m_modelPointsTable->horizontalHeader()->setStretchLastSection(true);
    m_modelPointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_modelPointsTable->setColumnWidth(0, 76);
    m_modelPointsTable->setColumnWidth(1, 70);
    m_modelPointsTable->setColumnWidth(2, 130);
    m_modelPointsTable->setColumnWidth(3, 220);
    m_modelPointsTable->setColumnWidth(4, 90);
    m_modelPointsTable->setColumnWidth(5, 120);
    m_modelPointsTable->setColumnWidth(6, 70);
    m_modelPointsTable->setColumnWidth(7, 260);
    m_modelPointsTable->setColumnWidth(8, 90);
    m_modelPointsTable->setColumnWidth(9, 70);
    modelDetailLayout->addWidget(m_modelPointsTable, 1);
    modelDetailLayout->setStretch(0, 0);
    modelDetailLayout->setStretch(1, 0);
    modelDetailLayout->setStretch(2, 0);
    modelDetailLayout->setStretch(3, 1);

    m_deviceEditorPage = new QWidget(this);
    auto *deviceEditorLayout = new QVBoxLayout(m_deviceEditorPage);
    deviceEditorLayout->setContentsMargins(0, 0, 0, 0);
    deviceEditorLayout->setSpacing(8);
    auto *deviceTopPanel = new QWidget(this);
    auto *deviceTopLayout = new QHBoxLayout(deviceTopPanel);
    deviceTopLayout->setContentsMargins(0, 0, 0, 0);
    deviceTopLayout->setSpacing(8);
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
    deviceFormLayout->addRow("DeviceId:", m_deviceIdEdit);
    deviceFormLayout->addRow("设备描述:", m_deviceDescEdit);
    deviceFormLayout->addRow("模型:", m_deviceModelEdit);
    deviceFormLayout->addRow("站地址:", m_deviceStationAddressEdit);
    deviceFormLayout->addRow("IP:", m_deviceIpEdit);
    deviceFormLayout->addRow("端口:", m_devicePortEdit);
    deviceTopLayout->addWidget(deviceFormFrame, 1);
    m_modbusParamsGroupBox = new QGroupBox(QStringLiteral("Modbus 参数"), this);
    m_modbusParamsGroupBox->setMaximumHeight(220);
    auto *modbusParamsLayout = new QGridLayout(m_modbusParamsGroupBox);
    modbusParamsLayout->setContentsMargins(10, 8, 10, 8);
    modbusParamsLayout->setHorizontalSpacing(8);
    modbusParamsLayout->setVerticalSpacing(4);
    m_modbusTypeCombo = new QComboBox(this);
    m_modbusTypeCombo->addItem(QStringLiteral("TCP"));
    m_modbusTypeCombo->addItem(QStringLiteral("RTU"));
    m_modbusSerialPortCombo = new QComboBox(this);
    for (int i = 1; i <= 8; ++i) {
        m_modbusSerialPortCombo->addItem(QStringLiteral("RS485_%1").arg(i));
    }
    m_modbusHwVariantCombo = new QComboBox(this);
    m_modbusHwVariantCombo->addItem(QStringLiteral("使用RS485_4时才需要选择，否则不需要选择"), QString());
    m_modbusHwVariantCombo->addItem(QStringLiteral("myir"), QStringLiteral("myir"));
    m_modbusHwVariantCombo->addItem(QStringLiteral("talowe"), QStringLiteral("talowe"));
    m_modbusBaudCombo = new QComboBox(this);
    for (const QString &baud : {QStringLiteral("600"), QStringLiteral("1200"), QStringLiteral("2400"),
                                QStringLiteral("4800"), QStringLiteral("9600"), QStringLiteral("19200"),
                                QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("115200"),
                                QStringLiteral("230400")}) {
        m_modbusBaudCombo->addItem(baud);
    }
    m_modbusDataBitsCombo = new QComboBox(this);
    for (const QString &dataBits : {QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")}) {
        m_modbusDataBitsCombo->addItem(dataBits);
    }
    m_modbusStopBitsCombo = new QComboBox(this);
    for (const QString &stopBits : {QStringLiteral("1"), QStringLiteral("2")}) {
        m_modbusStopBitsCombo->addItem(stopBits);
    }
    m_modbusParityCombo = new QComboBox(this);
    for (const QString &parity : {QStringLiteral("N"), QStringLiteral("E"), QStringLiteral("O")}) {
        m_modbusParityCombo->addItem(parity);
    }
    m_modbusDebugCheck = new QCheckBox(QStringLiteral("debug"), this);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("类型:"), this), 0, 0);
    modbusParamsLayout->addWidget(m_modbusTypeCombo, 0, 1);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("串口:"), this), 0, 2);
    modbusParamsLayout->addWidget(m_modbusSerialPortCombo, 0, 3);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("波特率:"), this), 0, 4);
    modbusParamsLayout->addWidget(m_modbusBaudCombo, 0, 5);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("数据位:"), this), 1, 0);
    modbusParamsLayout->addWidget(m_modbusDataBitsCombo, 1, 1);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("停止位:"), this), 1, 2);
    modbusParamsLayout->addWidget(m_modbusStopBitsCombo, 1, 3);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("校验:"), this), 1, 4);
    modbusParamsLayout->addWidget(m_modbusParityCombo, 1, 5);
    modbusParamsLayout->addWidget(m_modbusDebugCheck, 1, 6);
    modbusParamsLayout->addWidget(new QLabel(QStringLiteral("硬件型号:"), this), 2, 0);
    modbusParamsLayout->addWidget(m_modbusHwVariantCombo, 2, 1, 1, 6);
    deviceTopLayout->addWidget(m_modbusParamsGroupBox, 1);
    deviceEditorLayout->addWidget(deviceTopPanel);
    m_deviceValidationLabel = new QLabel(this);
    m_deviceValidationLabel->setWordWrap(true);
    deviceEditorLayout->addWidget(m_deviceValidationLabel);
    auto *bindingToolbar = new QHBoxLayout();
    bindingToolbar->addWidget(new QLabel(QStringLiteral("点位映射:"), this));
    bindingToolbar->addStretch();
    deviceEditorLayout->addLayout(bindingToolbar);

    auto *bindingFilterToolbar = new QHBoxLayout();
    bindingFilterToolbar->setSpacing(8);
    bindingFilterToolbar->addSpacing(2);
    bindingFilterToolbar->addWidget(new QLabel("DataRef:", this));
    m_deviceBindingDataRefFilterEdit = new QLineEdit(this);
    m_deviceBindingDataRefFilterEdit->setPlaceholderText("输入 DataRef 关键字实时筛选...");
    m_deviceBindingDataRefFilterEdit->setClearButtonEnabled(true);
    m_deviceBindingDataRefFilterEdit->setMinimumWidth(320);
    bindingFilterToolbar->addWidget(m_deviceBindingDataRefFilterEdit, 1);
    bindingFilterToolbar->addWidget(new QLabel("Description:", this));
    m_deviceBindingDescriptionFilterEdit = new QLineEdit(this);
    m_deviceBindingDescriptionFilterEdit->setPlaceholderText("输入描述关键字实时筛选...");
    m_deviceBindingDescriptionFilterEdit->setClearButtonEnabled(true);
    m_deviceBindingDescriptionFilterEdit->setMinimumWidth(320);
    bindingFilterToolbar->addWidget(m_deviceBindingDescriptionFilterEdit, 1);
    deviceEditorLayout->addLayout(bindingFilterToolbar);

    m_deviceBindingsTable = new QTableWidget(0, 6, this);
    m_deviceBindingsTable->setHorizontalHeaderLabels({
        QStringLiteral("启用"),
        QStringLiteral("DataRef"),
        QStringLiteral("描述"),
        QStringLiteral("地址"),
        QStringLiteral("初值"),
        QStringLiteral("自发标志")
    });
    m_deviceBindingsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_deviceBindingsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_deviceBindingsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_deviceBindingsTable->verticalHeader()->setVisible(false);
    m_deviceBindingsTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceBindingsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceBindingsTable->setColumnWidth(0, 56);
    m_deviceBindingsTable->setColumnWidth(1, 260);
    m_deviceBindingsTable->setColumnWidth(2, 220);
    m_deviceBindingsTable->setColumnWidth(3, 90);
    m_deviceBindingsTable->setColumnWidth(4, 90);
    m_deviceBindingsTable->setColumnWidth(5, 90);
    deviceEditorLayout->addWidget(m_deviceBindingsTable, 1);
    deviceEditorLayout->setStretch(0, 0);
    deviceEditorLayout->setStretch(1, 0);
    deviceEditorLayout->setStretch(2, 0);
    deviceEditorLayout->setStretch(3, 0);
    deviceEditorLayout->setStretch(4, 1);

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
    configWorkspaceSplitter->setStretchFactor(0, 2);
    configWorkspaceSplitter->setStretchFactor(1, 1);
    configWorkspaceSplitter->setSizes({960, 500});
    configLayout->addWidget(configWorkspaceSplitter, 1);

    m_configIssuePage = new QWidget(this);
    auto *configIssueLayout = new QVBoxLayout(m_configIssuePage);
    configIssueLayout->setContentsMargins(0, 0, 0, 0);
    configIssueLayout->setSpacing(8);
    auto *configIssueHint = new QLabel(
        QStringLiteral("导入、导出和配置校验产生的问题会统一显示在这里。双击问题行可尽量跳转到对应编辑页面和对象。"),
        this);
    configIssueHint->setWordWrap(true);
    configIssueLayout->addWidget(configIssueHint);
    auto *configIssueToolbar = new QHBoxLayout();
    m_checkConfigIssuesBtn = new QPushButton(QStringLiteral("检查当前问题"), this);
    configIssueToolbar->addWidget(m_checkConfigIssuesBtn);
    configIssueToolbar->addStretch();
    configIssueLayout->addLayout(configIssueToolbar);
    m_configIssueTable = new QTableWidget(0, 5, this);
    m_configIssueTable->setHorizontalHeaderLabels({
        QStringLiteral("来源"),
        QStringLiteral("级别"),
        QStringLiteral("对象"),
        QStringLiteral("说明"),
        QStringLiteral("文件")
    });
    m_configIssueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_configIssueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configIssueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_configIssueTable->setAlternatingRowColors(true);
    m_configIssueTable->verticalHeader()->setVisible(false);
    m_configIssueTable->horizontalHeader()->setStretchLastSection(true);
    m_configIssueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_configIssueTable->setColumnWidth(0, 80);
    m_configIssueTable->setColumnWidth(1, 80);
    m_configIssueTable->setColumnWidth(2, 260);
    m_configIssueTable->setColumnWidth(3, 520);
    configIssueLayout->addWidget(m_configIssueTable, 1);

    m_logicCenterPage = new QWidget(this);
    auto *logicLayout = new QVBoxLayout(m_logicCenterPage);
    logicLayout->setContentsMargins(0, 0, 0, 0);
    logicLayout->setSpacing(10);

    auto *logicSummaryFrame = new QFrame(this);
    logicSummaryFrame->setFrameShape(QFrame::StyledPanel);
    auto *logicSummaryLayout = new QGridLayout(logicSummaryFrame);
    logicSummaryLayout->setContentsMargins(10, 8, 10, 8);
    logicSummaryLayout->setHorizontalSpacing(28);
    logicSummaryLayout->setVerticalSpacing(6);
    m_logicAgcAvcGroupCountLabel = new QLabel(QStringLiteral("0"), this);
    m_logicComputationPointCountLabel = new QLabel(QStringLiteral("0"), this);
    m_logicControlRuleCountLabel = new QLabel(QStringLiteral("0"), this);
    m_logicOnlineLinkCountLabel = new QLabel(QStringLiteral("0"), this);
    m_logicDerivedDeviceCountLabel = new QLabel(QStringLiteral("0"), this);
    m_logicIssueCountLabel = new QLabel(QStringLiteral("0"), this);
    m_logicExportPathLabel = new QLabel(QStringLiteral("-"), this);
    m_logicExportPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("AGC/AVC 组:"), this), 0, 0);
    logicSummaryLayout->addWidget(m_logicAgcAvcGroupCountLabel, 0, 1);
    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("计算点:"), this), 0, 2);
    logicSummaryLayout->addWidget(m_logicComputationPointCountLabel, 0, 3);
    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("控制规则:"), this), 0, 4);
    logicSummaryLayout->addWidget(m_logicControlRuleCountLabel, 0, 5);
    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("在线联动:"), this), 1, 0);
    logicSummaryLayout->addWidget(m_logicOnlineLinkCountLabel, 1, 1);
    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("虚拟派生:"), this), 1, 2);
    logicSummaryLayout->addWidget(m_logicDerivedDeviceCountLabel, 1, 3);
    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("校验问题:"), this), 1, 4);
    logicSummaryLayout->addWidget(m_logicIssueCountLabel, 1, 5);
    logicSummaryLayout->addWidget(new QLabel(QStringLiteral("导出文件:"), this), 2, 0);
    logicSummaryLayout->addWidget(m_logicExportPathLabel, 2, 1, 1, 5);
    logicLayout->addWidget(logicSummaryFrame);

    auto *logicActionFrame = new QFrame(this);
    logicActionFrame->setFrameShape(QFrame::StyledPanel);
    auto *logicActionLayout = new QGridLayout(logicActionFrame);
    logicActionLayout->setContentsMargins(10, 8, 10, 8);
    logicActionLayout->setHorizontalSpacing(8);
    logicActionLayout->setVerticalSpacing(8);

    auto addLogicAction = [this, logicActionLayout](const QString &text, int row, int column) {
        auto *button = new QPushButton(text, this);
        logicActionLayout->addWidget(button, row, column);
        connect(button, &QPushButton::clicked, this, [this, text]() {
            if (text == QStringLiteral("AGC/AVC") && m_logicAgcAvcPage) {
                refreshLogicAgcAvcPage();
                m_mainTabWidget->setCurrentWidget(m_logicAgcAvcPage);
                return;
            }
            if (text.contains(QStringLiteral("算")) && m_logicComputationPointPage) {
                refreshLogicComputationPointPage();
                m_mainTabWidget->setCurrentWidget(m_logicComputationPointPage);
                return;
            }
            if (text == QStringLiteral("控制转换") && m_logicControlRulePage) {
                refreshLogicControlRulePage();
                m_mainTabWidget->setCurrentWidget(m_logicControlRulePage);
                return;
            }
            if (text == QStringLiteral("在线联动") && m_logicOnlineLinkPage) {
                refreshLogicOnlineLinkPage();
                m_mainTabWidget->setCurrentWidget(m_logicOnlineLinkPage);
                return;
            }
            statusBar()->showMessage(QStringLiteral("%1 编辑器将在后续步骤接入").arg(text), 5000);
        });
        return button;
    };
    addLogicAction(QStringLiteral("快速配置"), 0, 0);
    addLogicAction(QStringLiteral("AGC/AVC"), 0, 1);
    addLogicAction(QStringLiteral("虚拟设备派生"), 0, 2);
    addLogicAction(QStringLiteral("计算点"), 0, 3);
    addLogicAction(QStringLiteral("控制转换"), 1, 0);
    addLogicAction(QStringLiteral("在线联动"), 1, 1);
    addLogicAction(QStringLiteral("高级与调试"), 1, 2);
    addLogicAction(QStringLiteral("JSON 预览与校验"), 1, 3);
    logicLayout->addWidget(logicActionFrame);

    auto *issueTitle = new QLabel(QStringLiteral("校验问题"), this);
    QFont issueTitleFont = issueTitle->font();
    issueTitleFont.setBold(true);
    issueTitle->setFont(issueTitleFont);
    logicLayout->addWidget(issueTitle);
    m_logicIssueTable = new QTableWidget(0, 4, this);
    m_logicIssueTable->setHorizontalHeaderLabels({
        QStringLiteral("级别"),
        QStringLiteral("模块"),
        QStringLiteral("对象"),
        QStringLiteral("说明")
    });
    m_logicIssueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logicIssueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logicIssueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logicIssueTable->setAlternatingRowColors(true);
    m_logicIssueTable->verticalHeader()->setVisible(false);
    m_logicIssueTable->horizontalHeader()->setStretchLastSection(true);
    m_logicIssueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logicIssueTable->setColumnWidth(0, 80);
    m_logicIssueTable->setColumnWidth(1, 100);
    m_logicIssueTable->setColumnWidth(2, 260);
    logicLayout->addWidget(m_logicIssueTable, 1);

    m_logicAgcAvcPage = new QWidget(this);
    auto *agcAvcLayout = new QVBoxLayout(m_logicAgcAvcPage);
    agcAvcLayout->setContentsMargins(0, 0, 0, 0);
    agcAvcLayout->setSpacing(8);

    auto *agcAvcBasicFrame = new QFrame(this);
    agcAvcBasicFrame->setFrameShape(QFrame::StyledPanel);
    auto *agcAvcBasicLayout = new QGridLayout(agcAvcBasicFrame);
    agcAvcBasicLayout->setContentsMargins(10, 8, 10, 8);
    agcAvcBasicLayout->setHorizontalSpacing(10);
    agcAvcBasicLayout->setVerticalSpacing(6);
    m_logicAgcAvcGroupIdEdit = new QLineEdit(this);
    m_logicAgcAvcVirtualDeviceIdEdit = new QLineEdit(this);
    m_logicMeasurementTotalPEdit = new QDoubleSpinBox(this);
    m_logicMeasurementTotalQEdit = new QDoubleSpinBox(this);
    for (QDoubleSpinBox *spin : {m_logicMeasurementTotalPEdit, m_logicMeasurementTotalQEdit}) {
        spin->setDecimals(6);
        spin->setRange(-1000000000.0, 1000000000.0);
        spin->setValue(1.0);
    }
    agcAvcBasicLayout->addWidget(new QLabel(QStringLiteral("组 ID:"), this), 0, 0);
    agcAvcBasicLayout->addWidget(m_logicAgcAvcGroupIdEdit, 0, 1);
    agcAvcBasicLayout->addWidget(new QLabel(QStringLiteral("虚拟设备:"), this), 0, 2);
    agcAvcBasicLayout->addWidget(m_logicAgcAvcVirtualDeviceIdEdit, 0, 3);
    agcAvcBasicLayout->addWidget(new QLabel(QStringLiteral("总有功缩放:"), this), 1, 0);
    agcAvcBasicLayout->addWidget(m_logicMeasurementTotalPEdit, 1, 1);
    agcAvcBasicLayout->addWidget(new QLabel(QStringLiteral("总无功缩放:"), this), 1, 2);
    agcAvcBasicLayout->addWidget(m_logicMeasurementTotalQEdit, 1, 3);
    agcAvcLayout->addWidget(agcAvcBasicFrame);

    auto *agcAvcDeviceToolbar = new QHBoxLayout();
    agcAvcDeviceToolbar->addWidget(new QLabel(QStringLiteral("南向设备:"), this));
    m_addLogicAgcAvcDeviceBtn = new QPushButton(QStringLiteral("新增设备"), this);
    m_deleteLogicAgcAvcDeviceBtn = new QPushButton(QStringLiteral("删除设备"), this);
    agcAvcDeviceToolbar->addWidget(m_addLogicAgcAvcDeviceBtn);
    agcAvcDeviceToolbar->addWidget(m_deleteLogicAgcAvcDeviceBtn);
    m_generateLogicTotalPBtn = new QPushButton(QStringLiteral("生成 TotalP"), this);
    m_generateLogicTotalQBtn = new QPushButton(QStringLiteral("生成 TotalQ"), this);
    m_generateLogicCosBtn = new QPushButton(QStringLiteral("生成 Cos"), this);
    agcAvcDeviceToolbar->addSpacing(12);
    agcAvcDeviceToolbar->addWidget(m_generateLogicTotalPBtn);
    agcAvcDeviceToolbar->addWidget(m_generateLogicTotalQBtn);
    agcAvcDeviceToolbar->addWidget(m_generateLogicCosBtn);
    agcAvcDeviceToolbar->addStretch();
    agcAvcLayout->addLayout(agcAvcDeviceToolbar);

    m_logicAgcAvcDeviceTable = new QTableWidget(0, 12, this);
    m_logicAgcAvcDeviceTable->setHorizontalHeaderLabels({
        QStringLiteral("DeviceId"),
        QStringLiteral("P 控制点"),
        QStringLiteral("Q 控制点"),
        QStringLiteral("设备在线判断ID"),
        QStringLiteral("设备在线判定点位"),
        QStringLiteral("设备在线判定值"),
        QStringLiteral("Pmin"),
        QStringLiteral("Pmax"),
        QStringLiteral("Qmin"),
        QStringLiteral("Qmax"),
        QStringLiteral("scaleP"),
        QStringLiteral("scaleQ")
    });
    m_logicAgcAvcDeviceTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_logicAgcAvcDeviceTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_logicAgcAvcDeviceTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_logicAgcAvcDeviceTable->setAlternatingRowColors(true);
    m_logicAgcAvcDeviceTable->verticalHeader()->setVisible(false);
    m_logicAgcAvcDeviceTable->horizontalHeader()->setStretchLastSection(true);
    m_logicAgcAvcDeviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logicAgcAvcDeviceTable->setColumnWidth(0, 120);
    m_logicAgcAvcDeviceTable->setColumnWidth(1, 260);
    m_logicAgcAvcDeviceTable->setColumnWidth(2, 260);
    m_logicAgcAvcDeviceTable->setColumnWidth(3, 120);
    m_logicAgcAvcDeviceTable->setColumnWidth(4, 220);
    agcAvcLayout->addWidget(m_logicAgcAvcDeviceTable, 1);

    auto *onlineRuleHint = new QLabel(
        QStringLiteral("在线判定信息不填写时，以该 DeviceId 的通信状态作为状态判定；仅填写设备在线判断ID时，以填写的设备ID的通信状态作为状态判定；若填写了设备在线判定点位以及判定值，则以该点位是否等于判定值作为依据。"),
        this);
    onlineRuleHint->setWordWrap(true);
    onlineRuleHint->setStyleSheet(QStringLiteral("QLabel { color: #b0b0b0; }"));
    agcAvcLayout->addWidget(onlineRuleHint);

    auto *agcAvcOptionsPanel = new QWidget(this);
    auto *agcAvcOptionsLayout = new QHBoxLayout(agcAvcOptionsPanel);
    agcAvcOptionsLayout->setContentsMargins(0, 0, 0, 0);
    agcAvcOptionsLayout->setSpacing(8);

    auto *gateGroup = new QGroupBox(QStringLiteral("Gate 反相"), this);
    auto *gateLayout = new QGridLayout(gateGroup);
    m_logicGateEnableReverseCheck = new QCheckBox(QStringLiteral("投退"), this);
    m_logicGateDistantReverseCheck = new QCheckBox(QStringLiteral("远方"), this);
    m_logicGateLockReverseCheck = new QCheckBox(QStringLiteral("总闭锁"), this);
    m_logicGateUplockReverseCheck = new QCheckBox(QStringLiteral("增闭锁"), this);
    m_logicGateDownlockReverseCheck = new QCheckBox(QStringLiteral("减闭锁"), this);
    m_logicGateOpenloopReverseCheck = new QCheckBox(QStringLiteral("开环"), this);
    gateLayout->addWidget(m_logicGateEnableReverseCheck, 0, 0);
    gateLayout->addWidget(m_logicGateDistantReverseCheck, 0, 1);
    gateLayout->addWidget(m_logicGateLockReverseCheck, 0, 2);
    gateLayout->addWidget(m_logicGateUplockReverseCheck, 1, 0);
    gateLayout->addWidget(m_logicGateDownlockReverseCheck, 1, 1);
    gateLayout->addWidget(m_logicGateOpenloopReverseCheck, 1, 2);
    agcAvcOptionsLayout->addWidget(gateGroup, 1);

    auto makeFollowGroup = [this](const QString &title,
                                  QCheckBox **enableCheck,
                                  QSpinBox **periodEdit,
                                  QDoubleSpinBox **stepEdit,
                                  QDoubleSpinBox **toleranceEdit) {
        auto *group = new QGroupBox(title, this);
        auto *layout = new QGridLayout(group);
        *enableCheck = new QCheckBox(QStringLiteral("启用"), this);
        *periodEdit = new QSpinBox(this);
        (*periodEdit)->setRange(500, 3600000);
        (*periodEdit)->setSingleStep(500);
        (*periodEdit)->setSuffix(QStringLiteral(" ms"));
        *stepEdit = new QDoubleSpinBox(this);
        (*stepEdit)->setRange(1.0, 1000000000.0);
        (*stepEdit)->setDecimals(3);
        *toleranceEdit = new QDoubleSpinBox(this);
        (*toleranceEdit)->setRange(0.0, 1.0);
        (*toleranceEdit)->setDecimals(4);
        (*toleranceEdit)->setSingleStep(0.01);
        layout->addWidget(*enableCheck, 0, 0, 1, 2);
        layout->addWidget(new QLabel(QStringLiteral("周期:"), this), 1, 0);
        layout->addWidget(*periodEdit, 1, 1);
        layout->addWidget(new QLabel(QStringLiteral("步长:"), this), 2, 0);
        layout->addWidget(*stepEdit, 2, 1);
        layout->addWidget(new QLabel(QStringLiteral("容差:"), this), 3, 0);
        layout->addWidget(*toleranceEdit, 3, 1);
        return group;
    };
    agcAvcOptionsLayout->addWidget(makeFollowGroup(QStringLiteral("AGC 跟随"),
                                                   &m_logicAgcFollowEnableCheck,
                                                   &m_logicAgcFollowPeriodEdit,
                                                   &m_logicAgcFollowStepEdit,
                                                   &m_logicAgcFollowToleranceEdit), 1);
    agcAvcOptionsLayout->addWidget(makeFollowGroup(QStringLiteral("AVC 跟随"),
                                                   &m_logicAvcFollowEnableCheck,
                                                   &m_logicAvcFollowPeriodEdit,
                                                   &m_logicAvcFollowStepEdit,
                                                   &m_logicAvcFollowToleranceEdit), 1);
    agcAvcLayout->addWidget(agcAvcOptionsPanel);

    m_logicComputationPointPage = new QWidget(this);
    auto *logicComputationLayout = new QVBoxLayout(m_logicComputationPointPage);
    logicComputationLayout->setContentsMargins(0, 0, 0, 0);
    logicComputationLayout->setSpacing(8);
    auto *logicComputationHint = new QLabel(
        QStringLiteral("计算点模板生成结果会出现在这里。AGC/AVC 页可快速创建 TotalP、TotalQ 和 Cos。"),
        this);
    logicComputationHint->setWordWrap(true);
    logicComputationLayout->addWidget(logicComputationHint);
    auto *logicTemplateGroup = new QGroupBox(QStringLiteral("模板生成"), this);
    auto *logicTemplateLayout = new QGridLayout(logicTemplateGroup);
    logicTemplateLayout->setContentsMargins(10, 8, 10, 8);
    logicTemplateLayout->setHorizontalSpacing(8);
    logicTemplateLayout->setVerticalSpacing(8);
    const QStringList logicTemplateButtons = {
        QStringLiteral("单点映射/改名"),
        QStringLiteral("原点缩放"),
        QStringLiteral("遥信 OR"),
        QStringLiteral("遥信 AND")
    };
    for (int index = 0; index < logicTemplateButtons.size(); ++index) {
        auto *button = new QPushButton(logicTemplateButtons.at(index), this);
        button->setMinimumHeight(32);
        logicTemplateLayout->addWidget(button, index / 3, index % 3);
        connect(button, &QPushButton::clicked, this, [this, index]() {
            generateLogicComputationTemplate(index);
        });
    }
    logicComputationLayout->addWidget(logicTemplateGroup);

    auto *logicComputationToolbar = new QHBoxLayout();
    m_addLogicComputationPointBtn = new QPushButton(QStringLiteral("新增"), this);
    m_copyLogicComputationPointBtn = new QPushButton(QStringLiteral("复制"), this);
    m_deleteLogicComputationPointBtn = new QPushButton(QStringLiteral("删除"), this);
    logicComputationToolbar->addWidget(m_addLogicComputationPointBtn);
    logicComputationToolbar->addWidget(m_copyLogicComputationPointBtn);
    logicComputationToolbar->addWidget(m_deleteLogicComputationPointBtn);
    logicComputationToolbar->addStretch();
    logicComputationLayout->addLayout(logicComputationToolbar);
    m_logicComputationPointTable = new QTableWidget(0, 6, this);
    m_logicComputationPointTable->setHorizontalHeaderLabels({
        QStringLiteral("输出设备"),
        QStringLiteral("输出点"),
        QStringLiteral("公式"),
        QStringLiteral("剔除源点"),
        QStringLiteral("源点"),
        QStringLiteral("说明")
    });
    m_logicComputationPointTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                                  | QAbstractItemView::SelectedClicked
                                                  | QAbstractItemView::EditKeyPressed);
    m_logicComputationPointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logicComputationPointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logicComputationPointTable->setDragEnabled(true);
    m_logicComputationPointTable->setAcceptDrops(true);
    m_logicComputationPointTable->setDropIndicatorShown(true);
    m_logicComputationPointTable->setDragDropMode(QAbstractItemView::DragDrop);
    m_logicComputationPointTable->setDragDropOverwriteMode(false);
    m_logicComputationPointTable->setDefaultDropAction(Qt::CopyAction);
    m_logicComputationPointTable->viewport()->installEventFilter(this);
    m_logicComputationDropLine = new QFrame(m_logicComputationPointTable->viewport());
    m_logicComputationDropLine->setFixedHeight(3);
    m_logicComputationDropLine->setStyleSheet(QStringLiteral("background-color: #ff8c00; border-radius: 1px;"));
    m_logicComputationDropLine->hide();
    m_logicComputationPointTable->setAlternatingRowColors(true);
    m_logicComputationPointTable->verticalHeader()->setVisible(false);
    m_logicComputationPointTable->horizontalHeader()->setStretchLastSection(true);
    m_logicComputationPointTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logicComputationPointTable->setColumnWidth(0, 120);
    m_logicComputationPointTable->setColumnWidth(1, 240);
    m_logicComputationPointTable->setColumnWidth(2, 220);
    m_logicComputationPointTable->setColumnWidth(4, 360);
    logicComputationLayout->addWidget(m_logicComputationPointTable, 1);

    m_logicControlRulePage = new QWidget(this);
    auto *logicControlLayout = new QVBoxLayout(m_logicControlRulePage);
    logicControlLayout->setContentsMargins(0, 0, 0, 0);
    logicControlLayout->setSpacing(8);
    auto *logicControlHint = new QLabel(
        QStringLiteral("控制转换只面向遥控/遥调下发点位。规则仅按源设备和源控制点匹配，CtrlType 随北向 CtrlCmd 原样复用。"),
        this);
    logicControlHint->setWordWrap(true);
    logicControlLayout->addWidget(logicControlHint);

    auto *logicControlRuleToolbar = new QHBoxLayout();
    m_addLogicControlRuleBtn = new QPushButton(QStringLiteral("新增规则"), this);
    m_copyLogicControlRuleBtn = new QPushButton(QStringLiteral("复制规则"), this);
    m_deleteLogicControlRuleBtn = new QPushButton(QStringLiteral("删除规则"), this);
    logicControlRuleToolbar->addWidget(m_addLogicControlRuleBtn);
    logicControlRuleToolbar->addWidget(m_copyLogicControlRuleBtn);
    logicControlRuleToolbar->addWidget(m_deleteLogicControlRuleBtn);
    logicControlRuleToolbar->addStretch();
    logicControlLayout->addLayout(logicControlRuleToolbar);

    m_logicControlRuleTable = new QTableWidget(0, 4, this);
    m_logicControlRuleTable->setHorizontalHeaderLabels({
        QStringLiteral("源设备"),
        QStringLiteral("源控制点"),
        QStringLiteral("目标数"),
        QStringLiteral("说明")
    });
    m_logicControlRuleTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                             | QAbstractItemView::SelectedClicked
                                             | QAbstractItemView::EditKeyPressed);
    m_logicControlRuleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logicControlRuleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logicControlRuleTable->setAlternatingRowColors(true);
    m_logicControlRuleTable->verticalHeader()->setVisible(false);
    m_logicControlRuleTable->horizontalHeader()->setStretchLastSection(true);
    m_logicControlRuleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logicControlRuleTable->setColumnWidth(0, 120);
    m_logicControlRuleTable->setColumnWidth(1, 300);
    m_logicControlRuleTable->setColumnWidth(2, 80);
    logicControlLayout->addWidget(m_logicControlRuleTable, 1);

    auto *logicControlTargetGroup = new QGroupBox(QStringLiteral("目标动作"), this);
    auto *logicControlTargetLayout = new QVBoxLayout(logicControlTargetGroup);
    auto *logicControlTargetToolbar = new QHBoxLayout();
    m_addLogicControlTargetBtn = new QPushButton(QStringLiteral("新增目标"), this);
    m_deleteLogicControlTargetBtn = new QPushButton(QStringLiteral("删除目标"), this);
    m_logicControlTemplateOriginalBtn = new QPushButton(QStringLiteral("原值 {x}"), this);
    m_logicControlTemplateInvertBtn = new QPushButton(QStringLiteral("取反"), this);
    m_logicControlTemplateScaleBtn = new QPushButton(QStringLiteral("比例换算"), this);
    m_logicControlTemplateFixedBtn = new QPushButton(QStringLiteral("固定值"), this);
    m_insertLogicControlRealtimeRefBtn = new QPushButton(QStringLiteral("插入实时值"), this);
    logicControlTargetToolbar->addWidget(m_addLogicControlTargetBtn);
    logicControlTargetToolbar->addWidget(m_deleteLogicControlTargetBtn);
    logicControlTargetToolbar->addSpacing(16);
    logicControlTargetToolbar->addWidget(m_logicControlTemplateOriginalBtn);
    logicControlTargetToolbar->addWidget(m_logicControlTemplateInvertBtn);
    logicControlTargetToolbar->addWidget(m_logicControlTemplateScaleBtn);
    logicControlTargetToolbar->addWidget(m_logicControlTemplateFixedBtn);
    logicControlTargetToolbar->addWidget(m_insertLogicControlRealtimeRefBtn);
    logicControlTargetToolbar->addStretch();
    logicControlTargetLayout->addLayout(logicControlTargetToolbar);

    m_logicControlTargetTable = new QTableWidget(0, 5, this);
    m_logicControlTargetTable->setHorizontalHeaderLabels({
        QStringLiteral("类型"),
        QStringLiteral("目标设备"),
        QStringLiteral("目标点"),
        QStringLiteral("表达式"),
        QStringLiteral("预览")
    });
    m_logicControlTargetTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                               | QAbstractItemView::SelectedClicked
                                               | QAbstractItemView::EditKeyPressed);
    m_logicControlTargetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logicControlTargetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logicControlTargetTable->setAlternatingRowColors(true);
    m_logicControlTargetTable->verticalHeader()->setVisible(false);
    m_logicControlTargetTable->horizontalHeader()->setStretchLastSection(true);
    m_logicControlTargetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logicControlTargetTable->setColumnWidth(0, 110);
    m_logicControlTargetTable->setColumnWidth(1, 120);
    m_logicControlTargetTable->setColumnWidth(2, 300);
    m_logicControlTargetTable->setColumnWidth(3, 220);
    logicControlTargetLayout->addWidget(m_logicControlTargetTable, 1);

    auto *logicControlPreviewRow = new QHBoxLayout();
    logicControlPreviewRow->addWidget(new QLabel(QStringLiteral("模拟 CtrlVal:"), this));
    m_logicControlPreviewValueEdit = new QLineEdit(QStringLiteral("1"), this);
    m_logicControlPreviewValueEdit->setMaximumWidth(160);
    logicControlPreviewRow->addWidget(m_logicControlPreviewValueEdit);
    m_logicControlPreviewLabel = new QLabel(QStringLiteral("选择目标动作后显示表达式展开结果。"), this);
    m_logicControlPreviewLabel->setWordWrap(true);
    logicControlPreviewRow->addWidget(m_logicControlPreviewLabel, 1);
    logicControlTargetLayout->addLayout(logicControlPreviewRow);
    logicControlLayout->addWidget(logicControlTargetGroup, 1);

    m_logicOnlineLinkPage = new QWidget(this);
    auto *logicOnlineLayout = new QVBoxLayout(m_logicOnlineLinkPage);
    logicOnlineLayout->setContentsMargins(0, 0, 0, 0);
    logicOnlineLayout->setSpacing(8);
    auto *logicOnlineHint = new QLabel(
        QStringLiteral("在线状态联动用于让虚拟设备跟随真实设备的 DevUpdate 状态。双击设备列可从当前工程设备中选择。"),
        this);
    logicOnlineHint->setWordWrap(true);
    logicOnlineLayout->addWidget(logicOnlineHint);

    auto *logicOnlineToolbar = new QHBoxLayout();
    m_addLogicOnlineLinkBtn = new QPushButton(QStringLiteral("新增联动"), this);
    m_deleteLogicOnlineLinkBtn = new QPushButton(QStringLiteral("删除联动"), this);
    m_generateLogicVirtualOnlineLinksBtn = new QPushButton(QStringLiteral("虚拟设备跟随真实设备"), this);
    logicOnlineToolbar->addWidget(m_addLogicOnlineLinkBtn);
    logicOnlineToolbar->addWidget(m_deleteLogicOnlineLinkBtn);
    logicOnlineToolbar->addSpacing(12);
    logicOnlineToolbar->addWidget(m_generateLogicVirtualOnlineLinksBtn);
    logicOnlineToolbar->addStretch();
    logicOnlineLayout->addLayout(logicOnlineToolbar);

    m_logicOnlineLinkTable = new QTableWidget(0, 2, this);
    m_logicOnlineLinkTable->setHorizontalHeaderLabels({
        QStringLiteral("被联动设备 DeviceId"),
        QStringLiteral("跟随设备 LinkToDeviceId")
    });
    m_logicOnlineLinkTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                            | QAbstractItemView::SelectedClicked
                                            | QAbstractItemView::EditKeyPressed);
    m_logicOnlineLinkTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logicOnlineLinkTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logicOnlineLinkTable->setAlternatingRowColors(true);
    m_logicOnlineLinkTable->verticalHeader()->setVisible(false);
    m_logicOnlineLinkTable->horizontalHeader()->setStretchLastSection(true);
    m_logicOnlineLinkTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logicOnlineLinkTable->setColumnWidth(0, 220);
    m_logicOnlineLinkTable->setColumnWidth(1, 220);
    logicOnlineLayout->addWidget(m_logicOnlineLinkTable, 1);

    m_programControlPage = new QWidget(this);
    auto *programControlLayout = new QVBoxLayout(m_programControlPage);
    programControlLayout->setContentsMargins(0, 0, 0, 0);
    programControlLayout->setSpacing(8);

    auto *programConnectionRow = new QHBoxLayout();
    programConnectionRow->addWidget(new QLabel(QStringLiteral("设备:"), this));
    m_programRemoteHostEdit = new QLineEdit(QStringLiteral("192.168.7.10"), this);
    m_programRemoteHostEdit->setMinimumWidth(130);
    programConnectionRow->addWidget(m_programRemoteHostEdit);
    programConnectionRow->addWidget(new QLabel(QStringLiteral("用户:"), this));
    m_programRemoteUserEdit = new QLineEdit(QStringLiteral("root"), this);
    m_programRemoteUserEdit->setMaximumWidth(90);
    programConnectionRow->addWidget(m_programRemoteUserEdit);
    programConnectionRow->addWidget(new QLabel(QStringLiteral("密码:"), this));
    m_programRemotePasswordEdit = new QLineEdit(QStringLiteral("root"), this);
    m_programRemotePasswordEdit->setEchoMode(QLineEdit::Password);
    m_programRemotePasswordEdit->setMaximumWidth(90);
    programConnectionRow->addWidget(m_programRemotePasswordEdit);
    programConnectionRow->addWidget(new QLabel(QStringLiteral("端口:"), this));
    m_programRemotePortEdit = new QSpinBox(this);
    m_programRemotePortEdit->setRange(1, 65535);
    m_programRemotePortEdit->setValue(10022);
    m_programRemotePortEdit->setMaximumWidth(84);
    programConnectionRow->addWidget(m_programRemotePortEdit);
    programConnectionRow->addWidget(new QLabel(QStringLiteral("APP目录复用“配置概览”"), this));
    programConnectionRow->addStretch();
    m_connectProgramControlBtn = new QPushButton(QStringLiteral("连接"), this);
    m_disconnectProgramControlBtn = new QPushButton(QStringLiteral("断开"), this);
    m_disconnectProgramControlBtn->setEnabled(false);
    programConnectionRow->addWidget(m_connectProgramControlBtn);
    programConnectionRow->addWidget(m_disconnectProgramControlBtn);
    programControlLayout->addLayout(programConnectionRow);

    auto *programControlToolbar = new QHBoxLayout();
    programControlToolbar->addStretch();
    m_refreshProgramStatusBtn = new QPushButton(QStringLiteral("刷新状态"), this);
    m_refreshProgramStatusBtn->setEnabled(false);
    programControlToolbar->addWidget(m_refreshProgramStatusBtn);
    programControlLayout->addLayout(programControlToolbar);

    m_programControlTable = new QTableWidget(0, 7, this);
    m_programControlTable->setHorizontalHeaderLabels({
        QStringLiteral("状态"),
        QStringLiteral("APP"),
        QStringLiteral("PID"),
        QStringLiteral("命令"),
        QStringLiteral("启动"),
        QStringLiteral("停止"),
        QStringLiteral("重启")
    });
    m_programControlTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_programControlTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_programControlTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_programControlTable->setAlternatingRowColors(true);
    m_programControlTable->verticalHeader()->setVisible(false);
    m_programControlTable->horizontalHeader()->setStretchLastSection(true);
    m_programControlTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_programControlTable->setColumnWidth(0, 120);
    m_programControlTable->setColumnWidth(1, 180);
    m_programControlTable->setColumnWidth(2, 150);
    m_programControlTable->setColumnWidth(3, 360);
    m_programControlTable->setColumnWidth(4, 90);
    m_programControlTable->setColumnWidth(5, 150);
    m_programControlTable->setColumnWidth(6, 90);
    programControlLayout->addWidget(m_programControlTable, 1);

    m_mainTabWidget->addTab(debugPage, "调试控制");
    m_mainTabWidget->addTab(m_programControlPage, QStringLiteral("程序控制"));
    m_mainTabWidget->addTab(m_configPage, "配置概览");
    m_mainTabWidget->addTab(m_configIssuePage, "问题列表");
    m_mainTabWidget->addTab(m_modelEditorPage, "模型编辑器");
    m_mainTabWidget->addTab(m_deviceEditorPage, "设备编辑器");
    m_mainTabWidget->addTab(m_logicCenterPage, "逻辑中心");
    m_mainTabWidget->addTab(m_logicAgcAvcPage, "AGC/AVC");
    m_mainTabWidget->addTab(m_logicComputationPointPage, "计算点");
    m_mainTabWidget->addTab(m_logicControlRulePage, "控制转换");
    m_mainTabWidget->addTab(m_logicOnlineLinkPage, "在线联动");

    setCentralWidget(central);

    m_statusLabel = new QLabel("未连接");
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addWidget(m_modelValidationLabel, 1);

    m_versionLabel = new QLabel(QStringLiteral("v") + QStringLiteral(APP_VERSION), this);
    m_versionLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    statusBar()->addPermanentWidget(m_versionLabel);

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
    connect(m_exportIec104ConfigBtn, &QPushButton::clicked,
            this, &MainWindow::onExportIec104ConfigClicked);
    connect(m_uploadConfigBtn, &QPushButton::clicked,
            this, &MainWindow::onUploadConfigClicked);
    connect(m_downloadConfigBtn, &QPushButton::clicked,
            this, &MainWindow::onDownloadConfigClicked);
    connect(m_connectProgramControlBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectProgramControlClicked);
    connect(m_disconnectProgramControlBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectProgramControlClicked);
    connect(m_refreshProgramStatusBtn, &QPushButton::clicked,
            this, &MainWindow::onRefreshProgramStatusClicked);
    for (QLineEdit *edit : {m_logicAgcAvcGroupIdEdit, m_logicAgcAvcVirtualDeviceIdEdit}) {
        connect(edit, &QLineEdit::textEdited,
                this, &MainWindow::onLogicAgcAvcBasicEdited);
    }
    for (QDoubleSpinBox *spin : {m_logicMeasurementTotalPEdit,
                                 m_logicMeasurementTotalQEdit,
                                 m_logicAgcFollowStepEdit,
                                 m_logicAgcFollowToleranceEdit,
                                 m_logicAvcFollowStepEdit,
                                 m_logicAvcFollowToleranceEdit}) {
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this](double) { onLogicAgcAvcBasicEdited(); });
    }
    for (QSpinBox *spin : {m_logicAgcFollowPeriodEdit, m_logicAvcFollowPeriodEdit}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int) { onLogicAgcAvcBasicEdited(); });
    }
    for (QCheckBox *check : {m_logicGateEnableReverseCheck,
                             m_logicGateDistantReverseCheck,
                             m_logicGateLockReverseCheck,
                             m_logicGateUplockReverseCheck,
                             m_logicGateDownlockReverseCheck,
                             m_logicGateOpenloopReverseCheck,
                             m_logicAgcFollowEnableCheck,
                             m_logicAvcFollowEnableCheck}) {
        connect(check, &QCheckBox::toggled,
                this, [this](bool) { onLogicAgcAvcBasicEdited(); });
    }
    connect(m_logicAgcAvcDeviceTable, &QTableWidget::itemChanged,
            this, &MainWindow::onLogicAgcAvcDeviceItemChanged);
    connect(m_logicAgcAvcDeviceTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onLogicAgcAvcDeviceCellDoubleClicked);
    connect(m_addLogicAgcAvcDeviceBtn, &QPushButton::clicked,
            this, &MainWindow::onAddLogicAgcAvcDeviceClicked);
    connect(m_deleteLogicAgcAvcDeviceBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteLogicAgcAvcDeviceClicked);
    connect(m_generateLogicTotalPBtn, &QPushButton::clicked,
            this, &MainWindow::onGenerateLogicAgcAvcTotalPClicked);
    connect(m_generateLogicTotalQBtn, &QPushButton::clicked,
            this, &MainWindow::onGenerateLogicAgcAvcTotalQClicked);
    connect(m_generateLogicCosBtn, &QPushButton::clicked,
            this, &MainWindow::onGenerateLogicAgcAvcCosClicked);
    connect(m_addLogicComputationPointBtn, &QPushButton::clicked,
            this, &MainWindow::onAddLogicComputationPointClicked);
    connect(m_copyLogicComputationPointBtn, &QPushButton::clicked,
            this, &MainWindow::onCopyLogicComputationPointClicked);
    connect(m_deleteLogicComputationPointBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteLogicComputationPointClicked);
    connect(m_logicComputationPointTable, &QTableWidget::itemChanged,
            this, &MainWindow::onLogicComputationPointItemChanged);
    connect(m_logicComputationPointTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onLogicComputationPointCellDoubleClicked);
    connect(m_addLogicControlRuleBtn, &QPushButton::clicked,
            this, &MainWindow::onAddLogicControlRuleClicked);
    connect(m_copyLogicControlRuleBtn, &QPushButton::clicked,
            this, &MainWindow::onCopyLogicControlRuleClicked);
    connect(m_deleteLogicControlRuleBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteLogicControlRuleClicked);
    connect(m_addLogicControlTargetBtn, &QPushButton::clicked,
            this, &MainWindow::onAddLogicControlTargetClicked);
    connect(m_deleteLogicControlTargetBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteLogicControlTargetClicked);
    connect(m_logicControlRuleTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onLogicControlRuleSelectionChanged);
    connect(m_logicControlRuleTable, &QTableWidget::itemChanged,
            this, &MainWindow::onLogicControlRuleItemChanged);
    connect(m_logicControlRuleTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onLogicControlRuleCellDoubleClicked);
    connect(m_logicControlTargetTable, &QTableWidget::itemChanged,
            this, &MainWindow::onLogicControlTargetItemChanged);
    connect(m_logicControlTargetTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onLogicControlTargetCellDoubleClicked);
    connect(m_logicControlTargetTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::refreshLogicControlPreview);
    connect(m_logicControlPreviewValueEdit, &QLineEdit::textChanged,
            this, &MainWindow::onLogicControlPreviewEdited);
    for (QPushButton *button : {m_logicControlTemplateOriginalBtn,
                                m_logicControlTemplateInvertBtn,
                                m_logicControlTemplateScaleBtn,
                                m_logicControlTemplateFixedBtn}) {
        connect(button, &QPushButton::clicked,
                this, &MainWindow::onLogicControlTemplateClicked);
    }
    connect(m_insertLogicControlRealtimeRefBtn, &QPushButton::clicked,
            this, &MainWindow::onInsertLogicControlRealtimeRefClicked);
    connect(m_addLogicOnlineLinkBtn, &QPushButton::clicked,
            this, &MainWindow::onAddLogicOnlineLinkClicked);
    connect(m_deleteLogicOnlineLinkBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteLogicOnlineLinkClicked);
    connect(m_generateLogicVirtualOnlineLinksBtn, &QPushButton::clicked,
            this, &MainWindow::onGenerateLogicVirtualOnlineLinksClicked);
    connect(m_logicOnlineLinkTable, &QTableWidget::itemChanged,
            this, &MainWindow::onLogicOnlineLinkItemChanged);
    connect(m_logicOnlineLinkTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onLogicOnlineLinkCellDoubleClicked);
    connect(m_newModelBtn, &QPushButton::clicked,
            this, &MainWindow::onNewModelClicked);
    connect(m_createDeviceFromModelBtn, &QPushButton::clicked,
            this, &MainWindow::onCreateDeviceFromModelClicked);
    connect(m_deleteModelBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteModelClicked);
    connect(m_deleteDeviceBtn, &QPushButton::clicked,
            this, &MainWindow::onDeleteDeviceClicked);
    connect(m_configModelTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onConfigModelSelectionChanged);
    connect(m_configDeviceTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onConfigDeviceSelectionChanged);
    connect(m_configModelTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onConfigModelActivated);
    connect(m_configDeviceTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onConfigDeviceActivated);
    connect(m_configIssueTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onConfigIssueActivated);
    connect(m_checkConfigIssuesBtn, &QPushButton::clicked,
            this, &MainWindow::onCheckConfigIssuesClicked);
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
    connect(m_modelNorthVisibleCheck, &QCheckBox::toggled,
            this, &MainWindow::onModelFieldEdited);
    connect(m_addPointBtn, &QPushButton::clicked,
            this, &MainWindow::onAddPointClicked);
    connect(m_copyPointBtn, &QPushButton::clicked,
            this, &MainWindow::onCopyPointClicked);
    connect(m_deletePointBtn, &QPushButton::clicked,
            this, &MainWindow::onDeletePointClicked);
    connect(m_modelPointFilterTabBar, &QTabBar::currentChanged,
            this, &MainWindow::onModelPointFilterChanged);
    connect(m_modelPointDataRefFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onModelPointDataRefFilterTextChanged);
    connect(m_modelPointDescriptionFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onModelPointDescriptionFilterTextChanged);
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
    connect(m_modbusTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusSerialPortCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusHwVariantCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusBaudCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusDataBitsCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusStopBitsCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusParityCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_modbusDebugCheck, &QCheckBox::toggled,
            this, &MainWindow::onDeviceFieldEdited);
    connect(m_deviceBindingDataRefFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onDeviceBindingDataRefFilterTextChanged);
    connect(m_deviceBindingDescriptionFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onDeviceBindingDescriptionFilterTextChanged);
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
    connect(new QShortcut(QKeySequence::Paste, m_modelPointsTable), &QShortcut::activated,
            this, [this]() { pasteClipboardIntoModelPointsTable(); });
    connect(new QShortcut(QKeySequence::Paste, m_deviceBindingsTable), &QShortcut::activated,
            this, [this]() { pasteClipboardIntoDeviceBindingsTable(); });
    connect(new QShortcut(QKeySequence::Undo, m_modelPointsTable), &QShortcut::activated,
            this, [this]() { undoLastConfigEdit(); });
    connect(new QShortcut(QKeySequence::Undo, m_deviceBindingsTable), &QShortcut::activated,
            this, [this]() { undoLastConfigEdit(); });
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
    refreshProgramControlTable(QString());
    refreshLogicCenterOverview();
}

MainWindow::~MainWindow()
{
    closeProgramControlShell();
}
