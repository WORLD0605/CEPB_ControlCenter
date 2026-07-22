#include "mainwindow.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

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
    return table ? qobject_cast<QSpinBox *>(table->cellWidget(row, 1)) : nullptr;
}

} // namespace

void MainWindow::setupLogManagementPage()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("日志管理"));
    dialog->setModal(false);
    dialog->resize(620, 520);
    m_logManagementPage = dialog;

    auto *layout = new QVBoxLayout(m_logManagementPage);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_logRetentionProjectLabel = new QLabel(QStringLiteral("当前工程：未选择"), m_logManagementPage);
    m_logRetentionProjectLabel->setWordWrap(true);
    layout->addWidget(m_logRetentionProjectLabel);

    auto *hint = new QLabel(
        QStringLiteral("分别设置每个 APP 的日志和消息保留天数。保存后会写入工程 etc/system.json，"
                       "随“上传到设备”一并下发；重启对应 APP 后生效。"),
        m_logManagementPage);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    const QStringList appNames = logManagedAppNames();
    m_logRetentionTable = new QTableWidget(appNames.size(), 2, m_logManagementPage);
    m_logRetentionTable->setHorizontalHeaderLabels(
        {QStringLiteral("APP"), QStringLiteral("日志/消息保留天数")});
    m_logRetentionTable->verticalHeader()->setVisible(false);
    m_logRetentionTable->setAlternatingRowColors(true);
    m_logRetentionTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_logRetentionTable->setFocusPolicy(Qt::NoFocus);
    m_logRetentionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_logRetentionTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    for (int row = 0; row < appNames.size(); ++row) {
        auto *appItem = new QTableWidgetItem(appNames.at(row));
        appItem->setFlags(appItem->flags() & ~Qt::ItemIsEditable);
        m_logRetentionTable->setItem(row, 0, appItem);

        auto *spin = new QSpinBox(m_logRetentionTable);
        spin->setRange(1, kMaximumRetentionDays);
        spin->setSuffix(QStringLiteral(" 天"));
        spin->setAlignment(Qt::AlignCenter);
        spin->setValue(kDefaultRetentionDays);
        spin->setToolTip(QStringLiteral("允许 1～3650 天，默认 7 天"));
        m_logRetentionTable->setCellWidget(row, 1, spin);
    }
    layout->addWidget(m_logRetentionTable, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    m_saveLogRetentionBtn = new QPushButton(QStringLiteral("保存到当前工程"), m_logManagementPage);
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), m_logManagementPage);
    buttonRow->addWidget(m_saveLogRetentionBtn);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

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
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);
}

void MainWindow::resetLogRetentionDays()
{
    if (!m_logRetentionTable) {
        return;
    }
    for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
        if (QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row)) {
            spin->setValue(kDefaultRetentionDays);
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
    int legacyDays = positiveDays(rootObject.value(QStringLiteral("LOGMAX")), -1);
    if (legacyDays <= 0) {
        legacyDays = positiveDays(rootObject.value(QStringLiteral("MSGMAX")), kDefaultRetentionDays);
    }
    const QJsonObject appRetention = rootObject.value(QStringLiteral("APP_LOG_RETENTION")).toObject();

    for (int row = 0; row < m_logRetentionTable->rowCount(); ++row) {
        const QString appName = m_logRetentionTable->item(row, 0)->text();
        const QJsonValue appValue = appRetention.value(appName);
        int days = legacyDays;
        if (appValue.isObject()) {
            const QJsonObject appObject = appValue.toObject();
            days = positiveDays(appObject.value(QStringLiteral("LOGMAX")), days);
        } else if (!appValue.isUndefined()) {
            days = positiveDays(appValue, days);
        }
        if (QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row)) {
            spin->setValue(days);
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
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(existing.readAll(), &parseError);
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
        const QSpinBox *spin = retentionSpinBox(m_logRetentionTable, row);
        const int days = spin ? spin->value() : kDefaultRetentionDays;
        QJsonObject appObject = appRetention.value(appName).toObject();
        appObject.insert(QStringLiteral("LOGMAX"), days);
        appObject.insert(QStringLiteral("MSGMAX"), days);
        appRetention.insert(appName, appObject);
    }
    rootObject.insert(QStringLiteral("APP_LOG_RETENTION"), appRetention);
    if (!rootObject.contains(QStringLiteral("LOGMAX"))) {
        rootObject.insert(QStringLiteral("LOGMAX"), kDefaultRetentionDays);
    }
    if (!rootObject.contains(QStringLiteral("MSGMAX"))) {
        rootObject.insert(QStringLiteral("MSGMAX"), kDefaultRetentionDays);
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入日志配置：%1").arg(output.errorString());
        }
        return false;
    }
    output.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
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
