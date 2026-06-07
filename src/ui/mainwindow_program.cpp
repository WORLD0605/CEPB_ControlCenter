#include "mainwindow.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>

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

bool MainWindow::runProgramControlCommand(const QString &command,
                                          const QString &title,
                                          QString *output)
{
    if (configRemoteTarget().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("程序控制"), QStringLiteral("请先在“配置概览”中填写设备地址。"));
        return false;
    }

    const QString host = m_configRemoteHostEdit ? m_configRemoteHostEdit->text().trimmed() : QString();
    const QString user = m_configRemoteUserEdit ? m_configRemoteUserEdit->text().trimmed() : QString();
    const QString password = m_configRemotePasswordEdit ? m_configRemotePasswordEdit->text() : QString();
    const QString port = QString::number(m_configRemotePortEdit ? m_configRemotePortEdit->value() : 10022);
    const bool hasPutty = !QStandardPaths::findExecutable(QStringLiteral("plink")).isEmpty();

    QProgressDialog progress(title, QStringLiteral("取消"), 0, 0, this);
    progress.setWindowTitle(QStringLiteral("程序控制"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(300);

    bool ok = false;
    if (hasPutty) {
        QStringList args = {QStringLiteral("-batch"), QStringLiteral("-ssh"), QStringLiteral("-P"), port};
        if (!user.isEmpty()) {
            args << QStringLiteral("-l") << user;
        }
        if (!password.isEmpty()) {
            args << QStringLiteral("-pw") << password;
        }
        args << host << command;
        ok = runConfigTransferProcess(QStringLiteral("plink"), args, title, output, &progress);
    } else {
        ok = runConfigTransferProcess(QStringLiteral("ssh"),
                                      {QStringLiteral("-p"), port, configRemoteTarget(), command},
                                      title,
                                      output,
                                      &progress);
    }

    return ok;
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

        ProgramStatus status;
        status.state = parts.value(1).trimmed();
        status.pids = parts.value(2).trimmed();
        status.command = parts.mid(3).join(QStringLiteral("|")).trimmed();
        statusByApp.insert(parts.value(0).trimmed(), status);
    }

    const QStringList appNames = managedProgramAppNames();
    m_programControlTable->setRowCount(appNames.size());

    for (int row = 0; row < appNames.size(); ++row) {
        const QString appName = appNames.at(row);
        const ProgramStatus status = statusByApp.value(appName);
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
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    QStringList lines;
    for (const QString &appName : managedProgramAppNames()) {
        lines << QStringLiteral("%1; if [ -n \"$pids\" ]; then "
                                "echo \"%2|running|${pids# }|$first_cmd\"; "
                                "else echo \"%2|stopped||\"; fi")
                     .arg(programPidScanSnippet(baseDir, appName), appName);
    }

    QString output;
    if (!runProgramControlCommand(lines.join(QStringLiteral("; ")),
                                  QStringLiteral("刷新程序状态"),
                                  &output)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("刷新程序状态失败。\n\n%1").arg(output));
        statusBar()->showMessage(QStringLiteral("刷新程序状态失败"), 5000);
        return;
    }

    refreshProgramControlTable(output);
    statusBar()->showMessage(QStringLiteral("程序状态已刷新"), 5000);
}

void MainWindow::onStartProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    setProgramControlRowPending(appName, QStringLiteral("启动中"));
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString appDir = QStringLiteral("%1/%2").arg(baseDir, appName);
    const QString command = QStringLiteral(
        "set -e; cd %1; test -x %2; nohup %2 >/dev/null 2>&1 </dev/null &")
        .arg(remoteProgramShellQuote(appDir), remoteProgramShellQuote(QStringLiteral("./bin/%1").arg(appName)));

    QString output;
    if (!runProgramControlCommand(command, QStringLiteral("启动 %1").arg(appName), &output)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("启动 %1 失败。\n\n请确认设备上存在 %2/bin/%1 且有执行权限。\n\n%3")
                                 .arg(appName, appDir, output));
    }

    onRefreshProgramStatusClicked();
}

void MainWindow::onStopProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    setProgramControlRowPending(appName, QStringLiteral("停止中"));
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString scan = programPidScanSnippet(baseDir, appName);
    const QString command = QStringLiteral(
        "%1; if [ -z \"$pids\" ]; then echo 'no process'; exit 0; fi; "
        "kill -TERM $pids; sleep 2; %1; "
        "if [ -n \"$pids\" ]; then echo \"still_running:${pids# }\"; exit 3; fi")
        .arg(scan);

    QString output;
    if (!runProgramControlCommand(command, QStringLiteral("停止 %1").arg(appName), &output)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("停止 %1 后仍有进程存在，可使用“强制”停止。\n\n%2").arg(appName, output));
    }

    onRefreshProgramStatusClicked();
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

    setProgramControlRowPending(appName, QStringLiteral("强制停止中"));
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString scan = programPidScanSnippet(baseDir, appName);
    const QString command = QStringLiteral(
        "%1; if [ -z \"$pids\" ]; then echo 'no process'; exit 0; fi; kill -KILL $pids")
        .arg(scan);

    QString output;
    if (!runProgramControlCommand(command, QStringLiteral("强制停止 %1").arg(appName), &output)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("强制停止 %1 失败。\n\n%2").arg(appName, output));
    }

    onRefreshProgramStatusClicked();
}

void MainWindow::onRestartProgramClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    const QString appName = button ? button->property("appName").toString() : QString();
    if (appName.isEmpty()) {
        return;
    }

    setProgramControlRowPending(appName, QStringLiteral("重启中"));
    const QString baseDir = trimRemoteBaseDir(configRemoteBaseDir());
    const QString appDir = QStringLiteral("%1/%2").arg(baseDir, appName);
    const QString scan = programPidScanSnippet(baseDir, appName);
    const QString binary = QStringLiteral("./bin/%1").arg(appName);
    const QString command = QStringLiteral(
        "%1; [ -z \"$pids\" ] || kill -TERM $pids; sleep 2; "
        "%1; [ -z \"$pids\" ] || kill -KILL $pids; "
        "cd %2; test -x %3; nohup %3 >/dev/null 2>&1 </dev/null &")
        .arg(scan, remoteProgramShellQuote(appDir), remoteProgramShellQuote(binary));

    QString output;
    if (!runProgramControlCommand(command, QStringLiteral("重启 %1").arg(appName), &output)) {
        QMessageBox::warning(this,
                             QStringLiteral("程序控制"),
                             QStringLiteral("重启 %1 失败。\n\n%2").arg(appName, output));
    }

    onRefreshProgramStatusClicked();
}
