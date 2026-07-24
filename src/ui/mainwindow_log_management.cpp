#include "mainwindow.h"
#include "network/ssh_client.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace {

constexpr int kDefaultRetentionDays = 7;
constexpr int kMaximumRetentionDays = 3650;

QStringList logManagedAppNames()
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

int positiveDays(const QJsonValue &value, int fallback)
{
    bool ok = false;
    const int days = value.toVariant().toInt(&ok);
    return ok && days > 0 ? qBound(1, days, kMaximumRetentionDays) : fallback;
}

QSpinBox *retentionSpinBox(QTableWidget *table, int row)
{
    return table ? qobject_cast<QSpinBox *>(table->cellWidget(row, 2)) : nullptr;
}

QCheckBox *retentionOverrideCheckBox(QTableWidget *table, int row)
{
    return table ? qobject_cast<QCheckBox *>(table->cellWidget(row, 1)) : nullptr;
}

QString remoteLogShellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(quoted);
}

QString normalizedRemoteLogBaseDir(QString baseDir)
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

QString displayLogType(const QString &logType)
{
    return logType == QStringLiteral("msg") ? QStringLiteral("报文")
                                             : QStringLiteral("运行日志");
}

QString displayFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
}

QString logDateFromFileName(const QString &fileName, const QString &fallbackDate)
{
    static const QRegularExpression datePattern(QStringLiteral("(20\\d{2}-\\d{2}-\\d{2})"));
    const QRegularExpressionMatch match = datePattern.match(fileName);
    if (match.hasMatch()) {
        const QString candidate = match.captured(1);
        if (QDate::fromString(candidate, Qt::ISODate).isValid()) {
            return candidate;
        }
    }
    return fallbackDate;
}

} // namespace

void MainWindow::setupLogManagementPage()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("日志管理"));
    dialog->setModal(false);
    dialog->resize(900, 650);
    m_logManagementPage = dialog;

    auto *layout = new QVBoxLayout(m_logManagementPage);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_logRetentionProjectLabel = new QLabel(QStringLiteral("当前工程：未选择"), m_logManagementPage);
    m_logRetentionProjectLabel->setWordWrap(true);
    layout->addWidget(m_logRetentionProjectLabel);

    auto *tabs = new QTabWidget(m_logManagementPage);
    layout->addWidget(tabs, 1);

    auto *retentionPage = new QWidget(tabs);
    auto *retentionLayout = new QVBoxLayout(retentionPage);
    retentionLayout->setContentsMargins(10, 10, 10, 10);
    retentionLayout->setSpacing(10);

    auto *hint = new QLabel(
        QStringLiteral("默认所有 APP 使用全局保留天数。只有在高级设置中明确启用的 APP 才会单独覆盖。"
                       "保存后会写入工程 etc/system.json，"
                       "随“上传到设备”一并下发；重启对应 APP 后生效。"),
        retentionPage);
    hint->setWordWrap(true);
    retentionLayout->addWidget(hint);

    auto *globalRetentionWidget = new QWidget(retentionPage);
    auto *globalRetentionForm = new QFormLayout(globalRetentionWidget);
    globalRetentionForm->setContentsMargins(0, 0, 0, 0);
    globalRetentionForm->setHorizontalSpacing(20);

    m_globalLogRetentionDaysSpin = new QSpinBox(globalRetentionWidget);
    m_globalLogRetentionDaysSpin->setRange(1, kMaximumRetentionDays);
    m_globalLogRetentionDaysSpin->setSuffix(QStringLiteral(" 天"));
    m_globalLogRetentionDaysSpin->setValue(kDefaultRetentionDays);
    m_globalLogRetentionDaysSpin->setToolTip(QStringLiteral("写入 system.json 根级 LOGMAX"));
    globalRetentionForm->addRow(QStringLiteral("全局日志保留天数:"), m_globalLogRetentionDaysSpin);

    m_globalMsgRetentionDaysSpin = new QSpinBox(globalRetentionWidget);
    m_globalMsgRetentionDaysSpin->setRange(1, kMaximumRetentionDays);
    m_globalMsgRetentionDaysSpin->setSuffix(QStringLiteral(" 天"));
    m_globalMsgRetentionDaysSpin->setValue(kDefaultRetentionDays);
    m_globalMsgRetentionDaysSpin->setToolTip(QStringLiteral("写入 system.json 根级 MSGMAX"));
    globalRetentionForm->addRow(QStringLiteral("全局消息保留天数:"), m_globalMsgRetentionDaysSpin);
    retentionLayout->addWidget(globalRetentionWidget);

    m_logAdvancedRetentionBtn = new QPushButton(retentionPage);
    m_logAdvancedRetentionBtn->setCheckable(true);
    m_logAdvancedRetentionBtn->setChecked(false);
    retentionLayout->addWidget(m_logAdvancedRetentionBtn);

    m_logAdvancedRetentionWidget = new QWidget(retentionPage);
    auto *advancedRetentionLayout = new QVBoxLayout(m_logAdvancedRetentionWidget);
    advancedRetentionLayout->setContentsMargins(0, 0, 0, 0);
    advancedRetentionLayout->setSpacing(8);

    auto *advancedHint = new QLabel(
        QStringLiteral("勾选“启用”后，该 APP 才会写入 APP_LOG_RETENTION；取消勾选后将恢复继承全局设置。"),
        m_logAdvancedRetentionWidget);
    advancedHint->setWordWrap(true);
    advancedRetentionLayout->addWidget(advancedHint);

    const QStringList appNames = logManagedAppNames();
    m_logRetentionTable = new QTableWidget(appNames.size(), 3, m_logAdvancedRetentionWidget);
    m_logRetentionTable->setHorizontalHeaderLabels(
        {QStringLiteral("APP"), QStringLiteral("启用"), QStringLiteral("日志/消息保留天数")});
    m_logRetentionTable->verticalHeader()->setVisible(false);
    m_logRetentionTable->setAlternatingRowColors(true);
    m_logRetentionTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_logRetentionTable->setFocusPolicy(Qt::NoFocus);
    m_logRetentionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_logRetentionTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logRetentionTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    for (int row = 0; row < appNames.size(); ++row) {
        auto *appItem = new QTableWidgetItem(appNames.at(row));
        appItem->setFlags(appItem->flags() & ~Qt::ItemIsEditable);
        m_logRetentionTable->setItem(row, 0, appItem);

        auto *enabledCheck = new QCheckBox(m_logRetentionTable);
        enabledCheck->setChecked(false);
        enabledCheck->setToolTip(QStringLiteral("启用后为此 APP 单独写入保留策略"));
        m_logRetentionTable->setCellWidget(row, 1, enabledCheck);

        auto *spin = new QSpinBox(m_logRetentionTable);
        spin->setRange(1, kMaximumRetentionDays);
        spin->setSuffix(QStringLiteral(" 天"));
        spin->setAlignment(Qt::AlignCenter);
        spin->setValue(kDefaultRetentionDays);
        spin->setEnabled(false);
        spin->setToolTip(QStringLiteral("允许 1～3650 天，默认 7 天"));
        m_logRetentionTable->setCellWidget(row, 2, spin);

        connect(enabledCheck, &QCheckBox::toggled, spin, &QSpinBox::setEnabled);
    }
    advancedRetentionLayout->addWidget(m_logRetentionTable, 1);
    m_logAdvancedRetentionWidget->setVisible(false);
    retentionLayout->addWidget(m_logAdvancedRetentionWidget, 1);

    const auto updateAdvancedButtonText = [this]() {
        int enabledCount = 0;
        if (m_logRetentionTable) {
            for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
                const QCheckBox *check = retentionOverrideCheckBox(m_logRetentionTable, row);
                if (check && check->isChecked()) {
                    ++enabledCount;
                }
            }
        }
        if (m_logAdvancedRetentionBtn) {
            m_logAdvancedRetentionBtn->setText(
                QStringLiteral("%1高级设置：按 APP 单独覆盖（已启用 %2 项）")
                    .arg(m_logAdvancedRetentionBtn->isChecked()
                             ? QStringLiteral("收起")
                             : QStringLiteral("展开"),
                         QString::number(enabledCount)));
        }
    };
    connect(m_logAdvancedRetentionBtn, &QPushButton::toggled,
            this, [this, updateAdvancedButtonText](bool checked) {
                if (m_logAdvancedRetentionWidget) {
                    m_logAdvancedRetentionWidget->setVisible(checked);
                }
                updateAdvancedButtonText();
            });
    for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
        if (QCheckBox *check = retentionOverrideCheckBox(m_logRetentionTable, row)) {
            connect(check, &QCheckBox::toggled, this, [updateAdvancedButtonText](bool) {
                updateAdvancedButtonText();
            });
        }
    }
    connect(m_globalLogRetentionDaysSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int days) {
                for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
                    const QCheckBox *check = retentionOverrideCheckBox(m_logRetentionTable, row);
                    if (!check || !check->isChecked()) {
                        if (QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row)) {
                            spin->setValue(days);
                        }
                    }
                }
            });
    updateAdvancedButtonText();

    auto *retentionButtonRow = new QHBoxLayout();
    retentionButtonRow->addStretch();
    m_saveLogRetentionBtn = new QPushButton(QStringLiteral("保存到当前工程"), retentionPage);
    retentionButtonRow->addWidget(m_saveLogRetentionBtn);
    retentionLayout->addLayout(retentionButtonRow);
    tabs->addTab(retentionPage, QStringLiteral("保留策略"));

    auto *downloadPage = new QWidget(tabs);
    auto *downloadLayout = new QVBoxLayout(downloadPage);
    downloadLayout->setContentsMargins(10, 10, 10, 10);
    downloadLayout->setSpacing(10);

    auto *downloadHint = new QLabel(
        QStringLiteral("进入本页时会一次性查询全部 APP 的运行日志和报文文件，"
                       "之后可按 APP、日期和类型筛选并下载到本地。需要获取最新文件时可手动刷新。"),
        downloadPage);
    downloadHint->setWordWrap(true);
    downloadLayout->addWidget(downloadHint);

    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(QStringLiteral("APP："), downloadPage));
    m_logDownloadAppCombo = new QComboBox(downloadPage);
    m_logDownloadAppCombo->addItems(appNames);
    m_logDownloadAppCombo->setMinimumWidth(150);
    filterRow->addWidget(m_logDownloadAppCombo);

    filterRow->addWidget(new QLabel(QStringLiteral("日期："), downloadPage));
    m_logDownloadDateCombo = new QComboBox(downloadPage);
    m_logDownloadDateCombo->setMinimumWidth(130);
    m_logDownloadDateCombo->setEnabled(false);
    filterRow->addWidget(m_logDownloadDateCombo);

    filterRow->addWidget(new QLabel(QStringLiteral("类型："), downloadPage));
    m_logDownloadTypeCombo = new QComboBox(downloadPage);
    m_logDownloadTypeCombo->addItem(QStringLiteral("全部类型"), QString());
    m_logDownloadTypeCombo->addItem(QStringLiteral("运行日志（log）"), QStringLiteral("log"));
    m_logDownloadTypeCombo->addItem(QStringLiteral("报文（msg）"), QStringLiteral("msg"));
    m_logDownloadTypeCombo->setMinimumWidth(150);
    filterRow->addWidget(m_logDownloadTypeCombo);

    filterRow->addStretch();
    m_queryDeviceLogsBtn = new QPushButton(QStringLiteral("刷新设备日志"), downloadPage);
    filterRow->addWidget(m_queryDeviceLogsBtn);
    downloadLayout->addLayout(filterRow);

    m_deviceLogFileTable = new QTableWidget(0, 5, downloadPage);
    m_deviceLogFileTable->setHorizontalHeaderLabels({
        QStringLiteral("日期"),
        QStringLiteral("类型"),
        QStringLiteral("文件名"),
        QStringLiteral("大小"),
        QStringLiteral("最后修改")
    });
    m_deviceLogFileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceLogFileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceLogFileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_deviceLogFileTable->setAlternatingRowColors(true);
    m_deviceLogFileTable->verticalHeader()->setVisible(false);
    m_deviceLogFileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_deviceLogFileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_deviceLogFileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_deviceLogFileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_deviceLogFileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    downloadLayout->addWidget(m_deviceLogFileTable, 1);

    m_logDownloadSummaryLabel = new QLabel(
        QStringLiteral("切换到日志下载页后，将自动查询全部 APP 的设备日志。"),
        downloadPage);
    m_logDownloadSummaryLabel->setWordWrap(true);
    downloadLayout->addWidget(m_logDownloadSummaryLabel);

    auto *directoryRow = new QHBoxLayout();
    directoryRow->addWidget(new QLabel(QStringLiteral("本地目录："), downloadPage));
    m_logDownloadDirectoryEdit = new QLineEdit(downloadPage);
    QString defaultDownloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultDownloadDir.isEmpty()) {
        defaultDownloadDir = QDir::homePath();
    }
    m_logDownloadDirectoryEdit->setText(QDir(defaultDownloadDir).filePath(QStringLiteral("CEPBLogs")));
    directoryRow->addWidget(m_logDownloadDirectoryEdit, 1);
    auto *browseDownloadDirectoryBtn = new QPushButton(QStringLiteral("浏览..."), downloadPage);
    directoryRow->addWidget(browseDownloadDirectoryBtn);
    downloadLayout->addLayout(directoryRow);

    auto *downloadButtonRow = new QHBoxLayout();
    downloadButtonRow->addStretch();
    m_downloadSelectedLogsBtn = new QPushButton(QStringLiteral("下载选中文件"), downloadPage);
    m_downloadSelectedLogsBtn->setEnabled(false);
    m_downloadVisibleLogsBtn = new QPushButton(QStringLiteral("下载当前列表"), downloadPage);
    m_downloadVisibleLogsBtn->setEnabled(false);
    m_downloadVisibleLogsBtn->setToolTip(QStringLiteral("下载当前日期和类型筛选后显示的全部文件"));
    downloadButtonRow->addWidget(m_downloadSelectedLogsBtn);
    downloadButtonRow->addWidget(m_downloadVisibleLogsBtn);
    downloadLayout->addLayout(downloadButtonRow);
    tabs->addTab(downloadPage, QStringLiteral("日志下载"));

    auto *dialogButtonRow = new QHBoxLayout();
    dialogButtonRow->addStretch();
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), m_logManagementPage);
    dialogButtonRow->addWidget(closeButton);
    layout->addLayout(dialogButtonRow);

    connect(m_saveLogRetentionBtn, &QPushButton::clicked, this, [this]() {
        QString errorMessage;
        if (!saveLogRetentionConfigToProject(&errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("日志管理"), errorMessage);
            return;
        }
        statusBar()->showMessage(QStringLiteral("日志保留天数已保存到当前工程"), 5000);
        QMessageBox::information(this,
                                 QStringLiteral("日志管理"),
                                 QStringLiteral("已保存。上传配置并重启对应 APP 后生效。"));
    });
    connect(m_queryDeviceLogsBtn, &QPushButton::clicked,
            this, &MainWindow::queryDeviceLogFiles);
    connect(m_logDownloadAppCombo, &QComboBox::currentTextChanged,
            this, [this]() { refreshDeviceLogDateFilter(); });
    connect(m_logDownloadDateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshDeviceLogFileTable(); });
    connect(m_logDownloadTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshDeviceLogFileTable(); });
    connect(m_deviceLogFileTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (m_downloadSelectedLogsBtn) {
            m_downloadSelectedLogsBtn->setEnabled(!m_deviceLogFileTable->selectionModel()->selectedRows().isEmpty());
        }
    });
    connect(browseDownloadDirectoryBtn, &QPushButton::clicked,
            this, &MainWindow::chooseLogDownloadDirectory);
    connect(m_downloadSelectedLogsBtn, &QPushButton::clicked,
            this, [this]() { downloadDeviceLogFiles(true); });
    connect(m_downloadVisibleLogsBtn, &QPushButton::clicked,
            this, [this]() { downloadDeviceLogFiles(false); });
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs, downloadPage](int index) {
        if (tabs->widget(index) == downloadPage && !m_deviceLogFilesLoaded) {
            startDeviceLogFileQuery(true);
        }
    });
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);
}

void MainWindow::clearDeviceLogFileList(const QString &summaryText)
{
    m_deviceLogFiles.clear();
    if (m_deviceLogFileTable) {
        m_deviceLogFileTable->setRowCount(0);
    }
    if (m_logDownloadDateCombo) {
        const QSignalBlocker blocker(m_logDownloadDateCombo);
        m_logDownloadDateCombo->clear();
        m_logDownloadDateCombo->setEnabled(false);
    }
    if (m_downloadSelectedLogsBtn) {
        m_downloadSelectedLogsBtn->setEnabled(false);
    }
    if (m_downloadVisibleLogsBtn) {
        m_downloadVisibleLogsBtn->setEnabled(false);
    }
    if (m_logDownloadSummaryLabel) {
        m_logDownloadSummaryLabel->setText(
            summaryText.isEmpty()
                ? QStringLiteral("切换到日志下载页后，将自动查询全部 APP 的设备日志。")
                : summaryText);
    }
}

void MainWindow::queryDeviceLogFiles()
{
    startDeviceLogFileQuery(false);
}

void MainWindow::startDeviceLogFileQuery(bool automatic)
{
    if (!m_logDownloadAppCombo || !m_deviceLogFileTable) {
        return;
    }
    if (m_deviceLogQueryInProgress) {
        return;
    }

    const QStringList appNames = logManagedAppNames();
    const QString host = deviceHost().trimmed();
    QHostAddress hostAddress;
    if (host.isEmpty() || !hostAddress.setAddress(host)) {
        m_deviceLogFilesLoaded = true;
        clearDeviceLogFileList(QStringLiteral("请先在右上角填写有效的设备 IP，再点击“刷新设备日志”。"));
        if (!automatic) {
            QMessageBox::warning(this,
                                 QStringLiteral("日志下载"),
                                 QStringLiteral("请先在右上角填写有效的设备 IP。"));
        }
        return;
    }

    const quint64 queryId = ++m_deviceLogQueryId;
    m_deviceLogQueryInProgress = true;
    m_deviceLogFilesLoaded = false;
    clearDeviceLogFileList(
        QStringLiteral("正在后台查询 %1 上全部 APP 的设备日志，界面可继续操作...").arg(host));
    if (m_queryDeviceLogsBtn) {
        m_queryDeviceLogsBtn->setEnabled(false);
        m_queryDeviceLogsBtn->setText(QStringLiteral("正在查询..."));
    }

    const QString baseDir = normalizedRemoteLogBaseDir(configRemoteBaseDir());
    QString command;
    for (const QString &appName : appNames) {
        for (const QString &logType : {QStringLiteral("log"), QStringLiteral("msg")}) {
            const QString remoteDir = QStringLiteral("%1/%2/%3").arg(baseDir, appName, logType);
            // 兼容两种设备端布局：
            //   <APP>/<type>/<file>
            //   <APP>/<type>/<APP>/<file>
            // 当前运行日志使用第二种布局，部分 APP 的报文仍使用第一种布局。
            command += QStringLiteral(
                "if [ -d %1 ]; then "
                "find %1 -maxdepth 2 -type f "
                "-printf '__CEPB_LOG_FILE__|%2|%3|%p|%s|%TY-%Tm-%Td|%TH:%TM:%TS|%f\\n' "
                "2>/dev/null; "
                "fi; ")
                .arg(remoteLogShellQuote(remoteDir), appName, logType);
        }
    }

    const SshClient::Connection connection{host,
                                           fixedRemoteSshPort().toUShort(),
                                           fixedRemoteUser(),
                                           fixedRemotePassword(),
                                           3000};
    auto *watcher = new QFutureWatcher<SshClient::CommandResult>(this);
    connect(watcher, &QFutureWatcher<SshClient::CommandResult>::finished, this,
            [this, watcher, queryId, host, automatic]() {
        const SshClient::CommandResult result = watcher->result();
        watcher->deleteLater();
        finishDeviceLogFileQuery(queryId,
                                 host,
                                 automatic,
                                 result.ok,
                                 result.output,
                                 result.error);
    });
    watcher->setFuture(QtConcurrent::run([connection, command]() {
        return SshClient::execCommand(connection, command, 10000);
    }));
}

void MainWindow::finishDeviceLogFileQuery(quint64 queryId,
                                          const QString &queriedHost,
                                          bool automatic,
                                          bool ok,
                                          const QString &output,
                                          const QString &error)
{
    if (queryId != m_deviceLogQueryId) {
        return;
    }

    m_deviceLogQueryInProgress = false;
    if (m_queryDeviceLogsBtn) {
        m_queryDeviceLogsBtn->setEnabled(true);
        m_queryDeviceLogsBtn->setText(QStringLiteral("刷新设备日志"));
    }

    if (queriedHost != deviceHost().trimmed()) {
        m_deviceLogFilesLoaded = false;
        clearDeviceLogFileList(
            QStringLiteral("查询期间设备 IP 已改变，请点击“刷新设备日志”查询新设备。"));
        return;
    }

    if (!ok) {
        m_deviceLogFilesLoaded = true;
        const QString detail = error.isEmpty() ? output : error;
        clearDeviceLogFileList(
            QStringLiteral("未能连接 %1。请确认右上角设备 IP 和网线连接后，再点击“刷新设备日志”。")
                .arg(queriedHost));
        statusBar()->showMessage(QStringLiteral("设备日志自动查询失败：%1").arg(queriedHost), 5000);
        if (!automatic) {
            QMessageBox::warning(this,
                                 QStringLiteral("日志下载"),
                                 QStringLiteral("查询设备日志失败：\n%1").arg(detail));
        }
        return;
    }

    QSet<QString> remotePaths;
    const QStringList appNames = logManagedAppNames();
    for (const QString &rawLine : output.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith(QStringLiteral("__CEPB_LOG_FILE__|"))) {
            continue;
        }
        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() < 8) {
            continue;
        }

        bool sizeOk = false;
        DeviceLogFileInfo fileInfo;
        fileInfo.appName = parts.value(1).trimmed();
        fileInfo.logType = parts.value(2).trimmed();
        fileInfo.remotePath = parts.value(3).trimmed();
        fileInfo.sizeBytes = parts.value(4).trimmed().toLongLong(&sizeOk);
        const QString modifiedDate = parts.value(5).trimmed();
        const QString modifiedClock = parts.value(6).trimmed().left(8);
        fileInfo.fileName = parts.mid(7).join(QLatin1Char('|')).trimmed();
        fileInfo.logDate = logDateFromFileName(fileInfo.fileName, modifiedDate);
        fileInfo.modifiedTime = QStringLiteral("%1 %2").arg(modifiedDate, modifiedClock);
        if (!sizeOk || fileInfo.sizeBytes < 0
            || !appNames.contains(fileInfo.appName)
            || (fileInfo.logType != QStringLiteral("log") && fileInfo.logType != QStringLiteral("msg"))
            || fileInfo.remotePath.isEmpty() || fileInfo.fileName.isEmpty()
            || remotePaths.contains(fileInfo.remotePath)) {
            continue;
        }
        remotePaths.insert(fileInfo.remotePath);
        m_deviceLogFiles.append(fileInfo);
    }

    std::sort(m_deviceLogFiles.begin(), m_deviceLogFiles.end(), [](const DeviceLogFileInfo &left,
                                                                   const DeviceLogFileInfo &right) {
        if (left.appName != right.appName) {
            return left.appName < right.appName;
        }
        if (left.logDate != right.logDate) {
            return left.logDate > right.logDate;
        }
        if (left.logType != right.logType) {
            return left.logType < right.logType;
        }
        return left.fileName < right.fileName;
    });

    m_deviceLogFilesLoaded = true;
    refreshDeviceLogDateFilter();
    statusBar()->showMessage(
        m_deviceLogFiles.isEmpty()
            ? QStringLiteral("设备上未找到日志文件")
            : QStringLiteral("已查询全部 APP，共找到 %1 个日志文件").arg(m_deviceLogFiles.size()),
        5000);
}

void MainWindow::refreshDeviceLogDateFilter()
{
    if (!m_logDownloadAppCombo || !m_logDownloadDateCombo) {
        return;
    }

    const QString selectedApp = m_logDownloadAppCombo->currentText().trimmed();
    QStringList dates;
    for (const DeviceLogFileInfo &fileInfo : std::as_const(m_deviceLogFiles)) {
        if (fileInfo.appName == selectedApp && !dates.contains(fileInfo.logDate)) {
            dates.append(fileInfo.logDate);
        }
    }
    std::sort(dates.begin(), dates.end(), std::greater<QString>());

    {
        const QSignalBlocker blocker(m_logDownloadDateCombo);
        m_logDownloadDateCombo->clear();
        for (const QString &date : std::as_const(dates)) {
            m_logDownloadDateCombo->addItem(date, date);
        }
        if (!dates.isEmpty()) {
            m_logDownloadDateCombo->insertSeparator(m_logDownloadDateCombo->count());
            m_logDownloadDateCombo->addItem(QStringLiteral("全部日期"), QString());
        }
        m_logDownloadDateCombo->setEnabled(!dates.isEmpty());
    }

    refreshDeviceLogFileTable();
}

void MainWindow::refreshDeviceLogFileTable()
{
    if (!m_deviceLogFileTable || !m_logDownloadAppCombo
        || !m_logDownloadDateCombo || !m_logDownloadTypeCombo) {
        return;
    }

    const QString selectedApp = m_logDownloadAppCombo->currentText().trimmed();
    const QString selectedDate = m_logDownloadDateCombo->currentData().toString();
    const QString selectedType = m_logDownloadTypeCombo->currentData().toString();
    m_deviceLogFileTable->setRowCount(0);

    int appFileCount = 0;
    qint64 totalBytes = 0;
    for (int sourceIndex = 0; sourceIndex < m_deviceLogFiles.size(); ++sourceIndex) {
        const DeviceLogFileInfo &fileInfo = m_deviceLogFiles.at(sourceIndex);
        if (fileInfo.appName != selectedApp) {
            continue;
        }
        ++appFileCount;
        if (!selectedDate.isEmpty() && fileInfo.logDate != selectedDate) {
            continue;
        }
        if (!selectedType.isEmpty() && fileInfo.logType != selectedType) {
            continue;
        }

        const int row = m_deviceLogFileTable->rowCount();
        m_deviceLogFileTable->insertRow(row);
        auto *dateItem = new QTableWidgetItem(fileInfo.logDate);
        dateItem->setData(Qt::UserRole, sourceIndex);
        m_deviceLogFileTable->setItem(row, 0, dateItem);
        m_deviceLogFileTable->setItem(row, 1, new QTableWidgetItem(displayLogType(fileInfo.logType)));
        auto *fileItem = new QTableWidgetItem(fileInfo.fileName);
        fileItem->setToolTip(fileInfo.remotePath);
        m_deviceLogFileTable->setItem(row, 2, fileItem);
        auto *sizeItem = new QTableWidgetItem(displayFileSize(fileInfo.sizeBytes));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_deviceLogFileTable->setItem(row, 3, sizeItem);
        m_deviceLogFileTable->setItem(row, 4, new QTableWidgetItem(fileInfo.modifiedTime));
        totalBytes += fileInfo.sizeBytes;
    }

    const int visibleCount = m_deviceLogFileTable->rowCount();
    m_downloadSelectedLogsBtn->setEnabled(false);
    m_downloadVisibleLogsBtn->setEnabled(visibleCount > 0);
    if (m_logDownloadSummaryLabel) {
        if (!m_deviceLogFilesLoaded) {
            m_logDownloadSummaryLabel->setText(
                QStringLiteral("切换到日志下载页后，将自动查询全部 APP 的设备日志。"));
        } else if (appFileCount == 0) {
            m_logDownloadSummaryLabel->setText(QStringLiteral("设备上未找到该 APP 的运行日志或报文文件。"));
        } else if (visibleCount == 0) {
            m_logDownloadSummaryLabel->setText(QStringLiteral("当前日期和类型下没有日志文件。"));
        } else {
            m_logDownloadSummaryLabel->setText(
                QStringLiteral("当前列表：%1 个文件，共 %2；将鼠标停在文件名上可查看设备端完整路径。")
                    .arg(visibleCount)
                    .arg(displayFileSize(totalBytes)));
        }
    }
}

void MainWindow::chooseLogDownloadDirectory()
{
    const QString currentPath = m_logDownloadDirectoryEdit
        ? QDir::cleanPath(m_logDownloadDirectoryEdit->text().trimmed())
        : QString();
    const QString selectedPath = QFileDialog::getExistingDirectory(
        m_logManagementPage,
        QStringLiteral("选择日志保存目录"),
        currentPath.isEmpty() ? QDir::homePath() : currentPath);
    if (!selectedPath.isEmpty() && m_logDownloadDirectoryEdit) {
        m_logDownloadDirectoryEdit->setText(QDir::toNativeSeparators(selectedPath));
    }
}

void MainWindow::downloadDeviceLogFiles(bool selectedOnly)
{
    if (!m_deviceLogFileTable || !m_logDownloadDirectoryEdit || m_deviceLogFiles.isEmpty()) {
        return;
    }

    QSet<int> sourceIndexes;
    if (selectedOnly) {
        const QModelIndexList selectedRows = m_deviceLogFileTable->selectionModel()->selectedRows(0);
        for (const QModelIndex &index : selectedRows) {
            if (QTableWidgetItem *item = m_deviceLogFileTable->item(index.row(), 0)) {
                sourceIndexes.insert(item->data(Qt::UserRole).toInt());
            }
        }
    } else {
        for (int row = 0; row < m_deviceLogFileTable->rowCount(); ++row) {
            if (QTableWidgetItem *item = m_deviceLogFileTable->item(row, 0)) {
                sourceIndexes.insert(item->data(Qt::UserRole).toInt());
            }
        }
    }
    if (sourceIndexes.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("日志下载"),
                                 selectedOnly ? QStringLiteral("请先选择要下载的日志文件。")
                                              : QStringLiteral("当前列表没有可下载的日志文件。"));
        return;
    }

    QList<int> orderedIndexes = sourceIndexes.values();
    std::sort(orderedIndexes.begin(), orderedIndexes.end());
    QList<DeviceLogFileInfo> files;
    qint64 batchTotalBytes = 0;
    for (int sourceIndex : std::as_const(orderedIndexes)) {
        if (sourceIndex < 0 || sourceIndex >= m_deviceLogFiles.size()) {
            continue;
        }
        files.append(m_deviceLogFiles.at(sourceIndex));
        batchTotalBytes += m_deviceLogFiles.at(sourceIndex).sizeBytes;
    }
    if (files.isEmpty()) {
        return;
    }

    const QString downloadRoot = QDir::cleanPath(m_logDownloadDirectoryEdit->text().trimmed());
    if (downloadRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("日志下载"), QStringLiteral("请选择本地保存目录。"));
        return;
    }

    const QString baseDir = normalizedRemoteLogBaseDir(configRemoteBaseDir());
    QStringList existingFiles;
    QList<QPair<DeviceLogFileInfo, QString>> downloadTasks;
    for (const DeviceLogFileInfo &fileInfo : std::as_const(files)) {
        const QString expectedPrefix = QStringLiteral("%1/%2/%3/")
                                           .arg(baseDir, fileInfo.appName, fileInfo.logType);
        const QString safeFileName = QFileInfo(fileInfo.fileName).fileName();
        if (!fileInfo.remotePath.startsWith(expectedPrefix)
            || safeFileName.isEmpty() || safeFileName == QStringLiteral(".")
            || safeFileName == QStringLiteral("..")) {
            QMessageBox::warning(this,
                                 QStringLiteral("日志下载"),
                                 QStringLiteral("检测到无效的远程日志路径，已停止下载：\n%1")
                                     .arg(fileInfo.remotePath));
            return;
        }

        const QString localDir = QDir(downloadRoot).filePath(
            QStringLiteral("%1/%2/%3").arg(fileInfo.appName, fileInfo.logDate, fileInfo.logType));
        if (!QDir().mkpath(localDir)) {
            QMessageBox::warning(this,
                                 QStringLiteral("日志下载"),
                                 QStringLiteral("无法创建本地目录：\n%1").arg(localDir));
            return;
        }
        const QString localPath = QDir(localDir).filePath(safeFileName);
        if (QFileInfo::exists(localPath)) {
            existingFiles.append(localPath);
        }
        downloadTasks.append({fileInfo, localPath});
    }

    if (!existingFiles.isEmpty()) {
        const QMessageBox::StandardButton overwrite = QMessageBox::question(
            this,
            QStringLiteral("日志下载"),
            QStringLiteral("本地已有 %1 个同名文件，继续将覆盖这些文件。是否继续？")
                .arg(existingFiles.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (overwrite != QMessageBox::Yes) {
            return;
        }
    }

    QProgressDialog progress(QStringLiteral("正在连接设备..."),
                             QStringLiteral("取消"),
                             0,
                             100,
                             m_logManagementPage);
    progress.setWindowTitle(QStringLiteral("日志下载"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setValue(0);
    progress.show();
    QApplication::processEvents();

    const SshClient::Connection connection{deviceHost().trimmed(),
                                           fixedRemoteSshPort().toUShort(),
                                           fixedRemoteUser(),
                                           fixedRemotePassword(),
                                           10000};
    SshClient::Session session(connection);
    QString errorMessage;
    if (!session.connect(&errorMessage)) {
        progress.close();
        QMessageBox::warning(this,
                             QStringLiteral("日志下载"),
                             QStringLiteral("连接设备失败：\n%1").arg(errorMessage));
        return;
    }

    qint64 completedBytes = 0;
    int completedFiles = 0;
    bool canceled = false;
    for (int taskIndex = 0; taskIndex < downloadTasks.size(); ++taskIndex) {
        const DeviceLogFileInfo &fileInfo = downloadTasks.at(taskIndex).first;
        const QString &localPath = downloadTasks.at(taskIndex).second;
        progress.setLabelText(QStringLiteral("正在下载 %1（%2/%3）...")
                                  .arg(fileInfo.fileName)
                                  .arg(taskIndex + 1)
                                  .arg(downloadTasks.size()));
        QApplication::processEvents();

        const bool ok = session.downloadFileScp(
            fileInfo.remotePath,
            localPath,
            &errorMessage,
            [&](qint64 receivedBytes, qint64 fileTotalBytes) {
                int value = 0;
                if (batchTotalBytes > 0) {
                    value = int(((completedBytes + qMin(receivedBytes, fileInfo.sizeBytes)) * 100)
                                / batchTotalBytes);
                } else if (fileTotalBytes > 0) {
                    value = int(((taskIndex * 100)
                                 + (receivedBytes * 100 / fileTotalBytes))
                                / downloadTasks.size());
                }
                progress.setValue(qBound(0, value, 100));
                QApplication::processEvents();
                return !progress.wasCanceled();
            });
        if (!ok) {
            canceled = progress.wasCanceled() || errorMessage == QStringLiteral("SCP download canceled");
            break;
        }
        completedBytes += fileInfo.sizeBytes;
        ++completedFiles;
    }
    progress.close();

    if (canceled) {
        statusBar()->showMessage(QStringLiteral("日志下载已取消，已完成 %1 个文件").arg(completedFiles), 5000);
        return;
    }
    if (completedFiles != downloadTasks.size()) {
        QMessageBox::warning(this,
                             QStringLiteral("日志下载"),
                             QStringLiteral("下载中断，已完成 %1/%2 个文件。\n\n%3")
                                 .arg(completedFiles)
                                 .arg(downloadTasks.size())
                                 .arg(errorMessage));
        return;
    }

    statusBar()->showMessage(QStringLiteral("已下载 %1 个日志文件").arg(completedFiles), 5000);
    QMessageBox::information(this,
                             QStringLiteral("日志下载"),
                             QStringLiteral("已下载 %1 个文件到：\n%2\n\n文件按 APP/日期/类型 分目录保存。")
                                 .arg(completedFiles)
                                 .arg(QDir::toNativeSeparators(downloadRoot)));
}

void MainWindow::resetLogRetentionDays()
{
    if (m_globalLogRetentionDaysSpin) {
        m_globalLogRetentionDaysSpin->setValue(kDefaultRetentionDays);
    }
    if (m_globalMsgRetentionDaysSpin) {
        m_globalMsgRetentionDaysSpin->setValue(kDefaultRetentionDays);
    }
    if (!m_logRetentionTable) {
        return;
    }
    for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
        if (QCheckBox *check = retentionOverrideCheckBox(m_logRetentionTable, row)) {
            check->setChecked(false);
        }
        if (QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row)) {
            spin->setValue(kDefaultRetentionDays);
            spin->setEnabled(false);
        }
    }
}

bool MainWindow::loadLogRetentionConfigFromProject(const QString &projectRoot,
                                                   QString *errorMessage)
{
    resetLogRetentionDays();
    m_loadedLogRetentionProjectRoot.clear();

    const QString root = QDir::cleanPath(projectRoot.trimmed());
    if (root.isEmpty()) {
        if (m_logRetentionProjectLabel) {
            m_logRetentionProjectLabel->setText(QStringLiteral("当前工程：未选择"));
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先在配置概览选择工程目录。 ");
        }
        return false;
    }

    if (m_logRetentionProjectLabel) {
        m_logRetentionProjectLabel->setText(QStringLiteral("当前工程：%1").arg(root));
    }

    const QString filePath = QDir(root).filePath(QStringLiteral("etc/system.json"));
    QFile file(filePath);
    if (!file.exists()) {
        m_loadedLogRetentionProjectRoot = root;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取日志配置：%1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("日志配置文件格式无效：%1").arg(filePath);
        }
        return false;
    }

    const QJsonObject rootObject = document.object();
    const int globalLogDays =
        positiveDays(rootObject.value(QStringLiteral("LOGMAX")), kDefaultRetentionDays);
    const int globalMsgDays =
        positiveDays(rootObject.value(QStringLiteral("MSGMAX")), kDefaultRetentionDays);
    if (m_globalLogRetentionDaysSpin) {
        m_globalLogRetentionDaysSpin->setValue(globalLogDays);
    }
    if (m_globalMsgRetentionDaysSpin) {
        m_globalMsgRetentionDaysSpin->setValue(globalMsgDays);
    }
    const int legacyDays = globalLogDays;
    const QJsonObject appRetention = rootObject.value(QStringLiteral("APP_LOG_RETENTION")).toObject();

    for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
        const QString appName = m_logRetentionTable->item(row, 0)->text();
        const QJsonValue appValue = appRetention.value(appName);
        const bool hasOverride = !appValue.isUndefined();
        int days = legacyDays;
        if (appValue.isObject()) {
            const QJsonObject appObject = appValue.toObject();
            days = positiveDays(appObject.value(QStringLiteral("LOGMAX")), days);
        } else if (!appValue.isUndefined()) {
            days = positiveDays(appValue, days);
        }
        if (QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row)) {
            spin->setValue(days);
            spin->setEnabled(hasOverride);
        }
        if (QCheckBox *check = retentionOverrideCheckBox(m_logRetentionTable, row)) {
            check->setChecked(hasOverride);
        }
    }

    m_loadedLogRetentionProjectRoot = root;
    return true;
}

bool MainWindow::saveLogRetentionConfigToProject(QString *errorMessage)
{
    const QString root = QDir::cleanPath(normalizedConfigProjectRoot(m_configImportDirEdit->text()));
    if (root.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先在配置概览选择工程目录。 ");
        }
        return false;
    }

    if (m_loadedLogRetentionProjectRoot != root) {
        QString loadError;
        if (!loadLogRetentionConfigFromProject(root, &loadError)) {
            if (errorMessage) {
                *errorMessage = loadError;
            }
            return false;
        }
    }

    const QString etcDirPath = QDir(root).filePath(QStringLiteral("etc"));
    const QString filePath = QDir(etcDirPath).filePath(QStringLiteral("system.json"));
    if (!QDir().mkpath(etcDirPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建日志配置目录：%1").arg(etcDirPath);
        }
        return false;
    }

    QJsonObject rootObject;
    QFile existing(filePath);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法读取现有日志配置：%1").arg(existing.errorString());
            }
            return false;
        }
        const QByteArray existingData = existing.readAll();
        existing.close();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(existingData, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("现有日志配置格式无效，已停止覆盖：%1").arg(filePath);
            }
            return false;
        }
        rootObject = document.object();
    }

    QJsonObject appRetention = rootObject.value(QStringLiteral("APP_LOG_RETENTION")).toObject();
    for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
        const QString appName = m_logRetentionTable->item(row, 0)->text();
        appRetention.remove(appName);
        const QCheckBox *check = retentionOverrideCheckBox(m_logRetentionTable, row);
        if (!check || !check->isChecked()) {
            continue;
        }
        const QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row);
        const int days = spin ? spin->value() : kDefaultRetentionDays;
        QJsonObject appObject;
        appObject.insert(QStringLiteral("LOGMAX"), days);
        appObject.insert(QStringLiteral("MSGMAX"), days);
        appRetention.insert(appName, appObject);
    }
    if (appRetention.isEmpty()) {
        rootObject.remove(QStringLiteral("APP_LOG_RETENTION"));
    } else {
        rootObject.insert(QStringLiteral("APP_LOG_RETENTION"), appRetention);
    }
    rootObject.insert(QStringLiteral("LOGMAX"),
                      m_globalLogRetentionDaysSpin
                          ? m_globalLogRetentionDaysSpin->value()
                          : kDefaultRetentionDays);
    rootObject.insert(QStringLiteral("MSGMAX"),
                      m_globalMsgRetentionDaysSpin
                          ? m_globalMsgRetentionDaysSpin->value()
                          : kDefaultRetentionDays);

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入日志配置：%1").arg(output.errorString());
        }
        return false;
    }
    const QByteArray outputData = QJsonDocument(rootObject).toJson(QJsonDocument::Indented);
    if (output.write(outputData) != outputData.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入日志配置失败：%1").arg(output.errorString());
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("提交日志配置失败：%1").arg(output.errorString());
        }
        return false;
    }

    m_loadedLogRetentionProjectRoot = root;
    if (m_logRetentionProjectLabel) {
        m_logRetentionProjectLabel->setText(QStringLiteral("当前工程：%1").arg(root));
    }
    return true;
}
