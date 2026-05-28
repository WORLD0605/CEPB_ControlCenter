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
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_client(new DebugConsoleClient(this))
{
    setWindowTitle("CEPB Control Center");
    resize(960, 640);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // === 顶部连接配置 ===
    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("IP地址:"));
    m_ipEdit = new QLineEdit("192.168.2.10");
    m_ipEdit->setMinimumWidth(140);
    topLayout->addWidget(m_ipEdit);

    topLayout->addWidget(new QLabel("APP:"));
    m_appCombo = new QComboBox();
    // 格式：显示名 (端口)，data 存端口号
    m_appCombo->addItem("cepiec104", 6666);
    topLayout->addWidget(m_appCombo);

    m_connectBtn = new QPushButton("连接");
    m_disconnectBtn = new QPushButton("断开");
    m_disconnectBtn->setEnabled(false);
    topLayout->addWidget(m_connectBtn);
    topLayout->addWidget(m_disconnectBtn);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    // === 中部日志显示 ===
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
    mainLayout->addWidget(m_logView, 1);

    // === 底部命令区 ===
    auto *bottomLayout = new QHBoxLayout();
    m_cmdEdit = new QLineEdit();
    m_cmdEdit->setPlaceholderText("输入命令后按回车...");
    bottomLayout->addWidget(m_cmdEdit, 1);

    m_sendBtn = new QPushButton("发送");
    m_sendBtn->setEnabled(false);
    bottomLayout->addWidget(m_sendBtn);

    mainLayout->addLayout(bottomLayout);

    // === 快捷按钮区 ===
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
    mainLayout->addLayout(quickLayout);

    setCentralWidget(central);

    // === 状态栏 ===
    m_statusLabel = new QLabel("未连接");
    statusBar()->addWidget(m_statusLabel);

    // === 信号连接 ===
    connect(m_connectBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectClicked);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &MainWindow::onSendClicked);
    connect(m_cmdEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onSendClicked);

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
}

MainWindow::~MainWindow() = default;

void MainWindow::onConnectClicked()
{
    QString host = m_ipEdit->text().trimmed();
    quint16 port = static_cast<quint16>(m_appCombo->currentData().toInt());
    if (host.isEmpty()) {
        QMessageBox::warning(this, "警告", "IP地址不能为空");
        return;
    }

    // 根据 APP 名称推导 prompt，如 cepiec104>
    QString appName = m_appCombo->currentText();
    m_client->setPromptPattern(appName + ">");

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

void MainWindow::onConnected()
{
    updateUIState(true);
    appendSystem("已连接", "#32cd32");
    m_statusLabel->setText("已连接 | " + m_ipEdit->text() + ":" + m_appCombo->currentData().toString());
}

void MainWindow::onDisconnected()
{
    updateUIState(false);
    appendSystem("已断开", "#ff4500");
    m_statusLabel->setText("未连接");
}

void MainWindow::onError(const QString &err)
{
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
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_sendBtn->setEnabled(connected);
    m_ipEdit->setEnabled(!connected);
    m_appCombo->setEnabled(!connected);

    for (auto *btn : findChildren<QPushButton*>()) {
        if (btn->property("command").isValid()) {
            btn->setEnabled(connected);
        }
    }
}
