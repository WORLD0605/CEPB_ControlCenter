#include "mainwindow_config_p.h"

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
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

namespace {

constexpr int ProgramColumnStatus = 0;
constexpr int ProgramColumnApp = 1;
constexpr int ProgramColumnPid = 2;
constexpr int ProgramColumnStart = 3;
constexpr int ProgramColumnStop = 4;
constexpr int ProgramColumnRestart = 5;
constexpr int ProgramColumnAutostart = 6;

QString remoteProgramShellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(quoted);
}

using cepb_config_helpers::findRemoteToolExecutable;
using cepb_config_helpers::missingPasswordSshToolMessage;
using cepb_config_helpers::puttyHostKeyPromptNeedsAccept;

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

QString programServiceNameForApp(const QString &appName)
{
    if (appName == QStringLiteral("ServiceChannel")) {
        return QStringLiteral("CEP-ServiceChannel.service");
    }
    if (appName == QStringLiteral("IEC101ServiceChannel")) {
        return QStringLiteral("CEP-IEC101ServiceChannel.service");
    }
    if (appName == QStringLiteral("cepLogicCenter")) {
        return QStringLiteral("CEP-LogicCenter.service");
    }
    if (appName == QStringLiteral("cepmodbus")) {
        return QStringLiteral("CEP-Modbus.service");
    }
    if (appName == QStringLiteral("cepiec104")) {
        return QStringLiteral("CEP-IEC104.service");
    }
    if (appName == QStringLiteral("cepdlt645")) {
        return QStringLiteral("CEP-DLT645.service");
    }
    return appName + QStringLiteral(".service");
}

QString programStatusScanCommand(const QString &baseDir, const QStringList &appNames)
{
    Q_UNUSED(baseDir);
    QString command;
    for (const QString &appName : appNames) {
        const QString service = programServiceNameForApp(appName);
        command += QStringLiteral(
            "app=%2; service=%1; "
            "active=$(systemctl show \"$service\" -p ActiveState --value --no-page 2>/dev/null || true); "
            "sub=$(systemctl show \"$service\" -p SubState --value --no-page 2>/dev/null || true); "
            "pid=$(systemctl show \"$service\" -p MainPID --value --no-page 2>/dev/null || true); "
            "enabled=$(systemctl is-enabled \"$service\" 2>/dev/null || true); "
            "echo \"$app|$service|${active:-unknown}|${sub:-unknown}|${pid:-0}|${enabled:-unknown}\"; ")
            .arg(remoteProgramShellQuote(service), remoteProgramShellQuote(appName));
    }
    return command;
}

QString legacyProgramStatusScanCommand(const QString &baseDir, const QStringList &appNames)
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

QString MainWindow::programServiceName(const QString &appName) const
{
    return programServiceNameForApp(appName);
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
        const QByteArray output = m_programControlShell->readAllStandardOutput();
        if (!m_programControlShell->property("puttyHostKeyAccepted").toBool()
            && puttyHostKeyPromptNeedsAccept(output)) {
            m_programControlShell->write("y\n");
            m_programControlShell->waitForBytesWritten(1000);
            m_programControlShell->setProperty("puttyHostKeyAccepted", true);
        }
        return;
    }

    m_programControlCommandBuffer.append(m_programControlShell->readAllStandardOutput());
    if (!m_programControlShell->property("puttyHostKeyAccepted").toBool()
        && puttyHostKeyPromptNeedsAccept(m_programControlCommandBuffer)) {
        m_programControlShell->write("y\n");
        m_programControlShell->waitForBytesWritten(1000);
        m_programControlShell->setProperty("puttyHostKeyAccepted", true);
    }

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
        case ProgramControlCommandKind::Start:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("启动 %1 失败。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        case ProgramControlCommandKind::Stop:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("停止 %1 失败。\n\n%2").arg(appName, output));
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
        case ProgramControlCommandKind::EnableAutostart:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("启用 %1 开机自启失败。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        case ProgramControlCommandKind::DisableAutostart:
            QMessageBox::warning(this,
                                 QStringLiteral("程序控制"),
                                 QStringLiteral("禁用 %1 开机自启失败。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        }
        return;
    }

    switch (kind) {
    case ProgramControlCommandKind::Verify:
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 已连接"), 5000);
        startProgramStatusRefresh();
        break;
    case ProgramControlCommandKind::RefreshStatus:
        refreshProgramControlTable(output);
        statusBar()->showMessage(QStringLiteral("程序状态已刷新"), 5000);
        break;
    case ProgramControlCommandKind::Start:
    case ProgramControlCommandKind::Stop:
    case ProgramControlCommandKind::ForceStop:
    case ProgramControlCommandKind::Restart:
    case ProgramControlCommandKind::EnableAutostart:
    case ProgramControlCommandKind::DisableAutostart:
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
    const bool usePuttyPasswordLogin = !password.isEmpty()
        && !findRemoteToolExecutable(QStringLiteral("plink")).isEmpty();
    return QStringList({usePuttyPasswordLogin ? QStringLiteral("plink") : QStringLiteral("ssh"),
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
    const QString plinkPath = findRemoteToolExecutable(QStringLiteral("plink"));
    const bool usePuttyPasswordLogin = !password.isEmpty() && !plinkPath.isEmpty();
    if (!password.isEmpty() && !usePuttyPasswordLogin) {
        if (output) {
            *output = missingPasswordSshToolMessage();
        }
        return false;
    }

    QString program;
    QStringList args;
    if (usePuttyPasswordLogin) {
        program = plinkPath;
        args = {QStringLiteral("-ssh"), QStringLiteral("-P"), port};
        if (!user.isEmpty()) {
            args << QStringLiteral("-l") << user;
        }
        if (!password.isEmpty()) {
            args << QStringLiteral("-pw") << password;
        }
        args << host;
    } else {
        program = QStringLiteral("ssh");
        args = {QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
                QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=accept-new"),
                QStringLiteral("-o"), QStringLiteral("ConnectTimeout=10"),
                QStringLiteral("-p"), port, programControlRemoteTarget()};
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
    if (usePuttyPasswordLogin) {
        QByteArray initialOutput;
        QElapsedTimer elapsed;
        elapsed.start();
        while (elapsed.elapsed() < 3000 && m_programControlShell->state() != QProcess::NotRunning) {
            if (!m_programControlShell->waitForReadyRead(100)) {
                continue;
            }
            initialOutput.append(m_programControlShell->readAllStandardOutput());
            if (puttyHostKeyPromptNeedsAccept(initialOutput)) {
                m_programControlShell->write("y\n");
                m_programControlShell->waitForBytesWritten(1000);
                m_programControlShell->setProperty("puttyHostKeyAccepted", true);
                break;
            }
        }
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
        for (int column : {ProgramColumnStart,
                           ProgramColumnStop,
                           ProgramColumnRestart,
                           ProgramColumnAutostart}) {
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
        QString subState = QStringLiteral("unknown");
        QString service;
        QString autostart = QStringLiteral("unknown");
        QString pids;
    };

    QHash<QString, ProgramStatus> statusByApp;
    for (const QString &rawLine : statusOutput.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() < 6) {
            continue;
        }

        const QString appName = parts.value(0).trimmed();
        ProgramStatus status;
        status.service = parts.value(1).trimmed();
        status.state = parts.value(2).trimmed();
        status.subState = parts.value(3).trimmed();
        status.pids = parts.value(4).trimmed();
        status.autostart = parts.value(5).trimmed();
        statusByApp.insert(appName, status);
    }
    if (statusByApp.isEmpty() && !statusOutput.trimmed().isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("无法解析程序状态输出。\n\n%1").arg(statusOutput.trimmed()));
    }

    const QStringList appNames = managedProgramAppNames();
    m_programControlTable->setRowCount(appNames.size());

    auto makeCellButton = [](const QString &text, QWidget *parent) {
        auto *button = new QPushButton(text, parent);
        button->setObjectName(QStringLiteral("programControlCellButton"));
        return button;
    };
    auto wrapCellButton = [](QPushButton *button) {
        auto *container = new QWidget(button->parentWidget());
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(8, 3, 8, 3);
        layout->setSpacing(0);
        layout->addWidget(button);
        return container;
    };

    for (int row = 0; row < appNames.size(); ++row) {
        const QString appName = appNames.at(row);
        ProgramStatus status = statusByApp.value(appName);
        if (!statusByApp.contains(appName)) {
            status.service = programServiceName(appName);
            status.state = QStringLiteral("unknown");
            status.subState = QStringLiteral("unknown");
            status.pids = QStringLiteral("0");
            status.autostart = QStringLiteral("unknown");
        }
        const bool running = status.state == QStringLiteral("active");
        const bool enabled = status.autostart == QStringLiteral("enabled");
        const QString statusText = running ? QStringLiteral("● 运行中")
            : status.state == QStringLiteral("inactive") ? QStringLiteral("● 未运行")
            : status.state == QStringLiteral("failed") ? QStringLiteral("● 失败")
            : QStringLiteral("● %1").arg(status.state);
        const QString pidText = status.pids == QStringLiteral("0") ? QString() : status.pids;

        auto *statusItem = makeProgramItem(statusText);
        statusItem->setForeground(QColor(programStatusColor(running ? QStringLiteral("running")
            : status.state == QStringLiteral("inactive") ? QStringLiteral("stopped")
            : QStringLiteral("unknown"))));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);
        statusItem->setToolTip(QStringLiteral("ActiveState=%1\nSubState=%2").arg(status.state, status.subState));

        m_programControlTable->setItem(row, ProgramColumnStatus, statusItem);
        auto *appItem = makeProgramItem(appName);
        appItem->setToolTip(status.service);
        m_programControlTable->setItem(row, ProgramColumnApp, appItem);
        m_programControlTable->setItem(row, ProgramColumnPid, makeProgramItem(pidText));

        auto *startButton = makeCellButton(QStringLiteral("启动"), this);
        startButton->setProperty("appName", appName);
        startButton->setEnabled(!running);
        connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartProgramClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnStart, wrapCellButton(startButton));

        auto *stopContainer = new QWidget(this);
        auto *stopLayout = new QHBoxLayout(stopContainer);
        stopLayout->setContentsMargins(8, 3, 8, 3);
        stopLayout->setSpacing(6);
        auto *stopButton = makeCellButton(QStringLiteral("停止"), stopContainer);
        stopButton->setProperty("appName", appName);
        stopButton->setEnabled(running);
        connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopProgramClicked);
        auto *forceStopButton = makeCellButton(QStringLiteral("强制"), stopContainer);
        forceStopButton->setProperty("appName", appName);
        forceStopButton->setEnabled(running);
        connect(forceStopButton, &QPushButton::clicked, this, &MainWindow::onForceStopProgramClicked);
        stopLayout->addWidget(stopButton);
        stopLayout->addWidget(forceStopButton);
        m_programControlTable->setCellWidget(row, ProgramColumnStop, stopContainer);

        auto *restartButton = makeCellButton(QStringLiteral("重启"), this);
        restartButton->setProperty("appName", appName);
        restartButton->setEnabled(running);
        connect(restartButton, &QPushButton::clicked, this, &MainWindow::onRestartProgramClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnRestart, wrapCellButton(restartButton));

        auto *autostartButton = makeCellButton(enabled ? QStringLiteral("禁用自启") : QStringLiteral("启用自启"), this);
        autostartButton->setProperty("appName", appName);
        autostartButton->setProperty("autostartEnabled", enabled);
        autostartButton->setToolTip(enabled
            ? QStringLiteral("当前已启用开机自启，点击后禁用")
            : QStringLiteral("当前未启用开机自启，点击后启用"));
        autostartButton->setEnabled(status.autostart == QStringLiteral("enabled")
                                    || status.autostart == QStringLiteral("disabled"));
        connect(autostartButton, &QPushButton::clicked, this, &MainWindow::onToggleProgramAutostartClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnAutostart, wrapCellButton(autostartButton));
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

    const QString service = programServiceName(appName);
    const QString command = QStringLiteral("systemctl start %1").arg(remoteProgramShellQuote(service));

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

    const QString service = programServiceName(appName);
    const QString command = QStringLiteral("systemctl stop %1").arg(remoteProgramShellQuote(service));

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

    const QString service = programServiceName(appName);
    const QString command = QStringLiteral("systemctl kill --signal=SIGKILL %1").arg(remoteProgramShellQuote(service));

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

    const QString service = programServiceName(appName);
    const QString command = QStringLiteral("systemctl restart %1").arg(remoteProgramShellQuote(service));

    if (startProgramControlCommand(command,
                                   QStringLiteral("重启 %1").arg(appName),
                                   ProgramControlCommandKind::Restart,
                                   appName)) {
        setProgramControlRowPending(appName, QStringLiteral("重启中"));
    }
}

void MainWindow::onToggleProgramAutostartClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const bool autostartEnabled = button->property("autostartEnabled").toBool();
    const QString service = programServiceName(appName);
    const QString action = autostartEnabled ? QStringLiteral("disable") : QStringLiteral("enable");
    const ProgramControlCommandKind kind = autostartEnabled
        ? ProgramControlCommandKind::DisableAutostart
        : ProgramControlCommandKind::EnableAutostart;
    const QString title = autostartEnabled
        ? QStringLiteral("禁用 %1 开机自启").arg(appName)
        : QStringLiteral("启用 %1 开机自启").arg(appName);
    const QString command = QStringLiteral("systemctl %1 %2").arg(action, remoteProgramShellQuote(service));
    startProgramControlCommand(command,
                               title,
                               kind,
                               appName);
}
