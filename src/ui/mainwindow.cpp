#include "mainwindow.h"

#include <QApplication>
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
#include <QPalette>
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

namespace {

QPalette lightThemePalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(245, 246, 248));
    palette.setColor(QPalette::WindowText, QColor(28, 31, 35));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(239, 242, 246));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(28, 31, 35));
    palette.setColor(QPalette::Text, QColor(28, 31, 35));
    palette.setColor(QPalette::Button, QColor(245, 246, 248));
    palette.setColor(QPalette::ButtonText, QColor(28, 31, 35));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(27, 94, 163));
    palette.setColor(QPalette::Highlight, QColor(36, 116, 191));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(125, 132, 141));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(125, 132, 141));
    return palette;
}

QPalette darkThemePalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(35, 38, 42));
    palette.setColor(QPalette::WindowText, QColor(232, 235, 239));
    palette.setColor(QPalette::Base, QColor(27, 30, 34));
    palette.setColor(QPalette::AlternateBase, QColor(42, 46, 51));
    palette.setColor(QPalette::ToolTipBase, QColor(48, 52, 58));
    palette.setColor(QPalette::ToolTipText, QColor(232, 235, 239));
    palette.setColor(QPalette::Text, QColor(232, 235, 239));
    palette.setColor(QPalette::Button, QColor(45, 49, 55));
    palette.setColor(QPalette::ButtonText, QColor(232, 235, 239));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(114, 173, 232));
    palette.setColor(QPalette::Highlight, QColor(64, 128, 191));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(135, 141, 149));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(135, 141, 149));
    return palette;
}

bool paletteLooksDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

QString themeStyleSheet(bool darkMode)
{
    if (darkMode) {
        return QStringLiteral(
            "QWidget {"
            "  background-color: #23272e;"
            "  color: #e8edf3;"
            "  selection-background-color: #2f80d0;"
            "  selection-color: #ffffff;"
            "}"
            "QMainWindow, QDialog { background-color: #20242b; }"
            "QLabel { background: transparent; color: #dce3ec; }"
            "QLabel#onlineRuleHint { color: #aeb9c7; }"
            "QStatusBar {"
            "  background-color: #1b1f25;"
            "  border-top: 1px solid #343a44;"
            "  color: #c9d2dd;"
            "}"
            "QTabWidget::pane {"
            "  background-color: #272c34;"
            "  border: 1px solid #3b424d;"
            "  border-radius: 4px;"
            "  top: -1px;"
            "}"
            "QTabBar::tab {"
            "  background-color: #20242b;"
            "  color: #b9c4d1;"
            "  border: 1px solid #343b46;"
            "  border-bottom: none;"
            "  padding: 7px 14px;"
            "  margin-right: 2px;"
            "  border-top-left-radius: 4px;"
            "  border-top-right-radius: 4px;"
            "}"
            "QTabBar::tab:selected {"
            "  background-color: #2e3540;"
            "  color: #ffffff;"
            "  border-color: #4d8fd5;"
            "}"
            "QTabBar::tab:hover:!selected { background-color: #29303a; color: #edf3f9; }"
            "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {"
            "  background-color: #151a20;"
            "  color: #f0f4f8;"
            "  border: 1px solid #566171;"
            "  border-radius: 3px;"
            "  padding: 4px 7px;"
            "}"
            "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,"
            "QTextEdit:focus, QPlainTextEdit:focus {"
            "  border: 1px solid #62a8ee;"
            "  background-color: #11161c;"
            "}"
            "QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {"
            "  background-color: #252b33;"
            "  color: #8994a2;"
            "  border-color: #3b424d;"
            "}"
            "QComboBox { padding-right: 28px; }"
            "QComboBox::drop-down {"
            "  subcontrol-origin: padding;"
            "  subcontrol-position: top right;"
            "  width: 26px;"
            "  border-left: 1px solid #566171;"
            "  background-color: #202630;"
            "  border-top-right-radius: 3px;"
            "  border-bottom-right-radius: 3px;"
            "}"
            "QComboBox::down-arrow {"
            "  image: url(:/theme/icons/chevron-down-light.svg);"
            "  width: 10px;"
            "  height: 10px;"
            "  margin-right: 8px;"
            "}"
            "QComboBox::drop-down:disabled { background-color: #252b33; border-left-color: #3b424d; }"
            "QComboBox::down-arrow:disabled { border-top-color: #7f8a98; }"
            "QComboBox#modelPointCategoryCombo, QComboBox#modbusPointKindCombo {"
            "  padding: 2px 19px 2px 5px;"
            "  min-height: 20px;"
            "}"
            "QComboBox#modelPointCategoryCombo::drop-down, QComboBox#modbusPointKindCombo::drop-down { width: 18px; }"
            "QComboBox#modelPointCategoryCombo::down-arrow, QComboBox#modbusPointKindCombo::down-arrow {"
            "  width: 9px;"
            "  height: 9px;"
            "  margin-right: 5px;"
            "}"
            "QSpinBox, QDoubleSpinBox { padding-right: 23px; }"
            "QSpinBox::up-button, QDoubleSpinBox::up-button,"
            "QSpinBox::down-button, QDoubleSpinBox::down-button {"
            "  subcontrol-origin: border;"
            "  width: 19px;"
            "  background-color: #202630;"
            "  border-left: 1px solid #566171;"
            "}"
            "QSpinBox::up-button, QDoubleSpinBox::up-button {"
            "  subcontrol-position: top right;"
            "  border-top-right-radius: 3px;"
            "  border-bottom: 1px solid #404956;"
            "}"
            "QSpinBox::down-button, QDoubleSpinBox::down-button {"
            "  subcontrol-position: bottom right;"
            "  border-bottom-right-radius: 3px;"
            "}"
            "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,"
            "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {"
            "  background-color: #2a6aa8;"
            "}"
            "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
            "  image: url(:/theme/icons/spin-up-light.svg);"
            "  width: 8px;"
            "  height: 8px;"
            "}"
            "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
            "  image: url(:/theme/icons/spin-down-light.svg);"
            "  width: 8px;"
            "  height: 8px;"
            "}"
            "QPushButton {"
            "  background-color: #35404c;"
            "  color: #f0f4f8;"
            "  border: 1px solid #596779;"
            "  border-radius: 4px;"
            "  padding: 5px 13px;"
            "  min-height: 22px;"
            "}"
            "QPushButton:hover { background-color: #405065; border-color: #77a9dd; }"
            "QPushButton:pressed { background-color: #2b6fb3; border-color: #7cb8f2; }"
            "QPushButton:disabled {"
            "  background-color: #2a3038;"
            "  color: #7f8a98;"
            "  border-color: #3b434e;"
            "}"
            "QPushButton:flat {"
            "  background: transparent;"
            "  border: 1px solid transparent;"
            "  color: #d8e1eb;"
            "}"
            "QPushButton:flat:hover { background-color: #303844; border-color: #4d5968; }"
            "QPushButton#programControlCellButton {"
            "  min-height: 0;"
            "  padding: 2px 10px;"
            "}"
            "QTableWidget, QTableView {"
            "  background-color: #171c22;"
            "  alternate-background-color: #202630;"
            "  color: #edf3f9;"
            "  gridline-color: #343c47;"
            "  border: 1px solid #3b424d;"
            "  border-radius: 3px;"
            "}"
            "QHeaderView::section {"
            "  background-color: #303844;"
            "  color: #f3f7fb;"
            "  border: none;"
            "  border-right: 1px solid #444d5a;"
            "  border-bottom: 1px solid #4a5360;"
            "  padding: 6px 8px;"
            "  font-weight: 600;"
            "}"
            "QTableWidget::item { padding: 4px 6px; }"
            "QTableWidget::item:selected, QTableView::item:selected {"
            "  background-color: #2f80d0;"
            "  color: #ffffff;"
            "}"
            "QGroupBox, QFrame[frameShape=\"6\"] {"
            "  background-color: #272d36;"
            "  border: 1px solid #3f4855;"
            "  border-radius: 4px;"
            "}"
            "QGroupBox {"
            "  margin-top: 18px;"
            "  padding: 10px 8px 8px 8px;"
            "  font-weight: 600;"
            "}"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
            "QSplitter::handle { background-color: #3d4652; }"
            "QScrollBar:vertical, QScrollBar:horizontal { background-color: #20242b; }"
            "QScrollBar::handle { background-color: #4b5665; border-radius: 3px; }"
            "QToolTip {"
            "  background-color: #11161c;"
            "  color: #f0f4f8;"
            "  border: 1px solid #596779;"
            "}"
        );
    }

    return QStringLiteral(
        "QWidget {"
        "  background-color: #eef2f6;"
        "  color: #20252b;"
        "  selection-background-color: #1f6fb8;"
        "  selection-color: #ffffff;"
        "}"
        "QMainWindow, QDialog { background-color: #e8edf3; }"
        "QLabel { background: transparent; color: #20252b; }"
        "QLabel#onlineRuleHint { color: #52606f; }"
        "QStatusBar {"
        "  background-color: #f7f9fb;"
        "  border-top: 1px solid #c8d1dc;"
        "  color: #38434f;"
        "}"
        "QTabWidget::pane {"
        "  background-color: #ffffff;"
        "  border: 1px solid #c7d0dc;"
        "  border-radius: 4px;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background-color: #dfe6ee;"
        "  color: #34404d;"
        "  border: 1px solid #c3ccd8;"
        "  border-bottom: none;"
        "  padding: 7px 14px;"
        "  margin-right: 2px;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #ffffff;"
        "  color: #111820;"
        "  border-color: #3e7db8;"
        "}"
        "QTabBar::tab:hover:!selected { background-color: #edf3f8; color: #111820; }"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {"
        "  background-color: #ffffff;"
        "  color: #111820;"
        "  border: 1px solid #9aa8b8;"
        "  border-radius: 3px;"
        "  padding: 4px 7px;"
        "}"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,"
        "QTextEdit:focus, QPlainTextEdit:focus {"
        "  border: 1px solid #1f6fb8;"
        "  background-color: #ffffff;"
        "}"
        "QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {"
        "  background-color: #e3e9f0;"
        "  color: #667282;"
        "  border-color: #c1cad6;"
        "}"
        "QComboBox { padding-right: 28px; }"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 26px;"
        "  border-left: 1px solid #a8b5c4;"
        "  background-color: #eef3f8;"
        "  border-top-right-radius: 3px;"
        "  border-bottom-right-radius: 3px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(:/theme/icons/chevron-down-dark.svg);"
        "  width: 10px;"
        "  height: 10px;"
        "  margin-right: 8px;"
        "}"
        "QComboBox::drop-down:disabled { background-color: #e3e9f0; border-left-color: #c1cad6; }"
        "QComboBox::down-arrow:disabled { border-top-color: #8c98a7; }"
        "QComboBox#modelPointCategoryCombo, QComboBox#modbusPointKindCombo {"
        "  padding: 2px 19px 2px 5px;"
        "  min-height: 20px;"
        "}"
        "QComboBox#modelPointCategoryCombo::drop-down, QComboBox#modbusPointKindCombo::drop-down { width: 18px; }"
        "QComboBox#modelPointCategoryCombo::down-arrow, QComboBox#modbusPointKindCombo::down-arrow {"
        "  width: 9px;"
        "  height: 9px;"
        "  margin-right: 5px;"
        "}"
        "QSpinBox, QDoubleSpinBox { padding-right: 23px; }"
        "QSpinBox::up-button, QDoubleSpinBox::up-button,"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        "  subcontrol-origin: border;"
        "  width: 19px;"
        "  background-color: #eef3f8;"
        "  border-left: 1px solid #a8b5c4;"
        "}"
        "QSpinBox::up-button, QDoubleSpinBox::up-button {"
        "  subcontrol-position: top right;"
        "  border-top-right-radius: 3px;"
        "  border-bottom: 1px solid #c8d1dc;"
        "}"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        "  subcontrol-position: bottom right;"
        "  border-bottom-right-radius: 3px;"
        "}"
        "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,"
        "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {"
        "  background-color: #d8e9f8;"
        "}"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
        "  image: url(:/theme/icons/spin-up-dark.svg);"
        "  width: 8px;"
        "  height: 8px;"
        "}"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
        "  image: url(:/theme/icons/spin-down-dark.svg);"
        "  width: 8px;"
        "  height: 8px;"
        "}"
        "QPushButton {"
        "  background-color: #f7f9fb;"
        "  color: #17202a;"
        "  border: 1px solid #95a6b8;"
        "  border-radius: 4px;"
        "  padding: 5px 13px;"
        "  min-height: 22px;"
        "}"
        "QPushButton:hover { background-color: #eaf3fb; border-color: #3e7db8; }"
        "QPushButton:pressed { background-color: #d4e7f8; border-color: #1f6fb8; }"
        "QPushButton:disabled {"
        "  background-color: #e7edf3;"
        "  color: #8c98a7;"
        "  border-color: #c8d1dc;"
        "}"
        "QPushButton:flat {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  color: #25313d;"
        "}"
        "QPushButton:flat:hover { background-color: #e2e9f0; border-color: #b4c0ce; }"
        "QPushButton#programControlCellButton {"
        "  min-height: 0;"
        "  padding: 2px 10px;"
        "}"
        "QTableWidget, QTableView {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f4f7fa;"
        "  color: #17202a;"
        "  gridline-color: #d5dde7;"
        "  border: 1px solid #c7d0dc;"
        "  border-radius: 3px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #dde6ef;"
        "  color: #17202a;"
        "  border: none;"
        "  border-right: 1px solid #c2ccd8;"
        "  border-bottom: 1px solid #b8c4d1;"
        "  padding: 6px 8px;"
        "  font-weight: 600;"
        "}"
        "QTableWidget::item { padding: 4px 6px; }"
        "QTableWidget::item:selected, QTableView::item:selected {"
        "  background-color: #1f6fb8;"
        "  color: #ffffff;"
        "}"
        "QGroupBox, QFrame[frameShape=\"6\"] {"
        "  background-color: #ffffff;"
        "  border: 1px solid #c7d0dc;"
        "  border-radius: 4px;"
        "}"
        "QGroupBox {"
        "  margin-top: 18px;"
        "  padding: 10px 8px 8px 8px;"
        "  font-weight: 600;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QSplitter::handle { background-color: #c9d3df; }"
        "QScrollBar:vertical, QScrollBar:horizontal { background-color: #e8edf3; }"
        "QScrollBar::handle { background-color: #a9b6c5; border-radius: 3px; }"
        "QToolTip {"
        "  background-color: #ffffff;"
        "  color: #111820;"
        "  border: 1px solid #95a6b8;"
        "}"
    );
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_client(new DebugConsoleClient(this))
    , m_autoRefreshTimer(new QTimer(this))
    , m_highlightRefreshTimer(new QTimer(this))
    , m_controlResponseTimer(new QTimer(this))
{
    m_highlightRefreshTimer->setInterval(500);
    m_controlResponseTimer->setSingleShot(true);
    m_controlResponseTimer->setInterval(15000);

    m_appConfigs = {
        {"ServiceChannel", 4444, "ServiceChannel>", AppViewMode::DataTable},
        {"IEC101ServiceChannel", 3333, "IEC101>", AppViewMode::DataTable},
        {"cepmodbus", 7777, "modbus>", AppViewMode::DataTable},
        {"cepiec104", 6666, "cepiec104>", AppViewMode::DataTable},
        {"LogicCenter", 5555, "LogicCenter>", AppViewMode::LogicAgcAvcTable}
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
    m_rawFrameLogBtn = new QPushButton(QStringLiteral("原始报文"));
    m_rawFrameLogBtn->setToolTip(QStringLiteral("单独打开原始报文 debugconsole 窗口"));
    m_rawFrameLogBtn->setVisible(false);
    topLayout->addWidget(m_rawFrameLogBtn);
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

    auto *dataToolbar = new QVBoxLayout();
    dataToolbar->setSpacing(6);
    auto *dataFilterToolbar = new QHBoxLayout();
    dataFilterToolbar->setSpacing(8);

    m_serviceDataFilterWidget = new QWidget(dataPage);
    auto *serviceDataFilterLayout = new QHBoxLayout(m_serviceDataFilterWidget);
    serviceDataFilterLayout->setContentsMargins(0, 0, 0, 0);
    serviceDataFilterLayout->setSpacing(8);
    serviceDataFilterLayout->addWidget(new QLabel("DeviceId:"));
    m_deviceFilterCombo = new QComboBox();
    m_deviceFilterCombo->addItem("all", QString());
    m_deviceFilterCombo->setMinimumContentsLength(18);
    m_deviceFilterCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_deviceFilterCombo->setMinimumWidth(180);
    m_deviceFilterCombo->setEnabled(false);
    serviceDataFilterLayout->addWidget(m_deviceFilterCombo);
    serviceDataFilterLayout->addWidget(new QLabel("Type:"));
    m_serviceTypeFilterCombo = new QComboBox();
    m_serviceTypeFilterCombo->addItem(QStringLiteral("all"), QString());
    m_serviceTypeFilterCombo->addItem(QStringLiteral("遥测"), QStringLiteral("analog"));
    m_serviceTypeFilterCombo->addItem(QStringLiteral("遥信"), QStringLiteral("discrete"));
    m_serviceTypeFilterCombo->addItem(QStringLiteral("电度"), QStringLiteral("accumulator"));
    m_serviceTypeFilterCombo->addItem(QStringLiteral("控制"), QStringLiteral("control"));
    m_serviceTypeFilterCombo->setMinimumWidth(90);
    m_serviceTypeFilterCombo->setEnabled(false);
    serviceDataFilterLayout->addWidget(m_serviceTypeFilterCombo);
    serviceDataFilterLayout->addWidget(new QLabel("DataRef:"));
    m_dataRefFilterEdit = new QLineEdit();
    m_dataRefFilterEdit->setPlaceholderText("输入 DataRef 关键字实时筛选...");
    m_dataRefFilterEdit->setClearButtonEnabled(true);
    m_dataRefFilterEdit->setMinimumWidth(220);
    m_dataRefFilterEdit->setEnabled(false);
    serviceDataFilterLayout->addWidget(m_dataRefFilterEdit, 1);
    serviceDataFilterLayout->addWidget(new QLabel("Description:"));
    m_descriptionFilterEdit = new QLineEdit();
    m_descriptionFilterEdit->setPlaceholderText("输入描述关键字实时筛选...");
    m_descriptionFilterEdit->setClearButtonEnabled(true);
    m_descriptionFilterEdit->setMinimumWidth(220);
    m_descriptionFilterEdit->setEnabled(false);
    serviceDataFilterLayout->addWidget(m_descriptionFilterEdit, 1);
    dataFilterToolbar->addWidget(m_serviceDataFilterWidget, 1);
    dataFilterToolbar->addStretch();
    dataFilterToolbar->addWidget(new QLabel("自动刷新:"));
    m_autoRefreshCombo = new QComboBox();
    m_autoRefreshCombo->addItem("关闭", 0);
    m_autoRefreshCombo->addItem("1 秒", 1000);
    m_autoRefreshCombo->addItem("2 秒", 2000);
    m_autoRefreshCombo->addItem("5 秒", 5000);
    m_autoRefreshCombo->addItem("10 秒", 10000);
    m_autoRefreshCombo->setCurrentIndex(0);
    m_autoRefreshCombo->setEnabled(false);
    dataFilterToolbar->addWidget(m_autoRefreshCombo);
    dataToolbar->addLayout(dataFilterToolbar);

    m_dataFreezeBtn = new QPushButton(QStringLiteral("冻结数据"));
    m_dataFreezeBtn->setMinimumWidth(118);
    m_dataFreezeBtn->setToolTip(QStringLiteral("发送 datafreeze on/off，冻结或恢复 DataSpont 更新内部值"));
    m_dataFreezeBtn->setEnabled(false);
    topLayout->addWidget(m_dataFreezeBtn);
    m_sendControlBtn = new QPushButton(QStringLiteral("发送控制"));
    m_sendControlBtn->setEnabled(false);
    topLayout->addWidget(m_sendControlBtn);
    m_refreshDataBtn = new QPushButton("刷新数据");
    m_refreshDataBtn->setEnabled(false);
    topLayout->addWidget(m_refreshDataBtn);
    dataLayout->addLayout(dataToolbar);

    m_dataViewStack = new QStackedWidget(dataPage);
    m_serviceDataViewPage = new QWidget(dataPage);
    auto *serviceDataViewLayout = new QVBoxLayout(m_serviceDataViewPage);
    serviceDataViewLayout->setContentsMargins(0, 0, 0, 0);
    serviceDataViewLayout->setSpacing(0);

    m_dataTable = new QTableWidget(0, 7);
    m_dataTable->setHorizontalHeaderLabels({"DeviceId", "DataRef", "ServiceId", "Description", "DataTime", "Value", "Status"});
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
    m_dataTable->setColumnWidth(2, 90);
    m_dataTable->setColumnWidth(3, 360);
    m_dataTable->setColumnWidth(4, 170);
    m_dataTable->setColumnWidth(5, 90);
    m_dataTable->setColumnWidth(6, 180);
    serviceDataViewLayout->addWidget(m_dataTable, 1);
    m_dataViewStack->addWidget(m_serviceDataViewPage);

    m_logicAgcAvcStatusTabs = new QTabWidget(dataPage);
    m_logicAgcAvcStatusTabs->setDocumentMode(true);
    m_dataViewStack->addWidget(m_logicAgcAvcStatusTabs);
    dataLayout->addWidget(m_dataViewStack, 1);

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
    m_configImportDirEdit->setPlaceholderText("选择工程根目录，例如包含 cepiec104、cepmodbus、cepdlt645、cepLogicCenter、IEC101ServiceChannel 的目录");
    importRow->addWidget(m_configImportDirEdit, 1);
    m_selectConfigImportDirBtn = new QPushButton(QStringLiteral("..."));
    m_selectConfigImportDirBtn->setToolTip(QStringLiteral("选择配置工程目录"));
    m_selectConfigImportDirBtn->setFixedWidth(48);
    importRow->addWidget(m_selectConfigImportDirBtn);
    m_browseConfigImportDirBtn = new QPushButton("打开配置");
    importRow->addWidget(m_browseConfigImportDirBtn);
    m_exportIec104ConfigBtn = new QPushButton("保存配置");
    importRow->addWidget(m_exportIec104ConfigBtn);
    configLayout->addLayout(importRow);

    auto *transferRow = new QHBoxLayout();
    transferRow->addWidget(new QLabel(QStringLiteral("设备:")));
    m_configRemoteHostEdit = new QLineEdit(QStringLiteral("192.168.7.10"), this);
    m_configRemoteHostEdit->setMinimumWidth(100);
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
    m_configRemotePortEdit->setMaximumWidth(120);
    transferRow->addWidget(m_configRemotePortEdit);

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
    auto *modelContentLayout = new QHBoxLayout();
    modelContentLayout->setSpacing(8);
    auto *modelToolbar = new QVBoxLayout();
    modelToolbar->setSpacing(8);
    m_newModelBtn = new QPushButton("新建模型");
    modelToolbar->addWidget(m_newModelBtn);
    m_createDeviceFromModelBtn = new QPushButton("由模型创建设备");
    modelToolbar->addWidget(m_createDeviceFromModelBtn);
    m_deleteModelBtn = new QPushButton("删除模型");
    m_deleteModelBtn->setEnabled(false);
    modelToolbar->addWidget(m_deleteModelBtn);
    modelToolbar->addStretch();
    modelContentLayout->addLayout(modelToolbar);
    m_configModelTable = new QTableWidget(0, 4, this);
    m_configModelTable->setHorizontalHeaderLabels({"模型", "展示名", "设备类型", "点位数"});
    m_configModelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_configModelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configModelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_configModelTable->verticalHeader()->setVisible(false);
    m_configModelTable->horizontalHeader()->setStretchLastSection(false);
    m_configModelTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_configModelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_configModelTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_configModelTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_configModelTable->setColumnWidth(3, 80);
    m_configModelTable->setStyleSheet(
        "QTableWidget::item:selected {"
        "  background-color: #355c7d;"
        "  color: #ffffff;"
        "}"
    );
    modelContentLayout->addWidget(m_configModelTable, 1);
    modelGroupLayout->addLayout(modelContentLayout, 1);
    objectPanelLayout->addWidget(m_modelGroupBox, 1);

    m_deviceGroupBox = new QGroupBox("设备列表", this);
    auto *deviceGroupLayout = new QVBoxLayout(m_deviceGroupBox);
    auto *deviceContentLayout = new QHBoxLayout();
    deviceContentLayout->setSpacing(8);
    auto *deviceToolbar = new QVBoxLayout();
    deviceToolbar->setSpacing(8);
    m_deleteDeviceBtn = new QPushButton("删除设备");
    m_deleteDeviceBtn->setEnabled(false);
    deviceToolbar->addWidget(m_deleteDeviceBtn);
    deviceToolbar->addStretch();
    deviceContentLayout->addLayout(deviceToolbar);
    m_configDeviceTable = new QTableWidget(0, 5, this);
    m_configDeviceTable->setHorizontalHeaderLabels({"DeviceId", "描述", "模型", "协议地址", "点位数"});
    m_configDeviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_configDeviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configDeviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_configDeviceTable->verticalHeader()->setVisible(false);
    m_configDeviceTable->horizontalHeader()->setStretchLastSection(false);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_configDeviceTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_configDeviceTable->setColumnWidth(4, 80);
    m_configDeviceTable->setStyleSheet(
        "QTableWidget::item:selected {"
        "  background-color: #7d4f50;"
        "  color: #ffffff;"
        "}"
    );
    deviceContentLayout->addWidget(m_configDeviceTable, 1);
    deviceGroupLayout->addLayout(deviceContentLayout, 1);
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
    auto *modelNavLayout = new QHBoxLayout();
    modelNavLayout->setSpacing(8);
    modelNavLayout->addWidget(new QLabel(QStringLiteral("当前模型:"), this));
    m_modelEditorCombo = new QComboBox(this);
    m_modelEditorCombo->setMinimumWidth(320);
    m_modelEditorCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    modelNavLayout->addWidget(m_modelEditorCombo, 1);
    modelDetailLayout->addLayout(modelNavLayout);
    auto *modelFormFrame = new QFrame(this);
    modelFormFrame->setFrameShape(QFrame::StyledPanel);
    modelFormFrame->setMaximumHeight(250);
    auto *modelFormLayout = new QFormLayout(modelFormFrame);
    modelFormLayout->setContentsMargins(10, 8, 10, 8);
    modelFormLayout->setVerticalSpacing(6);
    m_modelIdEdit = new QLineEdit(this);
    m_modelDisplayNameEdit = new QLineEdit(this);
    m_modelDeviceTypeEdit = new QLineEdit(this);
    m_modelVersionEdit = new QLineEdit(this);
    m_modelManufacturerIdEdit = new QLineEdit(this);
    m_modelManufacturerDescEdit = new QLineEdit(this);
    m_modelSchemaEdit = new QLineEdit(this);
    m_modelNorthVisibleCheck = new QCheckBox(QStringLiteral("北向可见"), this);
    m_modelNorthVisibleCheck->setChecked(true);
    for (QLineEdit *edit : {m_modelIdEdit,
                            m_modelDisplayNameEdit,
                            m_modelDeviceTypeEdit,
                            m_modelVersionEdit,
                            m_modelManufacturerIdEdit,
                            m_modelManufacturerDescEdit,
                            m_modelSchemaEdit}) {
        edit->setMinimumHeight(28);
    }
    modelFormLayout->addRow("模型ID:", m_modelIdEdit);
    modelFormLayout->addRow("设备描述:", m_modelDisplayNameEdit);
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
    m_modelPointsTable = new QTableWidget(0, 11, this);
    m_modelPointsTable->setColumnCount(11);
    m_modelPointsTable->setHorizontalHeaderLabels({"", "北向可见", "类别", "DOname", "描述", "LDname", "LNtype", "LNinst", "DataRef", "数据类型", "单位"});
    m_modelPointsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_modelPointsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_modelPointsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_modelPointsTable->setDragEnabled(false);
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
    m_modelPointsTable->setColumnWidth(0, 26);
    m_modelPointsTable->setColumnWidth(1, 62);
    m_modelPointsTable->setColumnWidth(2, 82);
    m_modelPointsTable->setColumnWidth(3, 130);
    m_modelPointsTable->setColumnWidth(4, 220);
    m_modelPointsTable->setColumnWidth(5, 90);
    m_modelPointsTable->setColumnWidth(6, 120);
    m_modelPointsTable->setColumnWidth(7, 70);
    m_modelPointsTable->setColumnWidth(8, 260);
    m_modelPointsTable->setColumnWidth(9, 90);
    m_modelPointsTable->setColumnWidth(10, 70);
    modelDetailLayout->addWidget(m_modelPointsTable, 1);
    modelDetailLayout->setStretch(0, 0);
    modelDetailLayout->setStretch(1, 0);
    modelDetailLayout->setStretch(2, 0);
    modelDetailLayout->setStretch(3, 0);
    modelDetailLayout->setStretch(4, 1);

    m_deviceEditorPage = new QWidget(this);
    auto *deviceEditorLayout = new QVBoxLayout(m_deviceEditorPage);
    deviceEditorLayout->setContentsMargins(0, 0, 0, 0);
    deviceEditorLayout->setSpacing(8);
    auto *deviceNavLayout = new QHBoxLayout();
    deviceNavLayout->setSpacing(8);
    deviceNavLayout->addWidget(new QLabel(QStringLiteral("当前设备:"), this));
    m_deviceEditorCombo = new QComboBox(this);
    m_deviceEditorCombo->setMinimumWidth(320);
    m_deviceEditorCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    deviceNavLayout->addWidget(m_deviceEditorCombo, 1);
    deviceEditorLayout->addLayout(deviceNavLayout);
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
    deviceFormLayout->addRow("协议地址:", m_deviceStationAddressEdit);
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
        QStringLiteral("虚拟点标志")
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
    deviceEditorLayout->setStretch(4, 0);
    deviceEditorLayout->setStretch(5, 1);

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
    deviceDetailLayout->addRow("协议地址:", m_deviceDetailAddressLabel);
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
    onlineRuleHint->setObjectName(QStringLiteral("onlineRuleHint"));
    onlineRuleHint->setWordWrap(true);
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
    m_logicComputationPointTable = new QTableWidget(0, 7, this);
    m_logicComputationPointTable->setHorizontalHeaderLabels({
        QString(),
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
    m_logicComputationPointTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_logicComputationPointTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_logicComputationPointTable->setDragEnabled(false);
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
    m_logicComputationPointTable->setColumnWidth(0, 26);
    m_logicComputationPointTable->setColumnWidth(1, 120);
    m_logicComputationPointTable->setColumnWidth(2, 240);
    m_logicComputationPointTable->setColumnWidth(3, 220);
    m_logicComputationPointTable->setColumnWidth(5, 360);
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
    programConnectionRow->addWidget(new QLabel(QStringLiteral("IP:"), this));
    m_programRemoteHostEdit = new QLineEdit(QStringLiteral("192.168.7.10"), this);
    m_programRemoteHostEdit->setMinimumWidth(100);
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
    m_programRemotePortEdit->setMaximumWidth(120);
    programConnectionRow->addWidget(m_programRemotePortEdit);

    m_connectProgramControlBtn = new QPushButton(QStringLiteral("连接"), this);
    m_disconnectProgramControlBtn = new QPushButton(QStringLiteral("断开"), this);
    m_disconnectProgramControlBtn->setEnabled(false);
    programConnectionRow->addWidget(m_connectProgramControlBtn);
    programConnectionRow->addWidget(m_disconnectProgramControlBtn);

    m_refreshProgramStatusBtn = new QPushButton(QStringLiteral("刷新状态"), this);
    m_refreshProgramStatusBtn->setEnabled(false);
    programConnectionRow->addWidget(m_refreshProgramStatusBtn);

    programConnectionRow->addStretch();
    programControlLayout->addLayout(programConnectionRow);

    m_programControlTable = new QTableWidget(0, 7, this);
    m_programControlTable->setHorizontalHeaderLabels({
        QStringLiteral("状态"),
        QStringLiteral("APP"),
        QStringLiteral("PID"),
        QStringLiteral("启动"),
        QStringLiteral("停止"),
        QStringLiteral("重启"),
        QStringLiteral("开机自启")
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
    m_programControlTable->setColumnWidth(2, 120);
    m_programControlTable->setColumnWidth(3, 90);
    m_programControlTable->setColumnWidth(4, 150);
    m_programControlTable->setColumnWidth(5, 90);
    m_programControlTable->setColumnWidth(6, 120);
    programControlLayout->addWidget(m_programControlTable, 1);

    // ============================================================
    // IEC101 配置页面
    // ============================================================
    m_iec101ConfigPage = new QWidget(this);
    auto *iec101Layout = new QVBoxLayout(m_iec101ConfigPage);
    iec101Layout->setContentsMargins(0, 0, 0, 0);
    iec101Layout->setSpacing(6);

    // ---- 基本设置 ----
    auto *iec101BasicFrame = new QFrame(this);
    iec101BasicFrame->setFrameShape(QFrame::StyledPanel);
    auto *iec101BasicGrid = new QGridLayout(iec101BasicFrame);
    iec101BasicGrid->setContentsMargins(8, 6, 8, 6);
    iec101BasicGrid->setHorizontalSpacing(24);
    iec101BasicGrid->setVerticalSpacing(6);

    m_iec101CommModeCombo = new QComboBox(this);
    m_iec101CommModeCombo->addItem(QStringLiteral("0 — 串口"), 0);
    m_iec101CommModeCombo->addItem(QStringLiteral("1 — TCP"), 1);
    m_iec101CommModeCombo->addItem(QStringLiteral("2 — 双模(串口+TCP)"), 2);
    iec101BasicGrid->addWidget(new QLabel(QStringLiteral("连接模式:"), this), 0, 0);
    iec101BasicGrid->addWidget(m_iec101CommModeCombo, 0, 1);

    m_iec101ComAddrEdit = new QLineEdit(this);
    m_iec101ComAddrEdit->setPlaceholderText(QStringLiteral("IEC101 链路地址，例如 63"));
    iec101BasicGrid->addWidget(new QLabel(QStringLiteral("链路地址:"), this), 0, 2);
    iec101BasicGrid->addWidget(m_iec101ComAddrEdit, 0, 3);

    m_iec101CodePortEdit = new QLineEdit(this);
    m_iec101CodePortEdit->setPlaceholderText(QStringLiteral("2404"));
    m_iec101CodePortEdit->setText(QStringLiteral("2404"));
    iec101BasicGrid->addWidget(new QLabel(QStringLiteral("端口:"), this), 1, 0);
    iec101BasicGrid->addWidget(m_iec101CodePortEdit, 1, 1);
    iec101BasicGrid->setColumnStretch(1, 1);
    iec101BasicGrid->setColumnStretch(3, 1);

    iec101Layout->addWidget(iec101BasicFrame);

    // ---- 串口连接参数 ----
    m_iec101SerialParamsGroup = new QGroupBox(QStringLiteral("串口连接参数"), this);
    auto *iec101SerialGrid = new QGridLayout(m_iec101SerialParamsGroup);
    iec101SerialGrid->setContentsMargins(8, 12, 8, 6);
    iec101SerialGrid->setHorizontalSpacing(24);
    iec101SerialGrid->setVerticalSpacing(6);

    m_iec101UsartNameEdit = new QLineEdit(this);
    m_iec101UsartNameEdit->setPlaceholderText(QStringLiteral("例如 /dev/ttyS13"));
    iec101SerialGrid->addWidget(new QLabel(QStringLiteral("串口名:"), this), 0, 0);
    iec101SerialGrid->addWidget(m_iec101UsartNameEdit, 0, 1, 1, 3);

    m_iec101BaudrateCombo = new QComboBox(this);
    m_iec101BaudrateCombo->setEditable(true);
    m_iec101BaudrateCombo->addItems({
        QStringLiteral("1200"),
        QStringLiteral("2400"),
        QStringLiteral("4800"),
        QStringLiteral("9600"),
        QStringLiteral("19200"),
        QStringLiteral("38400"),
        QStringLiteral("57600"),
        QStringLiteral("115200")
    });
    iec101SerialGrid->addWidget(new QLabel(QStringLiteral("波特率:"), this), 1, 0);
    iec101SerialGrid->addWidget(m_iec101BaudrateCombo, 1, 1);

    m_iec101DataBitCombo = new QComboBox(this);
    m_iec101DataBitCombo->addItems({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
    m_iec101DataBitCombo->setCurrentIndex(3); // 默认 8
    iec101SerialGrid->addWidget(new QLabel(QStringLiteral("数据位:"), this), 1, 2);
    iec101SerialGrid->addWidget(m_iec101DataBitCombo, 1, 3);

    m_iec101StopBitCombo = new QComboBox(this);
    m_iec101StopBitCombo->addItems({QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")});
    iec101SerialGrid->addWidget(new QLabel(QStringLiteral("停止位:"), this), 2, 0);
    iec101SerialGrid->addWidget(m_iec101StopBitCombo, 2, 1);

    m_iec101ParityCombo = new QComboBox(this);
    m_iec101ParityCombo->addItems({
        QStringLiteral("None"),
        QStringLiteral("Odd"),
        QStringLiteral("Even"),
        QStringLiteral("Mark"),
        QStringLiteral("Space")
    });
    iec101SerialGrid->addWidget(new QLabel(QStringLiteral("校验:"), this), 2, 2);
    iec101SerialGrid->addWidget(m_iec101ParityCombo, 2, 3);
    iec101SerialGrid->setColumnStretch(1, 1);
    iec101SerialGrid->setColumnStretch(3, 1);

    iec101Layout->addWidget(m_iec101SerialParamsGroup);

    // ---- 协议参数 ----
    auto *iec101ProtoFrame = new QFrame(this);
    iec101ProtoFrame->setFrameShape(QFrame::StyledPanel);
    auto *iec101ProtoGrid = new QGridLayout(iec101ProtoFrame);
    iec101ProtoGrid->setContentsMargins(8, 6, 8, 6);
    iec101ProtoGrid->setHorizontalSpacing(24);
    iec101ProtoGrid->setVerticalSpacing(6);

    m_iec101CotCombo = new QComboBox(this);
    m_iec101CotCombo->addItem(QStringLiteral("1"), 1);
    m_iec101CotCombo->addItem(QStringLiteral("2"), 2);
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("传送原因 COT 字节数:"), this), 0, 0);
    iec101ProtoGrid->addWidget(m_iec101CotCombo, 0, 1);

    m_iec101CaCombo = new QComboBox(this);
    m_iec101CaCombo->addItem(QStringLiteral("1"), 1);
    m_iec101CaCombo->addItem(QStringLiteral("2"), 2);
    m_iec101CaCombo->setCurrentIndex(1); // 默认 2
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("公共地址 CA 字节数:"), this), 0, 2);
    iec101ProtoGrid->addWidget(m_iec101CaCombo, 0, 3);

    m_iec101IoaCombo = new QComboBox(this);
    m_iec101IoaCombo->addItem(QStringLiteral("1"), 1);
    m_iec101IoaCombo->addItem(QStringLiteral("2"), 2);
    m_iec101IoaCombo->setCurrentIndex(1); // 默认 2
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("信息对象地址 IOA 字节数:"), this), 1, 0);
    iec101ProtoGrid->addWidget(m_iec101IoaCombo, 1, 1);

    m_iec101LinkAddrCombo = new QComboBox(this);
    m_iec101LinkAddrCombo->addItem(QStringLiteral("1"), 1);
    m_iec101LinkAddrCombo->addItem(QStringLiteral("2"), 2);
    m_iec101LinkAddrCombo->setCurrentIndex(1); // 默认 2
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("链路地址字节数:"), this), 1, 2);
    iec101ProtoGrid->addWidget(m_iec101LinkAddrCombo, 1, 3);

    m_iec101TelecontrolTypeCombo = new QComboBox(this);
    m_iec101TelecontrolTypeCombo->addItem(QStringLiteral("（空）"), QString());
    m_iec101TelecontrolTypeCombo->addItem(QStringLiteral("单命令"), QStringLiteral("单命令"));
    m_iec101TelecontrolTypeCombo->addItem(QStringLiteral("双命令"), QStringLiteral("双命令"));
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("遥控类型:"), this), 2, 0);
    iec101ProtoGrid->addWidget(m_iec101TelecontrolTypeCombo, 2, 1);

    m_iec101TelemetryTypeCombo = new QComboBox(this);
    m_iec101TelemetryTypeCombo->addItem(QStringLiteral("（空）"), QString());
    m_iec101TelemetryTypeCombo->addItem(QStringLiteral("归一化值"), QStringLiteral("归一化值"));
    m_iec101TelemetryTypeCombo->addItem(QStringLiteral("短浮点数"), QStringLiteral("短浮点数"));
    m_iec101TelemetryTypeCombo->addItem(QStringLiteral("标度化值"), QStringLiteral("标度化值"));
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("遥测类型:"), this), 2, 2);
    iec101ProtoGrid->addWidget(m_iec101TelemetryTypeCombo, 2, 3);

    m_iec101SequenceCombo = new QComboBox(this);
    m_iec101SequenceCombo->addItem(QStringLiteral("0 — 逐点发送"), 0);
    m_iec101SequenceCombo->addItem(QStringLiteral("1 — 连续地址批量打包"), 1);
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("总召唤模式:"), this), 3, 0);
    iec101ProtoGrid->addWidget(m_iec101SequenceCombo, 3, 1);

    m_iec101YxUseDoubleValueCombo = new QComboBox(this);
    m_iec101YxUseDoubleValueCombo->addItem(QStringLiteral("0 — 不转换"), 0);
    m_iec101YxUseDoubleValueCombo->addItem(QStringLiteral("1 — 转为双点标准格式"), 1);
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("遥信双点转换:"), this), 3, 2);
    iec101ProtoGrid->addWidget(m_iec101YxUseDoubleValueCombo, 3, 3);

    m_iec101YxAllSTransDFlagCombo = new QComboBox(this);
    m_iec101YxAllSTransDFlagCombo->addItem(QStringLiteral("0 — 不转换"), 0);
    m_iec101YxAllSTransDFlagCombo->addItem(QStringLiteral("1 — 双命令值转双点格式"), 1);
    iec101ProtoGrid->addWidget(new QLabel(QStringLiteral("双命令遥信转换:"), this), 4, 0);
    iec101ProtoGrid->addWidget(m_iec101YxAllSTransDFlagCombo, 4, 1);
    iec101ProtoGrid->setColumnStretch(1, 1);
    iec101ProtoGrid->setColumnStretch(3, 1);

    iec101Layout->addWidget(iec101ProtoFrame);

    // ---- 点表 meas_points ----
    auto *iec101PointsFrame = new QFrame(this);
    iec101PointsFrame->setFrameShape(QFrame::StyledPanel);
    auto *iec101PointsLayout = new QVBoxLayout(iec101PointsFrame);
    iec101PointsLayout->setContentsMargins(8, 8, 8, 6);
    iec101PointsLayout->setSpacing(6);

    auto *iec101PointsHeader = new QHBoxLayout();
    auto *iec101PointsTitle = new QLabel(QStringLiteral("点表（来自南向设备）"), this);
    QFont boldFont = iec101PointsTitle->font();
    boldFont.setBold(true);
    iec101PointsTitle->setFont(boldFont);
    iec101PointsHeader->addWidget(iec101PointsTitle);
    iec101PointsHeader->addStretch();
    m_refreshIec101PointsBtn = new QPushButton(QStringLiteral("从设备刷新点位"), this);
    iec101PointsHeader->addWidget(m_refreshIec101PointsBtn);
    iec101PointsLayout->addLayout(iec101PointsHeader);

    // 筛选栏
    auto *iec101FilterRow = new QHBoxLayout();
    m_iec101PointFilterTabBar = new QTabBar(this);
    m_iec101PointFilterTabBar->addTab(QStringLiteral("全部"));
    m_iec101PointFilterTabBar->addTab(QStringLiteral("遥测"));
    m_iec101PointFilterTabBar->addTab(QStringLiteral("遥信"));
    m_iec101PointFilterTabBar->addTab(QStringLiteral("控制"));
    m_iec101PointFilterTabBar->setExpanding(false);
    m_iec101PointFilterTabBar->setCurrentIndex(0);
    iec101FilterRow->addWidget(m_iec101PointFilterTabBar);
    iec101FilterRow->addStretch();
    m_iec101PointDataRefFilterEdit = new QLineEdit(this);
    m_iec101PointDataRefFilterEdit->setPlaceholderText(QStringLiteral("筛选 DataRef..."));
    m_iec101PointDataRefFilterEdit->setClearButtonEnabled(true);
    m_iec101PointDataRefFilterEdit->setFixedWidth(220);
    iec101FilterRow->addWidget(m_iec101PointDataRefFilterEdit);
    m_iec101PointDescriptionFilterEdit = new QLineEdit(this);
    m_iec101PointDescriptionFilterEdit->setPlaceholderText(QStringLiteral("筛选 Description..."));
    m_iec101PointDescriptionFilterEdit->setClearButtonEnabled(true);
    m_iec101PointDescriptionFilterEdit->setFixedWidth(180);
    iec101FilterRow->addWidget(m_iec101PointDescriptionFilterEdit);
    iec101PointsLayout->addLayout(iec101FilterRow);

    m_iec101PointsTable = new QTableWidget(0, 8, this);
    m_iec101PointsTable->setHorizontalHeaderLabels({
        QString(),
        QStringLiteral("启用"),
        QStringLiteral("DeviceId"),
        QStringLiteral("DataRef"),
        QStringLiteral("Description"),
        QStringLiteral("北向101地址"),
        QStringLiteral("死区类型"),
        QStringLiteral("死区值")
    });
    m_iec101PointsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_iec101PointsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_iec101PointsTable->setSelectionMode(QAbstractItemView::ContiguousSelection);
    m_iec101PointsTable->setAlternatingRowColors(true);
    m_iec101PointsTable->verticalHeader()->setVisible(false);
    m_iec101PointsTable->horizontalHeader()->setStretchLastSection(true);
    m_iec101PointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_iec101PointsTable->setColumnWidth(0, 28);
    m_iec101PointsTable->setColumnWidth(1, 50);
    m_iec101PointsTable->setColumnWidth(2, 130);
    m_iec101PointsTable->setColumnWidth(3, 220);
    m_iec101PointsTable->setColumnWidth(4, 150);
    m_iec101PointsTable->setColumnWidth(5, 100);
    m_iec101PointsTable->setColumnWidth(6, 130);
    // Drag-drop setup
    m_iec101PointsTable->setDragEnabled(false);
    m_iec101PointsTable->setAcceptDrops(true);
    m_iec101PointsTable->setDropIndicatorShown(true);
    m_iec101PointsTable->setDragDropMode(QAbstractItemView::DragDrop);
    m_iec101PointsTable->setDragDropOverwriteMode(false);
    m_iec101PointsTable->setDefaultDropAction(Qt::CopyAction);
    m_iec101PointsTable->viewport()->installEventFilter(this);

    m_iec101PointDropLine = new QFrame(m_iec101PointsTable->viewport());
    m_iec101PointDropLine->setFixedHeight(3);
    m_iec101PointDropLine->setStyleSheet(QStringLiteral("background-color: #ff8c00; border-radius: 1px;"));
    m_iec101PointDropLine->hide();

    iec101PointsLayout->addWidget(m_iec101PointsTable, 1);

    m_iec101ValidationLabel = new QLabel(this);
    m_iec101ValidationLabel->setWordWrap(true);
    m_iec101ValidationLabel->setStyleSheet(QStringLiteral("QLabel { color: #2e7d32; }"));
    m_iec101ValidationLabel->setText(QStringLiteral("✓ 当前地址分配未发现问题。"));
    iec101PointsLayout->addWidget(m_iec101ValidationLabel);

    iec101Layout->addWidget(iec101PointsFrame, 1);

    // ---- 连接信号 ----
    connect(m_iec101CommModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onIec101CommModeChanged);
    connect(m_refreshIec101PointsBtn, &QPushButton::clicked,
            this, &MainWindow::onRefreshIec101PointsClicked);
    connect(m_iec101PointsTable, &QTableWidget::itemChanged,
            this, &MainWindow::onIec101PointItemChanged);
    connect(m_iec101PointFilterTabBar, &QTabBar::currentChanged,
            this, &MainWindow::onIec101PointFilterChanged);
    connect(m_iec101PointDataRefFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onIec101PointFilterTextChanged);
    connect(m_iec101PointDescriptionFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onIec101PointFilterTextChanged);

    // 初始化连接参数显隐（默认选中 TCP=index 1）
    m_iec101CommModeCombo->setCurrentIndex(1);
    onIec101CommModeChanged(1);

    m_mainTabWidget->addTab(m_configPage, "配置概览");
    m_mainTabWidget->addTab(m_programControlPage, QStringLiteral("程序控制"));
    m_mainTabWidget->addTab(debugPage, "调试控制");
    m_mainTabWidget->addTab(m_iec101ConfigPage, QStringLiteral("IEC101配置"));
    m_mainTabWidget->addTab(m_modelEditorPage, "模型编辑器");
    m_mainTabWidget->addTab(m_deviceEditorPage, "设备编辑器");
    m_mainTabWidget->addTab(m_logicCenterPage, "逻辑中心");
    m_mainTabWidget->addTab(m_logicAgcAvcPage, "AGC/AVC");
    m_mainTabWidget->addTab(m_logicComputationPointPage, "计算点");
    m_mainTabWidget->addTab(m_logicControlRulePage, "控制转换");
    m_mainTabWidget->addTab(m_logicOnlineLinkPage, "在线联动");
    m_mainTabWidget->addTab(m_configIssuePage, "问题列表");

    setCentralWidget(central);

    m_statusLabel = new QLabel("未连接");
    statusBar()->addWidget(m_statusLabel);
    m_controlStatusLabel = new QLabel(QStringLiteral("状态: -"));
    m_controlStatusLabel->setMinimumWidth(360);
    statusBar()->addWidget(m_controlStatusLabel, 1);
    statusBar()->addWidget(m_modelValidationLabel, 1);

    m_themeToggleBtn = new QPushButton(this);
    m_themeToggleBtn->setFlat(true);
    m_themeToggleBtn->setMaximumWidth(58);
    statusBar()->addPermanentWidget(m_themeToggleBtn);

    m_versionLabel = new QLabel(QStringLiteral("v") + QStringLiteral(APP_VERSION), this);
    m_versionLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    statusBar()->addPermanentWidget(m_versionLabel);

    connect(m_connectBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectClicked);
    connect(m_themeToggleBtn, &QPushButton::clicked,
            this, &MainWindow::onThemeToggleClicked);
    connect(m_appCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onAppSelectionChanged);
    connect(m_rawFrameLogBtn, &QPushButton::clicked,
            this, &MainWindow::onOpenRawFrameLogClicked);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &MainWindow::onSendClicked);
    connect(m_cmdEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onSendClicked);

    connect(m_refreshDataBtn, &QPushButton::clicked,
            this, [this]() { requestServiceChannelData(); });
    connect(m_sendControlBtn, &QPushButton::clicked,
            this, &MainWindow::onSendControlClicked);
    connect(m_dataFreezeBtn, &QPushButton::clicked, this, [this]() {
        sendServiceChannelDataFreezeCommand(m_serviceChannelDataFrozen
            ? QStringLiteral("off")
            : QStringLiteral("on"));
    });
    connect(m_deviceFilterCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onDeviceFilterChanged);
    connect(m_serviceTypeFilterCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onServiceTypeFilterChanged);
    connect(m_dataRefFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onDataRefFilterTextChanged);
    connect(m_descriptionFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onDescriptionFilterTextChanged);
    connect(m_autoRefreshCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onAutoRefreshIntervalChanged);
    connect(m_selectConfigImportDirBtn, &QPushButton::clicked,
            this, &MainWindow::onBrowseConfigImportDirClicked);
    connect(m_browseConfigImportDirBtn, &QPushButton::clicked,
            this, &MainWindow::onImportIec104ConfigClicked);
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
    connect(m_modelEditorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModelEditorSelectionChanged);
    connect(m_deviceEditorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceEditorSelectionChanged);
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
    connect(m_controlResponseTimer, &QTimer::timeout,
            this, &MainWindow::handleControlResponseTimeout);
    connect(new QShortcut(QKeySequence::Copy, m_dataTable), &QShortcut::activated,
            this, [this]() { copySelectedTableCells(); });
    connect(m_dataTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onDataTableCellDoubleClicked);
    connect(m_dataTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onDataTableSelectionChanged);
    connect(new QShortcut(QKeySequence::Paste, m_modelPointsTable), &QShortcut::activated,
            this, [this]() { pasteClipboardIntoModelPointsTable(); });
    connect(new QShortcut(QKeySequence::Paste, m_deviceBindingsTable), &QShortcut::activated,
            this, [this]() { pasteClipboardIntoDeviceBindingsTable(); });
    connect(new QShortcut(QKeySequence::Paste, m_iec101PointsTable), &QShortcut::activated,
            this, [this]() { pasteClipboardIntoIec101PointsTable(); });
    connect(new QShortcut(QKeySequence::Undo, m_modelPointsTable), &QShortcut::activated,
            this, [this]() { undoLastConfigEdit(); });
    connect(new QShortcut(QKeySequence::Undo, m_deviceBindingsTable), &QShortcut::activated,
            this, [this]() { undoLastConfigEdit(); });
    connect(new QShortcut(QKeySequence::Undo, m_iec101PointsTable), &QShortcut::activated,
            this, [this]() { undoIec101PointsLastEdit(); });
    connect(m_dataTable, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &position) {
                QMenu menu(this);
                QAction *copyAction = menu.addAction("复制");
                copyAction->setEnabled(m_dataTable->selectionModel() &&
                                       !m_dataTable->selectionModel()->selectedIndexes().isEmpty());
                QAction *controlAction = menu.addAction(QStringLiteral("发送控制..."));
                controlAction->setEnabled(isServiceChannelControlRow(m_dataTable->rowAt(position.y())) &&
                                          m_client->isConnected() &&
                                          !m_client->isExecutingCommand());
                QAction *dataWriteAction = menu.addAction(QStringLiteral("写入数据..."));
                dataWriteAction->setEnabled(isServiceChannelDataWriteRow(m_dataTable->rowAt(position.y())) &&
                                            m_client->isConnected() &&
                                            !m_client->isExecutingCommand());
                QAction *selectedAction = menu.exec(m_dataTable->viewport()->mapToGlobal(position));
                if (selectedAction == copyAction) {
                    copySelectedTableCells();
                } else if (selectedAction == controlAction) {
                    openControlCommandDialog(m_dataTable->rowAt(position.y()));
                } else if (selectedAction == dataWriteAction) {
                    openDataWriteDialog(m_dataTable->rowAt(position.y()));
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

    m_themeMode = loadThemeMode();
    applyTheme(m_themeMode);

    applyCurrentAppView();
    m_navigationCurrentState = currentNavigationState();
    connect(m_mainTabWidget, &QTabWidget::currentChanged,
            this, [this](int) {
                if (m_mainTabWidget->currentWidget() == m_deviceEditorPage) {
                    const int deviceIndex = currentConfigDeviceIndex();
                    refreshDeviceDetail(deviceIndex);
                    refreshDeviceEditor(deviceIndex);
                }
                recordNavigationState();
            });
    connect(m_contentStack, &QStackedWidget::currentChanged,
            this, [this](int) { recordNavigationState(); });
    if (QApplication::instance()) {
        QApplication::instance()->installEventFilter(this);
    }
    refreshProgramControlTable(QString());
    refreshLogicCenterOverview();
}

MainWindow::~MainWindow()
{
    if (QApplication::instance()) {
        QApplication::instance()->removeEventFilter(this);
    }
    closeProgramControlShell();
}

MainWindow::ThemeMode MainWindow::loadThemeMode() const
{
    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    const QString mode = settings.value(QStringLiteral("ui/themeMode")).toString();
    if (mode == QStringLiteral("light")) {
        return ThemeMode::Light;
    }
    if (mode == QStringLiteral("dark")) {
        return ThemeMode::Dark;
    }

    const QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    return app && paletteLooksDark(app->palette()) ? ThemeMode::Dark : ThemeMode::Light;
}

void MainWindow::saveThemeMode(ThemeMode mode) const
{
    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    settings.setValue(QStringLiteral("ui/themeMode"),
                      mode == ThemeMode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
}

void MainWindow::applyTheme(ThemeMode mode)
{
    m_themeMode = mode;

    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (app) {
        app->setPalette(mode == ThemeMode::Dark ? darkThemePalette() : lightThemePalette());
        app->setStyleSheet(themeStyleSheet(mode == ThemeMode::Dark));
    }

    if (m_logView) {
        const QString background = mode == ThemeMode::Dark ? QStringLiteral("#1e1e1e")
                                                           : QStringLiteral("#ffffff");
        const QString foreground = mode == ThemeMode::Dark ? QStringLiteral("#d4d4d4")
                                                           : QStringLiteral("#202124");
        m_logView->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: Consolas, 'Courier New', monospace;"
            "  font-size: 12px;"
            "  background-color: %1;"
            "  color: %2;"
            "}").arg(background, foreground));
    }

    if (m_themeToggleBtn) {
        const bool darkMode = mode == ThemeMode::Dark;
        m_themeToggleBtn->setText(darkMode ? QStringLiteral("暗色") : QStringLiteral("亮色"));
        m_themeToggleBtn->setToolTip(darkMode ? QStringLiteral("切换到亮色主题")
                                               : QStringLiteral("切换到暗色主题"));
    }
}

void MainWindow::onThemeToggleClicked()
{
    const ThemeMode nextMode = m_themeMode == ThemeMode::Dark ? ThemeMode::Light
                                                              : ThemeMode::Dark;
    applyTheme(nextMode);
    saveThemeMode(nextMode);

    if (currentAppConfig().viewMode == AppViewMode::DataTable) {
        applyServiceChannelFilter();
    }
}

QColor MainWindow::serviceChannelDefaultTextColor() const
{
    return m_themeMode == ThemeMode::Dark ? QColor(QStringLiteral("#f0f4f8"))
                                          : QColor(QStringLiteral("#17202a"));
}

QColor MainWindow::serviceChannelChangedTextColor() const
{
    return m_themeMode == ThemeMode::Dark ? QColor(QStringLiteral("#67d26f"))
                                          : QColor(QStringLiteral("#188038"));
}

QPair<int, int> MainWindow::currentNavigationState() const
{
    return qMakePair(m_mainTabWidget ? m_mainTabWidget->currentIndex() : -1,
                     m_contentStack ? m_contentStack->currentIndex() : -1);
}

void MainWindow::recordNavigationState()
{
    if (m_restoringNavigation) {
        return;
    }

    const QPair<int, int> state = currentNavigationState();
    if (state.first < 0 || state == m_navigationCurrentState) {
        return;
    }

    if (m_navigationCurrentState.first >= 0) {
        m_navigationBackStack.append(m_navigationCurrentState);
        constexpr int MaxNavigationHistory = 100;
        while (m_navigationBackStack.size() > MaxNavigationHistory) {
            m_navigationBackStack.removeFirst();
        }
    }
    m_navigationCurrentState = state;
    m_navigationForwardStack.clear();
}

void MainWindow::applyNavigationState(const QPair<int, int> &state)
{
    if (!m_mainTabWidget || state.first < 0 || state.first >= m_mainTabWidget->count()) {
        return;
    }

    m_restoringNavigation = true;
    if (m_contentStack && state.second >= 0 && state.second < m_contentStack->count()) {
        m_contentStack->setCurrentIndex(state.second);
    }
    m_mainTabWidget->setCurrentIndex(state.first);
    m_navigationCurrentState = currentNavigationState();
    m_restoringNavigation = false;
}

void MainWindow::navigateBack()
{
    if (m_navigationBackStack.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有更早的界面"), 2000);
        return;
    }

    m_navigationForwardStack.append(currentNavigationState());
    const QPair<int, int> state = m_navigationBackStack.takeLast();
    applyNavigationState(state);
}

void MainWindow::navigateForward()
{
    if (m_navigationForwardStack.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有更后的界面"), 2000);
        return;
    }

    m_navigationBackStack.append(currentNavigationState());
    const QPair<int, int> state = m_navigationForwardStack.takeLast();
    applyNavigationState(state);
}
