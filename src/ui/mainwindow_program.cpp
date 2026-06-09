#include "mainwindow.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

namespace {

constexpr int ProgramColumnStatus = 0;
constexpr int ProgramColumnApp = 1;
constexpr int ProgramColumnPid = 2;
constexpr int ProgramColumnCommand = 3;
constexpr int ProgramColumnStart = 4;
constexpr int ProgramColumnStop = 5;
constexpr int ProgramColumnRestart = 6;

QString remoteProgramShellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(quoted);
}

QString trimRemoteBaseDir(QString baseDir)
{
    baseDir = baseDir.trimmed();
    if (baseDir.isEmpty()) {
        baseDir = QStringLiteral("/home/cepgateway/app");
    }
    while (baseDir.endsWith(QLatin1Char('/')) && baseDir.size() > 1) {
        baseDir.chop(1);
    }
    return baseDir;
}

QString programPidScanSnippet(const QString &baseDir, const QString &appName)
{
    return QStringLiteral(
        "base=%1; app=%2; pids=''; first_cmd=''; "
        "for proc in /proc/[0-9]*; do "
        "pid=${proc##*/}; [ \"$pid\" = \"$$\" ] && continue; "
        "cmd=$(tr '\\000' ' ' < \"$proc/cmdline\" 2>/dev/null); "
        "cwd=$(readlink \"$proc/cwd\" 2>/dev/null); "
        "case \" $cmd $cwd \" in "
        "*\"$base/$app/\"*|*\" ./bin/$app \"*|*\" bin/$app \"*) "
        "pids=\"$pids $pid\"; [ -n \"$first_cmd\" ] || first_cmd=\"$cmd\";; "
        "esac; "
        "done")
        .arg(remoteProgramShellQuote(baseDir), remoteProgramShellQuote(appName));
}

QString programStatusScanCommand(const QString &baseDir, const QStringList &appNames)
{
    return QStringLiteral(
        "base=%1; apps=%2; "
        "for proc in /proc/[0-9]*; do "
        "pid=${proc##*/}; [ \"$pid\" = \"$$\" ] && continue; "
        "cmd=$(tr '\\000' ' ' < \"$proc/cmdline\" 2>/dev/null); "
        "cwd=$(readlink \"$proc/cwd\" 2>/dev/null); "
        "for app in $apps; do "
        "case \" $cmd $cwd \" in "
        "*\"$base/$app/\"*|*\" ./bin/$app \"*|*\" bin/$app \"*) "
        "echo \"$app|match|$pid|$cmd\"; break;; "
        "esac; "
        "done; "
        "done")
        .arg(remoteProgramShellQuote(baseDir), remoteProgramShellQuote(appNames.join(QLatin1Char(' '))));
}

QString programStatusColor(const QString &state)
{
    if (state == QStringLiteral("running")) {
        return QStringLiteral("#1f9d55");
    }
    if (state == QStringLiteral("stopped")) {
        return QStringLiteral("#c0392b");
    }
    if (state == QStringLiteral("pending")) {
        return QStringLiteral("#b9770e");
    }
    return QStringLiteral("#6c757d");
}

QTableWidgetItem *makeProgramItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setToolTip(text);
    return item;
}

} // namespace

QStringList MainWindow::managedProgramAppNames() const
{
    return {
        QStringLiteral("ServiceChannel"),
        QStringLiteral("IEC101ServiceChannel"),
        QStringLiteral("cepLogicCenter"),
        QStringLiteral("cepmodbus"),
        QStringLiteral("cepiec104"),
        QStringLiteral("cepdlt645")
    };
}

bool MainWindow::startProgramControlCommand(const QString &command,
                                            const QString &title,
                                            ProgramControlCommandKind kind,
                                            const QString &appName)
{
    if (!m_programControlShell || m_programControlShell->state() == QProcess::NotRunning) {
        closeProgramControlShell();
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 已断开，请手动连接"), 5000);
        QMessageBox::warning(this, QStringLiteral("程序控制"), QStringLiteral("请先在“程序控制”页面点击“连接”。"));
        return false;
    }
    if (m_programControlShellKey != programControlShellKey()) {
        statusBar()->showMessage(QStringLiteral("程序控制连接参数已变化，请断开后重新连接"), 5000);
        QMessageBox::warning(this, QStringLiteral("程序控制"), QStringLiteral("连接参数已变化，请先断开，再重新连接。"));
        return false;
    }
    if (m_programControlCommandRunning) {
        statusBar()->showMessage(QStringLiteral("程序控制正在执行上一条命令"), 3000);
        return false;
    }

    const QString token = QStringLiteral("__CEPB_PROGRAM_DONE_%1__").arg(++m_programControlCommandSerial);
    const QString wrappedCommand = QStringLiteral(
        "(\n%1\n); __cepb_program_code=$?; "
        "printf '\\n%2|%s\\n' \"$__cepb_program_code\"\n")
        .arg(command, token);

    m_programControlCommandRunning = true;
    m_programControlCommandKind = kind;
    m_programControlCommandToken = token;
    m_programControlCommandTitle = title;
    m_programControlCommandAppName = appName;
    m_programControlCommandBuffer.clear();
    m_programControlCommandBuffer.append(m_programControlShell->readAllStandardOutput());
    updateProgramControlBusyUi(true);

    m_programControlShell->write(wrappedCommand.toUtf8());
    if (m_programControlShell->state() == QProcess::NotRunning) {
        const QString error = m_programControlShell->errorString();
        closeProgramControlShell();
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("%1 失败。\n\n%2").arg(title, error));
        return false;
    }

    QTimer::singleShot(180000, this, [this, token]() {
        if (!m_programControlCommandRunning || m_programControlCommandToken != token) {
            return;
        }
        const QString title = m_programControlCommandTitle;
        clearProgramControlCommandState();
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("%1 超时。").arg(title));
        closeProgramControlShell();
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 已断开"), 5000);
    });

    statusBar()->showMessage(title, 3000);
    return true;
}

void MainWindow::handleProgramControlShellReadyRead()
{
    if (!m_programControlShell) {
        return;
    }

    if (!m_programControlCommandRunning) {
        m_programControlShell->readAllStandardOutput();
        return;
    }

    m_programControlCommandBuffer.append(m_programControlShell->readAllStandardOutput());

    const QByteArray tokenBytes = m_programControlCommandToken.toUtf8() + '|';
    const int tokenIndex = m_programControlCommandBuffer.indexOf(tokenBytes);
    if (tokenIndex < 0) {
        return;
    }

    const int codeStart = tokenIndex + tokenBytes.size();
    const int codeEnd = m_programControlCommandBuffer.indexOf('\n', codeStart);
    if (codeEnd < 0) {
        return;
    }

    bool ok = false;
    const int exitCode = m_programControlCommandBuffer.mid(codeStart, codeEnd - codeStart).trimmed().toInt(&ok);
    const QString output = QString::fromUtf8(m_programControlCommandBuffer.left(tokenIndex));
    finishProgramControlCommand(ok ? exitCode : -1, output);
}

void MainWindow::finishProgramControlCommand(int exitCode, const QString &output)
{
    const ProgramControlCommandKind kind = m_programControlCommandKind;
    const QString title = m_programControlCommandTitle;
    const QString appName = m_programControlCommandAppName;
    clearProgramControlCommandState();

    if (exitCode != 0) {
        switch (kind) {
        case ProgramControlCommandKind::Verify:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("验证程序控制 SSH 失败。\n\n%1").arg(output));
            closeProgramControlShell();
            statusBar()->showMessage(QStringLiteral("程序控制 SSH 连接失败"), 5000);
            break;
        case ProgramControlCommandKind::RefreshStatus:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("刷新程序状态失败。\n\n%1").arg(output));
            statusBar()->showMessage(QStringLiteral("刷新程序状态失败"), 5000);
            break;
        case ProgramControlCommandKind::Start: {
            const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
            const QString appDir = QStringLiteral("%1/%2").arg(baseDir, appName);
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("启动 %1 失败。\n\n请确认设备上存在 %2/bin/%1 且有执行权限。\n\n%3")
                                     .arg(appName, appDir, output));
            startProgramStatusRefresh();
            break;
        }
        case ProgramControlCommandKind::Stop:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("停止 %1 后仍有进程存在，可使用“强制”停止。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        case ProgramControlCommandKind::ForceStop:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("强制停止 %1 失败。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        case ProgramControlCommandKind::Restart:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("重启 %1 失败。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        }
        return;
    }

    switch (kind) {
    case ProgramControlCommandKind::Verify:
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 已连接"), 5000);
        break;
    case ProgramControlCommandKind::RefreshStatus:
        refreshProgramControlTable(output);
        statusBar()->showMessage(QStringLiteral("程序状态已刷新"), 5000);
        break;
    case ProgramControlCommandKind::Start:
    case ProgramControlCommandKind::Stop:
    case ProgramControlCommandKind::ForceStop:
    case ProgramControlCommandKind::Restart:
        statusBar()->showMessage(QStringLiteral("%1 完成").arg(title), 3000);
        startProgramStatusRefresh();
        break;
    }
}

void MainWindow::clearProgramControlCommandState()
{
    m_programControlCommandRunning = false;
    m_programControlCommandToken.clear();
    m_programControlCommandTitle.clear();
    m_programControlCommandAppName.clear();
    m_programControlCommandBuffer.clear();
    updateProgramControlBusyUi(false);
}

void MainWindow::closeProgramControlShell()
{
    if (!m_programControlShell) {
        return;
    }

    QProcess *shell = m_programControlShell;
    m_programControlShell = nullptr;
    m_programControlShellKey.clear();
    clearProgramControlCommandState();
    updateProgramControlConnectionUi(false);

    if (shell->state() != QProcess::NotRunning) {
        shell->write("exit\n");
        shell->waitForBytesWritten(500);
        if (!shell->waitForFinished(1000)) {
            shell->terminate();
            if (!shell->waitForFinished(1000)) {
                shell->kill();
                shell->waitForFinished(1000);
            }
        }
    }

    delete shell;
}

QString MainWindow::programControlShellKey() const
{
    const QString host = m_programRemoteHostEdit ? m_programRemoteHostEdit->text().trimmed() : QString();
    const QString user = m_programRemoteUserEdit ? m_programRemoteUserEdit->text().trimmed() : QString();
    const QString password = m_programRemotePasswordEdit ? m_programRemotePasswordEdit->text() : QString();
    const QString port = QString::number(m_programRemotePortEdit ? m_programRemotePortEdit->value() : 10022);
    const bool hasPutty = !QStandardPaths::findExecutable(QStringLiteral("plink")).isEmpty();
    return QStringList({hasPutty ? QStringLiteral("plink") : QStringLiteral("ssh"),
                        host,
                        user,
                        password,
                        port}).join(QLatin1Char('\n'));
}

QString MainWindow::programControlRemoteTarget() const
{
    const QString host = m_programRemoteHostEdit ? m_programRemoteHostEdit->text().trimmed() : QString();
    const QString user = m_programRemoteUserEdit ? m_programRemoteUserEdit->text().trimmed() : QString();
    if (host.isEmpty()) {
        return QString();
    }
    return user.isEmpty() ? host : QStringLiteral("%1@%2").arg(user, host);
}

bool MainWindow::openProgramControlShell(const QString &title,
                                         QString *output,
                                         QProgressDialog *progress)
{
    if (programControlRemoteTarget().isEmpty()) {
        if (output) {
            *output = QStringLiteral("请先填写程序控制设备地址。");
        }
        return false;
    }

    const QString key = programControlShellKey();
    if (m_programControlShell
        && m_programControlShell->state() != QProcess::NotRunning
        && m_programControlShellKey == key) {
        return true;
    }

    closeProgramControlShell();

    const QString host = m_programRemoteHostEdit ? m_programRemoteHostEdit->text().trimmed() : QString();
    const QString user = m_programRemoteUserEdit ? m_programRemoteUserEdit->text().trimmed() : QString();
    const QString password = m_programRemotePasswordEdit ? m_programRemotePasswordEdit->text() : QString();
    const QString port = QString::number(m_programRemotePortEdit ? m_programRemotePortEdit->value() : 10022);
    const bool hasPutty = !QStandardPaths::findExecutable(QStringLiteral("plink")).isEmpty();

    QString program;
    QStringList args;
    if (hasPutty) {
        program = QStringLiteral("plink");
        args = {QStringLiteral("-batch"), QStringLiteral("-ssh"), QStringLiteral("-P"), port};
        if (!user.isEmpty()) {
            args << QStringLiteral("-l") << user;
        }
        if (!password.isEmpty()) {
            args << QStringLiteral("-pw") << password;
        }
        args << host;
    } else {
        program = QStringLiteral("ssh");
        args = {QStringLiteral("-p"), port, programControlRemoteTarget()};
    }

    m_programControlShell = new QProcess(this);
    m_programControlShell->setProcessChannelMode(QProcess::MergedChannels);
    QProcess *shell = m_programControlShell;
    connect(shell, &QProcess::readyReadStandardOutput, this, &MainWindow::handleProgramControlShellReadyRead);
    connect(shell, &QProcess::finished, this, [this, shell]() {
        if (m_programControlShell != shell) {
            return;
        }
        m_programControlShell = nullptr;
        m_programControlShellKey.clear();
        clearProgramControlCommandState();
        updateProgramControlConnectionUi(false);
        shell->deleteLater();
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 已断开"), 5000);
    });
    if (progress) {
        progress->setLabelText(QStringLiteral("%1\n正在建立 SSH 长连接...").arg(title));
        progress->show();
        QApplication::processEvents();
    }

    m_programControlShell->start(program, args);
    if (!m_programControlShell->waitForStarted(10000)) {
        if (output) {
            *output = m_programControlShell->errorString();
        }
        closeProgramControlShell();
        return false;
    }

    m_programControlShellKey = key;
    updateProgramControlConnectionUi(true);
    return true;
}

void MainWindow::onConnectProgramControlClicked()
{
    QString output;
    QProgressDialog progress(QStringLiteral("连接程序控制 SSH"), QStringLiteral("取消"), 0, 0, this);
    progress.setWindowTitle(QStringLiteral("程序控制"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(300);

    if (!openProgramControlShell(QStringLiteral("连接程序控制 SSH"), &output, &progress)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("连接程序控制 SSH 失败。\n\n%1").arg(output));
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 连接失败"), 5000);
        return;
    }

    startProgramControlCommand(QStringLiteral(":"),
                               QStringLiteral("验证程序控制 SSH"),
                               ProgramControlCommandKind::Verify);
}

void MainWindow::onDisconnectProgramControlClicked()
{
    closeProgramControlShell();
    statusBar()->showMessage(QStringLiteral("程序控制 SSH 已断开"), 5000);
}

void MainWindow::updateProgramControlConnectionUi(bool connected)
{
    if (m_programRemoteHostEdit) {
        m_programRemoteHostEdit->setEnabled(!connected);
    }
    if (m_programRemoteUserEdit) {
        m_programRemoteUserEdit->setEnabled(!connected);
    }
    if (m_programRemotePasswordEdit) {
        m_programRemotePasswordEdit->setEnabled(!connected);
    }
    if (m_programRemotePortEdit) {
        m_programRemotePortEdit->setEnabled(!connected);
    }
    if (m_connectProgramControlBtn) {
        m_connectProgramControlBtn->setEnabled(!connected);
    }
    if (m_disconnectProgramControlBtn) {
        m_disconnectProgramControlBtn->setEnabled(connected && !m_programControlCommandRunning);
    }
    if (m_refreshProgramStatusBtn) {
        m_refreshProgramStatusBtn->setEnabled(connected && !m_programControlCommandRunning);
    }
}

void MainWindow::updateProgramControlBusyUi(bool busy)
{
    if (m_refreshProgramStatusBtn) {
        m_refreshProgramStatusBtn->setEnabled(!busy && m_programControlShell);
    }
    if (m_connectProgramControlBtn) {
        m_connectProgramControlBtn->setEnabled(!busy && !m_programControlShell);
    }
    if (m_disconnectProgramControlBtn) {
        m_disconnectProgramControlBtn->setEnabled(!busy && m_programControlShell);
    }
    if (!m_programControlTable) {
        return;
    }

    for (int row = 0; row < m_programControlTable->rowCount(); ++row) {
        for (int column : {ProgramColumnStart, ProgramColumnStop, ProgramColumnRestart}) {
            QWidget *widget = m_programControlTable->cellWidget(row, column);
            if (widget) {
                widget->setEnabled(!busy);
            }
        }
    }
}

void MainWindow::startProgramStatusRefresh()
{
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QStringList appNames = managedProgramAppNames();
    startProgramControlCommand(programStatusScanCommand(baseDir, appNames),
                               QStringLiteral("刷新程序状态"),
                               ProgramControlCommandKind::RefreshStatus);
}

void MainWindow::refreshProgramControlTable(const QString &statusOutput)
{
    struct ProgramStatus {
        QString state = QStringLiteral("unknown");
        QString pids;
        QString command;
    };

    QHash<QString, ProgramStatus> statusByApp;
    for (const QString &rawLine : statusOutput.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() < 3) {
            continue;
        }

        const QString appName = parts.value(0).trimmed();
        const QString state = parts.value(1).trimmed();
        if (state == QStringLiteral("match")) {
            ProgramStatus status = statusByApp.value(appName);
            status.state = QStringLiteral("running");
            const QString pid = parts.value(2).trimmed();
            if (!pid.isEmpty()) {
                if (!status.pids.isEmpty()) {
                    status.pids.append(QLatin1Char(' '));
                }
                status.pids.append(pid);
            }
            if (status.command.isEmpty()) {
                status.command = parts.mid(3).join(QStringLiteral("|")).trimmed();
            }
            statusByApp.insert(appName, status);
            continue;
        }

        ProgramStatus status;
        status.state = state;
        status.pids = parts.value(2).trimmed();
        status.command = parts.mid(3).join(QStringLiteral("|")).trimmed();
        statusByApp.insert(appName, status);
    }

    const QStringList appNames = managedProgramAppNames();
    m_programControlTable->setRowCount(appNames.size());

    for (int row = 0; row < appNames.size(); ++row) {
        const QString appName = appNames.at(row);
        ProgramStatus status = statusByApp.value(appName);
        if (!statusByApp.contains(appName)) {
            status.state = QStringLiteral("stopped");
        }
        const bool running = status.state == QStringLiteral("running");
        const QString statusText = running ? QStringLiteral("● 运行中")
            : status.state == QStringLiteral("stopped") ? QStringLiteral("● 未运行")
            : QStringLiteral("● 未知");

        auto *statusItem = makeProgramItem(statusText);
        statusItem->setForeground(QColor(programStatusColor(status.state)));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);

        m_programControlTable->setItem(row, ProgramColumnStatus, statusItem);
        m_programControlTable->setItem(row, ProgramColumnApp, makeProgramItem(appName));
        m_programControlTable->setItem(row, ProgramColumnPid, makeProgramItem(status.pids));
        m_programControlTable->setItem(row, ProgramColumnCommand, makeProgramItem(status.command));

        auto *startButton = new QPushButton(QStringLiteral("启动"), this);
        startButton->setProperty("appName", appName);
        startButton->setEnabled(!running);
        connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartProgramClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnStart, startButton);

        auto *stopContainer = new QWidget(this);
        auto *stopLayout = new QHBoxLayout(stopContainer);
        stopLayout->setContentsMargins(0, 0, 0, 0);
        stopLayout->setSpacing(4);
        auto *stopButton = new QPushButton(QStringLiteral("停止"), stopContainer);
        stopButton->setProperty("appName", appName);
        stopButton->setEnabled(running);
        connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopProgramClicked);
        auto *forceStopButton = new QPushButton(QStringLiteral("强制"), stopContainer);
        forceStopButton->setProperty("appName", appName);
        forceStopButton->setEnabled(running);
        connect(forceStopButton, &QPushButton::clicked, this, &MainWindow::onForceStopProgramClicked);
        stopLayout->addWidget(stopButton);
        stopLayout->addWidget(forceStopButton);
        m_programControlTable->setCellWidget(row, ProgramColumnStop, stopContainer);

        auto *restartButton = new QPushButton(QStringLiteral("重启"), this);
        restartButton->setProperty("appName", appName);
        restartButton->setEnabled(running);
        connect(restartButton, &QPushButton::clicked, this, &MainWindow::onRestartProgramClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnRestart, restartButton);
    }

    m_programControlTable->resizeRowsToContents();
}

void MainWindow::setProgramControlRowPending(const QString &appName, const QString &statusText)
{
    for (int row = 0; row < m_programControlTable->rowCount(); ++row) {
        QTableWidgetItem *appItem = m_programControlTable->item(row, ProgramColumnApp);
        if (!appItem || appItem->text() != appName) {
            continue;
        }

        auto *statusItem = makeProgramItem(QStringLiteral("● %1").arg(statusText));
        statusItem->setForeground(QColor(programStatusColor(QStringLiteral("pending"))));
        QFont font = statusItem->font();
        font.setBold(true);
        statusItem->setFont(font);
        m_programControlTable->setItem(row, ProgramColumnStatus, statusItem);
        return;
    }
}

void MainWindow::onRefreshProgramStatusClicked()
{
    startProgramStatusRefresh();
}

void MainWindow::onStartProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString appDir = QStringLiteral("%1/%2").arg(baseDir, appName);
    const QString command = QStringLiteral(
        "set -e; cd %1; test -x %2; nohup %2 >/dev/null 2>&1 </dev/null &")
        .arg(remoteProgramShellQuote(appDir), remoteProgramShellQuote(QStringLiteral("./bin/%1").arg(appName)));

    if (startProgramControlCommand(command,
                                   QStringLiteral("启动 %1").arg(appName),
                                   ProgramControlCommandKind::Start,
                                   appName)) {
        setProgramControlRowPending(appName, QStringLiteral("启动中"));
    }
}

void MainWindow::onStopProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString scan = programPidScanSnippet(baseDir, appName);
    const QString command = QStringLiteral(
        "%1; if [ -z \"$pids\" ]; then echo 'no process'; exit 0; fi; "
        "kill -TERM $pids; sleep 2; %1; "
        "if [ -n \"$pids\" ]; then echo \"still_running:${pids# }\"; exit 3; fi")
        .arg(scan);

    if (startProgramControlCommand(command,
                                   QStringLiteral("停止 %1").arg(appName),
                                   ProgramControlCommandKind::Stop,
                                   appName)) {
        setProgramControlRowPending(appName, QStringLiteral("停止中"));
    }
}

void MainWindow::onForceStopProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this,
        QStringLiteral("强制停止"),
        QStringLiteral("将对 %1 发送 SIGKILL，程序没有机会做退出清理。是否继续？").arg(appName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString scan = programPidScanSnippet(baseDir, appName);
    const QString command = QStringLiteral(
        "%1; if [ -z \"$pids\" ]; then echo 'no process'; exit 0; fi; kill -KILL $pids")
        .arg(scan);

    if (startProgramControlCommand(command,
                                   QStringLiteral("强制停止 %1").arg(appName),
                                   ProgramControlCommandKind::ForceStop,
                                   appName)) {
        setProgramControlRowPending(appName, QStringLiteral("强制停止中"));
    }
}

void MainWindow::onRestartProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString appDir = QStringLiteral("%1/%2").arg(baseDir, appName);
    const QString scan = programPidScanSnippet(baseDir, appName);
    const QString binary = QStringLiteral("./bin/%1").arg(appName);
    const QString command = QStringLiteral(
        "%1; [ -z \"$pids\" ] || kill -TERM $pids; sleep 2; "
        "%1; [ -z \"$pids\" ] || kill -KILL $pids; "
        "cd %2; test -x %3; nohup %3 >/dev/null 2>&1 </dev/null &")
        .arg(scan, remoteProgramShellQuote(appDir), remoteProgramShellQuote(binary));

    if (startProgramControlCommand(command,
                                   QStringLiteral("重启 %1").arg(appName),
                                   ProgramControlCommandKind::Restart,
                                   appName)) {
        setProgramControlRowPending(appName, QStringLiteral("重启中"));
    }
}
