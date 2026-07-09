#include "mainwindow_config_p.h"
#include "network/ssh_client.h"
#include "program_control_ssh_worker.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QPointer>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>

#include <memory>

namespace {

constexpr int ProgramColumnStatus = 0;
constexpr int ProgramColumnApp = 1;
constexpr int ProgramColumnPid = 2;
constexpr int ProgramColumnStart = 3;
constexpr int ProgramColumnStop = 4;
constexpr int ProgramColumnRestart = 5;
constexpr int ProgramColumnAutostart = 6;
constexpr int ProgramColumnInstall = 7;
constexpr int ProgramColumnUpgrade = 8;

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

QString programServiceNameForApp(const QString &appName)
{
    if (appName == QStringLiteral("North_CEP")) {
        return QStringLiteral("CEP-North_CEP.service");
    }
    if (appName == QStringLiteral("North_101")) {
        return QStringLiteral("CEP-North_101.service");
    }
    if (appName == QStringLiteral("North_104")) {
        return QStringLiteral("CEP-North_104.service");
    }
    if (appName == QStringLiteral("North_Mqtt")) {
        return QStringLiteral("CEP-North_Mqtt.service");
    }
    if (appName == QStringLiteral("LogicCenter")) {
        return QStringLiteral("CEP-LogicCenter.service");
    }
    if (appName == QStringLiteral("South_Modbus")) {
        return QStringLiteral("CEP-Modbus.service");
    }
    if (appName == QStringLiteral("South_104")) {
        return QStringLiteral("CEP-IEC104.service");
    }
    if (appName == QStringLiteral("South_645")) {
        return QStringLiteral("CEP-DLT645.service");
    }
    return appName + QStringLiteral(".service");
}

QString localAppBinaryPathForApp(const QString &appName)
{
    const QString binaryDirName = QStringLiteral("app_binaries");
    const QString appPath = QDir(QCoreApplication::applicationDirPath())
        .filePath(binaryDirName + QLatin1Char('/') + appName);
    if (QFileInfo::exists(appPath)) {
        return appPath;
    }

    const QString currentPath = QDir(QDir::currentPath())
        .filePath(binaryDirName + QLatin1Char('/') + appName);
    if (QFileInfo::exists(currentPath)) {
        return currentPath;
    }

    return appPath;
}

QString localServicePathForApp(const QString &serviceName)
{
    const QString serviceDirName = QStringLiteral("systemd");
    const QString releasePath = QDir(QCoreApplication::applicationDirPath())
        .filePath(serviceDirName + QLatin1Char('/') + serviceName);
    if (QFileInfo::exists(releasePath)) {
        return releasePath;
    }

    const QString currentPath = QDir(QDir::currentPath())
        .filePath(serviceDirName + QLatin1Char('/') + serviceName);
    if (QFileInfo::exists(currentPath)) {
        return currentPath;
    }

    const QString sourceTreePath = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("../scripts/systemd/") + serviceName);
    if (QFileInfo::exists(sourceTreePath)) {
        return QFileInfo(sourceTreePath).absoluteFilePath();
    }

    const QString currentSourceTreePath = QDir(QDir::currentPath())
        .filePath(QStringLiteral("scripts/systemd/") + serviceName);
    if (QFileInfo::exists(currentSourceTreePath)) {
        return currentSourceTreePath;
    }

    return releasePath;
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

QString programActionAndStatusCommand(const QString &actionCommand,
                                      const QString &serviceName,
                                      const QString &waitActiveState,
                                      const QString &waitAutostartState,
                                      const QString &baseDir,
                                      const QStringList &appNames)
{
    QString waitCondition;
    if (!waitActiveState.isEmpty()) {
        waitCondition = QStringLiteral(
            "state=$(systemctl show \"$wait_service\" -p ActiveState --value --no-page 2>/dev/null || true); "
            "[ \"$state\" = %1 ] && break; ")
            .arg(remoteProgramShellQuote(waitActiveState));
    } else if (!waitAutostartState.isEmpty()) {
        waitCondition = QStringLiteral(
            "enabled=$(systemctl is-enabled \"$wait_service\" 2>/dev/null || true); "
            "[ \"$enabled\" = %1 ] && break; ")
            .arg(remoteProgramShellQuote(waitAutostartState));
    }

    QString waitSnippet;
    if (!waitCondition.isEmpty()) {
        waitSnippet = QStringLiteral(
            "if [ \"$action_rc\" -eq 0 ]; then "
            "wait_service=%1; i=0; "
            "while [ \"$i\" -lt 15 ]; do "
            "%2"
            "sleep 0.2; i=$((i + 1)); "
            "done; "
            "fi; ")
            .arg(remoteProgramShellQuote(serviceName), waitCondition);
    }

    return QStringLiteral(
        "action_rc=0; { %1; } || action_rc=$?; "
        "%2"
        "%3"
        "exit \"$action_rc\"")
        .arg(actionCommand,
             waitSnippet,
             programStatusScanCommand(baseDir, appNames));
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

QString counterpartNorthboundProgram(const QString &appName)
{
    if (appName == QStringLiteral("North_CEP")) {
        return QStringLiteral("North_101");
    }
    if (appName == QStringLiteral("North_101")) {
        return QStringLiteral("North_CEP");
    }
    return QString();
}

bool programTableAppIsRunning(const QTableWidget *table, const QString &appName)
{
    if (!table) {
        return false;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        const QTableWidgetItem *appItem = table->item(row, ProgramColumnApp);
        if (!appItem || appItem->text() != appName) {
            continue;
        }
        const QTableWidgetItem *statusItem = table->item(row, ProgramColumnStatus);
        return statusItem && statusItem->text().contains(QStringLiteral("运行中"));
    }
    return false;
}

} // namespace

QStringList MainWindow::managedProgramAppNames() const
{
    return {
        QStringLiteral("North_CEP"),
        QStringLiteral("North_101"),
        QStringLiteral("North_104"),
        QStringLiteral("North_Mqtt"),
        QStringLiteral("LogicCenter"),
        QStringLiteral("South_Modbus"),
        QStringLiteral("South_104"),
        QStringLiteral("South_645")
    };
}

QString MainWindow::programServiceName(const QString &appName) const
{
    return programServiceNameForApp(appName);
}

QString MainWindow::localProgramBinaryPath(const QString &appName) const
{
    return localAppBinaryPathForApp(appName);
}

QString MainWindow::localProgramServicePath(const QString &appName) const
{
    return localServicePathForApp(programServiceName(appName));
}

QString MainWindow::remoteProgramBinaryPath(const QString &appName) const
{
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    return QStringLiteral("%1/%2/bin/%2").arg(baseDir, appName);
}

bool MainWindow::startProgramControlCommand(const QString &command,
                                            const QString &title,
                                            ProgramControlCommandKind kind,
                                            const QString &appName)
{
    if (!m_programControlConnected) {
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
    if (!m_programControlWorker || !m_programControlThread || !m_programControlThread->isRunning()) {
        closeProgramControlShell();
        statusBar()->showMessage(QStringLiteral("程序控制 SSH 已断开，请重新连接"), 5000);
        QMessageBox::warning(this, QStringLiteral("程序控制"), QStringLiteral("程序控制 SSH 已断开，请重新连接。"));
        return false;
    }

    const quint64 serial = ++m_programControlCommandSerial;
    m_programControlCommandRunning = true;
    m_programControlCommandKind = kind;
    m_programControlCommandTitle = title;
    m_programControlCommandAppName = appName;
    m_programControlCommandBuffer.clear();
    updateProgramControlBusyUi(true);
    statusBar()->showMessage(title, 3000);

    QMetaObject::invokeMethod(m_programControlWorker,
                              "runCommand",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, serial),
                              Q_ARG(int, static_cast<int>(kind)),
                              Q_ARG(QString, title),
                              Q_ARG(QString, appName),
                              Q_ARG(QString, command));
    return true;
}

void MainWindow::handleProgramControlShellReadyRead()
{
}

void MainWindow::finishProgramControlCommand(int exitCode, const QString &output)
{
    const ProgramControlCommandKind kind = m_programControlCommandKind;
    const QString title = m_programControlCommandTitle;
    const QString appName = m_programControlCommandAppName;
    clearProgramControlCommandState();
    if ((kind == ProgramControlCommandKind::Install || kind == ProgramControlCommandKind::Upgrade)
        && m_programControlUpgradeProgress) {
        m_programControlUpgradeProgress->close();
        m_programControlUpgradeProgress = nullptr;
    }

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
        case ProgramControlCommandKind::Install:
            QMessageBox::warning(this,
                                 QStringLiteral("程序安装"),
                                 QStringLiteral("安装 %1 失败。\n\n%2").arg(appName, output));
            startProgramStatusRefresh();
            break;
        case ProgramControlCommandKind::Upgrade:
            QMessageBox::warning(this,
                                 QStringLiteral("程序升级"),
                                 QStringLiteral("升级 %1 失败。\n\n%2").arg(appName, output));
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
        refreshProgramControlTable(output);
        statusBar()->showMessage(QStringLiteral("%1 完成").arg(title), 3000);
        break;
    case ProgramControlCommandKind::Install:
        QMessageBox::information(this,
                                 QStringLiteral("程序安装"),
                                 QStringLiteral("%1 完成。\n\n%2").arg(title, output.trimmed()));
        statusBar()->showMessage(QStringLiteral("%1 完成").arg(title), 3000);
        startProgramStatusRefresh();
        break;
    case ProgramControlCommandKind::Upgrade:
        QMessageBox::information(this,
                                 QStringLiteral("程序升级"),
                                 QStringLiteral("%1 完成。\n\n%2").arg(title, output.trimmed()));
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
    if (m_programControlUpgradeProgress) {
        m_programControlUpgradeProgress->close();
        m_programControlUpgradeProgress = nullptr;
    }

    ProgramControlSshWorker *worker = m_programControlWorker;
    QThread *thread = m_programControlThread;
    m_programControlWorker = nullptr;
    m_programControlThread = nullptr;

    if (worker && thread) {
        if (thread->isRunning()) {
            if (QThread::currentThread() != thread) {
                QMetaObject::invokeMethod(worker, "disconnectSession", Qt::BlockingQueuedConnection);
            }
            thread->quit();
            thread->wait(3000);
        }
    }

    m_programControlConnected = false;
    m_programControlShellKey.clear();
    clearProgramControlCommandState();
    updateProgramControlConnectionUi(false);
}

QString MainWindow::programControlShellKey() const
{
    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    const QString password = fixedRemotePassword();
    const QString port = fixedRemoteSshPort();
    return QStringList({QStringLiteral("libssh2"),
                        host,
                        user,
                        password,
                        port}).join(QLatin1Char('\n'));
}

QString MainWindow::programControlRemoteTarget() const
{
    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    if (host.isEmpty()) {
        return QString();
    }
    return user.isEmpty() ? host : QStringLiteral("%1@%2").arg(user, host);
}

void MainWindow::startOpenProgramControlShell(const QString &title,
                                              QProgressDialog *progress)
{
    if (programControlRemoteTarget().isEmpty()) {
        if (progress) {
            progress->close();
        }
        QMessageBox::warning(this, QStringLiteral("程序控制"), QStringLiteral("请先填写程序控制设备地址。"));
        return;
    }

    const QString key = programControlShellKey();
    if (m_programControlConnected && m_programControlShellKey == key) {
        if (progress) {
            progress->close();
        }
        startProgramStatusRefresh();
        return;
    }

    closeProgramControlShell();

    const QString host = deviceHost();
    const QString user = fixedRemoteUser();
    const QString password = fixedRemotePassword();
    const QString port = fixedRemoteSshPort();
    if (progress) {
        progress->setLabelText(QStringLiteral("%1\n正在验证 SSH 连接...").arg(title));
        progress->show();
    }

    m_programControlThread = new QThread(this);
    m_programControlWorker = new ProgramControlSshWorker();
    m_programControlWorker->moveToThread(m_programControlThread);
    connect(m_programControlThread, &QThread::finished, m_programControlWorker, &QObject::deleteLater);
    connect(m_programControlThread, &QThread::finished, m_programControlThread, &QObject::deleteLater);

    const QPointer<QProgressDialog> progressGuard(progress);
    connect(m_programControlWorker, &ProgramControlSshWorker::connected, this,
            [this, progressGuard](quint64 serial, const QString &connectedKey) {
                if (progressGuard) {
                    progressGuard->close();
                }
                handleProgramControlConnected(serial, connectedKey);
            },
            Qt::QueuedConnection);
    connect(m_programControlWorker, &ProgramControlSshWorker::connectFailed, this,
            [this, progressGuard](quint64 serial, const QString &message) {
                if (progressGuard) {
                    progressGuard->close();
                }
                handleProgramControlConnectFailed(serial, message);
            },
            Qt::QueuedConnection);
    connect(m_programControlWorker, &ProgramControlSshWorker::commandFinished,
            this, &MainWindow::handleProgramControlCommandFinished, Qt::QueuedConnection);
    connect(m_programControlWorker, &ProgramControlSshWorker::upgradeProgress,
            this, &MainWindow::handleProgramControlUpgradeProgress, Qt::QueuedConnection);

    m_programControlCommandRunning = true;
    updateProgramControlBusyUi(true);
    statusBar()->showMessage(title, 3000);

    const quint64 serial = ++m_programControlConnectSerial;
    m_programControlThread->start();
    QMetaObject::invokeMethod(m_programControlWorker,
                              "connectSession",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, serial),
                              Q_ARG(QString, key),
                              Q_ARG(QString, host),
                              Q_ARG(quint16, port.toUShort()),
                              Q_ARG(QString, user),
                              Q_ARG(QString, password),
                              Q_ARG(int, 10000));
}

void MainWindow::handleProgramControlConnected(quint64 serial, const QString &key)
{
    if (serial != m_programControlConnectSerial) {
        return;
    }

    m_programControlShellKey = key;
    m_programControlConnected = true;
    clearProgramControlCommandState();
    updateProgramControlConnectionUi(true);
    statusBar()->showMessage(QStringLiteral("程序控制 SSH 已连接"), 5000);
    startProgramStatusRefresh();
}

void MainWindow::handleProgramControlConnectFailed(quint64 serial, const QString &message)
{
    if (serial != m_programControlConnectSerial) {
        return;
    }

    closeProgramControlShell();
    QMessageBox::warning(this,
                         QStringLiteral("程序控制"),
                         QStringLiteral("连接程序控制 SSH 失败。\n\n%1").arg(message));
    statusBar()->showMessage(QStringLiteral("程序控制 SSH 连接失败"), 5000);
}

void MainWindow::handleProgramControlCommandFinished(quint64 serial,
                                                     int kind,
                                                     const QString &title,
                                                     const QString &appName,
                                                     int exitCode,
                                                     const QString &output)
{
    if (serial != m_programControlCommandSerial || !m_programControlCommandRunning) {
        return;
    }

    m_programControlCommandKind = static_cast<ProgramControlCommandKind>(kind);
    m_programControlCommandTitle = title;
    m_programControlCommandAppName = appName;
    finishProgramControlCommand(exitCode, output);
}

void MainWindow::handleProgramControlUpgradeProgress(quint64 serial,
                                                     const QString &appName,
                                                     qint64 sentBytes,
                                                     qint64 totalBytes)
{
    if (serial != m_programControlCommandSerial || !m_programControlUpgradeProgress) {
        return;
    }

    const int value = totalBytes > 0 ? int((sentBytes * 100) / totalBytes) : 100;
    m_programControlUpgradeProgress->setRange(0, 100);
    m_programControlUpgradeProgress->setValue(qBound(0, value, 100));
    m_programControlUpgradeProgress->setLabelText(QStringLiteral("正在上传 %1... %2/%3 KB")
                                                      .arg(appName)
                                                      .arg(sentBytes / 1024)
                                                      .arg(qMax<qint64>(1, totalBytes / 1024)));
}

void MainWindow::onConnectProgramControlClicked()
{
    auto *progress = new QProgressDialog(QStringLiteral("连接程序控制 SSH"),
                                         QString(),
                                         0,
                                         0,
                                         this);
    progress->setWindowTitle(QStringLiteral("程序控制"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setCancelButton(nullptr);

    startOpenProgramControlShell(QStringLiteral("连接程序控制 SSH"), progress);
}

void MainWindow::onDisconnectProgramControlClicked()
{
    closeProgramControlShell();
    statusBar()->showMessage(QStringLiteral("程序控制 SSH 已断开"), 5000);
}

void MainWindow::updateProgramControlConnectionUi(bool connected)
{
    if (m_ipEdit) {
        const bool debugConnected = anyDebugClientConnected();
        m_ipEdit->setEnabled(!connected && !debugConnected);
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
        m_refreshProgramStatusBtn->setEnabled(!busy && m_programControlConnected);
    }
    if (m_connectProgramControlBtn) {
        m_connectProgramControlBtn->setEnabled(!busy && !m_programControlConnected);
    }
    if (m_disconnectProgramControlBtn) {
        m_disconnectProgramControlBtn->setEnabled(!busy && m_programControlConnected);
    }
    if (!m_programControlTable) {
        return;
    }

    for (int row = 0; row < m_programControlTable->rowCount(); ++row) {
        for (int column : {ProgramColumnStart,
                           ProgramColumnStop,
                           ProgramColumnRestart,
                           ProgramColumnAutostart,
                           ProgramColumnInstall,
                           ProgramColumnUpgrade}) {
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
        const QString counterpart = counterpartNorthboundProgram(appName);
        const bool blockedByNorthboundPeer = !counterpart.isEmpty()
            && statusByApp.value(counterpart).state == QStringLiteral("active");
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
        startButton->setEnabled(!running && !blockedByNorthboundPeer);
        if (blockedByNorthboundPeer) {
            startButton->setToolTip(QStringLiteral("%1 与 %2 互斥，请先停止 %2").arg(appName, counterpart));
        }
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

        auto *autostartButton = makeCellButton(
            status.autostart == QStringLiteral("enabled") ? QStringLiteral("已启用")
            : status.autostart == QStringLiteral("disabled") ? QStringLiteral("未启用")
            : QStringLiteral("未知"),
            this);
        autostartButton->setProperty("appName", appName);
        autostartButton->setProperty("autostartEnabled", enabled);
        autostartButton->setProperty("autostartState", status.autostart);
        autostartButton->setToolTip(status.autostart == QStringLiteral("enabled")
            ? QStringLiteral("当前已启用开机自启，点击后禁用")
            : status.autostart == QStringLiteral("disabled")
                ? QStringLiteral("当前未启用开机自启，点击后启用")
                : QStringLiteral("未获取到开机自启状态，请刷新状态"));
        autostartButton->setEnabled(status.autostart == QStringLiteral("enabled")
                                    || status.autostart == QStringLiteral("disabled"));
        connect(autostartButton, &QPushButton::clicked, this, &MainWindow::onToggleProgramAutostartClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnAutostart, wrapCellButton(autostartButton));

        auto *installButton = makeCellButton(QStringLiteral("安装"), this);
        installButton->setProperty("appName", appName);
        installButton->setToolTip(QStringLiteral("创建设备端 APP 目录，上传 bin，并安装 systemd 服务"));
        connect(installButton, &QPushButton::clicked, this, &MainWindow::onInstallProgramClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnInstall, wrapCellButton(installButton));

        auto *upgradeButton = makeCellButton(QStringLiteral("升级"), this);
        upgradeButton->setProperty("appName", appName);
        upgradeButton->setToolTip(QStringLiteral("使用发布包内 app_binaries/%1 覆盖设备端 bin").arg(appName));
        connect(upgradeButton, &QPushButton::clicked, this, &MainWindow::onUpgradeProgramClicked);
        m_programControlTable->setCellWidget(row, ProgramColumnUpgrade, wrapCellButton(upgradeButton));
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

bool MainWindow::upgradeProgramBinary(const QString &appName,
                                      QString *output,
                                      QProgressDialog *progress)
{
    const QString localPath = localProgramBinaryPath(appName);
    if (!QFileInfo::exists(localPath)) {
        if (output) {
            *output = QStringLiteral("未找到本地升级文件：%1\n请将 %2 放到发布目录的 app_binaries 目录中。")
                .arg(localPath, appName);
        }
        return false;
    }

    const QString remotePath = remoteProgramBinaryPath(appName);
    const QString remoteTempPath = QStringLiteral("/tmp/cepb_upgrade_%1_%2")
        .arg(appName, QString::number(QDateTime::currentMSecsSinceEpoch()));
    const QString service = programServiceName(appName);
    const SshClient::Connection sshConnection{deviceHost(),
                                              fixedRemoteSshPort().toUShort(),
                                              fixedRemoteUser(),
                                              fixedRemotePassword()};

    if (progress) {
        progress->setWindowTitle(QStringLiteral("程序升级"));
        progress->setLabelText(QStringLiteral("正在上传 %1...").arg(appName));
        progress->setRange(0, 100);
        progress->setValue(0);
        progress->show();
        QApplication::processEvents();
    }

    QString scpError;
    auto updateProgress = [progress, appName](qint64 sent, qint64 total) {
        if (!progress) {
            return true;
        }
        const int value = total > 0 ? int((sent * 100) / total) : 100;
        progress->setLabelText(QStringLiteral("正在上传 %1... %2/%3 KB")
                                   .arg(appName)
                                   .arg(sent / 1024)
                                   .arg(qMax<qint64>(1, total / 1024)));
        progress->setValue(qBound(0, value, 100));
        QApplication::processEvents();
        return !progress->wasCanceled();
    };
    if (!SshClient::uploadFileScp(sshConnection, localPath, remoteTempPath, &scpError, updateProgress)) {
        if (output) {
            *output = scpError;
        }
        return false;
    }

    if (progress) {
        progress->setLabelText(QStringLiteral("正在停止服务并覆盖程序 %1...").arg(appName));
        progress->setRange(0, 0);
        QApplication::processEvents();
    }

    const QString command = QStringLiteral(
        "set -e; "
        "target=%1; tmp=%2; service=%3; "
        "target_dir=$(dirname \"$target\"); "
        "mkdir -p \"$target_dir\"; "
        "systemctl stop \"$service\" 2>/dev/null || true; "
        "install -m 0755 \"$tmp\" \"$target\"; "
        "rm -f \"$tmp\"; "
        "systemctl start \"$service\"; "
        "echo upgraded \"$target\"")
        .arg(remoteProgramShellQuote(remotePath),
             remoteProgramShellQuote(remoteTempPath),
             remoteProgramShellQuote(service));
    const SshClient::CommandResult result = SshClient::execCommand(sshConnection, command);
    if (output) {
        *output = result.output.isEmpty() ? result.error : result.output;
    }
    return result.ok;
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

    const QString counterpart = counterpartNorthboundProgram(appName);
    if (!counterpart.isEmpty() && programTableAppIsRunning(m_programControlTable, counterpart)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("%1 与 %2 互斥，请先停止 %2。").arg(appName, counterpart));
        return;
    }

    const QString service = programServiceName(appName);
    const QString command = programActionAndStatusCommand(
        QStringLiteral("systemctl start %1").arg(remoteProgramShellQuote(service)),
        service,
        QStringLiteral("active"),
        QString(),
        trimRemoteBaseDir(configRemoteBaseDir()),
        managedProgramAppNames());

    setProgramControlRowPending(appName, QStringLiteral("启动中"));
    QApplication::processEvents();
    startProgramControlCommand(command,
                               QStringLiteral("启动 %1").arg(appName),
                               ProgramControlCommandKind::Start,
                               appName);
}

void MainWindow::onStopProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const QString service = programServiceName(appName);
    const QString command = programActionAndStatusCommand(
        QStringLiteral("systemctl stop %1").arg(remoteProgramShellQuote(service)),
        service,
        QStringLiteral("inactive"),
        QString(),
        trimRemoteBaseDir(configRemoteBaseDir()),
        managedProgramAppNames());

    setProgramControlRowPending(appName, QStringLiteral("停止中"));
    QApplication::processEvents();
    startProgramControlCommand(command,
                               QStringLiteral("停止 %1").arg(appName),
                               ProgramControlCommandKind::Stop,
                               appName);
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
    const QString command = programActionAndStatusCommand(
        QStringLiteral("systemctl kill --signal=SIGKILL %1").arg(remoteProgramShellQuote(service)),
        service,
        QStringLiteral("inactive"),
        QString(),
        trimRemoteBaseDir(configRemoteBaseDir()),
        managedProgramAppNames());

    setProgramControlRowPending(appName, QStringLiteral("强制停止中"));
    QApplication::processEvents();
    startProgramControlCommand(command,
                               QStringLiteral("强制停止 %1").arg(appName),
                               ProgramControlCommandKind::ForceStop,
                               appName);
}

void MainWindow::onRestartProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    const QString service = programServiceName(appName);
    const QString command = programActionAndStatusCommand(
        QStringLiteral("systemctl restart %1").arg(remoteProgramShellQuote(service)),
        service,
        QStringLiteral("active"),
        QString(),
        trimRemoteBaseDir(configRemoteBaseDir()),
        managedProgramAppNames());

    setProgramControlRowPending(appName, QStringLiteral("重启中"));
    QApplication::processEvents();
    startProgramControlCommand(command,
                               QStringLiteral("重启 %1").arg(appName),
                               ProgramControlCommandKind::Restart,
                               appName);
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
    const QString command = programActionAndStatusCommand(
        QStringLiteral("systemctl %1 %2").arg(action, remoteProgramShellQuote(service)),
        service,
        QString(),
        autostartEnabled ? QStringLiteral("disabled") : QStringLiteral("enabled"),
        trimRemoteBaseDir(configRemoteBaseDir()),
        managedProgramAppNames());
    setProgramControlRowPending(appName, autostartEnabled ? QStringLiteral("禁用自启中") : QStringLiteral("启用自启中"));
    QApplication::processEvents();
    startProgramControlCommand(command,
                               title,
                               kind,
                               appName);
}

void MainWindow::onInstallProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    if (!m_programControlConnected) {
        closeProgramControlShell();
        QMessageBox::warning(this, QStringLiteral("程序安装"), QStringLiteral("请先连接程序控制 SSH。"));
        return;
    }
    if (m_programControlShellKey != programControlShellKey()) {
        QMessageBox::warning(this, QStringLiteral("程序安装"), QStringLiteral("连接参数已变化，请断开后重新连接。"));
        return;
    }
    if (m_programControlCommandRunning) {
        statusBar()->showMessage(QStringLiteral("程序控制正在执行上一条命令"), 3000);
        return;
    }
    if (!m_programControlWorker || !m_programControlThread || !m_programControlThread->isRunning()) {
        closeProgramControlShell();
        QMessageBox::warning(this, QStringLiteral("程序安装"), QStringLiteral("程序控制 SSH 已断开，请重新连接。"));
        return;
    }

    const QString localBinaryPath = localProgramBinaryPath(appName);
    const QString localServicePath = localProgramServicePath(appName);
    const QString service = programServiceName(appName);
    const QString remoteAppDir = QStringLiteral("%1/%2").arg(trimRemoteBaseDir(configRemoteBaseDir()), appName);
    const QString remoteBinaryPath = remoteProgramBinaryPath(appName);
    if (!QFileInfo::exists(localBinaryPath)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序安装"),
                             QStringLiteral("未找到本地 APP 文件：\n%1").arg(localBinaryPath));
        return;
    }
    if (!QFileInfo::exists(localServicePath)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序安装"),
                             QStringLiteral("未找到本地 Service 文件：\n%1").arg(localServicePath));
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::question(
        this,
        QStringLiteral("程序安装"),
        QStringLiteral("将在设备端安装 %1：\n\nAPP 目录：%2\n程序文件：%3\nService：/etc/systemd/system/%4\n\n安装会创建 bin/dev/model/etc/log/config 目录，并执行 systemctl daemon-reload。是否继续？")
            .arg(appName, remoteAppDir, remoteBinaryPath, service),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    const QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString remoteBinaryTempPath = QStringLiteral("/tmp/cepb_install_%1_%2.bin").arg(appName, timestamp);
    const QString remoteServiceTempPath = QStringLiteral("/tmp/cepb_install_%1_%2.service").arg(appName, timestamp);
    const QString command = QStringLiteral(
        "set -e; "
        "app_dir=%1; bin_tmp=%2; service_tmp=%3; service=%4; app_bin=%5; "
        "mkdir -p \"$app_dir\" \"$app_dir/bin\" \"$app_dir/dev\" \"$app_dir/model\" \"$app_dir/etc\" \"$app_dir/log\" \"$app_dir/config\"; "
        "install -m 0755 \"$bin_tmp\" \"$app_dir/bin/$app_bin\"; "
        "install -m 0644 \"$service_tmp\" \"/etc/systemd/system/$service\"; "
        "rm -f \"$bin_tmp\" \"$service_tmp\"; "
        "systemctl daemon-reload; "
        "echo installed \"$app_dir\"; "
        "echo installed \"/etc/systemd/system/$service\"")
        .arg(remoteProgramShellQuote(remoteAppDir),
             remoteProgramShellQuote(remoteBinaryTempPath),
             remoteProgramShellQuote(remoteServiceTempPath),
             remoteProgramShellQuote(service),
             remoteProgramShellQuote(appName));

    const quint64 serial = ++m_programControlCommandSerial;
    m_programControlCommandRunning = true;
    m_programControlCommandKind = ProgramControlCommandKind::Install;
    m_programControlCommandTitle = QStringLiteral("安装 %1").arg(appName);
    m_programControlCommandAppName = appName;
    updateProgramControlBusyUi(true);
    setProgramControlRowPending(appName, QStringLiteral("安装中"));
    statusBar()->showMessage(QStringLiteral("正在安装 %1").arg(appName), 3000);

    auto *progress = new QProgressDialog(QStringLiteral("正在上传 %1...").arg(appName),
                                         QString(),
                                         0,
                                         100,
                                         this);
    progress->setWindowTitle(QStringLiteral("程序安装"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setCancelButton(nullptr);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    connect(progress, &QObject::destroyed, this, [this, progress]() {
        if (m_programControlUpgradeProgress == progress) {
            m_programControlUpgradeProgress = nullptr;
        }
    });
    m_programControlUpgradeProgress = progress;
    progress->show();

    QMetaObject::invokeMethod(m_programControlWorker,
                              "runInstall",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, serial),
                              Q_ARG(int, static_cast<int>(ProgramControlCommandKind::Install)),
                              Q_ARG(QString, m_programControlCommandTitle),
                              Q_ARG(QString, appName),
                              Q_ARG(QString, localBinaryPath),
                              Q_ARG(QString, remoteBinaryTempPath),
                              Q_ARG(QString, localServicePath),
                              Q_ARG(QString, remoteServiceTempPath),
                              Q_ARG(QString, command));
}

void MainWindow::onUpgradeProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    if (!m_programControlConnected) {
        closeProgramControlShell();
        QMessageBox::warning(this, QStringLiteral("程序升级"), QStringLiteral("请先连接程序控制 SSH。"));
        return;
    }
    if (m_programControlShellKey != programControlShellKey()) {
        QMessageBox::warning(this, QStringLiteral("程序升级"), QStringLiteral("连接参数已变化，请断开后重新连接。"));
        return;
    }
    if (m_programControlCommandRunning) {
        statusBar()->showMessage(QStringLiteral("程序控制正在执行上一条命令"), 3000);
        return;
    }

    const QString localPath = localProgramBinaryPath(appName);
    const QString remotePath = remoteProgramBinaryPath(appName);
    const QMessageBox::StandardButton confirm = QMessageBox::question(
        this,
        QStringLiteral("程序升级"),
        QStringLiteral("将使用本地文件覆盖设备端程序：\n\n%1\n-> %2\n\n升级过程会停止 %3，但不会自动重新启动，是否继续？")
            .arg(localPath, remotePath, appName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    if (!m_programControlWorker || !m_programControlThread || !m_programControlThread->isRunning()) {
        closeProgramControlShell();
        QMessageBox::warning(this, QStringLiteral("程序升级"), QStringLiteral("程序控制 SSH 已断开，请重新连接。"));
        return;
    }

    const QString remoteTempPath = QStringLiteral("/tmp/cepb_upgrade_%1_%2")
        .arg(appName, QString::number(QDateTime::currentMSecsSinceEpoch()));
    const QString service = programServiceName(appName);
    const QString command = QStringLiteral(
        "set -e; "
        "target=%1; tmp=%2; service=%3; "
        "target_dir=$(dirname \"$target\"); "
        "mkdir -p \"$target_dir\"; "
        "systemctl stop \"$service\" 2>/dev/null || true; "
        "install -m 0755 \"$tmp\" \"$target\"; "
        "rm -f \"$tmp\"; "
        "echo upgraded \"$target\"")
        .arg(remoteProgramShellQuote(remotePath),
             remoteProgramShellQuote(remoteTempPath),
             remoteProgramShellQuote(service));

    const quint64 serial = ++m_programControlCommandSerial;
    m_programControlCommandRunning = true;
    m_programControlCommandKind = ProgramControlCommandKind::Upgrade;
    m_programControlCommandTitle = QStringLiteral("升级 %1").arg(appName);
    m_programControlCommandAppName = appName;
    updateProgramControlBusyUi(true);
    setProgramControlRowPending(appName, QStringLiteral("升级中"));
    statusBar()->showMessage(QStringLiteral("正在升级 %1").arg(appName), 3000);

    auto *progress = new QProgressDialog(QStringLiteral("正在上传 %1...").arg(appName),
                                         QString(),
                                         0,
                                         100,
                                         this);
    progress->setWindowTitle(QStringLiteral("程序升级"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setCancelButton(nullptr);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    connect(progress, &QObject::destroyed, this, [this, progress]() {
        if (m_programControlUpgradeProgress == progress) {
            m_programControlUpgradeProgress = nullptr;
        }
    });
    m_programControlUpgradeProgress = progress;
    progress->show();

    QMetaObject::invokeMethod(m_programControlWorker,
                              "runUpgrade",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, serial),
                              Q_ARG(int, static_cast<int>(ProgramControlCommandKind::Upgrade)),
                              Q_ARG(QString, m_programControlCommandTitle),
                              Q_ARG(QString, appName),
                              Q_ARG(QString, localPath),
                              Q_ARG(QString, remoteTempPath),
                              Q_ARG(QString, command));
}
