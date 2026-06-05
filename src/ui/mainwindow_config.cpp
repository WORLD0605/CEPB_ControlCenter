#include "mainwindow.h"
#include "config/modbus_mapping_utils.h"
#include "ui/point_selector_dialog.h"

#include <algorithm>
#include <functional>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDropEvent>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProgressDialog>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStandardPaths>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

namespace {

constexpr int Iec104BindingColumnCount = 6;
constexpr int ModbusBindingColumnCount = 12;
constexpr int ModbusColumnEnabled = 0;
constexpr int ModbusColumnKind = 1;
constexpr int ModbusColumnDataRef = 2;
constexpr int ModbusColumnDescription = 3;
constexpr int ModbusColumnFunCode = 4;
constexpr int ModbusColumnRegister = 5;
constexpr int ModbusColumnDataType = 6;
constexpr int ModbusColumnScale = 7;
constexpr int ModbusColumnGroupNo = 8;
constexpr int ModbusColumnEntryNo = 9;
constexpr int ModbusColumnDataIndex = 10;
constexpr int ModbusColumnSelfSignal = 11;
constexpr int LogicAgcAvcDeviceColumnDeviceId = 0;
constexpr int LogicAgcAvcDeviceColumnCtrlP = 1;
constexpr int LogicAgcAvcDeviceColumnCtrlQ = 2;
constexpr int LogicAgcAvcDeviceColumnOnlineDevice = 3;
constexpr int LogicAgcAvcDeviceColumnOnlinePoint = 4;
constexpr int LogicAgcAvcDeviceColumnOnlineOk = 5;
constexpr int LogicAgcAvcDeviceColumnPMin = 6;
constexpr int LogicAgcAvcDeviceColumnPMax = 7;
constexpr int LogicAgcAvcDeviceColumnQMin = 8;
constexpr int LogicAgcAvcDeviceColumnQMax = 9;
constexpr int LogicAgcAvcDeviceColumnScaleP = 10;
constexpr int LogicAgcAvcDeviceColumnScaleQ = 11;
constexpr int LogicComputationColumnOutputDevice = 0;
constexpr int LogicComputationColumnOutputPoint = 1;
constexpr int LogicComputationColumnFormula = 2;
constexpr int LogicComputationColumnDropOperands = 3;
constexpr int LogicComputationColumnOperands = 4;
constexpr int LogicComputationColumnDescription = 5;
constexpr int LogicControlRuleColumnDevice = 0;
constexpr int LogicControlRuleColumnPoint = 1;
constexpr int LogicControlRuleColumnTargetCount = 2;
constexpr int LogicControlRuleColumnDescription = 3;
constexpr int LogicControlTargetColumnType = 0;
constexpr int LogicControlTargetColumnDevice = 1;
constexpr int LogicControlTargetColumnPoint = 2;
constexpr int LogicControlTargetColumnExpr = 3;
constexpr int LogicControlTargetColumnPreview = 4;
constexpr int LogicOnlineLinkColumnDevice = 0;
constexpr int LogicOnlineLinkColumnFollowDevice = 1;
constexpr int ConfigIssueRoleTargetType = Qt::UserRole + 1;
constexpr int ConfigIssueRoleTargetKey = Qt::UserRole + 2;
constexpr const char *ModelEditorOriginalModelIdProperty = "originalModelId";
constexpr const char *DeviceEditorOriginalDeviceIdProperty = "originalDeviceId";

using configtool::buildModbusDataIndex;
using configtool::isModbusDevice;
using configtool::isModbusSetKind;
using configtool::modbusBindingInt;
using configtool::modbusBindingKind;
using configtool::modbusBindingString;
using configtool::modbusKindDisplayName;
using configtool::modbusKindFromString;
using configtool::modbusMaxReadQuantity;
using configtool::modbusTypeRegisterCount;
using configtool::normalizedModbusKind;

bool selfSignalFlagChecked(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("1")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("on");
}

QString uiJsonValueToString(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        const int integer = value.toInt();
        return qFuzzyCompare(number + 1.0, static_cast<double>(integer) + 1.0)
            ? QString::number(integer)
            : QString::number(number);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return QString();
}

int modelPointFilterTabIndex(configtool::ModelServiceType type)
{
    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return 1;
    case configtool::ModelServiceType::Status:
        return 2;
    case configtool::ModelServiceType::Control:
        return 3;
    }

    return 0;
}

configtool::ModelServiceType modelServiceTypeFromTabIndex(int index)
{
    switch (index) {
    case 2:
        return configtool::ModelServiceType::Status;
    case 3:
        return configtool::ModelServiceType::Control;
    case 1:
    default:
        return configtool::ModelServiceType::Measurement;
    }
}

configtool::PointSignalType signalTypeForModelService(configtool::ModelServiceType type)
{
    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return configtool::PointSignalType::Yc;
    case configtool::ModelServiceType::Status:
        return configtool::PointSignalType::Yx;
    case configtool::ModelServiceType::Control:
        return configtool::PointSignalType::Ctrl;
    }

    return configtool::PointSignalType::Yc;
}

QString defaultLnTypeForModelService(configtool::ModelServiceType type)
{
    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return QStringLiteral("SlrInvMMXU");
    case configtool::ModelServiceType::Status:
        return QStringLiteral("SlrInvAlmGGIO");
    case configtool::ModelServiceType::Control:
        return QStringLiteral("SlrInvCtlGGIO");
    }

    return QStringLiteral("SlrInvMMXU");
}

QStringList splitClipboardLine(const QString &line)
{
    return line.split('\t');
}

QString deviceChoiceText(const configtool::ProtocolDeviceInstance &device)
{
    const QString description = device.deviceDesc.trimmed();
    return description.isEmpty()
        ? device.deviceId
        : QStringLiteral("%1 - %2").arg(device.deviceId, description);
}

QString deviceIdFromChoiceText(const QString &text)
{
    return text.section(QStringLiteral(" - "), 0, 0).trimmed();
}

QString modelChoiceText(const configtool::ModelTemplate &model)
{
    const QString name = model.displayName.trimmed();
    return name.isEmpty()
        ? model.modelId
        : QStringLiteral("%1 - %2").arg(model.modelId, name);
}

QString modelIdFromChoiceText(const QString &text)
{
    return text.section(QStringLiteral(" - "), 0, 0).trimmed();
}

QString pointShortNameFromDataRef(const QString &dataRef)
{
    return dataRef.section(QLatin1Char('.'), -1).trimmed();
}

QString pointShortName(const configtool::PointTemplate &point)
{
    const QString name = point.name.trimmed();
    if (!name.isEmpty()) {
        return name;
    }
    const QString doName = point.doName.trimmed();
    return doName.isEmpty() ? pointShortNameFromDataRef(point.dataRef()) : doName;
}

const configtool::ModelTemplate *findModelById(const configtool::ConfigProject &project,
                                               const QString &modelId)
{
    for (const configtool::ModelTemplate &model : project.models) {
        if (model.modelId == modelId) {
            return &model;
        }
    }
    return nullptr;
}

const configtool::ProtocolDeviceInstance *findDeviceById(const configtool::ConfigProject &project,
                                                         const QString &deviceId)
{
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.deviceId == deviceId) {
            return &device;
        }
    }
    return nullptr;
}

QString remoteShellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(quoted);
}

QString remotePathJoin(const QString &baseDir, const QString &relativePath)
{
    QString base = baseDir.trimmed();
    while (base.endsWith(QLatin1Char('/')) && base.size() > 1) {
        base.chop(1);
    }
    return QStringLiteral("%1/%2").arg(base, relativePath);
}

QStringList configTransferPathList()
{
    return {
        QStringLiteral("cepiec104/model"),
        QStringLiteral("cepiec104/dev"),
        QStringLiteral("cepmodbus/model"),
        QStringLiteral("cepmodbus/dev"),
        QStringLiteral("cepmodbus/etc"),
        QStringLiteral("cepLogicCenter/etc")
    };
}

QList<QStringList> parseClipboardTable(const QString &text)
{
    QList<QStringList> table;
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QStringList lines = normalized.split('\n');
    for (int index = 0; index < lines.size(); ++index) {
        if (index == lines.size() - 1 && lines.at(index).isEmpty()) {
            continue;
        }
        table.append(splitClipboardLine(lines.at(index)));
    }

    return table;
}

QModelIndexList sortedEditableTargetIndexes(QTableWidget *table)
{
    QModelIndexList indexes;
    if (table && table->selectionModel()) {
        indexes = table->selectionModel()->selectedIndexes();
    }

    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &left, const QModelIndex &right) {
        if (left.row() != right.row()) {
            return left.row() < right.row();
        }
        return left.column() < right.column();
    });

    return indexes;
}

QString configIssueSeverityText(configtool::ConfigIssueSeverity severity)
{
    switch (severity) {
    case configtool::ConfigIssueSeverity::Info:
        return QStringLiteral("提示");
    case configtool::ConfigIssueSeverity::Warning:
        return QStringLiteral("警告");
    case configtool::ConfigIssueSeverity::Error:
        return QStringLiteral("错误");
    }

    return QStringLiteral("错误");
}

QColor configIssueSeverityColor(configtool::ConfigIssueSeverity severity)
{
    switch (severity) {
    case configtool::ConfigIssueSeverity::Info:
        return QColor(QStringLiteral("#2f6f9f"));
    case configtool::ConfigIssueSeverity::Warning:
        return QColor(QStringLiteral("#b9770e"));
    case configtool::ConfigIssueSeverity::Error:
        return QColor(QStringLiteral("#c0392b"));
    }

    return QColor(QStringLiteral("#c0392b"));
}

QString importIssueSeverityText(configtool::ImportIssueSeverity severity)
{
    switch (severity) {
    case configtool::ImportIssueSeverity::Info:
        return QStringLiteral("提示");
    case configtool::ImportIssueSeverity::Warning:
        return QStringLiteral("警告");
    case configtool::ImportIssueSeverity::Error:
        return QStringLiteral("错误");
    }

    return QStringLiteral("错误");
}

QColor importIssueSeverityColor(configtool::ImportIssueSeverity severity)
{
    switch (severity) {
    case configtool::ImportIssueSeverity::Info:
        return QColor(QStringLiteral("#2f6f9f"));
    case configtool::ImportIssueSeverity::Warning:
        return QColor(QStringLiteral("#b9770e"));
    case configtool::ImportIssueSeverity::Error:
        return QColor(QStringLiteral("#c0392b"));
    }

    return QColor(QStringLiteral("#c0392b"));
}

int importIssueProblemCount(const QList<configtool::ImportIssue> &issues)
{
    int count = 0;
    for (const configtool::ImportIssue &issue : issues) {
        if (issue.severity != configtool::ImportIssueSeverity::Info) {
            ++count;
        }
    }
    return count;
}

configtool::ImportIssueSeverity importSeverityForConfigIssue(configtool::ConfigIssueSeverity severity)
{
    switch (severity) {
    case configtool::ConfigIssueSeverity::Info:
        return configtool::ImportIssueSeverity::Info;
    case configtool::ConfigIssueSeverity::Warning:
        return configtool::ImportIssueSeverity::Warning;
    case configtool::ConfigIssueSeverity::Error:
        return configtool::ImportIssueSeverity::Error;
    }

    return configtool::ImportIssueSeverity::Error;
}

QString formatConfigIssueForIssueTable(const configtool::ConfigIssue &issue)
{
    QStringList parts;
    if (!issue.module.trimmed().isEmpty()) {
        parts << issue.module.trimmed();
    }
    if (!issue.objectId.trimmed().isEmpty()) {
        parts << issue.objectId.trimmed();
    }
    parts << issue.message;
    return parts.join(QStringLiteral(" / "));
}

void appendConfigIssueForIssueTable(const configtool::ConfigIssue &issue,
                                    const QString &filePath,
                                    QList<configtool::ImportIssue> &issues)
{
    configtool::ImportIssue importIssue;
    importIssue.severity = importSeverityForConfigIssue(issue.severity);
    importIssue.filePath = filePath;
    importIssue.message = formatConfigIssueForIssueTable(issue);
    issues.append(importIssue);
}

bool resolveLogicIssueTarget(const QString &message, QString *targetType, QString *targetKey)
{
    const QStringList rawParts = message.split(QStringLiteral(" / "));
    QStringList parts;
    parts.reserve(rawParts.size());
    for (const QString &rawPart : rawParts) {
        const QString part = rawPart.trimmed();
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }

    for (int index = 0; index < parts.size(); ++index) {
        QString part = parts.at(index);
        const int bracketIndex = part.indexOf(QLatin1Char(']'));
        if (bracketIndex >= 0) {
            part = part.mid(bracketIndex + 1).trimmed();
        }

        QString type;
        if (part == QStringLiteral("计算点")) {
            type = QStringLiteral("logic-computation");
        } else if (part == QStringLiteral("控制转换")) {
            type = QStringLiteral("logic-control");
        } else if (part == QStringLiteral("在线联动")) {
            type = QStringLiteral("logic-online");
        } else if (part == QStringLiteral("AGC/AVC")) {
            type = QStringLiteral("logic-agcavc");
        }

        if (!type.isEmpty()) {
            if (targetType) {
                *targetType = type;
            }
            if (targetKey) {
                *targetKey = index + 1 < parts.size() ? parts.at(index + 1).trimmed() : QString();
            }
            return true;
        }
    }

    return false;
}

QString doubleToUiText(double value)
{
    return QString::number(value, 'g', 12);
}

double uiTextToDouble(const QString &text, double fallback)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    return ok ? value : fallback;
}

int uiTextToInt(const QString &text, int fallback)
{
    bool ok = false;
    const int value = text.trimmed().toInt(&ok);
    return ok ? value : fallback;
}

bool isLogicControlTotalTarget(const configtool::LogicControlTarget &target)
{
    if (target.targetType.trimmed().compare(QStringLiteral("ctrlcmd"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    return target.dataRef.contains(QStringLiteral("TotalP_Ctrl"), Qt::CaseInsensitive)
        || target.dataRef.contains(QStringLiteral("TotalQ_Ctrl"), Qt::CaseInsensitive);
}

QString expandedLogicControlExpression(const QString &expr, const QString &xValue)
{
    QString expanded = expr;
    expanded.replace(QStringLiteral("{x}"), xValue.trimmed().isEmpty() ? QStringLiteral("0") : xValue.trimmed());
    static const QRegularExpression realtimeRefPattern(QStringLiteral("\\{rt:([^{}]+)\\}"));
    return expanded.replace(realtimeRefPattern, QStringLiteral("<实时:$1>"));
}

} // namespace

void MainWindow::onBrowseConfigImportDirClicked()
{
    const QString startDir = configBrowseStartDir();
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择配置工程目录"),
        startDir);
    if (!dir.isEmpty()) {
        const QString projectRoot = normalizedConfigProjectRoot(dir);
        m_configImportDirEdit->setText(projectRoot);
        QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
        settings.setValue(QStringLiteral("config/lastBrowseDir"), projectRoot);
        onImportIec104ConfigClicked();
    }
}

void MainWindow::onImportIec104ConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString modbusAppDir = resolveModbusAppDir(projectRoot);
    const QString logicCenterAppDir = resolveLogicCenterAppDir(projectRoot);
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到 cepiec104、cepmodbus 或 cepLogicCenter 子目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ImportReport report;
    const QString projectName = QFileInfo(projectRoot).fileName().trimmed().isEmpty()
        ? QStringLiteral("配置工程")
        : QFileInfo(projectRoot).fileName();
    m_configProjectManager.createEmptyProject(projectName, projectRoot);
    bool ok = true;
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.importIec104AppDirectory(iec104AppDir, report) && ok;
    }
    if (!modbusAppDir.isEmpty()) {
        ok = m_configProjectManager.importModbusAppDirectory(modbusAppDir, report) && ok;
    }
    if (!logicCenterAppDir.isEmpty()) {
        const QString logicConfigPath = QDir(logicCenterAppDir)
            .filePath(QStringLiteral("etc/LogicCenter_Config.json"));
        if (QFileInfo::exists(logicConfigPath)) {
            ok = m_configProjectManager.importLogicCenterConfigFile(logicConfigPath, report) && ok;
        } else {
            report.addIssue(configtool::ImportIssueSeverity::Warning,
                            logicConfigPath,
                            QStringLiteral("未找到 LogicCenter_Config.json，已跳过 LogicCenter 配置导入"));
        }
    }
    refreshConfigImportSummary(report);

    if (!ok && report.hasErrors()) {
        statusBar()->showMessage(QStringLiteral("配置导入失败"), 5000);
        return;
    }

    statusBar()->showMessage(QStringLiteral("配置导入完成"), 5000);
}

void MainWindow::onExportIec104ConfigClicked()
{
    m_lastConfigExportOk = false;

    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString resolvedModbusDir = resolveModbusAppDir(projectRoot);
    const QString resolvedLogicCenterDir = resolveLogicCenterAppDir(projectRoot);
    const QString modbusAppDir = resolvedModbusDir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("cepmodbus"))
        : resolvedModbusDir;
    const QString logicCenterAppDir = resolvedLogicCenterDir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"))
        : resolvedLogicCenterDir;
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到可导出的配置目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ExportReport report;
    bool ok = true;
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.exportIec104AppDirectory(iec104AppDir, report) && ok;
    }
    ok = m_configProjectManager.exportModbusAppDirectory(modbusAppDir, report) && ok;
    ok = m_configProjectManager.exportLogicCenterConfigFile(
        QDir(logicCenterAppDir).filePath(QStringLiteral("etc/LogicCenter_Config.json")),
        report) && ok;

    QStringList issueLines;
    bool hasErrors = false;
    int warningCount = 0;
    int infoCount = 0;
    for (const configtool::ImportIssue &issue : report.issues) {
        const QString severity = importIssueSeverityText(issue.severity);
        hasErrors = hasErrors || issue.severity == configtool::ImportIssueSeverity::Error;
        if (issue.severity == configtool::ImportIssueSeverity::Warning) {
            ++warningCount;
        } else if (issue.severity == configtool::ImportIssueSeverity::Info) {
            ++infoCount;
        }
        issueLines << QStringLiteral("[%1] %2").arg(severity, issue.message);
    }

    if (!ok && hasErrors) {
        const QString detail = issueLines.isEmpty()
            ? QStringLiteral("导出失败，但未返回详细错误。")
            : QStringLiteral("导出失败，详细问题已更新到“问题列表”。");
        refreshConfigIssueTable(report.issues, QStringLiteral("导出"));
        QMessageBox::warning(this, QStringLiteral("导出失败"), detail);
        statusBar()->showMessage(QStringLiteral("配置导出失败"), 5000);
        return;
    }

    QString statusMessage = QStringLiteral("配置导出完成: 模型 %1，设备 %2")
        .arg(report.exportedModelCount)
        .arg(report.exportedDeviceCount);
    if (warningCount > 0) {
        statusMessage += QStringLiteral("，警告 %1 条").arg(warningCount);
    }
    if (infoCount > 0) {
        statusMessage += QStringLiteral("，提示 %1 条").arg(infoCount);
    }
    statusBar()->showMessage(statusMessage, 8000);
    refreshConfigIssueTable(report.issues, QStringLiteral("导出"));

    m_lastConfigExportOk = true;
}

void MainWindow::onCheckConfigIssuesClicked()
{
    const QList<configtool::ImportIssue> issues = collectCurrentConfigIssues();
    refreshConfigIssueTable(issues, QStringLiteral("检查"));
    if (m_mainTabWidget && m_configIssuePage) {
        m_mainTabWidget->setCurrentWidget(m_configIssuePage);
    }

    const int problemCount = importIssueProblemCount(issues);
    int infoCount = 0;
    for (const configtool::ImportIssue &issue : issues) {
        if (issue.severity == configtool::ImportIssueSeverity::Info) {
            ++infoCount;
        }
    }
    if (problemCount == 0) {
        statusBar()->showMessage(infoCount > 0
                                     ? QStringLiteral("当前检查完成：无错误或警告，提示 %1 条").arg(infoCount)
                                     : QStringLiteral("当前检查完成：未发现问题"),
                                 5000);
    } else {
        statusBar()->showMessage(QStringLiteral("当前检查完成：问题 %1 项，提示 %2 条")
                                     .arg(problemCount)
                                     .arg(infoCount),
                                 5000);
    }
}

void MainWindow::refreshConfigIssueTable(const QList<configtool::ImportIssue> &issues,
                                         const QString &source)
{
    if (!m_configIssueTable) {
        return;
    }

    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_configIssueTable->setRowCount(issues.size());
    for (int row = 0; row < issues.size(); ++row) {
        const configtool::ImportIssue &issue = issues.at(row);
        const QString severity = importIssueSeverityText(issue.severity);
        const QColor color = importIssueSeverityColor(issue.severity);

        QString targetType;
        QString targetKey;
        const QString filePath = QFileInfo(issue.filePath).absoluteFilePath();
        resolveLogicIssueTarget(issue.message, &targetType, &targetKey);
        if (targetType.isEmpty()) {
            for (const configtool::ModelTemplate &model : project.models) {
                const QString modelSource = model.source.filePath.trimmed().isEmpty()
                    ? QString()
                    : QFileInfo(model.source.filePath).absoluteFilePath();
                if ((!modelSource.isEmpty() && !issue.filePath.trimmed().isEmpty() && modelSource == filePath)
                    || issue.filePath == model.modelId
                    || issue.message.contains(model.modelId)) {
                    targetType = QStringLiteral("model");
                    targetKey = model.modelId;
                    break;
                }
            }
        }
        if (targetType.isEmpty()) {
            for (const configtool::ProtocolDeviceInstance &device : project.devices) {
                const QString deviceSource = device.source.filePath.trimmed().isEmpty()
                    ? QString()
                    : QFileInfo(device.source.filePath).absoluteFilePath();
                if ((!deviceSource.isEmpty() && !issue.filePath.trimmed().isEmpty() && deviceSource == filePath)
                    || issue.filePath == device.deviceId
                    || issue.message.contains(device.deviceId)) {
                    targetType = QStringLiteral("device");
                    targetKey = device.deviceId;
                    break;
                }
            }
        }
        if (targetType.isEmpty()) {
            if (issue.message.contains(QStringLiteral("计算点"))) {
                targetType = QStringLiteral("logic-computation");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            } else if (issue.message.contains(QStringLiteral("控制转换"))) {
                targetType = QStringLiteral("logic-control");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            } else if (issue.message.contains(QStringLiteral("在线联动"))) {
                targetType = QStringLiteral("logic-online");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            } else if (issue.message.contains(QStringLiteral("AGC/AVC"))) {
                targetType = QStringLiteral("logic-agcavc");
                targetKey = issue.message.section(QStringLiteral(" / "), 1, 1).trimmed();
            }
        }

        const QString objectText = targetKey.isEmpty() ? issue.filePath : targetKey;
        const QStringList values = {source, severity, objectText, issue.message, issue.filePath};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setForeground(color);
            item->setData(ConfigIssueRoleTargetType, targetType);
            item->setData(ConfigIssueRoleTargetKey, targetKey);
            m_configIssueTable->setItem(row, column, item);
        }
    }
    if (m_configIssueCountValueLabel) {
        m_configIssueCountValueLabel->setText(QString::number(importIssueProblemCount(issues)));
    }
}

QList<configtool::ImportIssue> MainWindow::collectCurrentConfigIssues() const
{
    QList<configtool::ImportIssue> issues;
    const configtool::ConfigProject &project = m_configProjectManager.project();
    const QString projectPath = project.sourceRoot.isEmpty() ? project.projectName : project.sourceRoot;

    auto appendIssue = [&](configtool::ImportIssueSeverity severity,
                           const QString &filePath,
                           const QString &message) {
        configtool::ImportIssue issue;
        issue.severity = severity;
        issue.filePath = filePath;
        issue.message = message;
        issues.append(issue);
    };
    auto objectPath = [](const QString &sourcePath, const QString &fallback) {
        return sourcePath.trimmed().isEmpty() ? fallback : sourcePath;
    };

    if (project.projectId.trimmed().isEmpty()) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("当前没有可检查的配置工程"));
    }

    QSet<QString> seenModelIds;
    QSet<QString> duplicateModelIds;
    for (const configtool::ModelTemplate &model : project.models) {
        const QString modelId = model.modelId.trimmed();
        if (modelId.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        objectPath(model.source.filePath, projectPath),
                        QStringLiteral("存在模型 modelId 为空"));
        } else if (seenModelIds.contains(modelId)) {
            duplicateModelIds.insert(modelId);
        } else {
            seenModelIds.insert(modelId);
        }

        const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
        if (!duplicateRefs.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        objectPath(model.source.filePath, model.modelId),
                        QStringLiteral("模型存在重复 DataRef：%1")
                            .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
        }
    }
    for (const QString &modelId : duplicateModelIds) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("模型 modelId 重复：%1").arg(modelId));
    }

    QSet<QString> seenDeviceIds;
    QSet<QString> duplicateDeviceIds;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        const QString deviceId = device.deviceId.trimmed();
        const QString devicePath = objectPath(device.source.filePath, device.deviceId);
        if (deviceId.isEmpty()) {
            appendIssue(configtool::ImportIssueSeverity::Error,
                        devicePath.isEmpty() ? projectPath : devicePath,
                        QStringLiteral("存在设备 DeviceId 为空"));
        } else if (seenDeviceIds.contains(deviceId)) {
            duplicateDeviceIds.insert(deviceId);
        } else {
            seenDeviceIds.insert(deviceId);
        }

        if (!device.modelId.trimmed().isEmpty() && !findModelById(project, device.modelId)) {
            appendIssue(configtool::ImportIssueSeverity::Warning,
                        devicePath,
                        QStringLiteral("设备 %1 引用的模型 %2 在当前工程中不存在")
                            .arg(device.deviceId, device.modelId));
        }

        if (device.protocol == configtool::ProtocolType::Iec104) {
            const QSet<QString> duplicateAddresses = duplicateBindingAddresses(device);
            if (!duplicateAddresses.isEmpty()) {
                appendIssue(configtool::ImportIssueSeverity::Error,
                            devicePath,
                            QStringLiteral("设备存在重复 104 地址：%1")
                                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，"))));
            }
            for (const configtool::PointBinding &binding : device.bindings) {
                if (binding.enabled && binding.address.trimmed().isEmpty()) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("设备 %1 存在启用但未填写地址的点位：%2")
                                    .arg(device.deviceId, binding.dataRef));
                    break;
                }
            }
        } else if (isModbusDevice(device)) {
            const QString type = device.transport.protocolOptions
                                     .value(QStringLiteral("type"))
                                     .toString(QStringLiteral("TCP"))
                                     .trimmed()
                                     .toUpper();
            if (type != QStringLiteral("TCP") && type != QStringLiteral("RTU")) {
                appendIssue(configtool::ImportIssueSeverity::Error,
                            devicePath,
                            QStringLiteral("Modbus 设备 %1 的 type 必须为 TCP 或 RTU").arg(device.deviceId));
            }
            bool portOk = false;
            device.transport.port.trimmed().toInt(&portOk);
            if (type == QStringLiteral("TCP") && (device.transport.ip.trimmed().isEmpty() || !portOk)) {
                appendIssue(configtool::ImportIssueSeverity::Error,
                            devicePath,
                            QStringLiteral("Modbus TCP 设备 %1 缺少 ip 或数字 port").arg(device.deviceId));
            }
            if (type == QStringLiteral("RTU")) {
                const QJsonObject rtu = device.transport.serial;
                if (uiJsonValueToString(rtu.value(QStringLiteral("serialPort"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("baud"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("dataBits"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("stopBits"))).trimmed().isEmpty()
                    || uiJsonValueToString(rtu.value(QStringLiteral("parity"))).trimmed().isEmpty()) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("Modbus RTU 设备 %1 缺少完整 rtu 参数").arg(device.deviceId));
                }
            }
            for (const configtool::PointBinding &binding : device.bindings) {
                if (!binding.enabled) {
                    continue;
                }
                if (binding.address.trimmed().isEmpty()) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("Modbus 设备 %1 存在启用但未生成 dataIndex 的点位：%2")
                                    .arg(device.deviceId, binding.dataRef));
                    break;
                }
                if (!binding.extensions.contains(QStringLiteral("modbusRegisterAddress"))) {
                    appendIssue(configtool::ImportIssueSeverity::Error,
                                devicePath,
                                QStringLiteral("Modbus 设备 %1 的点位 %2 未填写寄存器地址")
                                    .arg(device.deviceId, binding.dataRef));
                    break;
                }
            }
        }
    }
    for (const QString &deviceId : duplicateDeviceIds) {
        appendIssue(configtool::ImportIssueSeverity::Error,
                    projectPath,
                    QStringLiteral("设备 DeviceId 重复：%1").arg(deviceId));
    }

    const QList<configtool::ConfigIssue> logicIssues =
        configtool::validateLogicCenterConfig(project.logicCenter, &project);
    for (const configtool::ConfigIssue &issue : logicIssues) {
        appendConfigIssueForIssueTable(issue,
                                       QStringLiteral("cepLogicCenter/etc/LogicCenter_Config.json"),
                                       issues);
    }

    return issues;
}

QStringList MainWindow::configTransferRelativePaths(const QString &projectRoot) const
{
    QStringList paths;
    const QDir rootDir(projectRoot);
    for (const QString &relativePath : configTransferPathList()) {
        if (rootDir.exists(relativePath)) {
            paths.append(relativePath);
        }
    }
    return paths;
}

QString MainWindow::configRemoteTarget() const
{
    const QString host = m_configRemoteHostEdit ? m_configRemoteHostEdit->text().trimmed() : QString();
    const QString user = m_configRemoteUserEdit ? m_configRemoteUserEdit->text().trimmed() : QString();
    if (host.isEmpty()) {
        return QString();
    }
    return user.isEmpty() ? host : QStringLiteral("%1@%2").arg(user, host);
}

QString MainWindow::configRemoteBaseDir() const
{
    const QString baseDir = m_configRemoteBaseDirEdit
        ? m_configRemoteBaseDirEdit->text().trimmed()
        : QStringLiteral("/home/cepgateway/app");
    return baseDir.isEmpty() ? QStringLiteral("/home/cepgateway/app") : baseDir;
}

bool MainWindow::runConfigTransferProcess(const QString &program,
                                          const QStringList &arguments,
                                          const QString &title,
                                          QString *output,
                                          QProgressDialog *progress)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    if (progress) {
        progress->setLabelText(title);
        progress->show();
        QApplication::processEvents();
    }
    process.start(program, arguments);
    if (!process.waitForStarted(10000)) {
        if (output) {
            *output = process.errorString();
        }
        return false;
    }

    QByteArray outputBytes;
    QElapsedTimer elapsed;
    elapsed.start();
    while (!process.waitForFinished(100)) {
        outputBytes.append(process.readAllStandardOutput());
        if (progress) {
            const QString text = QString::fromUtf8(outputBytes);
            const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                progress->setLabelText(QStringLiteral("%1\n%2").arg(title, lines.last().trimmed()));
            }
            QApplication::processEvents();
            if (progress->wasCanceled()) {
                process.kill();
                process.waitForFinished(3000);
                if (output) {
                    *output = QStringLiteral("%1 已取消。").arg(title);
                }
                return false;
            }
        }
        if (elapsed.elapsed() > 180000) {
            process.kill();
            process.waitForFinished(3000);
            if (output) {
                *output = QStringLiteral("%1 超时。").arg(title);
            }
            return false;
        }
    }
    outputBytes.append(process.readAllStandardOutput());

    const QString text = QString::fromUtf8(outputBytes);
    if (output) {
        *output = text;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }
    return true;
}

void MainWindow::onUploadConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("请先选择配置工作区。"));
        return;
    }
    if (configRemoteTarget().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("请先填写设备地址。"));
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this,
        QStringLiteral("上传配置"),
        QStringLiteral("上传会先清空设备内对应配置目录，再写入当前工作区配置。\n\n请确认已经自行备份设备内原配置。\n\n是否继续上传？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    onExportIec104ConfigClicked();
    if (!m_lastConfigExportOk) {
        statusBar()->showMessage(QStringLiteral("导出未完成，已取消上传"), 5000);
        return;
    }

    const QStringList relativePaths = configTransferRelativePaths(projectRoot);
    if (relativePaths.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("当前工作区没有可上传的配置目录。"));
        return;
    }

    const QString archivePath = QDir::temp().filePath(
        QStringLiteral("cepb_config_upload_%1.tar.gz").arg(QUuid::createUuid().toString(QUuid::Id128)));
    QProgressDialog progress(QStringLiteral("准备上传配置..."), QStringLiteral("取消"), 0, 4, this);
    progress.setWindowTitle(QStringLiteral("上传配置"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    auto setUploadStep = [&progress](int value, const QString &text) {
        progress.setValue(value);
        progress.setLabelText(text);
        QApplication::processEvents();
    };

    QString output;
    QStringList tarArgs = {QStringLiteral("--options"), QStringLiteral("hdrcharset=UTF-8"),
                           QStringLiteral("-czvf"), archivePath, QStringLiteral("-C"), projectRoot};
    tarArgs.append(relativePaths);
    setUploadStep(0, QStringLiteral("本地打包配置..."));
    if (!runConfigTransferProcess(QStringLiteral("tar"), tarArgs, QStringLiteral("本地打包配置"), &output, &progress)) {
        QMessageBox::warning(this, QStringLiteral("上传配置"), QStringLiteral("本地配置打包失败：\n%1").arg(output));
        return;
    }
    setUploadStep(1, QStringLiteral("本地打包完成。"));

    const QString host = m_configRemoteHostEdit->text().trimmed();
    const QString user = m_configRemoteUserEdit->text().trimmed();
    const QString password = m_configRemotePasswordEdit ? m_configRemotePasswordEdit->text() : QString();
    const QString port = QString::number(m_configRemotePortEdit ? m_configRemotePortEdit->value() : 10022);
    const QString remoteBaseDir = configRemoteBaseDir();
    const QString remoteArchivePath = QStringLiteral("/tmp/cepb_config_upload.tar.gz");
    const bool hasPutty = !QStandardPaths::findExecutable(QStringLiteral("plink")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("pscp")).isEmpty();

    QStringList cleanupParts;
    cleanupParts << QStringLiteral("set -e")
                 << QStringLiteral("mkdir -p %1").arg(remoteShellQuote(remoteBaseDir));
    for (const QString &relativePath : relativePaths) {
        const QString remotePath = remotePathJoin(remoteBaseDir, relativePath);
        cleanupParts << QStringLiteral("rm -rf %1").arg(remoteShellQuote(remotePath))
                     << QStringLiteral("mkdir -p %1").arg(remoteShellQuote(remotePath));
    }
    const QString cleanupCommand = cleanupParts.join(QStringLiteral("; "));
    const QString extractCommand = QStringLiteral("set -e; tar -xzf %1 -C %2; rm -f %1")
        .arg(remoteShellQuote(remoteArchivePath), remoteShellQuote(remoteBaseDir));

    auto puttyArgs = [&](const QString &command) {
        QStringList args = {QStringLiteral("-batch"), QStringLiteral("-ssh"), QStringLiteral("-P"), port};
        if (!user.isEmpty()) {
            args << QStringLiteral("-l") << user;
        }
        if (!password.isEmpty()) {
            args << QStringLiteral("-pw") << password;
        }
        args << host << command;
        return args;
    };

    bool ok = false;
    if (hasPutty) {
        setUploadStep(1, QStringLiteral("清空设备内对应配置目录..."));
        ok = runConfigTransferProcess(QStringLiteral("plink"), puttyArgs(cleanupCommand), QStringLiteral("清空设备内对应配置目录"), &output, &progress);
        if (ok) {
            setUploadStep(2, QStringLiteral("上传配置压缩包..."));
            QStringList args = {QStringLiteral("-batch"), QStringLiteral("-P"), port};
            if (!user.isEmpty()) {
                args << QStringLiteral("-l") << user;
            }
            if (!password.isEmpty()) {
                args << QStringLiteral("-pw") << password;
            }
            args << archivePath << QStringLiteral("%1:%2").arg(host, remoteArchivePath);
            ok = runConfigTransferProcess(QStringLiteral("pscp"), args, QStringLiteral("上传配置压缩包"), &output, &progress);
        }
        if (ok) {
            setUploadStep(3, QStringLiteral("设备端解包配置..."));
            ok = runConfigTransferProcess(QStringLiteral("plink"), puttyArgs(extractCommand), QStringLiteral("设备端解包配置"), &output, &progress);
        }
    } else {
        const QString target = configRemoteTarget();
        setUploadStep(1, QStringLiteral("清空设备内对应配置目录..."));
        ok = runConfigTransferProcess(QStringLiteral("ssh"),
                                      {QStringLiteral("-p"), port, target, cleanupCommand},
                                      QStringLiteral("清空设备内对应配置目录"),
                                      &output,
                                      &progress);
        if (ok) {
            setUploadStep(2, QStringLiteral("上传配置压缩包..."));
            ok = runConfigTransferProcess(QStringLiteral("scp"),
                                          {QStringLiteral("-P"), port, archivePath, QStringLiteral("%1:%2").arg(target, remoteArchivePath)},
                                          QStringLiteral("上传配置压缩包"),
                                          &output,
                                          &progress);
        }
        if (ok) {
            setUploadStep(3, QStringLiteral("设备端解包配置..."));
            ok = runConfigTransferProcess(QStringLiteral("ssh"),
                                          {QStringLiteral("-p"), port, target, extractCommand},
                                          QStringLiteral("设备端解包配置"),
                                          &output,
                                          &progress);
        }
    }

    QFile::remove(archivePath);
    if (!ok) {
        QMessageBox::warning(this,
                             QStringLiteral("上传配置"),
                             QStringLiteral("配置上传失败。\n\n若设备需要密码，请确认本机已安装 plink/pscp，程序会使用密码 root 自动连接。\n\n%1").arg(output));
        statusBar()->showMessage(QStringLiteral("配置上传失败"), 5000);
        return;
    }

    progress.setValue(4);
    statusBar()->showMessage(QStringLiteral("配置上传完成"), 8000);
    QMessageBox::information(this, QStringLiteral("上传配置"), QStringLiteral("配置上传完成。"));
}

void MainWindow::onDownloadConfigClicked()
{
    if (configRemoteTarget().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("下载配置"), QStringLiteral("请先填写设备地址。"));
        return;
    }

    const QString startDir = configBrowseStartDir();
    const QString targetDir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择下载保存目录"), startDir);
    if (targetDir.isEmpty()) {
        return;
    }

    const QString host = m_configRemoteHostEdit->text().trimmed();
    const QString user = m_configRemoteUserEdit->text().trimmed();
    const QString password = m_configRemotePasswordEdit ? m_configRemotePasswordEdit->text() : QString();
    const QString port = QString::number(m_configRemotePortEdit ? m_configRemotePortEdit->value() : 10022);
    const QString remoteBaseDir = configRemoteBaseDir();
    const QString remoteArchivePath = QStringLiteral("/tmp/cepb_config_download.tar.gz");
    const QString archivePath = QDir::temp().filePath(
        QStringLiteral("cepb_config_download_%1.tar.gz").arg(QUuid::createUuid().toString(QUuid::Id128)));
    const bool hasPutty = !QStandardPaths::findExecutable(QStringLiteral("plink")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("pscp")).isEmpty();

    QStringList quotedPaths;
    for (const QString &relativePath : configTransferPathList()) {
        quotedPaths << remoteShellQuote(relativePath);
    }
    const QString packageCommand = QStringLiteral(
        "set -e; cd %1; paths=\"\"; for p in %2; do [ -e \"$p\" ] && { find \"$p\" -type f -print; paths=\"$paths $p\"; }; done; "
        "[ -n \"$paths\" ] || { echo 'no config paths found'; exit 2; }; tar -czf %3 $paths")
        .arg(remoteShellQuote(remoteBaseDir),
             quotedPaths.join(QLatin1Char(' ')),
             remoteShellQuote(remoteArchivePath));

    auto puttyArgs = [&](const QString &command) {
        QStringList args = {QStringLiteral("-batch"), QStringLiteral("-ssh"), QStringLiteral("-P"), port};
        if (!user.isEmpty()) {
            args << QStringLiteral("-l") << user;
        }
        if (!password.isEmpty()) {
            args << QStringLiteral("-pw") << password;
        }
        args << host << command;
        return args;
    };

    QProgressDialog progress(QStringLiteral("准备下载配置..."), QStringLiteral("取消"), 0, 4, this);
    progress.setWindowTitle(QStringLiteral("下载配置"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    auto setDownloadStep = [&progress](int value, const QString &text) {
        progress.setValue(value);
        progress.setLabelText(text);
        QApplication::processEvents();
    };

    QString output;
    bool ok = false;
    if (hasPutty) {
        setDownloadStep(0, QStringLiteral("设备端扫描并打包配置..."));
        ok = runConfigTransferProcess(QStringLiteral("plink"), puttyArgs(packageCommand), QStringLiteral("设备端扫描并打包配置"), &output, &progress);
        if (ok) {
            setDownloadStep(1, QStringLiteral("下载配置压缩包..."));
            QStringList args = {QStringLiteral("-batch"), QStringLiteral("-P"), port};
            if (!user.isEmpty()) {
                args << QStringLiteral("-l") << user;
            }
            if (!password.isEmpty()) {
                args << QStringLiteral("-pw") << password;
            }
            args << QStringLiteral("%1:%2").arg(host, remoteArchivePath) << archivePath;
            ok = runConfigTransferProcess(QStringLiteral("pscp"), args, QStringLiteral("下载配置压缩包"), &output, &progress);
        }
        if (ok) {
            runConfigTransferProcess(QStringLiteral("plink"),
                                     puttyArgs(QStringLiteral("rm -f %1").arg(remoteShellQuote(remoteArchivePath))),
                                     QStringLiteral("清理远端临时文件"));
        }
    } else {
        const QString target = configRemoteTarget();
        setDownloadStep(0, QStringLiteral("设备端扫描并打包配置..."));
        ok = runConfigTransferProcess(QStringLiteral("ssh"),
                                      {QStringLiteral("-p"), port, target, packageCommand},
                                      QStringLiteral("设备端扫描并打包配置"),
                                      &output,
                                      &progress);
        if (ok) {
            setDownloadStep(1, QStringLiteral("下载配置压缩包..."));
            ok = runConfigTransferProcess(QStringLiteral("scp"),
                                          {QStringLiteral("-P"), port, QStringLiteral("%1:%2").arg(target, remoteArchivePath), archivePath},
                                          QStringLiteral("下载配置压缩包"),
                                          &output,
                                          &progress);
        }
        if (ok) {
            runConfigTransferProcess(QStringLiteral("ssh"),
                                     {QStringLiteral("-p"), port, target, QStringLiteral("rm -f %1").arg(remoteShellQuote(remoteArchivePath))},
                                     QStringLiteral("清理远端临时文件"));
        }
    }

    if (!ok) {
        QFile::remove(archivePath);
        QMessageBox::warning(this,
                             QStringLiteral("下载配置"),
                             QStringLiteral("配置下载失败。\n\n若设备需要密码，请确认本机已安装 plink/pscp，程序会使用密码 root 自动连接。\n\n%1").arg(output));
        statusBar()->showMessage(QStringLiteral("配置下载失败"), 5000);
        return;
    }

    setDownloadStep(2, QStringLiteral("本地解包配置..."));
    if (!runConfigTransferProcess(QStringLiteral("tar"),
                                  {QStringLiteral("--options"), QStringLiteral("hdrcharset=UTF-8"),
                                   QStringLiteral("-xzvf"), archivePath, QStringLiteral("-C"), targetDir},
                                  QStringLiteral("本地解包配置"),
                                  &output,
                                  &progress)) {
        QFile::remove(archivePath);
        QMessageBox::warning(this, QStringLiteral("下载配置"), QStringLiteral("配置解包失败：\n%1").arg(output));
        return;
    }
    QFile::remove(archivePath);

    m_configImportDirEdit->setText(targetDir);
    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    settings.setValue(QStringLiteral("config/lastBrowseDir"), targetDir);
    setDownloadStep(3, QStringLiteral("重新导入下载后的配置..."));
    onImportIec104ConfigClicked();
    progress.setValue(4);
    statusBar()->showMessage(QStringLiteral("配置下载完成"), 8000);
}

QString MainWindow::normalizedConfigProjectRoot(const QString &selectedPath) const
{
    if (selectedPath.isEmpty()) {
        return QString();
    }

    const QFileInfo selectedInfo(selectedPath);
    const QString absolutePath = selectedInfo.absoluteFilePath();
    const QString folderName = selectedInfo.fileName().trimmed();
    if (folderName.compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }
    if (folderName.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }
    if (folderName.compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }

    return absolutePath;
}

QString MainWindow::resolveIec104AppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepiec104"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::resolveModbusAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepmodbus"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::resolveLogicCenterAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::configBrowseStartDir() const
{
    const QString currentValue = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (!currentValue.isEmpty() && QDir(currentValue).exists()) {
        return currentValue;
    }

    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    const QString lastDir = settings.value(QStringLiteral("config/lastBrowseDir")).toString().trimmed();
    if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
        return lastDir;
    }

    return QDir::homePath();
}

void MainWindow::onConfigModelSelectionChanged()
{
    const int modelIndex = currentConfigModelIndex();
    if (m_deleteModelBtn) {
        m_deleteModelBtn->setEnabled(modelIndex >= 0);
    }
    refreshModelOverview(modelIndex);
    refreshModelDetail(modelIndex);
    refreshSelectionOverview();
}

void MainWindow::onConfigDeviceSelectionChanged()
{
    const int deviceIndex = currentConfigDeviceIndex();
    if (m_deleteDeviceBtn) {
        m_deleteDeviceBtn->setEnabled(deviceIndex >= 0);
    }
    refreshDeviceDetail(deviceIndex);
    if (deviceIndex >= 0) {
        refreshDeviceEditor(deviceIndex);
    } else {
        refreshDeviceEditor(-1);
    }
    refreshSelectionOverview();
    if (m_deleteModelBtn) {
        m_deleteModelBtn->setEnabled(currentConfigModelIndex() >= 0);
    }
    if (m_deleteDeviceBtn) {
        m_deleteDeviceBtn->setEnabled(currentConfigDeviceIndex() >= 0);
    }
}

void MainWindow::onConfigModelActivated(int row, int /*column*/)
{
    if (row < 0) {
        return;
    }

    m_configModelTable->selectRow(row);
    refreshModelDetail(row);
    m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
}

void MainWindow::onConfigDeviceActivated(int row, int /*column*/)
{
    if (row < 0) {
        return;
    }

    m_configDeviceTable->selectRow(row);
    refreshDeviceDetail(row);
    refreshDeviceEditor(row);
    m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
}

void MainWindow::onConfigIssueActivated(int row, int /*column*/)
{
    navigateToConfigIssue(row);
}

void MainWindow::navigateToConfigIssue(int row)
{
    if (!m_configIssueTable || row < 0 || row >= m_configIssueTable->rowCount()) {
        return;
    }

    QTableWidgetItem *anchorItem = m_configIssueTable->item(row, 0);
    if (!anchorItem) {
        return;
    }

    const QString targetType = anchorItem->data(ConfigIssueRoleTargetType).toString();
    const QString targetKey = anchorItem->data(ConfigIssueRoleTargetKey).toString();
    const configtool::ConfigProject &project = m_configProjectManager.project();

    if (targetType == QStringLiteral("model")) {
        for (int index = 0; index < project.models.size(); ++index) {
            if (project.models.at(index).modelId == targetKey) {
                onConfigModelActivated(index, 0);
                return;
            }
        }
    } else if (targetType == QStringLiteral("device")) {
        for (int index = 0; index < project.devices.size(); ++index) {
            if (project.devices.at(index).deviceId == targetKey) {
                onConfigDeviceActivated(index, 0);
                return;
            }
        }
    } else if (targetType == QStringLiteral("logic-computation")) {
        refreshLogicComputationPointPage();
        m_mainTabWidget->setCurrentWidget(m_logicComputationPointPage);
        const QString deviceId = targetKey.section(QLatin1Char('#'), 0, 0);
        const QString dataRef = targetKey.section(QLatin1Char('#'), 1);
        const QList<configtool::LogicComputationPoint> &points = project.logicCenter.computationPoints;
        for (int index = 0; index < points.size(); ++index) {
            if ((points.at(index).deviceId == deviceId && points.at(index).dataRef == dataRef)
                || targetKey.contains(points.at(index).deviceId + QLatin1Char('#') + points.at(index).dataRef)) {
                m_logicComputationPointTable->selectRow(index);
                return;
            }
        }
        statusBar()->showMessage(QStringLiteral("已进入计算点页面，但未找到精确对象"), 5000);
        return;
    } else if (targetType == QStringLiteral("logic-control")) {
        refreshLogicControlRulePage();
        m_mainTabWidget->setCurrentWidget(m_logicControlRulePage);
        const QString deviceId = targetKey.section(QLatin1Char('#'), 0, 0);
        const QString dataRef = targetKey.section(QLatin1Char('#'), 1);
        const QList<configtool::LogicControlRule> &rules = project.logicCenter.controlRules;
        for (int index = 0; index < rules.size(); ++index) {
            if ((rules.at(index).matchDeviceId == deviceId && rules.at(index).matchDataRef == dataRef)
                || targetKey.contains(rules.at(index).matchDeviceId + QLatin1Char('#') + rules.at(index).matchDataRef)) {
                m_logicControlRuleTable->selectRow(index);
                refreshLogicControlTargetTable();
                return;
            }
        }
        statusBar()->showMessage(QStringLiteral("已进入控制转换页面，但未找到精确对象"), 5000);
        return;
    } else if (targetType == QStringLiteral("logic-online")) {
        refreshLogicOnlineLinkPage();
        m_mainTabWidget->setCurrentWidget(m_logicOnlineLinkPage);
        const QList<configtool::LogicOnlineStatusLink> &links = project.logicCenter.onlineStatusLinks;
        for (int index = 0; index < links.size(); ++index) {
            const QString objectId = QStringLiteral("%1 -> %2").arg(links.at(index).deviceId, links.at(index).linkToDeviceId);
            if (objectId == targetKey || targetKey.contains(objectId)) {
                m_logicOnlineLinkTable->selectRow(index);
                return;
            }
        }
        statusBar()->showMessage(QStringLiteral("已进入在线联动页面，但未找到精确对象"), 5000);
        return;
    } else if (targetType == QStringLiteral("logic-agcavc")) {
        refreshLogicAgcAvcPage();
        m_mainTabWidget->setCurrentWidget(m_logicAgcAvcPage);
        statusBar()->showMessage(QStringLiteral("已进入 AGC/AVC 页面"), 5000);
        return;
    }

    m_mainTabWidget->setCurrentWidget(m_configIssuePage);
    statusBar()->showMessage(QStringLiteral("该问题暂时无法定位到具体编辑对象"), 5000);
}

void MainWindow::onNewModelClicked()
{
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (project.projectId.isEmpty()) {
        m_configProjectManager.createEmptyProject(QStringLiteral("本地配置工程"), QString());
    }

    configtool::ModelTemplate model;
    model.modelId = QStringLiteral("model_new_%1").arg(project.models.size() + 1);
    model.name = model.modelId;
    model.displayName = QStringLiteral("新模型%1").arg(project.models.size() + 1);
    model.deviceType = QStringLiteral("未定义设备");
    model.version = QStringLiteral("1.0");
    model.ensureDefaultServices();
    project.models.append(model);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int row = m_configModelTable->rowCount() - 1;
    if (row >= 0) {
        m_configModelTable->selectRow(row);
    }
    m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    statusBar()->showMessage(QStringLiteral("已创建模型骨架"), 4000);
}

void MainWindow::onModelFieldEdited()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    QString oldModelId = m_modelIdEdit
        ? m_modelIdEdit->property(ModelEditorOriginalModelIdProperty).toString().trimmed()
        : QString();
    if (oldModelId.isEmpty()) {
        oldModelId = model.modelId.trimmed();
    }
    const QString newModelId = m_modelIdEdit->text().trimmed();
    model.modelId = newModelId;
    model.name = model.modelId;
    model.displayName = m_modelDisplayNameEdit->text().trimmed();
    model.deviceType = m_modelDeviceTypeEdit->text().trimmed();
    model.version = m_modelVersionEdit->text().trimmed();
    model.manufacturerId = m_modelManufacturerIdEdit->text().trimmed();
    model.manufacturerDesc = m_modelManufacturerDescEdit->text().trimmed();
    model.schema = m_modelSchemaEdit->text().trimmed();
    model.northVisible = !m_modelNorthVisibleCheck || m_modelNorthVisibleCheck->isChecked();

    const int renamedReferenceCount = renameModelReferences(oldModelId, newModelId);
    if (m_modelIdEdit && !newModelId.isEmpty() && oldModelId != newModelId) {
        m_modelIdEdit->setProperty(ModelEditorOriginalModelIdProperty, newModelId);
    }
    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    if (sender() == m_modelNorthVisibleCheck) {
        refreshModelDetail(modelIndex);
    }
    if (renamedReferenceCount > 0) {
        statusBar()->showMessage(QStringLiteral("已同步更新 %1 处模型引用").arg(renamedReferenceCount), 5000);
    }
}

int MainWindow::renameModelReferences(const QString &oldModelId, const QString &newModelId)
{
    const QString oldId = oldModelId.trimmed();
    const QString newId = newModelId.trimmed();
    if (oldId.isEmpty() || newId.isEmpty() || oldId == newId) {
        return 0;
    }

    int updateCount = 0;
    configtool::ConfigProject &project = m_configProjectManager.project();
    for (configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.modelId.trimmed() == oldId) {
            device.modelId = newId;
            ++updateCount;
        }
        const QString oldPointRefPrefix = oldId + QLatin1Char('#');
        const QString newPointRefPrefix = newId + QLatin1Char('#');
        for (configtool::PointBinding &binding : device.bindings) {
            if (binding.pointRef.startsWith(oldPointRefPrefix)) {
                binding.pointRef = newPointRefPrefix + binding.pointRef.mid(oldPointRefPrefix.size());
                ++updateCount;
            }
        }
    }

    return updateCount;
}

int MainWindow::renameModelPointReferences(const QString &modelId,
                                           const QString &oldDataRef,
                                           const QString &newDataRef)
{
    const QString targetModelId = modelId.trimmed();
    const QString oldRef = oldDataRef.trimmed();
    const QString newRef = newDataRef.trimmed();
    if (targetModelId.isEmpty() || oldRef.isEmpty() || newRef.isEmpty() || oldRef == newRef) {
        return 0;
    }

    int updateCount = 0;
    QSet<QString> affectedDeviceIds;
    configtool::ConfigProject &project = m_configProjectManager.project();
    const QString oldPointRef = targetModelId + QLatin1Char('#') + oldRef;
    const QString newPointRef = targetModelId + QLatin1Char('#') + newRef;

    for (configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.modelId.trimmed() != targetModelId) {
            continue;
        }

        const QString deviceId = device.deviceId.trimmed();
        if (!deviceId.isEmpty()) {
            affectedDeviceIds.insert(deviceId);
        }
        for (configtool::PointBinding &binding : device.bindings) {
            if (binding.dataRef.trimmed() == oldRef) {
                binding.dataRef = newRef;
                ++updateCount;
            }
            if (binding.pointRef.trimmed() == oldPointRef) {
                binding.pointRef = newPointRef;
                ++updateCount;
            }
        }
    }

    if (affectedDeviceIds.isEmpty()) {
        return updateCount;
    }

    auto isAffectedDevice = [&](const QString &deviceId) {
        return affectedDeviceIds.contains(deviceId.trimmed());
    };
    auto renamePointRef = [&](const QString &deviceId, QString &dataRef) {
        if (isAffectedDevice(deviceId) && dataRef.trimmed() == oldRef) {
            dataRef = newRef;
            ++updateCount;
        }
    };
    auto renameRealtimeRefs = [&](QString &expr) {
        if (expr.isEmpty()) {
            return;
        }
        const QString before = expr;
        for (const QString &deviceId : affectedDeviceIds) {
            const QRegularExpression pattern(QStringLiteral("\\{rt:%1#%2\\}")
                                                 .arg(QRegularExpression::escape(deviceId),
                                                      QRegularExpression::escape(oldRef)));
            expr.replace(pattern, QStringLiteral("{rt:%1#%2}").arg(deviceId, newRef));
        }
        if (expr != before) {
            ++updateCount;
        }
    };

    configtool::LogicCenterConfig &logic = project.logicCenter;
    for (configtool::LogicComputationPoint &point : logic.computationPoints) {
        renamePointRef(point.deviceId, point.dataRef);
        for (configtool::LogicOperand &operand : point.operands) {
            renamePointRef(operand.deviceId, operand.dataRef);
        }
    }

    for (configtool::LogicControlRule &rule : logic.controlRules) {
        renamePointRef(rule.matchDeviceId, rule.matchDataRef);
        for (configtool::LogicControlTarget &target : rule.targets) {
            renamePointRef(target.deviceId, target.dataRef);
            renameRealtimeRefs(target.expr);
        }
    }

    for (configtool::AgcAvcGroup &group : logic.agcAvcGroups) {
        for (configtool::AgcAvcDevice &device : group.devices) {
            renamePointRef(device.deviceId, device.ctrlDataRefP);
            renamePointRef(device.deviceId, device.ctrlDataRefQ);
            const QString onlineDeviceId = device.onlineDeviceId.trimmed().isEmpty()
                ? device.deviceId
                : device.onlineDeviceId;
            renamePointRef(onlineDeviceId, device.onlineDataRef);
        }
    }

    return updateCount;
}

void MainWindow::onAddPointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个模型。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    model.ensureDefaultServices();
    configtool::ModelServiceType newPointType = static_cast<configtool::ModelServiceType>(
        m_newPointCategoryCombo->currentData().toInt());
    if (m_modelPointFilterTabBar->currentIndex() > 0) {
        newPointType = modelServiceTypeFromTabIndex(m_modelPointFilterTabBar->currentIndex());
    }

    configtool::ServiceTemplate *service = model.findService(newPointType);
    if (!service) {
        return;
    }

    configtool::PointTemplate point;
    point.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    point.category = newPointType;
    point.signalType = signalTypeForModelService(newPointType);
    point.controlKind = newPointType == configtool::ModelServiceType::Control
        ? configtool::ControlKind::RemoteControl
        : configtool::ControlKind::None;
    const int nextIndex = service->points.size() + 1;
    point.name = QStringLiteral("NewPoint%1").arg(nextIndex);
    point.description = QStringLiteral("新建点位%1").arg(nextIndex);
    point.ldName = QStringLiteral("PROT");
    point.lnType = defaultLnTypeForModelService(newPointType);
    point.lnInst = QStringLiteral("1");
    point.doName = point.name;
    point.doType = newPointType == configtool::ModelServiceType::Status
        ? QStringLiteral("SPS")
        : (newPointType == configtool::ModelServiceType::Control
            ? QStringLiteral("SPC")
            : QStringLiteral("MV"));
    point.dataType = newPointType == configtool::ModelServiceType::Measurement
        ? QStringLiteral("Float")
        : QStringLiteral("Bool");
    service->points.append(point);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    selectModelPointById(point.pointId);
    statusBar()->showMessage(QStringLiteral("已新增模型点位"), 3000);
}

void MainWindow::onModelPointFilterChanged(int index)
{
    if (index > 0) {
        const int comboIndex = m_newPointCategoryCombo->findData(static_cast<int>(modelServiceTypeFromTabIndex(index)));
        if (comboIndex >= 0) {
            QSignalBlocker blocker(m_newPointCategoryCombo);
            m_newPointCategoryCombo->setCurrentIndex(comboIndex);
        }
    }

    refreshModelDetail(currentConfigModelIndex());
}

void MainWindow::onModelPointDataRefFilterTextChanged(const QString & /*text*/)
{
    refreshModelDetail(currentConfigModelIndex());
}

void MainWindow::onModelPointDescriptionFilterTextChanged(const QString & /*text*/)
{
    refreshModelDetail(currentConfigModelIndex());
}

void MainWindow::onModelPointCategoryChanged(int index)
{
    if (m_updatingModelPointCategory || index < 0) {
        return;
    }

    auto *combo = qobject_cast<QComboBox*>(sender());
    if (!combo) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    const int serviceIndex = combo->property("serviceIndex").toInt();
    const int pointIndex = combo->property("pointIndex").toInt();
    const configtool::ModelServiceType targetType = static_cast<configtool::ModelServiceType>(combo->itemData(index).toInt());

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &sourceService = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= sourceService.points.size()) {
        return;
    }

    configtool::PointTemplate point = sourceService.points.at(pointIndex);
    if (point.category == targetType) {
        return;
    }

    sourceService.points.removeAt(pointIndex);
    point.category = targetType;
    point.signalType = signalTypeForModelService(targetType);
    point.controlKind = targetType == configtool::ModelServiceType::Control
        ? configtool::ControlKind::RemoteControl
        : configtool::ControlKind::None;
    configtool::ServiceTemplate *targetService = model.findService(targetType);
    if (!targetService) {
        return;
    }
    targetService->points.append(point);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    selectModelPointById(point.pointId);
    statusBar()->showMessage(QStringLiteral("已调整点位类别"), 3000);
}

void MainWindow::onCopyPointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    const QPair<int, int> pointLocation = currentModelPointLocation();
    if (modelIndex < 0 || pointLocation.first < 0 || pointLocation.second < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (pointLocation.first >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[pointLocation.first];
    if (pointLocation.second >= service.points.size()) {
        return;
    }

    configtool::PointTemplate copied = service.points.at(pointLocation.second);
    copied.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copied.name += QStringLiteral("_copy");
    copied.doName = copied.name;
    copied.description += QStringLiteral("-副本");
    service.points.insert(pointLocation.second + 1, copied);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    if (pointLocation.second + 1 < m_modelPointsTable->rowCount()) {
        m_modelPointsTable->selectRow(pointLocation.second + 1);
    }
    statusBar()->showMessage(QStringLiteral("已复制模型点位"), 3000);
}

void MainWindow::onDeletePointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    const QPair<int, int> pointLocation = currentModelPointLocation();
    if (modelIndex < 0 || pointLocation.first < 0 || pointLocation.second < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除点位"),
                              QStringLiteral("确定删除当前选中的模型点位吗？")) != QMessageBox::Yes) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (pointLocation.first >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[pointLocation.first];
    if (pointLocation.second >= service.points.size()) {
        return;
    }

    service.points.removeAt(pointLocation.second);
    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    statusBar()->showMessage(QStringLiteral("已删除模型点位"), 3000);
}

void MainWindow::onCreateDeviceFromModelClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个模型。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    bool accepted = false;
    const QString protocolName = QInputDialog::getItem(
        this,
        QStringLiteral("选择协议设备"),
        QStringLiteral("请选择要创建的协议设备:"),
        {QStringLiteral("104"), QStringLiteral("Modbus")},
        0,
        false,
        &accepted);
    if (!accepted || protocolName.isEmpty()) {
        return;
    }
    const bool createModbus = protocolName.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0;

    const QString deviceId = QInputDialog::getText(
        this,
        createModbus ? QStringLiteral("创建 Modbus 设备") : QStringLiteral("创建 104 设备"),
        QStringLiteral("请输入 DeviceId:"),
        QLineEdit::Normal,
        QStringLiteral("1"),
        &accepted).trimmed();
    if (!accepted || deviceId.isEmpty()) {
        return;
    }

    configtool::ProtocolDeviceInstance device;
    device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    device.appType = createModbus ? QStringLiteral("cepmodbus") : QStringLiteral("cepiec104");
    device.protocol = createModbus ? configtool::ProtocolType::Modbus : configtool::ProtocolType::Iec104;
    device.deviceId = deviceId;
    device.deviceDesc = model.displayName.isEmpty() ? model.modelId : model.displayName;
    device.modelId = model.modelId;
    if (createModbus) {
        device.transport.protocolOptions.insert(QStringLiteral("type"), QStringLiteral("RTU"));
        device.transport.protocolOptions.insert(QStringLiteral("debug"), QStringLiteral("off"));
        device.transport.serial.insert(QStringLiteral("serialPort"), QStringLiteral("RS485_1"));
        device.transport.serial.insert(QStringLiteral("baud"), QStringLiteral("9600"));
        device.transport.serial.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
        device.transport.serial.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
        device.transport.serial.insert(QStringLiteral("parity"), QStringLiteral("N"));
        device.modbus.yxType = QStringLiteral("BIT");
        device.modbus.ycType = QStringLiteral("WORD");
        device.modbus.ytType = QStringLiteral("WORD");
        device.modbus.ycScale = QStringLiteral("1.0");
        device.modbus.ytScale = QStringLiteral("1.0");
    }

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            configtool::PointBinding binding;
            binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            binding.pointRef = point.pointRef(model.modelId);
            binding.dataRef = point.dataRef();
            binding.descriptionOverride = point.description;
            binding.enabled = true;
            if (createModbus) {
                QString kind = QStringLiteral("yc");
                if (point.category == configtool::ModelServiceType::Status) {
                    kind = QStringLiteral("yx");
                } else if (point.category == configtool::ModelServiceType::Control) {
                    const QString dataType = point.dataType.trimmed().toLower();
                    kind = dataType == QStringLiteral("boolean") || dataType == QStringLiteral("dbool")
                        ? QStringLiteral("yk")
                        : QStringLiteral("yt");
                }
                const QString lowerDataType = point.dataType.toLower();
                const QString modbusDataType = kind == QStringLiteral("yx")
                    ? QStringLiteral("BIT")
                    : (lowerDataType.contains(QStringLiteral("float")) ? QStringLiteral("FLOAT") : QStringLiteral("WORD"));
                binding.extensions.insert(QStringLiteral("modbusKind"), kind);
                binding.extensions.insert(QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : (kind == QStringLiteral("yc") ? 3 : 6));
                binding.extensions.insert(QStringLiteral("modbusDataType"), modbusDataType);
                binding.extensions.insert(QStringLiteral("modbusScale"), QStringLiteral("1.0"));
            }
            device.bindings.append(binding);
        }
    }

    project.devices.append(device);
    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int row = m_configDeviceTable->rowCount() - 1;
    if (row >= 0) {
        m_configDeviceTable->selectRow(row);
    }
    m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    statusBar()->showMessage(QStringLiteral("已根据模型生成 104 设备绑定骨架"), 4000);
}

void MainWindow::onDeleteModelClicked()
{
    const int modelIndex = currentConfigModelIndex();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        QMessageBox::information(this, QStringLiteral("删除模型"), QStringLiteral("请先选择要删除的模型。"));
        return;
    }

    const configtool::ModelTemplate model = project.models.at(modelIndex);
    int referencedDeviceCount = 0;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.modelId == model.modelId) {
            ++referencedDeviceCount;
        }
    }

    QString message = QStringLiteral("确定删除模型“%1”？")
        .arg(model.displayName.isEmpty() ? model.modelId : model.displayName);
    if (referencedDeviceCount > 0) {
        message += QStringLiteral("\n\n当前有 %1 个设备使用该模型，删除模型时这些设备也会一起删除。")
            .arg(referencedDeviceCount);
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除模型"),
                              message,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    pushConfigUndoSnapshot();
    const QString deletedModelId = model.modelId;
    for (int index = project.devices.size() - 1; index >= 0; --index) {
        if (project.devices.at(index).modelId == deletedModelId) {
            project.devices.removeAt(index);
        }
    }
    project.models.removeAt(modelIndex);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int nextModelIndex = qMin(modelIndex, project.models.size() - 1);
    if (nextModelIndex >= 0) {
        m_configModelTable->selectRow(nextModelIndex);
    }
    refreshModelOverview(currentConfigModelIndex());
    refreshModelDetail(currentConfigModelIndex());
    refreshDeviceDetail(currentConfigDeviceIndex());
    refreshDeviceEditor(currentConfigDeviceIndex());
    statusBar()->showMessage(QStringLiteral("已删除模型"), 3000);
}

void MainWindow::onDeleteDeviceClicked()
{
    const int deviceIndex = currentConfigDeviceIndex();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        QMessageBox::information(this, QStringLiteral("删除设备"), QStringLiteral("请先选择要删除的设备。"));
        return;
    }

    const configtool::ProtocolDeviceInstance device = project.devices.at(deviceIndex);
    const QString deviceName = device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc;
    if (QMessageBox::question(this,
                              QStringLiteral("删除设备"),
                              QStringLiteral("确定删除设备“%1”？").arg(deviceName),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    pushConfigUndoSnapshot();
    project.devices.removeAt(deviceIndex);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int nextDeviceIndex = qMin(deviceIndex, project.devices.size() - 1);
    if (nextDeviceIndex >= 0) {
        m_configDeviceTable->selectRow(nextDeviceIndex);
    }
    refreshSelectionOverview();
    refreshDeviceDetail(currentConfigDeviceIndex());
    refreshDeviceEditor(currentConfigDeviceIndex());
    statusBar()->showMessage(QStringLiteral("已删除设备"), 3000);
}

void MainWindow::onDeviceFieldEdited()
{
    const int deviceIndex = currentConfigDeviceIndex();
    if (deviceIndex < 0) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    QString oldDeviceId = m_deviceIdEdit
        ? m_deviceIdEdit->property(DeviceEditorOriginalDeviceIdProperty).toString().trimmed()
        : QString();
    if (oldDeviceId.isEmpty()) {
        oldDeviceId = device.deviceId.trimmed();
    }
    const QString newDeviceId = m_deviceIdEdit->text().trimmed();
    device.deviceId = newDeviceId;
    device.deviceDesc = m_deviceDescEdit->text().trimmed();
    device.transport.stationAddress = m_deviceStationAddressEdit->text().trimmed();
    device.transport.ip = m_deviceIpEdit->text().trimmed();
    device.transport.port = m_devicePortEdit->text().trimmed();
    if (isModbusDevice(device)) {
        const QString type = m_modbusTypeCombo && m_modbusTypeCombo->currentIndex() >= 0
            ? m_modbusTypeCombo->currentText().trimmed().toUpper()
            : QStringLiteral("TCP");
        device.transport.protocolOptions.insert(QStringLiteral("type"), type);
        device.transport.protocolOptions.insert(QStringLiteral("debug"), m_modbusDebugCheck && m_modbusDebugCheck->isChecked()
            ? QStringLiteral("on")
            : QStringLiteral("off"));
        if (m_modbusHwVariantCombo) {
            project.modbus.hwVariant = m_modbusHwVariantCombo->currentData().toString().trimmed();
        }

        QJsonObject rtu;
        rtu.insert(QStringLiteral("serialPort"), m_modbusSerialPortCombo && m_modbusSerialPortCombo->currentIndex() >= 0
            ? m_modbusSerialPortCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("baud"), m_modbusBaudCombo && m_modbusBaudCombo->currentIndex() >= 0
            ? m_modbusBaudCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("dataBits"), m_modbusDataBitsCombo && m_modbusDataBitsCombo->currentIndex() >= 0
            ? m_modbusDataBitsCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("stopBits"), m_modbusStopBitsCombo && m_modbusStopBitsCombo->currentIndex() >= 0
            ? m_modbusStopBitsCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("parity"), m_modbusParityCombo && m_modbusParityCombo->currentIndex() >= 0
            ? m_modbusParityCombo->currentText().trimmed()
            : QString());
        device.transport.serial = rtu;
    }

    const int renamedReferenceCount = renameLogicDeviceReferences(oldDeviceId, newDeviceId);
    if (m_deviceIdEdit && !newDeviceId.isEmpty() && oldDeviceId != newDeviceId) {
        m_deviceIdEdit->setProperty(DeviceEditorOriginalDeviceIdProperty, newDeviceId);
    }
    refreshConfigObjectViews();
    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    if (deviceIndex < m_configDeviceTable->rowCount()) {
        m_configDeviceTable->selectRow(deviceIndex);
    }
    if (renamedReferenceCount > 0) {
        statusBar()->showMessage(QStringLiteral("已同步更新 %1 处 DeviceId 引用").arg(renamedReferenceCount), 5000);
    }
}

int MainWindow::renameLogicDeviceReferences(const QString &oldDeviceId, const QString &newDeviceId)
{
    const QString oldId = oldDeviceId.trimmed();
    const QString newId = newDeviceId.trimmed();
    if (oldId.isEmpty() || newId.isEmpty() || oldId == newId) {
        return 0;
    }

    int updateCount = 0;
    auto renameDeviceId = [&](QString &deviceId) {
        if (deviceId.trimmed() != oldId) {
            return;
        }
        deviceId = newId;
        ++updateCount;
    };
    auto renameRealtimeRefs = [&](QString &expr) {
        if (expr.isEmpty()) {
            return;
        }
        const QString before = expr;
        const QRegularExpression pattern(QStringLiteral("\\{rt:%1#").arg(QRegularExpression::escape(oldId)));
        expr.replace(pattern, QStringLiteral("{rt:%1#").arg(newId));
        if (expr != before) {
            ++updateCount;
        }
    };

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    for (configtool::LogicComputationPoint &point : logic.computationPoints) {
        renameDeviceId(point.deviceId);
        for (configtool::LogicOperand &operand : point.operands) {
            renameDeviceId(operand.deviceId);
        }
    }

    for (configtool::LogicControlRule &rule : logic.controlRules) {
        renameDeviceId(rule.matchDeviceId);
        for (configtool::LogicControlTarget &target : rule.targets) {
            renameDeviceId(target.deviceId);
            renameRealtimeRefs(target.expr);
        }
    }

    for (configtool::AgcAvcGroup &group : logic.agcAvcGroups) {
        renameDeviceId(group.virtualDeviceId);
        for (configtool::AgcAvcDevice &agcDevice : group.devices) {
            renameDeviceId(agcDevice.deviceId);
            renameDeviceId(agcDevice.onlineDeviceId);
        }
    }

    for (configtool::LogicOnlineStatusLink &link : logic.onlineStatusLinks) {
        renameDeviceId(link.deviceId);
        renameDeviceId(link.linkToDeviceId);
    }

    return updateCount;
}

void MainWindow::onDeviceBindingDataRefFilterTextChanged(const QString & /*text*/)
{
    refreshDeviceEditor(currentConfigDeviceIndex());
}

void MainWindow::onDeviceBindingDescriptionFilterTextChanged(const QString & /*text*/)
{
    refreshDeviceEditor(currentConfigDeviceIndex());
}

void MainWindow::onDeviceBindingItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingDeviceBindingsTable || m_restoringConfigUndo) {
        return;
    }

    const int deviceIndex = currentConfigDeviceIndex();
    if (deviceIndex < 0) {
        return;
    }

    QTableWidgetItem *enabledItem = m_deviceBindingsTable->item(item->row(), 0);
    if (!enabledItem) {
        return;
    }

    const int bindingIndex = enabledItem->data(Qt::UserRole).toInt();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::PointBinding &binding = device.bindings[bindingIndex];
    const bool modbusDevice = isModbusDevice(device);
    const int editedRow = item->row();
    const int editedColumn = item->column();
    bool refreshEditor = true;
    if (item->column() == 0) {
        binding.enabled = item->checkState() == Qt::Checked;
    } else if ((modbusDevice && item->column() == ModbusColumnSelfSignal)
               || (!modbusDevice && item->column() == 5)) {
        binding.selfSignalFlag = item->checkState() == Qt::Checked ? QStringLiteral("1") : QString();
        refreshEditor = false;
    } else {
        applyDeviceBindingCellText(item->row(), item->column(), item->text());
    }
    if (modbusDevice && refreshEditor) {
        rebuildModbusDeviceConfig(device);
    }

    if (refreshEditor) {
        refreshDeviceDetail(deviceIndex);
        refreshDeviceEditor(deviceIndex);
        m_deviceBindingsTable->setCurrentCell(editedRow, editedColumn);
    }
}

void MainWindow::onModelPointItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingModelPointsTable || m_restoringConfigUndo) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    QTableWidgetItem *anchorItem = m_modelPointsTable->item(item->row(), 0);
    if (!anchorItem) {
        return;
    }

    const int serviceIndex = anchorItem->data(Qt::UserRole).toInt();
    const int pointIndex = anchorItem->data(Qt::UserRole + 1).toInt();

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= service.points.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    const int editedRow = item->row();
    const int editedColumn = item->column();
    const QString pointId = service.points.at(pointIndex).pointId;
    if (editedColumn == 0) {
        service.points[pointIndex].northVisible = item->checkState() == Qt::Checked;
    } else {
        applyModelPointCellText(item->row(), item->column(), item->text());
    }

    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    refreshModelDetail(modelIndex);
    selectModelPointById(pointId);
    m_modelPointsTable->setCurrentCell(editedRow, editedColumn);
}

void MainWindow::applyModelPointCellText(int row, int column, const QString &text)
{
    QTableWidgetItem *anchorItem = m_modelPointsTable->item(row, 0);
    if (!anchorItem) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    const int serviceIndex = anchorItem->data(Qt::UserRole).toInt();
    const int pointIndex = anchorItem->data(Qt::UserRole + 1).toInt();

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= service.points.size()) {
        return;
    }

    configtool::PointTemplate &point = service.points[pointIndex];
    const QString oldDataRef = point.dataRef();
    const QString value = text.trimmed();
    switch (column) {
    case 2:
        point.name = value;
        point.doName = point.name;
        break;
    case 3:
        point.description = value;
        break;
    case 4:
        point.ldName = value;
        break;
    case 5:
        point.lnType = value;
        break;
    case 6:
        point.lnInst = value;
        break;
    case 8:
        point.dataType = value;
        break;
    case 9:
        point.unit = value;
        break;
    default:
        break;
    }
    const QString newDataRef = point.dataRef();
    const int renamedReferenceCount = renameModelPointReferences(model.modelId, oldDataRef, newDataRef);
    if (renamedReferenceCount > 0) {
        statusBar()->showMessage(QStringLiteral("已同步更新 %1 处点位引用").arg(renamedReferenceCount), 5000);
    }
}

void MainWindow::applyDeviceBindingCellText(int row, int column, const QString &text)
{
    QTableWidgetItem *enabledItem = m_deviceBindingsTable->item(row, 0);
    if (!enabledItem) {
        return;
    }

    const int deviceIndex = currentConfigDeviceIndex();
    const int bindingIndex = enabledItem->data(Qt::UserRole).toInt();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
        return;
    }

    configtool::PointBinding &binding = device.bindings[bindingIndex];
    const QString value = text.trimmed();
    if (isModbusDevice(device)) {
        switch (column) {
        case ModbusColumnKind:
            binding.extensions.insert(QStringLiteral("modbusKind"), normalizedModbusKind(value));
            break;
        case ModbusColumnDescription:
            binding.descriptionOverride = value;
            break;
        case ModbusColumnFunCode:
            if (value.isEmpty()) {
                binding.extensions.remove(QStringLiteral("modbusFunctionCode"));
            } else {
                binding.extensions.insert(QStringLiteral("modbusFunctionCode"), value.toInt());
            }
            break;
        case ModbusColumnRegister:
            if (value.isEmpty()) {
                binding.extensions.remove(QStringLiteral("modbusRegisterAddress"));
            } else {
                binding.extensions.insert(QStringLiteral("modbusRegisterAddress"), value.toInt());
            }
            break;
        case ModbusColumnDataType:
            binding.extensions.insert(QStringLiteral("modbusDataType"), value);
            break;
        case ModbusColumnScale:
            binding.extensions.insert(QStringLiteral("modbusScale"), value);
            break;
        case ModbusColumnSelfSignal:
            binding.selfSignalFlag = selfSignalFlagChecked(value) ? QStringLiteral("1") : QString();
            break;
        default:
            break;
        }
        rebuildModbusDeviceConfig(device);
        return;
    }

    switch (column) {
    case 2:
        binding.descriptionOverride = value;
        break;
    case 3:
        binding.address = value;
        break;
    case 4:
        binding.initValue = value;
        break;
    case 5:
        binding.selfSignalFlag = selfSignalFlagChecked(value) ? QStringLiteral("1") : QString();
        break;
    default:
        break;
    }
}

void MainWindow::rebuildModbusDeviceConfig(configtool::ProtocolDeviceInstance &device)
{
    if (!isModbusDevice(device)) {
        return;
    }

    device.modbus.pollGroups.clear();
    device.modbus.setPoints.clear();
    if (device.modbus.yxType.trimmed().isEmpty()) {
        device.modbus.yxType = QStringLiteral("BIT");
    }
    if (device.modbus.ycType.trimmed().isEmpty()) {
        device.modbus.ycType = QStringLiteral("WORD");
    }
    if (device.modbus.ytType.trimmed().isEmpty()) {
        device.modbus.ytType = QStringLiteral("WORD");
    }
    if (device.modbus.ycScale.trimmed().isEmpty()) {
        device.modbus.ycScale = QStringLiteral("1.0");
    }
    if (device.modbus.ytScale.trimmed().isEmpty()) {
        device.modbus.ytScale = QStringLiteral("1.0");
    }

    QMap<QString, QList<int>> pollGroupIndexes;
    QMap<QString, QList<int>> setGroupIndexes;

    for (int index = 0; index < device.bindings.size(); ++index) {
        configtool::PointBinding &binding = device.bindings[index];
        if (!binding.enabled || !binding.extensions.contains(QStringLiteral("modbusRegisterAddress"))) {
            continue;
        }

        const QString kind = modbusBindingKind(binding);
        QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"));
        if (dataType.trimmed().isEmpty()) {
            dataType = kind == QStringLiteral("yx")
                ? QStringLiteral("BIT")
                : QStringLiteral("WORD");
            binding.extensions.insert(QStringLiteral("modbusDataType"), dataType);
        }

        const int defaultFunCode = kind == QStringLiteral("yx")
            ? 2
            : (kind == QStringLiteral("yc") ? 3 : 6);
        const int funCode = modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), defaultFunCode);
        binding.extensions.insert(QStringLiteral("modbusFunctionCode"), funCode);

        QString scale = modbusBindingString(binding, QStringLiteral("modbusScale"));
        if (scale.trimmed().isEmpty()) {
            scale = QStringLiteral("1.0");
            binding.extensions.insert(QStringLiteral("modbusScale"), scale);
        }

        const QString groupKey = QStringLiteral("%1|%2|%3|%4")
            .arg(kind)
            .arg(funCode)
            .arg(dataType.trimmed().toUpper())
            .arg(scale.trimmed());
        if (isModbusSetKind(kind)) {
            setGroupIndexes[groupKey].append(index);
        } else {
            pollGroupIndexes[groupKey].append(index);
        }
    }

    int nextGroupNo = 1;
    int yxOrder = 1;
    int ycOrder = 1;
    int ykOrder = 1;
    int ytOrder = 1;

    for (const QString &groupKey : pollGroupIndexes.keys()) {
        QList<int> indexes = pollGroupIndexes.value(groupKey);
        std::sort(indexes.begin(), indexes.end(), [&](int left, int right) {
            return modbusBindingInt(device.bindings.at(left), QStringLiteral("modbusRegisterAddress"))
                < modbusBindingInt(device.bindings.at(right), QStringLiteral("modbusRegisterAddress"));
        });

        if (indexes.isEmpty()) {
            continue;
        }

        QList<int> runIndexes;
        int runStartAddr = 0;
        int runRegisterEnd = 0;

        auto flushRun = [&]() {
            if (runIndexes.isEmpty()) {
                return;
            }

            configtool::PointBinding &firstBinding = device.bindings[runIndexes.first()];
            const QString kind = modbusBindingKind(firstBinding);
            const QString dataType = modbusBindingString(firstBinding, QStringLiteral("modbusDataType"), QStringLiteral("WORD"));
            const QString scale = modbusBindingString(firstBinding, QStringLiteral("modbusScale"), QStringLiteral("1.0"));
            const int funCode = modbusBindingInt(firstBinding, QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : 3);
            const int groupNo = nextGroupNo++;

            int entryNo = 1;
            for (int bindingIndex : runIndexes) {
                configtool::PointBinding &binding = device.bindings[bindingIndex];
                binding.extensions.insert(QStringLiteral("modbusGroupNo"), groupNo);
                binding.extensions.insert(QStringLiteral("modbusEntryNo"), entryNo);
                binding.address = QString::number(buildModbusDataIndex(groupNo, entryNo));
                ++entryNo;
            }

            configtool::ModbusPollGroup group;
            group.kind = modbusKindFromString(kind);
            group.order = kind == QStringLiteral("yx") ? yxOrder++ : ycOrder++;
            group.groupNo = groupNo;
            group.funCode = funCode;
            group.startAddr = runStartAddr;
            group.regNum = qMax(modbusTypeRegisterCount(dataType), runRegisterEnd - runStartAddr);
            group.dataType = dataType;
            group.scale = kind == QStringLiteral("yc") ? scale : QStringLiteral("1.0");
            device.modbus.pollGroups.append(group);

            runIndexes.clear();
            runStartAddr = 0;
            runRegisterEnd = 0;
        };

        for (int bindingIndex : indexes) {
            const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
            const QString kind = modbusBindingKind(binding);
            const QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"), QStringLiteral("WORD"));
            const int funCode = modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : 3);
            const int regStep = qMax(1, modbusTypeRegisterCount(dataType));
            const int regAddr = modbusBindingInt(binding, QStringLiteral("modbusRegisterAddress"));
            const int nextRegisterEnd = regAddr + regStep;
            const bool continuous = runIndexes.isEmpty() || regAddr == runRegisterEnd;
            const bool withinFrameLimit = runIndexes.isEmpty()
                || (nextRegisterEnd - runStartAddr) <= modbusMaxReadQuantity(funCode);

            if (!runIndexes.isEmpty() && (!continuous || !withinFrameLimit)) {
                flushRun();
            }

            if (runIndexes.isEmpty()) {
                runStartAddr = regAddr;
                runRegisterEnd = nextRegisterEnd;
            } else {
                runRegisterEnd = qMax(runRegisterEnd, nextRegisterEnd);
            }
            runIndexes.append(bindingIndex);
        }
        flushRun();
    }

    for (const QString &groupKey : setGroupIndexes.keys()) {
        QList<int> indexes = setGroupIndexes.value(groupKey);
        std::sort(indexes.begin(), indexes.end(), [&](int left, int right) {
            return modbusBindingInt(device.bindings.at(left), QStringLiteral("modbusRegisterAddress"))
                < modbusBindingInt(device.bindings.at(right), QStringLiteral("modbusRegisterAddress"));
        });

        if (indexes.isEmpty()) {
            continue;
        }

        const int groupNo = nextGroupNo++;
        int entryNo = 1;
        for (int bindingIndex : indexes) {
            configtool::PointBinding &binding = device.bindings[bindingIndex];
            const QString kind = modbusBindingKind(binding);
            const QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"), QStringLiteral("WORD"));
            const QString scale = modbusBindingString(binding, QStringLiteral("modbusScale"), QStringLiteral("1.0"));

            configtool::ModbusSetPoint setPoint;
            setPoint.kind = modbusKindFromString(kind);
            setPoint.order = kind == QStringLiteral("yk") ? ykOrder++ : ytOrder++;
            setPoint.groupNo = groupNo;
            setPoint.entryNo = entryNo;
            setPoint.funCode = modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), 6);
            setPoint.regAddr = modbusBindingInt(binding, QStringLiteral("modbusRegisterAddress"));
            setPoint.dataType = kind == QStringLiteral("yt") ? dataType : QStringLiteral("WORD");
            setPoint.scale = kind == QStringLiteral("yt") ? scale : QStringLiteral("1.0");
            device.modbus.setPoints.append(setPoint);

            binding.extensions.insert(QStringLiteral("modbusGroupNo"), groupNo);
            binding.extensions.insert(QStringLiteral("modbusEntryNo"), entryNo);
            binding.address = QString::number(buildModbusDataIndex(groupNo, entryNo));
            ++entryNo;
        }
    }
}

void MainWindow::pasteClipboardIntoModelPointsTable()
{
    const QList<QStringList> clipboardRows = parseClipboardTable(QApplication::clipboard()->text());
    if (clipboardRows.isEmpty()) {
        return;
    }

    QModelIndexList targets = sortedEditableTargetIndexes(m_modelPointsTable);
    const bool useSelectedCells = targets.size() > 1;
    const int startRow = useSelectedCells ? targets.first().row() : m_modelPointsTable->currentRow();
    const int startColumn = useSelectedCells ? targets.first().column() : m_modelPointsTable->currentColumn();
    if (startRow < 0 || startColumn < 0) {
        return;
    }

    pushConfigUndoSnapshot();
    m_updatingModelPointsTable = true;
    if (useSelectedCells && clipboardRows.size() == 1 && clipboardRows.first().size() == 1) {
        const QString value = clipboardRows.first().first();
        for (const QModelIndex &target : targets) {
            if (QTableWidgetItem *item = m_modelPointsTable->item(target.row(), target.column());
                item && (item->flags() & Qt::ItemIsEditable)) {
                item->setText(value);
                applyModelPointCellText(target.row(), target.column(), value);
            }
        }
    } else if (useSelectedCells && clipboardRows.size() * clipboardRows.first().size() == targets.size()) {
        int valueIndex = 0;
        for (const QStringList &clipboardRow : clipboardRows) {
            for (const QString &value : clipboardRow) {
                const QModelIndex target = targets.at(valueIndex++);
                if (QTableWidgetItem *item = m_modelPointsTable->item(target.row(), target.column());
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(value);
                    applyModelPointCellText(target.row(), target.column(), value);
                }
            }
        }
    } else {
        for (int rowOffset = 0; rowOffset < clipboardRows.size(); ++rowOffset) {
            const int row = startRow + rowOffset;
            if (row >= m_modelPointsTable->rowCount()) {
                break;
            }
            for (int columnOffset = 0; columnOffset < clipboardRows.at(rowOffset).size(); ++columnOffset) {
                const int column = startColumn + columnOffset;
                if (column >= m_modelPointsTable->columnCount()) {
                    break;
                }
                if (QTableWidgetItem *item = m_modelPointsTable->item(row, column);
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    const QString value = clipboardRows.at(rowOffset).at(columnOffset);
                    item->setText(value);
                    applyModelPointCellText(row, column, value);
                }
            }
        }
    }
    m_updatingModelPointsTable = false;

    const int modelIndex = currentConfigModelIndex();
    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    refreshModelDetail(modelIndex);
    m_modelPointsTable->setCurrentCell(startRow, startColumn);
}

void MainWindow::pasteClipboardIntoDeviceBindingsTable()
{
    const QList<QStringList> clipboardRows = parseClipboardTable(QApplication::clipboard()->text());
    if (clipboardRows.isEmpty()) {
        return;
    }

    QModelIndexList targets = sortedEditableTargetIndexes(m_deviceBindingsTable);
    const bool useSelectedCells = targets.size() > 1;
    const int startRow = useSelectedCells ? targets.first().row() : m_deviceBindingsTable->currentRow();
    const int startColumn = useSelectedCells ? targets.first().column() : m_deviceBindingsTable->currentColumn();
    if (startRow < 0 || startColumn < 0) {
        return;
    }

    pushConfigUndoSnapshot();
    m_updatingDeviceBindingsTable = true;
    if (useSelectedCells && clipboardRows.size() == 1 && clipboardRows.first().size() == 1) {
        const QString value = clipboardRows.first().first();
        for (const QModelIndex &target : targets) {
            if (QTableWidgetItem *item = m_deviceBindingsTable->item(target.row(), target.column());
                item && (item->flags() & Qt::ItemIsEditable)) {
                item->setText(value);
                applyDeviceBindingCellText(target.row(), target.column(), value);
            }
        }
    } else if (useSelectedCells && clipboardRows.size() * clipboardRows.first().size() == targets.size()) {
        int valueIndex = 0;
        for (const QStringList &clipboardRow : clipboardRows) {
            for (const QString &value : clipboardRow) {
                const QModelIndex target = targets.at(valueIndex++);
                if (QTableWidgetItem *item = m_deviceBindingsTable->item(target.row(), target.column());
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(value);
                    applyDeviceBindingCellText(target.row(), target.column(), value);
                }
            }
        }
    } else {
        for (int rowOffset = 0; rowOffset < clipboardRows.size(); ++rowOffset) {
            const int row = startRow + rowOffset;
            if (row >= m_deviceBindingsTable->rowCount()) {
                break;
            }
            for (int columnOffset = 0; columnOffset < clipboardRows.at(rowOffset).size(); ++columnOffset) {
                const int column = startColumn + columnOffset;
                if (column >= m_deviceBindingsTable->columnCount()) {
                    break;
                }
                if (QTableWidgetItem *item = m_deviceBindingsTable->item(row, column);
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    const QString value = clipboardRows.at(rowOffset).at(columnOffset);
                    item->setText(value);
                    applyDeviceBindingCellText(row, column, value);
                }
            }
        }
    }
    m_updatingDeviceBindingsTable = false;

    const int deviceIndex = currentConfigDeviceIndex();
    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    m_deviceBindingsTable->setCurrentCell(startRow, startColumn);
}

void MainWindow::pushConfigUndoSnapshot()
{
    if (m_restoringConfigUndo) {
        return;
    }

    m_configUndoStack.append(m_configProjectManager.project());
    constexpr int maxUndoSnapshots = 50;
    while (m_configUndoStack.size() > maxUndoSnapshots) {
        m_configUndoStack.removeFirst();
    }
}

void MainWindow::undoLastConfigEdit()
{
    if (m_configUndoStack.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有可撤回的编辑"), 2000);
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    const int deviceIndex = currentConfigDeviceIndex();
    const QWidget *currentPage = m_mainTabWidget ? m_mainTabWidget->currentWidget() : nullptr;

    m_restoringConfigUndo = true;
    m_configProjectManager.project() = m_configUndoStack.takeLast();
    m_restoringConfigUndo = false;

    refreshConfigObjectViews();
    if (modelIndex >= 0 && modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    if (deviceIndex >= 0 && deviceIndex < m_configDeviceTable->rowCount()) {
        m_configDeviceTable->selectRow(deviceIndex);
    }

    if (currentPage == m_modelEditorPage) {
        refreshModelDetail(currentConfigModelIndex());
        m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    } else if (currentPage == m_deviceEditorPage) {
        refreshDeviceEditor(currentConfigDeviceIndex());
        m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    }
    statusBar()->showMessage(QStringLiteral("已撤回上一步编辑"), 2000);
}

void MainWindow::refreshConfigImportSummary(const configtool::ImportReport &report)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_configProjectNameValueLabel->setText(project.projectName.isEmpty() ? QStringLiteral("-") : project.projectName);
    m_configSourceRootValueLabel->setText(project.sourceRoot.isEmpty() ? QStringLiteral("-") : project.sourceRoot);
    m_configModelCountValueLabel->setText(QString::number(project.models.size()));
    m_configDeviceCountValueLabel->setText(QString::number(project.devices.size()));
    m_configIssueCountValueLabel->setText(QString::number(importIssueProblemCount(report.issues)));

    QString statusMessage = QStringLiteral("导入结果: 模型 %1，设备 %2")
        .arg(report.importedModelCount)
        .arg(report.importedDeviceCount);
    if (report.issues.isEmpty()) {
        statusMessage += QStringLiteral("，未发现错误或警告。");
    } else {
        statusMessage += QStringLiteral("，问题数 %1。")
            .arg(importIssueProblemCount(report.issues));
    }
    statusBar()->showMessage(statusMessage, 8000);
    refreshConfigIssueTable(report.issues, QStringLiteral("导入"));
    refreshConfigObjectViews();
}

void MainWindow::refreshConfigObjectViews()
{
    const configtool::ConfigProject &project = m_configProjectManager.project();

    const int previousModelIndex = currentConfigModelIndex();
    const int previousDeviceIndex = currentConfigDeviceIndex();

    m_configModelTable->setRowCount(project.models.size());
    for (int row = 0; row < project.models.size(); ++row) {
        const configtool::ModelTemplate &model = project.models.at(row);
        int pointCount = 0;
        for (const configtool::ServiceTemplate &service : model.services) {
            pointCount += service.points.size();
        }

        m_configModelTable->setItem(row, 0, new QTableWidgetItem(model.modelId));
        m_configModelTable->setItem(row, 1, new QTableWidgetItem(model.displayName));
        m_configModelTable->setItem(row, 2, new QTableWidgetItem(model.deviceType));
        m_configModelTable->setItem(row, 3, new QTableWidgetItem(QString::number(pointCount)));
    }

    m_configDeviceTable->setRowCount(project.devices.size());
    for (int row = 0; row < project.devices.size(); ++row) {
        const configtool::ProtocolDeviceInstance &device = project.devices.at(row);
        m_configDeviceTable->setItem(row, 0, new QTableWidgetItem(device.deviceId));
        m_configDeviceTable->setItem(row, 1, new QTableWidgetItem(device.deviceDesc));
        m_configDeviceTable->setItem(row, 2, new QTableWidgetItem(device.modelId));
        m_configDeviceTable->setItem(row, 3, new QTableWidgetItem(device.transport.stationAddress));
        m_configDeviceTable->setItem(row, 4, new QTableWidgetItem(QString::number(device.bindings.size())));
    }

    if (previousModelIndex >= 0 && previousModelIndex < project.models.size()) {
        m_configModelTable->selectRow(previousModelIndex);
    } else if (!project.models.isEmpty()) {
        m_configModelTable->selectRow(0);
    } else {
        refreshModelOverview(-1);
        refreshModelDetail(-1);
    }

    if (previousDeviceIndex >= 0 && previousDeviceIndex < project.devices.size()) {
        m_configDeviceTable->selectRow(previousDeviceIndex);
    } else if (!project.devices.isEmpty()) {
        m_configDeviceTable->selectRow(0);
    } else {
        refreshDeviceDetail(-1);
        refreshDeviceEditor(-1);
    }

    refreshSelectionOverview();
    refreshLogicCenterOverview();
    refreshLogicAgcAvcPage();
    refreshLogicComputationPointPage();
    refreshLogicControlRulePage();
    refreshLogicOnlineLinkPage();
}

int formulaOperandCount(const QString &formula)
{
    static const QRegularExpression placeholderPattern(QStringLiteral("\\{(\\d+)\\}"));
    int count = 0;
    QRegularExpressionMatchIterator it = placeholderPattern.globalMatch(formula);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        count = qMax(count, match.captured(1).toInt());
    }
    return count;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto dropPosition = [](QDropEvent *dropEvent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return dropEvent->position().toPoint();
#else
        return dropEvent->pos();
#endif
    };
    auto insertRowAt = [](QTableWidget *table, const QPoint &dropPos) {
        const int rowCount = table->rowCount();
        int insertRow = table->rowAt(dropPos.y());
        if (insertRow < 0) {
            return rowCount;
        }

        const int rowTop = table->rowViewportPosition(insertRow);
        const int rowHeight = table->rowHeight(insertRow);
        if (dropPos.y() > rowTop + rowHeight / 2) {
            ++insertRow;
        }
        return insertRow;
    };
    auto showDropLine = [](QTableWidget *table, QFrame *line, int insertRow) {
        if (!table || !line) {
            return;
        }

        const int rowCount = table->rowCount();
        int y = 0;
        if (insertRow <= 0) {
            y = 0;
        } else if (insertRow >= rowCount) {
            const int lastRow = rowCount - 1;
            y = table->rowViewportPosition(lastRow) + table->rowHeight(lastRow);
        } else {
            y = table->rowViewportPosition(insertRow);
        }

        y = qMax(0, y - line->height() / 2);
        line->setGeometry(0, y, table->viewport()->width(), line->height());
        line->raise();
        line->show();
    };
    auto hideDropLine = [](QFrame *line) {
        if (line) {
            line->hide();
        }
    };

    if (m_logicComputationPointTable
        && watched == m_logicComputationPointTable->viewport()) {

        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_logicComputationDragRow = m_logicComputationPointTable->rowAt(mouseEvent->pos().y());
            }
        } else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDropEvent *>(event);
            if (m_logicComputationDragRow >= 0) {
                showDropLine(m_logicComputationPointTable,
                             m_logicComputationDropLine,
                             insertRowAt(m_logicComputationPointTable, dropPosition(dragEvent)));
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            hideDropLine(m_logicComputationDropLine);
            m_logicComputationDragRow = -1;
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
            const int rowCount = logic.computationPoints.size();
            const int sourceRow = m_logicComputationDragRow;
            m_logicComputationDragRow = -1;
            hideDropLine(m_logicComputationDropLine);

            if (sourceRow < 0 || sourceRow >= rowCount || rowCount < 2) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int insertRow = insertRowAt(m_logicComputationPointTable, dropPosition(dropEvent));
            const int destinationRow = insertRow > sourceRow ? insertRow - 1 : insertRow;
            if (destinationRow < 0 || destinationRow >= rowCount || destinationRow == sourceRow) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            pushConfigUndoSnapshot();
            logic.computationPoints.move(sourceRow, destinationRow);
            refreshLogicCenterOverview();
            refreshLogicComputationPointPage();
            m_logicComputationPointTable->selectRow(destinationRow);
            statusBar()->showMessage(QStringLiteral("已调整计算点顺序"), 5000);

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            return true;
        }
    }

    if (m_modelPointsTable
        && watched == m_modelPointsTable->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_modelPointDragRow = m_modelPointsTable->rowAt(mouseEvent->pos().y());
            }
        } else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDropEvent *>(event);
            if (m_modelPointDragRow >= 0) {
                if (m_modelPointFilterTabBar && m_modelPointFilterTabBar->currentIndex() > 0) {
                    showDropLine(m_modelPointsTable,
                                 m_modelPointDropLine,
                                 insertRowAt(m_modelPointsTable, dropPosition(dragEvent)));
                } else {
                    hideDropLine(m_modelPointDropLine);
                }
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            hideDropLine(m_modelPointDropLine);
            m_modelPointDragRow = -1;
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            hideDropLine(m_modelPointDropLine);

            const int sourceRow = m_modelPointDragRow;
            m_modelPointDragRow = -1;
            const int modelIndex = currentConfigModelIndex();
            configtool::ConfigProject &project = m_configProjectManager.project();
            const bool singleCategoryView = m_modelPointFilterTabBar
                && m_modelPointFilterTabBar->currentIndex() > 0;
            if (!singleCategoryView
                || modelIndex < 0
                || modelIndex >= project.models.size()
                || sourceRow < 0
                || sourceRow >= m_modelPointsTable->rowCount()) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            QTableWidgetItem *sourceItem = m_modelPointsTable->item(sourceRow, 0);
            if (!sourceItem) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int serviceIndex = sourceItem->data(Qt::UserRole).toInt();
            const int sourcePointIndex = sourceItem->data(Qt::UserRole + 1).toInt();
            configtool::ModelTemplate &model = project.models[modelIndex];
            if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            configtool::ServiceTemplate &service = model.services[serviceIndex];
            if (sourcePointIndex < 0 || sourcePointIndex >= service.points.size() || service.points.size() < 2) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int insertRow = qBound(0,
                                         insertRowAt(m_modelPointsTable, dropPosition(dropEvent)),
                                         service.points.size());
            const int destinationPointIndex = insertRow > sourcePointIndex ? insertRow - 1 : insertRow;
            if (destinationPointIndex < 0
                || destinationPointIndex >= service.points.size()
                || destinationPointIndex == sourcePointIndex) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            pushConfigUndoSnapshot();
            const QString movedPointId = service.points.at(sourcePointIndex).pointId;
            service.points.move(sourcePointIndex, destinationPointIndex);
            refreshConfigObjectViews();
            refreshModelDetail(modelIndex);
            selectModelPointById(movedPointId);
            statusBar()->showMessage(QStringLiteral("已调整模型点位顺序"), 5000);

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::refreshSelectionOverview()
{
    const int modelIndex = currentConfigModelIndex();
    const int deviceIndex = currentConfigDeviceIndex();
    const configtool::ConfigProject &project = m_configProjectManager.project();

    QString modelGroupTitle = QStringLiteral("模型列表");
    QString deviceGroupTitle = QStringLiteral("设备列表");

    if (modelIndex >= 0 && modelIndex < project.models.size()) {
        const configtool::ModelTemplate &model = project.models.at(modelIndex);
        const QString displayName = model.displayName.isEmpty() ? model.modelId : model.displayName;
        modelGroupTitle = QStringLiteral("模型列表  [当前: %1]").arg(displayName);
    }

    if (deviceIndex >= 0 && deviceIndex < project.devices.size()) {
        const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
        const QString deviceName = device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc;
        deviceGroupTitle = QStringLiteral("设备列表  [当前: %1]").arg(deviceName);
    }

    m_modelGroupBox->setTitle(modelGroupTitle);
    m_deviceGroupBox->setTitle(deviceGroupTitle);
}

void MainWindow::refreshModelDetail(int modelIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        if (m_modelIdEdit && !m_modelIdEdit->hasFocus()) {
            m_modelIdEdit->setProperty(ModelEditorOriginalModelIdProperty, QString());
        }
        for (QLineEdit *edit : {m_modelIdEdit, m_modelDisplayNameEdit, m_modelDeviceTypeEdit,
                                m_modelVersionEdit, m_modelManufacturerIdEdit,
                                m_modelManufacturerDescEdit, m_modelSchemaEdit}) {
            edit->clear();
        }
        if (m_modelNorthVisibleCheck) {
            QSignalBlocker blocker(m_modelNorthVisibleCheck);
            m_modelNorthVisibleCheck->setChecked(true);
        }
        m_modelValidationLabel->setText(QStringLiteral("请选择一个模型。"));
        m_modelPointsTable->setRowCount(0);
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
    if (m_modelIdEdit && !m_modelIdEdit->hasFocus()) {
        m_modelIdEdit->setProperty(ModelEditorOriginalModelIdProperty, model.modelId);
    }
    for (auto pair : {qMakePair(m_modelIdEdit, model.modelId),
                      qMakePair(m_modelDisplayNameEdit, model.displayName),
                      qMakePair(m_modelDeviceTypeEdit, model.deviceType),
                      qMakePair(m_modelVersionEdit, model.version),
                      qMakePair(m_modelManufacturerIdEdit, model.manufacturerId),
                      qMakePair(m_modelManufacturerDescEdit, model.manufacturerDesc),
                      qMakePair(m_modelSchemaEdit, model.schema)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }
    if (m_modelNorthVisibleCheck) {
        QSignalBlocker blocker(m_modelNorthVisibleCheck);
        m_modelNorthVisibleCheck->setChecked(model.northVisible);
    }

    const int filterTabIndex = m_modelPointFilterTabBar ? m_modelPointFilterTabBar->currentIndex() : 0;
    const QString dataRefKeyword = m_modelPointDataRefFilterEdit
        ? m_modelPointDataRefFilterEdit->text().trimmed()
        : QString();
    const QString descriptionKeyword = m_modelPointDescriptionFilterEdit
        ? m_modelPointDescriptionFilterEdit->text().trimmed()
        : QString();
    int totalPointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        if (filterTabIndex != 0 && modelPointFilterTabIndex(service.type) != filterTabIndex) {
            continue;
        }

        for (const configtool::PointTemplate &point : service.points) {
            const bool matchesDataRef = dataRefKeyword.isEmpty()
                || point.dataRef().contains(dataRefKeyword, Qt::CaseInsensitive);
            const bool matchesDescription = descriptionKeyword.isEmpty()
                || point.description.contains(descriptionKeyword, Qt::CaseInsensitive);
            if (matchesDataRef && matchesDescription) {
                ++totalPointCount;
            }
        }
    }

    m_modelPointsTable->setRowCount(totalPointCount);
    m_updatingModelPointsTable = true;
    m_updatingModelPointCategory = true;
    int row = 0;
    for (int serviceIndex = 0; serviceIndex < model.services.size(); ++serviceIndex) {
        const configtool::ServiceTemplate &service = model.services.at(serviceIndex);
        if (filterTabIndex != 0 && modelPointFilterTabIndex(service.type) != filterTabIndex) {
            continue;
        }
        for (int pointIndex = 0; pointIndex < service.points.size(); ++pointIndex) {
            const configtool::PointTemplate &point = service.points.at(pointIndex);
            const bool matchesDataRef = dataRefKeyword.isEmpty()
                || point.dataRef().contains(dataRefKeyword, Qt::CaseInsensitive);
            const bool matchesDescription = descriptionKeyword.isEmpty()
                || point.description.contains(descriptionKeyword, Qt::CaseInsensitive);
            if (!matchesDataRef || !matchesDescription) {
                continue;
            }

            auto *northVisibleItem = new QTableWidgetItem();
            auto *categoryItem = new QTableWidgetItem(configtool::modelServiceTypeDisplayName(point.category));
            auto *nameItem = new QTableWidgetItem(point.doName);
            auto *descriptionItem = new QTableWidgetItem(point.description);
            auto *ldNameItem = new QTableWidgetItem(point.ldName);
            auto *lnTypeItem = new QTableWidgetItem(point.lnType);
            auto *lnInstItem = new QTableWidgetItem(point.lnInst);
            auto *dataRefItem = new QTableWidgetItem(point.dataRef());
            auto *dataTypeItem = new QTableWidgetItem(point.dataType);
            auto *unitItem = new QTableWidgetItem(point.unit);

            northVisibleItem->setFlags((northVisibleItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            northVisibleItem->setCheckState(point.northVisible ? Qt::Checked : Qt::Unchecked);
            northVisibleItem->setData(Qt::UserRole, serviceIndex);
            northVisibleItem->setData(Qt::UserRole + 1, pointIndex);
            northVisibleItem->setData(Qt::UserRole + 2, point.pointId);
            categoryItem->setData(Qt::UserRole, serviceIndex);
            categoryItem->setData(Qt::UserRole + 1, pointIndex);
            categoryItem->setData(Qt::UserRole + 2, point.pointId);
            categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsEditable);
            dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);

            if (duplicateRefs.contains(point.dataRef())) {
                const QColor duplicateColor(QStringLiteral("#c0392b"));
                northVisibleItem->setForeground(duplicateColor);
                categoryItem->setForeground(duplicateColor);
                nameItem->setForeground(duplicateColor);
                descriptionItem->setForeground(duplicateColor);
                ldNameItem->setForeground(duplicateColor);
                lnTypeItem->setForeground(duplicateColor);
                lnInstItem->setForeground(duplicateColor);
                dataRefItem->setForeground(duplicateColor);
                dataTypeItem->setForeground(duplicateColor);
                unitItem->setForeground(duplicateColor);
            }

            m_modelPointsTable->setItem(row, 0, northVisibleItem);
            m_modelPointsTable->setItem(row, 1, categoryItem);
            m_modelPointsTable->setItem(row, 2, nameItem);
            m_modelPointsTable->setItem(row, 3, descriptionItem);
            m_modelPointsTable->setItem(row, 4, ldNameItem);
            m_modelPointsTable->setItem(row, 5, lnTypeItem);
            m_modelPointsTable->setItem(row, 6, lnInstItem);
            m_modelPointsTable->setItem(row, 7, dataRefItem);
            m_modelPointsTable->setItem(row, 8, dataTypeItem);
            m_modelPointsTable->setItem(row, 9, unitItem);

            auto *categoryCombo = new QComboBox(m_modelPointsTable);
            categoryCombo->addItem(QStringLiteral("遥测"), static_cast<int>(configtool::ModelServiceType::Measurement));
            categoryCombo->addItem(QStringLiteral("遥信"), static_cast<int>(configtool::ModelServiceType::Status));
            categoryCombo->addItem(QStringLiteral("控制"), static_cast<int>(configtool::ModelServiceType::Control));
            categoryCombo->setProperty("serviceIndex", serviceIndex);
            categoryCombo->setProperty("pointIndex", pointIndex);
            const int categoryComboIndex = categoryCombo->findData(static_cast<int>(point.category));
            categoryCombo->setCurrentIndex(categoryComboIndex >= 0 ? categoryComboIndex : 0);
            connect(categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                    this, &MainWindow::onModelPointCategoryChanged);
            m_modelPointsTable->setCellWidget(row, 1, categoryCombo);
            ++row;
        }
    }
    m_updatingModelPointsTable = false;
    m_updatingModelPointCategory = false;

    int hiddenNorthPointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            if (!point.northVisible) {
                ++hiddenNorthPointCount;
            }
        }
    }

    if (duplicateRefs.isEmpty()) {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        QString message = QStringLiteral("当前模型点位的 DataRef 唯一。");
        if (!model.northVisible) {
            message += QStringLiteral(" 当前模型北向不可见，导出时不会生成 northmodel。");
        } else if (hiddenNorthPointCount > 0) {
            message += QStringLiteral(" 已隐藏 %1 个北向点位。").arg(hiddenNorthPointCount);
        }
        m_modelValidationLabel->setText(message);
    } else {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
        m_modelValidationLabel->setText(
            QStringLiteral("检测到重复 DataRef：%1。请调整 LDname/LNtype/LNinst/DOname 组合。")
                .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
    }
}

void MainWindow::selectModelPointById(const QString &pointId)
{
    if (pointId.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_modelPointsTable->rowCount(); ++row) {
        QTableWidgetItem *categoryItem = m_modelPointsTable->item(row, 0);
        if (categoryItem && categoryItem->data(Qt::UserRole + 2).toString() == pointId) {
            m_modelPointsTable->selectRow(row);
            return;
        }
    }
}

void MainWindow::refreshModelOverview(int modelIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        m_modelOverviewIdLabel->setText(QStringLiteral("-"));
        m_modelOverviewDisplayNameLabel->setText(QStringLiteral("-"));
        m_modelOverviewDeviceTypeLabel->setText(QStringLiteral("-"));
        m_modelOverviewVersionLabel->setText(QStringLiteral("-"));
        m_modelOverviewPointCountLabel->setText(QStringLiteral("0"));
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    int pointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        pointCount += service.points.size();
    }

    m_modelOverviewIdLabel->setText(model.modelId.isEmpty() ? QStringLiteral("-") : model.modelId);
    m_modelOverviewDisplayNameLabel->setText(model.displayName.isEmpty() ? QStringLiteral("-") : model.displayName);
    m_modelOverviewDeviceTypeLabel->setText(model.deviceType.isEmpty() ? QStringLiteral("-") : model.deviceType);
    m_modelOverviewVersionLabel->setText(model.version.isEmpty() ? QStringLiteral("-") : model.version);
    m_modelOverviewPointCountLabel->setText(QString::number(pointCount));
}

void MainWindow::refreshDeviceDetail(int deviceIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        m_deviceDetailTitleLabel->setText(QStringLiteral("-"));
        m_deviceDetailModelLabel->setText(QStringLiteral("-"));
        m_deviceDetailAddressLabel->setText(QStringLiteral("-"));
        m_deviceDetailIpLabel->setText(QStringLiteral("-"));
        m_deviceDetailPortLabel->setText(QStringLiteral("-"));
        m_deviceDetailBindingCountLabel->setText(QStringLiteral("0"));
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    m_deviceDetailTitleLabel->setText(device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc);
    m_deviceDetailModelLabel->setText(device.modelId);
    m_deviceDetailAddressLabel->setText(device.transport.stationAddress);
    m_deviceDetailIpLabel->setText(device.transport.ip);
    m_deviceDetailPortLabel->setText(device.transport.port);
    m_deviceDetailBindingCountLabel->setText(QString::number(device.bindings.size()));
}

void MainWindow::refreshDeviceEditor(int deviceIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        if (m_deviceIdEdit && !m_deviceIdEdit->hasFocus()) {
            m_deviceIdEdit->setProperty(DeviceEditorOriginalDeviceIdProperty, QString());
        }
        for (QLineEdit *edit : {m_deviceIdEdit, m_deviceDescEdit, m_deviceModelEdit,
                                m_deviceStationAddressEdit, m_deviceIpEdit,
                                m_devicePortEdit}) {
            if (edit) {
                edit->clear();
            }
        }
        if (m_modbusParamsGroupBox) {
            m_modbusParamsGroupBox->setVisible(false);
        }
        if (m_modbusSerialPortCombo) {
            QSignalBlocker blocker(m_modbusSerialPortCombo);
            m_modbusSerialPortCombo->setCurrentIndex(0);
        }
        if (m_modbusHwVariantCombo) {
            QSignalBlocker blocker(m_modbusHwVariantCombo);
            m_modbusHwVariantCombo->setCurrentIndex(0);
        }
        for (auto pair : {qMakePair(m_modbusBaudCombo, QStringLiteral("9600")),
                          qMakePair(m_modbusDataBitsCombo, QStringLiteral("8")),
                          qMakePair(m_modbusStopBitsCombo, QStringLiteral("1")),
                          qMakePair(m_modbusParityCombo, QStringLiteral("N"))}) {
            QSignalBlocker blocker(pair.first);
            const int index = pair.first->findText(pair.second);
            pair.first->setCurrentIndex(index >= 0 ? index : 0);
        }
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #666666; }");
        m_deviceValidationLabel->setText(QStringLiteral("请选择一个 104 设备。"));
        m_deviceBindingsTable->setRowCount(0);
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    if (m_deviceIdEdit && !m_deviceIdEdit->hasFocus()) {
        m_deviceIdEdit->setProperty(DeviceEditorOriginalDeviceIdProperty, device.deviceId);
    }
    for (auto pair : {qMakePair(m_deviceIdEdit, device.deviceId),
                      qMakePair(m_deviceDescEdit, device.deviceDesc),
                      qMakePair(m_deviceModelEdit, device.modelId),
                      qMakePair(m_deviceStationAddressEdit, device.transport.stationAddress),
                      qMakePair(m_deviceIpEdit, device.transport.ip),
                      qMakePair(m_devicePortEdit, device.transport.port)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }

    const bool modbusDevice = isModbusDevice(device);
    if (m_modbusParamsGroupBox) {
        m_modbusParamsGroupBox->setVisible(modbusDevice);
    }
    if (modbusDevice) {
        const QString type = device.transport.protocolOptions.value(QStringLiteral("type")).toString(QStringLiteral("TCP")).toUpper();
        {
            QSignalBlocker blocker(m_modbusTypeCombo);
            const int typeIndex = m_modbusTypeCombo->findText(type);
            m_modbusTypeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
        }
        const QJsonObject rtu = device.transport.serial;
        {
            QSignalBlocker blocker(m_modbusSerialPortCombo);
            const QString serialPort = uiJsonValueToString(rtu.value(QStringLiteral("serialPort")));
            const int serialIndex = m_modbusSerialPortCombo->findText(serialPort);
            m_modbusSerialPortCombo->setCurrentIndex(serialIndex >= 0 ? serialIndex : 0);
        }
        {
            QSignalBlocker blocker(m_modbusHwVariantCombo);
            const QString hwVariant = project.modbus.hwVariant.trimmed();
            const int hwIndex = m_modbusHwVariantCombo->findData(hwVariant);
            m_modbusHwVariantCombo->setCurrentIndex(hwIndex >= 0 ? hwIndex : 0);
        }
        for (auto pair : {qMakePair(m_modbusBaudCombo, uiJsonValueToString(rtu.value(QStringLiteral("baud")))),
                          qMakePair(m_modbusDataBitsCombo, uiJsonValueToString(rtu.value(QStringLiteral("dataBits")))),
                          qMakePair(m_modbusStopBitsCombo, uiJsonValueToString(rtu.value(QStringLiteral("stopBits")))),
                          qMakePair(m_modbusParityCombo, uiJsonValueToString(rtu.value(QStringLiteral("parity"))))}) {
            QSignalBlocker blocker(pair.first);
            const int index = pair.first->findText(pair.second);
            pair.first->setCurrentIndex(index >= 0 ? index : 0);
        }
        QSignalBlocker debugBlocker(m_modbusDebugCheck);
        const QString debug = device.transport.protocolOptions.value(QStringLiteral("debug")).toString().toLower();
        m_modbusDebugCheck->setChecked(debug == QStringLiteral("on")
            || debug == QStringLiteral("1")
            || debug == QStringLiteral("true")
            || debug == QStringLiteral("yes"));
    }

    const QSet<QString> duplicateAddresses = duplicateBindingAddresses(device);
    int emptyAddressCount = 0;
    for (const configtool::PointBinding &binding : device.bindings) {
        if (binding.enabled && binding.address.trimmed().isEmpty()) {
            ++emptyAddressCount;
        }
    }

    const QString dataRefKeyword = m_deviceBindingDataRefFilterEdit
        ? m_deviceBindingDataRefFilterEdit->text().trimmed()
        : QString();
    const QString descriptionKeyword = m_deviceBindingDescriptionFilterEdit
        ? m_deviceBindingDescriptionFilterEdit->text().trimmed()
        : QString();
    QList<int> visibleBindingIndexes;
    for (int bindingIndex = 0; bindingIndex < device.bindings.size(); ++bindingIndex) {
        const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
        const bool matchesDataRef = dataRefKeyword.isEmpty()
            || binding.dataRef.contains(dataRefKeyword, Qt::CaseInsensitive);
        const bool matchesDescription = descriptionKeyword.isEmpty()
            || binding.descriptionOverride.contains(descriptionKeyword, Qt::CaseInsensitive);
        if (matchesDataRef && matchesDescription) {
            visibleBindingIndexes.append(bindingIndex);
        }
    }

    if (modbusDevice) {
        int missingRegisterCount = 0;
        m_updatingDeviceBindingsTable = true;
        m_deviceBindingsTable->clear();
        m_deviceBindingsTable->setColumnCount(ModbusBindingColumnCount);
        m_deviceBindingsTable->setHorizontalHeaderLabels({
            QStringLiteral("启用"),
            QStringLiteral("点位类型"),
            QStringLiteral("DataRef"),
            QStringLiteral("描述"),
            QStringLiteral("功能码"),
            QStringLiteral("寄存器"),
            QStringLiteral("数据类型"),
            QStringLiteral("比例"),
            QStringLiteral("组号"),
            QStringLiteral("序号"),
            QStringLiteral("dataIndex"),
            QStringLiteral("自发标志")
        });
        m_deviceBindingsTable->setRowCount(visibleBindingIndexes.size());
        m_deviceBindingsTable->setColumnWidth(ModbusColumnEnabled, 56);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnKind, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataRef, 260);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDescription, 180);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnFunCode, 64);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnRegister, 80);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataType, 90);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnScale, 64);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnGroupNo, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnEntryNo, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataIndex, 90);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnSelfSignal, 80);

        for (int row = 0; row < visibleBindingIndexes.size(); ++row) {
            const int bindingIndex = visibleBindingIndexes.at(row);
            const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
            auto *enabledItem = new QTableWidgetItem();
            enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
            enabledItem->setData(Qt::UserRole, bindingIndex);

            const QString kind = modbusBindingKind(binding);
            const QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"), kind == QStringLiteral("yx") ? QStringLiteral("BIT") : QStringLiteral("WORD"));
            const QString scale = modbusBindingString(binding, QStringLiteral("modbusScale"), QStringLiteral("1.0"));
            const QString funCode = QString::number(modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : (kind == QStringLiteral("yc") ? 3 : 6)));
            const bool hasRegister = binding.extensions.contains(QStringLiteral("modbusRegisterAddress"));
            const QString registerAddress = hasRegister
                ? QString::number(modbusBindingInt(binding, QStringLiteral("modbusRegisterAddress")))
                : QString();
            const QString groupNo = binding.extensions.contains(QStringLiteral("modbusGroupNo"))
                ? QString::number(modbusBindingInt(binding, QStringLiteral("modbusGroupNo")))
                : QString();
            const QString entryNo = binding.extensions.contains(QStringLiteral("modbusEntryNo"))
                ? QString::number(modbusBindingInt(binding, QStringLiteral("modbusEntryNo")))
                : QString();

            auto *kindItem = new QTableWidgetItem(modbusKindDisplayName(kind));
            kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
            kindItem->setData(Qt::UserRole, bindingIndex);
            auto *dataRefItem = new QTableWidgetItem(binding.dataRef);
            auto *descriptionItem = new QTableWidgetItem(binding.descriptionOverride);
            auto *funCodeItem = new QTableWidgetItem(funCode);
            auto *registerItem = new QTableWidgetItem(registerAddress);
            auto *dataTypeItem = new QTableWidgetItem(dataType);
            auto *scaleItem = new QTableWidgetItem(scale);
            auto *groupItem = new QTableWidgetItem(groupNo);
            auto *entryItem = new QTableWidgetItem(entryNo);
            auto *dataIndexItem = new QTableWidgetItem(binding.address);
            auto *selfSignalItem = new QTableWidgetItem();
            selfSignalItem->setFlags((selfSignalItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            selfSignalItem->setCheckState(selfSignalFlagChecked(binding.selfSignalFlag) ? Qt::Checked : Qt::Unchecked);

            dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsEditable);
            entryItem->setFlags(entryItem->flags() & ~Qt::ItemIsEditable);
            dataIndexItem->setFlags(dataIndexItem->flags() & ~Qt::ItemIsEditable);

            if (binding.enabled && !hasRegister) {
                ++missingRegisterCount;
                const QColor warningColor(QStringLiteral("#b9770e"));
                dataRefItem->setForeground(warningColor);
                registerItem->setForeground(warningColor);
            }

            m_deviceBindingsTable->setItem(row, ModbusColumnEnabled, enabledItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnKind, kindItem);
            auto *kindCombo = new QComboBox(m_deviceBindingsTable);
            kindCombo->addItem(QStringLiteral("遥信"), QStringLiteral("yx"));
            kindCombo->addItem(QStringLiteral("遥测"), QStringLiteral("yc"));
            kindCombo->addItem(QStringLiteral("遥控"), QStringLiteral("yk"));
            kindCombo->addItem(QStringLiteral("遥调"), QStringLiteral("yt"));
            const int kindIndex = kindCombo->findData(kind);
            kindCombo->setCurrentIndex(kindIndex >= 0 ? kindIndex : kindCombo->findData(QStringLiteral("yc")));
            kindCombo->setProperty("bindingIndex", bindingIndex);
            connect(kindCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, kindCombo](int) {
                if (m_updatingDeviceBindingsTable || m_restoringConfigUndo) {
                    return;
                }

                const int deviceIndex = currentConfigDeviceIndex();
                if (deviceIndex < 0) {
                    return;
                }

                configtool::ConfigProject &project = m_configProjectManager.project();
                if (deviceIndex >= project.devices.size()) {
                    return;
                }

                configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
                const int bindingIndex = kindCombo->property("bindingIndex").toInt();
                if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
                    return;
                }

                pushConfigUndoSnapshot();
                device.bindings[bindingIndex].extensions.insert(QStringLiteral("modbusKind"), kindCombo->currentData().toString());
                rebuildModbusDeviceConfig(device);
                refreshDeviceDetail(deviceIndex);
                refreshDeviceEditor(deviceIndex);
                for (int row = 0; row < m_deviceBindingsTable->rowCount(); ++row) {
                    const QTableWidgetItem *item = m_deviceBindingsTable->item(row, ModbusColumnEnabled);
                    if (item && item->data(Qt::UserRole).toInt() == bindingIndex) {
                        m_deviceBindingsTable->setCurrentCell(row, ModbusColumnKind);
                        break;
                    }
                }
            });
            m_deviceBindingsTable->setCellWidget(row, ModbusColumnKind, kindCombo);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataRef, dataRefItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDescription, descriptionItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnFunCode, funCodeItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnRegister, registerItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataType, dataTypeItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnScale, scaleItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnGroupNo, groupItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnEntryNo, entryItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataIndex, dataIndexItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnSelfSignal, selfSignalItem);
        }
        m_updatingDeviceBindingsTable = false;

        if (missingRegisterCount > 0) {
            m_deviceValidationLabel->setStyleSheet("QLabel { color: #b9770e; }");
            m_deviceValidationLabel->setText(QStringLiteral("当前有 %1 个启用点位未填写 Modbus 寄存器地址。填写后会自动生成分组、组内序号和 dataIndex。")
                .arg(missingRegisterCount));
        } else {
            m_deviceValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
            m_deviceValidationLabel->setText(QStringLiteral("Modbus 映射已生成：轮询组 %1 个，写入项 %2 个。")
                .arg(device.modbus.pollGroups.size())
                .arg(device.modbus.setPoints.size()));
        }
        return;
    }

    m_updatingDeviceBindingsTable = true;
    m_deviceBindingsTable->clear();
    m_deviceBindingsTable->setColumnCount(Iec104BindingColumnCount);
    m_deviceBindingsTable->setHorizontalHeaderLabels({
        QStringLiteral("启用"),
        QStringLiteral("DataRef"),
        QStringLiteral("描述"),
        QStringLiteral("地址"),
        QStringLiteral("初值"),
        QStringLiteral("自发标志")
    });
    m_deviceBindingsTable->setRowCount(visibleBindingIndexes.size());
    for (int row = 0; row < visibleBindingIndexes.size(); ++row) {
        const int bindingIndex = visibleBindingIndexes.at(row);
        const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
        auto *enabledItem = new QTableWidgetItem();
        enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, bindingIndex);
        auto *dataRefItem = new QTableWidgetItem(binding.dataRef);
        auto *descriptionItem = new QTableWidgetItem(binding.descriptionOverride);
        auto *addressItem = new QTableWidgetItem(binding.address);
        auto *initValueItem = new QTableWidgetItem(binding.initValue);
        auto *selfSignalItem = new QTableWidgetItem();
        selfSignalItem->setFlags((selfSignalItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        selfSignalItem->setCheckState(selfSignalFlagChecked(binding.selfSignalFlag) ? Qt::Checked : Qt::Unchecked);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);

        if (binding.enabled && duplicateAddresses.contains(binding.address.trimmed())) {
            const QColor duplicateColor(QStringLiteral("#c0392b"));
            enabledItem->setForeground(duplicateColor);
            dataRefItem->setForeground(duplicateColor);
            descriptionItem->setForeground(duplicateColor);
            addressItem->setForeground(duplicateColor);
            initValueItem->setForeground(duplicateColor);
            selfSignalItem->setForeground(duplicateColor);
        } else if (binding.enabled && binding.address.trimmed().isEmpty()) {
            const QColor warningColor(QStringLiteral("#b9770e"));
            dataRefItem->setForeground(warningColor);
            addressItem->setForeground(warningColor);
        }

        m_deviceBindingsTable->setItem(row, 0, enabledItem);
        m_deviceBindingsTable->setItem(row, 1, dataRefItem);
        m_deviceBindingsTable->setItem(row, 2, descriptionItem);
        m_deviceBindingsTable->setItem(row, 3, addressItem);
        m_deviceBindingsTable->setItem(row, 4, initValueItem);
        m_deviceBindingsTable->setItem(row, 5, selfSignalItem);
    }
    m_updatingDeviceBindingsTable = false;

    if (!duplicateAddresses.isEmpty()) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("检测到重复 104 地址：%1。请调整地址列，避免启用点位地址冲突。")
                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，"))));
    } else if (emptyAddressCount > 0) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #b9770e; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("当前仍有 %1 个启用点位未填写 104 地址，导出前需要补齐。")
                .arg(emptyAddressCount));
    } else {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        m_deviceValidationLabel->setText(QStringLiteral("当前设备地址分配未发现重复。"));
    }
}

void MainWindow::refreshLogicCenterOverview()
{
    if (!m_logicCenterPage) {
        return;
    }

    const configtool::ConfigProject &project = m_configProjectManager.project();
    const configtool::LogicCenterConfig &logic = project.logicCenter;
    const QList<configtool::ConfigIssue> issues = configtool::validateLogicCenterConfig(logic, &project);

    if (m_logicAgcAvcGroupCountLabel) {
        m_logicAgcAvcGroupCountLabel->setText(QString::number(logic.agcAvcGroups.size()));
    }
    if (m_logicComputationPointCountLabel) {
        m_logicComputationPointCountLabel->setText(QString::number(logic.computationPoints.size()));
    }
    if (m_logicControlRuleCountLabel) {
        m_logicControlRuleCountLabel->setText(QString::number(logic.controlRules.size()));
    }
    if (m_logicOnlineLinkCountLabel) {
        m_logicOnlineLinkCountLabel->setText(QString::number(logic.onlineStatusLinks.size()));
    }
    if (m_logicDerivedDeviceCountLabel) {
        m_logicDerivedDeviceCountLabel->setText(QStringLiteral("0"));
    }
    if (m_logicIssueCountLabel) {
        int errorCount = 0;
        int warningCount = 0;
        int infoCount = 0;
        for (const configtool::ConfigIssue &issue : issues) {
            if (issue.severity == configtool::ConfigIssueSeverity::Error) {
                ++errorCount;
            } else if (issue.severity == configtool::ConfigIssueSeverity::Warning) {
                ++warningCount;
            } else {
                ++infoCount;
            }
        }
        m_logicIssueCountLabel->setText(QStringLiteral("%1 项（错误 %2，警告 %3，提示 %4）")
            .arg(errorCount + warningCount)
            .arg(errorCount)
            .arg(warningCount)
            .arg(infoCount));
    }
    if (m_logicExportPathLabel) {
        const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
        const QString appDir = resolveLogicCenterAppDir(projectRoot).isEmpty()
            ? QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"))
            : resolveLogicCenterAppDir(projectRoot);
        const QString exportPath = projectRoot.trimmed().isEmpty()
            ? QStringLiteral("cepLogicCenter/etc/LogicCenter_Config.json")
            : QDir(appDir).filePath(QStringLiteral("etc/LogicCenter_Config.json"));
        m_logicExportPathLabel->setText(exportPath);
    }

    if (!m_logicIssueTable) {
        return;
    }

    m_logicIssueTable->setRowCount(issues.size());
    for (int row = 0; row < issues.size(); ++row) {
        const configtool::ConfigIssue &issue = issues.at(row);
        const QColor color = configIssueSeverityColor(issue.severity);
        auto *severityItem = new QTableWidgetItem(configIssueSeverityText(issue.severity));
        auto *moduleItem = new QTableWidgetItem(issue.module);
        auto *objectItem = new QTableWidgetItem(issue.objectId);
        auto *messageItem = new QTableWidgetItem(issue.message);
        for (QTableWidgetItem *item : {severityItem, moduleItem, objectItem, messageItem}) {
            item->setForeground(color);
        }
        m_logicIssueTable->setItem(row, 0, severityItem);
        m_logicIssueTable->setItem(row, 1, moduleItem);
        m_logicIssueTable->setItem(row, 2, objectItem);
        m_logicIssueTable->setItem(row, 3, messageItem);
    }
}

configtool::AgcAvcGroup *MainWindow::ensureLogicAgcAvcGroup()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (logic.agcAvcGroups.isEmpty()) {
        configtool::AgcAvcGroup group;
        group.groupId = QStringLiteral("default");
        group.virtualDeviceId = QStringLiteral("999");
        logic.agcAvcGroups.append(group);
    }
    return &logic.agcAvcGroups[0];
}

configtool::AgcAvcGroup *MainWindow::currentLogicAgcAvcGroup()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (logic.agcAvcGroups.isEmpty()) {
        return nullptr;
    }
    return &logic.agcAvcGroups[0];
}

void MainWindow::refreshLogicAgcAvcPage()
{
    if (!m_logicAgcAvcPage) {
        return;
    }

    const configtool::AgcAvcGroup defaultGroup;
    const configtool::AgcAvcGroup *group = currentLogicAgcAvcGroup();
    if (!group) {
        group = &defaultGroup;
    }
    m_updatingLogicAgcAvcPage = true;

    m_logicAgcAvcGroupIdEdit->setText(group->groupId);
    m_logicAgcAvcVirtualDeviceIdEdit->setText(group->virtualDeviceId);
    m_logicMeasurementTotalPEdit->setValue(group->measurementScale.totalP);
    m_logicMeasurementTotalQEdit->setValue(group->measurementScale.totalQ);

    m_logicGateEnableReverseCheck->setChecked(group->gateReverse.enable);
    m_logicGateDistantReverseCheck->setChecked(group->gateReverse.distant);
    m_logicGateLockReverseCheck->setChecked(group->gateReverse.lock);
    m_logicGateUplockReverseCheck->setChecked(group->gateReverse.uplock);
    m_logicGateDownlockReverseCheck->setChecked(group->gateReverse.downlock);
    m_logicGateOpenloopReverseCheck->setChecked(group->gateReverse.openloop);

    m_logicAgcFollowEnableCheck->setChecked(group->agcFollow.enable);
    m_logicAgcFollowPeriodEdit->setValue(group->agcFollow.periodMs);
    m_logicAgcFollowStepEdit->setValue(group->agcFollow.step);
    m_logicAgcFollowToleranceEdit->setValue(group->agcFollow.tolerance);
    m_logicAvcFollowEnableCheck->setChecked(group->avcFollow.enable);
    m_logicAvcFollowPeriodEdit->setValue(group->avcFollow.periodMs);
    m_logicAvcFollowStepEdit->setValue(group->avcFollow.step);
    m_logicAvcFollowToleranceEdit->setValue(group->avcFollow.tolerance);

    m_logicAgcAvcDeviceTable->setRowCount(group->devices.size());
    for (int row = 0; row < group->devices.size(); ++row) {
        const configtool::AgcAvcDevice &device = group->devices.at(row);
        const QStringList values = {
            device.deviceId,
            device.ctrlDataRefP,
            device.ctrlDataRefQ,
            device.onlineDeviceId,
            device.onlineDataRef,
            QString::number(device.onlineOkValue),
            doubleToUiText(device.pMin),
            doubleToUiText(device.pMax),
            doubleToUiText(device.qMin),
            doubleToUiText(device.qMax),
            doubleToUiText(device.scaleP),
            doubleToUiText(device.scaleQ)
        };
        for (int column = 0; column < values.size(); ++column) {
            m_logicAgcAvcDeviceTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }

    m_updatingLogicAgcAvcPage = false;
}

void MainWindow::refreshLogicComputationPointPage()
{
    if (!m_logicComputationPointTable) {
        return;
    }

    m_updatingLogicComputationPointPage = true;
    const QList<configtool::LogicComputationPoint> &points =
        m_configProjectManager.project().logicCenter.computationPoints;
    m_logicComputationPointTable->setRowCount(points.size());
    for (int row = 0; row < points.size(); ++row) {
        const configtool::LogicComputationPoint &point = points.at(row);
        QStringList operands;
        for (const configtool::LogicOperand &operand : point.operands) {
            operands.append(QStringLiteral("%1#%2").arg(operand.deviceId, operand.dataRef));
        }

        auto *deviceItem = new QTableWidgetItem(point.deviceId);
        auto *dataRefItem = new QTableWidgetItem(point.dataRef);
        auto *formulaItem = new QTableWidgetItem(point.formula);
        auto *dropItem = new QTableWidgetItem();
        auto *operandsItem = new QTableWidgetItem(operands.join(QStringLiteral("; ")));
        auto *descriptionItem = new QTableWidgetItem(point.description);

        const Qt::ItemFlags readOnlyFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
        const Qt::ItemFlags editableFlags = readOnlyFlags | Qt::ItemIsEditable;
        deviceItem->setFlags(readOnlyFlags);
        dataRefItem->setFlags(readOnlyFlags);
        formulaItem->setFlags(editableFlags);
        dropItem->setFlags(readOnlyFlags | Qt::ItemIsUserCheckable);
        dropItem->setCheckState(point.dropOperands ? Qt::Checked : Qt::Unchecked);
        operandsItem->setFlags(readOnlyFlags);
        descriptionItem->setFlags(editableFlags);

        m_logicComputationPointTable->setItem(row, LogicComputationColumnOutputDevice, deviceItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnOutputPoint, dataRefItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnFormula, formulaItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnDropOperands, dropItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnOperands, operandsItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnDescription, descriptionItem);
    }
    m_updatingLogicComputationPointPage = false;
}

QList<configtool::LogicOperand> MainWindow::collectLogicTemplateOperands(configtool::ModelServiceType preferredType,
                                                                         const QString &title,
                                                                         int minimumCount)
{
    bool ok = false;
    const int count = QInputDialog::getInt(this,
                                           title,
                                           QStringLiteral("源点数量:"),
                                           minimumCount,
                                           minimumCount,
                                           128,
                                           1,
                                           &ok);
    if (!ok) {
        return {};
    }

    QList<configtool::LogicOperand> operands;
    for (int index = 0; index < count; ++index) {
        PointSelectorDialog dialog(this);
        dialog.setProject(&m_configProjectManager.project());
        dialog.setServiceTypeFilter(preferredType);
        dialog.setWindowTitle(QStringLiteral("%1 - 选择第 %2 个源点").arg(title).arg(index + 1));
        if (dialog.exec() != QDialog::Accepted) {
            return {};
        }

        const PointSelectorDialog::SelectedPoint selected = dialog.selectedPoint();
        if (!selected.valid) {
            return {};
        }

        configtool::LogicOperand operand;
        operand.deviceId = selected.deviceId;
        operand.dataRef = selected.dataRef;
        operands.append(operand);
    }

    return operands;
}

void MainWindow::upsertLogicTemplatePoint(const configtool::LogicComputationTemplateRequest &request)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    configtool::LogicComputationPoint point = configtool::buildLogicComputationPointFromTemplate(request);
    if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成计算点"), QStringLiteral("输出设备和输出点不能为空。"));
        return;
    }
    if (point.operands.isEmpty() || point.formula.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成计算点"), QStringLiteral("源点不足，无法生成有效公式。"));
        return;
    }

    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicComputationTemplate(int templateIndex)
{
    const QStringList templateNames = {
        QStringLiteral("单点映射/改名"),
        QStringLiteral("原点缩放"),
        QStringLiteral("遥信 OR"),
        QStringLiteral("遥信 AND")
    };

    if (templateIndex < 0 || templateIndex >= templateNames.size()) {
        return;
    }
    if (templateIndex == 0) {
        generateLogicSinglePointTemplateVisual();
        return;
    }
    if (templateIndex == 1) {
        generateLogicSourcePointScaleTemplateVisual();
        return;
    }
    if (templateIndex == 2) {
        generateLogicStatusOrTemplateVisual();
        return;
    }
    if (templateIndex == 3) {
        generateLogicStatusAndTemplateVisual();
        return;
    }
}

void MainWindow::generateLogicSinglePointTemplateVisual()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("单点映射/改名 模板"));
    dialog.resize(900, 360);

    configtool::LogicOperand inputPoint;
    configtool::LogicOperand outputPoint;

    auto pointText = [](const QString &title, const configtool::LogicOperand &point) {
        if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
            return QStringLiteral("%1\n点击选择").arg(title);
        }
        return QStringLiteral("%1\n%2\n%3").arg(title, point.deviceId, point.dataRef);
    };

    auto selectMeasurementPoint = [this](const QString &title, configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Measurement);
        selector.setWindowTitle(title);
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("输入点乘以系数，映射为输出点"), &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(14);

    auto *inputBtn = new QPushButton(&dialog);
    inputBtn->setMinimumSize(220, 110);
    inputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(inputBtn, 1);

    auto *middlePanel = new QWidget(&dialog);
    auto *middleLayout = new QVBoxLayout(middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(8);
    auto *multiplyLabel = new QLabel(QStringLiteral("x"), &dialog);
    multiplyLabel->setAlignment(Qt::AlignCenter);
    QFont opFont = multiplyLabel->font();
    opFont.setBold(true);
    opFont.setPointSize(18);
    multiplyLabel->setFont(opFont);
    auto *coefficientEdit = new QDoubleSpinBox(&dialog);
    coefficientEdit->setDecimals(6);
    coefficientEdit->setRange(-1000000000.0, 1000000000.0);
    coefficientEdit->setValue(1.0);
    coefficientEdit->setSingleStep(0.1);
    coefficientEdit->setPrefix(QStringLiteral("系数 "));
    coefficientEdit->setMinimumWidth(160);
    auto *mapLabel = new QLabel(QStringLiteral("映射到"), &dialog);
    mapLabel->setAlignment(Qt::AlignCenter);
    middleLayout->addWidget(multiplyLabel);
    middleLayout->addWidget(coefficientEdit);
    middleLayout->addWidget(mapLabel);
    sceneLayout->addWidget(middlePanel);

    auto *outputBtn = new QPushButton(&dialog);
    outputBtn->setMinimumSize(220, 110);
    outputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(outputBtn, 1);
    mainLayout->addLayout(sceneLayout, 1);

    auto updateInput = [&]() {
        inputBtn->setText(pointText(QStringLiteral("输入点"), inputPoint));
    };
    auto updateOutput = [&]() {
        outputBtn->setText(pointText(QStringLiteral("输出点"), outputPoint));
    };
    connect(inputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(QStringLiteral("选择输入点"), inputPoint)) {
            updateInput();
        }
    });
    connect(outputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(QStringLiteral("选择输出点"), outputPoint)) {
            updateOutput();
        }
    });
    updateInput();
    updateOutput();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (inputPoint.deviceId.trimmed().isEmpty() || inputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("单点映射/改名"), QStringLiteral("请先选择输入点。"));
            return;
        }
        if (outputPoint.deviceId.trimmed().isEmpty() || outputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("单点映射/改名"), QStringLiteral("请先选择输出点。"));
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationPoint point;
    point.deviceId = outputPoint.deviceId;
    point.dataRef = outputPoint.dataRef;
    point.dropOperands = true;
    point.description = QStringLiteral("可视化模板生成: 单点映射/改名");
    point.operands.append(inputPoint);
    const double coefficient = coefficientEdit->value();
    point.formula = qFuzzyCompare(coefficient + 1.0, 2.0)
        ? QStringLiteral("{1}")
        : QStringLiteral("{1} * %1").arg(doubleToUiText(coefficient));

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicSourcePointScaleTemplateVisual()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("原点缩放 模板"));
    dialog.resize(620, 300);

    configtool::LogicOperand pointRef;

    auto pointText = [](const configtool::LogicOperand &point) {
        if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
            return QStringLiteral("原点\n点击选择");
        }
        return QStringLiteral("原点\n%1\n%2").arg(point.deviceId, point.dataRef);
    };

    auto selectMeasurementPoint = [this](configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Measurement);
        selector.setWindowTitle(QStringLiteral("选择原点"));
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("原点值乘以系数后，仍写回同一个点"), &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(14);

    auto *pointBtn = new QPushButton(&dialog);
    pointBtn->setMinimumSize(220, 110);
    pointBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(pointBtn, 1);

    auto *middlePanel = new QWidget(&dialog);
    auto *middleLayout = new QVBoxLayout(middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(8);
    auto *multiplyLabel = new QLabel(QStringLiteral("x"), &dialog);
    multiplyLabel->setAlignment(Qt::AlignCenter);
    QFont opFont = multiplyLabel->font();
    opFont.setBold(true);
    opFont.setPointSize(18);
    multiplyLabel->setFont(opFont);
    auto *coefficientEdit = new QDoubleSpinBox(&dialog);
    coefficientEdit->setDecimals(6);
    coefficientEdit->setRange(-1000000000.0, 1000000000.0);
    coefficientEdit->setValue(1.0);
    coefficientEdit->setSingleStep(0.1);
    coefficientEdit->setPrefix(QStringLiteral("系数 "));
    coefficientEdit->setMinimumWidth(160);
    auto *equalsLabel = new QLabel(QStringLiteral("=> 同一点"), &dialog);
    equalsLabel->setAlignment(Qt::AlignCenter);
    middleLayout->addWidget(multiplyLabel);
    middleLayout->addWidget(coefficientEdit);
    middleLayout->addWidget(equalsLabel);
    sceneLayout->addWidget(middlePanel);
    mainLayout->addLayout(sceneLayout, 1);

    auto updatePoint = [&]() {
        pointBtn->setText(pointText(pointRef));
    };
    connect(pointBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(pointRef)) {
            updatePoint();
        }
    });
    updatePoint();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (pointRef.deviceId.trimmed().isEmpty() || pointRef.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("原点缩放"), QStringLiteral("请先选择原点。"));
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationPoint point;
    point.deviceId = pointRef.deviceId;
    point.dataRef = pointRef.dataRef;
    point.dropOperands = true;
    point.description = QStringLiteral("可视化模板生成: 原点缩放");
    point.operands.append(pointRef);
    point.formula = QStringLiteral("{1} * %1").arg(doubleToUiText(coefficientEdit->value()));

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicStatusOrTemplateVisual()
{
    generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType::StatusOr,
                                      QStringLiteral("遥信 OR"),
                                      QStringLiteral("OR"),
                                      QStringLiteral("任一输入点为 1 时，输出点为 1"),
                                      QStringLiteral("StatusOr"));
}

void MainWindow::generateLogicStatusAndTemplateVisual()
{
    generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType::StatusAnd,
                                      QStringLiteral("遥信 AND"),
                                      QStringLiteral("AND"),
                                      QStringLiteral("所有输入点都为 1 时，输出点为 1"),
                                      QStringLiteral("StatusAnd"));
}

void MainWindow::generateLogicDerivedDeviceMappingVisual()
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (project.devices.isEmpty() || project.models.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("派生设备"),
                                 QStringLiteral("请先导入或创建源设备和目标模型。"));
        return;
    }

    QStringList deviceChoices;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (!device.deviceId.trimmed().isEmpty()) {
            deviceChoices.append(deviceChoiceText(device));
        }
    }
    deviceChoices.removeDuplicates();

    QStringList modelChoices;
    for (const configtool::ModelTemplate &model : project.models) {
        if (!model.modelId.trimmed().isEmpty()) {
            modelChoices.append(modelChoiceText(model));
        }
    }
    modelChoices.removeDuplicates();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("派生设备批量生成"));
    dialog.resize(980, 720);

    auto *mainLayout = new QVBoxLayout(&dialog);
    auto *basicGroup = new QGroupBox(QStringLiteral("基本设置"), &dialog);
    auto *basicLayout = new QGridLayout(basicGroup);
    auto *sourceDeviceCombo = new QComboBox(basicGroup);
    sourceDeviceCombo->addItems(deviceChoices);
    auto *targetModelCombo = new QComboBox(basicGroup);
    targetModelCombo->addItems(modelChoices);
    auto *createMissingDeviceCheck = new QCheckBox(QStringLiteral("自动创建缺失的目标设备"), basicGroup);
    createMissingDeviceCheck->setChecked(true);
    auto *generateOnlineLinkCheck = new QCheckBox(QStringLiteral("同时生成在线联动"), basicGroup);
    generateOnlineLinkCheck->setChecked(true);
    basicLayout->addWidget(new QLabel(QStringLiteral("源真实设备:"), basicGroup), 0, 0);
    basicLayout->addWidget(sourceDeviceCombo, 0, 1);
    basicLayout->addWidget(new QLabel(QStringLiteral("目标设备模型:"), basicGroup), 0, 2);
    basicLayout->addWidget(targetModelCombo, 0, 3);
    basicLayout->addWidget(createMissingDeviceCheck, 1, 1);
    basicLayout->addWidget(generateOnlineLinkCheck, 1, 3);
    mainLayout->addWidget(basicGroup);

    auto *instanceGroup = new QGroupBox(QStringLiteral("派生实例"), &dialog);
    auto *instanceLayout = new QVBoxLayout(instanceGroup);
    auto *instanceToolbar = new QHBoxLayout();
    auto *detectPrefixBtn = new QPushButton(QStringLiteral("从源点识别前缀"), instanceGroup);
    auto *addInstanceBtn = new QPushButton(QStringLiteral("新增实例"), instanceGroup);
    auto *removeInstanceBtn = new QPushButton(QStringLiteral("删除实例"), instanceGroup);
    instanceToolbar->addWidget(detectPrefixBtn);
    instanceToolbar->addSpacing(12);
    instanceToolbar->addWidget(addInstanceBtn);
    instanceToolbar->addWidget(removeInstanceBtn);
    instanceToolbar->addStretch();
    instanceLayout->addLayout(instanceToolbar);

    auto *instanceTable = new QTableWidget(0, 3, instanceGroup);
    instanceTable->setHorizontalHeaderLabels({
        QStringLiteral("源点前缀"),
        QStringLiteral("目标 DeviceId"),
        QStringLiteral("目标设备描述")
    });
    instanceTable->horizontalHeader()->setStretchLastSection(true);
    instanceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    instanceTable->setColumnWidth(0, 180);
    instanceTable->setColumnWidth(1, 160);
    instanceTable->setColumnWidth(2, 260);
    instanceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    instanceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    instanceLayout->addWidget(instanceTable, 1);
    mainLayout->addWidget(instanceGroup, 1);

    auto *previewGroup = new QGroupBox(QStringLiteral("生成预览"), &dialog);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    auto *previewToolbar = new QHBoxLayout();
    auto *refreshPreviewBtn = new QPushButton(QStringLiteral("刷新预览"), previewGroup);
    auto *previewSummaryLabel = new QLabel(QStringLiteral("尚未生成预览"), previewGroup);
    previewToolbar->addWidget(refreshPreviewBtn);
    previewToolbar->addWidget(previewSummaryLabel, 1);
    previewLayout->addLayout(previewToolbar);
    auto *previewTable = new QTableWidget(0, 5, previewGroup);
    previewTable->setHorizontalHeaderLabels({
        QStringLiteral("源点"),
        QStringLiteral("目标设备"),
        QStringLiteral("目标点"),
        QStringLiteral("动作"),
        QStringLiteral("状态")
    });
    previewTable->horizontalHeader()->setStretchLastSection(true);
    previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    previewTable->setColumnWidth(0, 320);
    previewTable->setColumnWidth(1, 120);
    previewTable->setColumnWidth(2, 320);
    previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    previewTable->setAlternatingRowColors(true);
    previewLayout->addWidget(previewTable, 1);
    mainLayout->addWidget(previewGroup, 2);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);

    struct DerivedInstance {
        QString prefix;
        QString targetDeviceId;
        QString description;
    };
    struct DerivedPreview {
        QString sourceDataRef;
        QString targetDeviceId;
        QString targetDataRef;
        QString action;
        QString status;
        bool valid = false;
    };

    QList<DerivedPreview> previews;

    auto collectInstances = [&]() {
        QList<DerivedInstance> instances;
        for (int row = 0; row < instanceTable->rowCount(); ++row) {
            DerivedInstance instance;
            instance.prefix = instanceTable->item(row, 0) ? instanceTable->item(row, 0)->text().trimmed() : QString();
            instance.targetDeviceId = instanceTable->item(row, 1) ? instanceTable->item(row, 1)->text().trimmed() : QString();
            instance.description = instanceTable->item(row, 2) ? instanceTable->item(row, 2)->text().trimmed() : QString();
            if (!instance.prefix.isEmpty() && !instance.targetDeviceId.isEmpty()) {
                instances.append(instance);
            }
        }
        return instances;
    };

    auto setInstanceRows = [&](const QList<DerivedInstance> &instances) {
        instanceTable->setRowCount(instances.size());
        for (int row = 0; row < instances.size(); ++row) {
            const DerivedInstance &instance = instances.at(row);
            instanceTable->setItem(row, 0, new QTableWidgetItem(instance.prefix));
            instanceTable->setItem(row, 1, new QTableWidgetItem(instance.targetDeviceId));
            instanceTable->setItem(row, 2, new QTableWidgetItem(instance.description));
        }
    };

    auto selectedContext = [&]() {
        struct Context {
            const configtool::ProtocolDeviceInstance *sourceDevice = nullptr;
            const configtool::ModelTemplate *targetModel = nullptr;
        };
        Context context;
        context.sourceDevice = findDeviceById(m_configProjectManager.project(),
                                              deviceIdFromChoiceText(sourceDeviceCombo->currentText()));
        context.targetModel = findModelById(m_configProjectManager.project(),
                                            modelIdFromChoiceText(targetModelCombo->currentText()));
        return context;
    };

    auto targetPointMap = [](const configtool::ModelTemplate &model) {
        QHash<QString, QString> pointsByShortName;
        for (const configtool::ServiceTemplate &service : model.services) {
            for (const configtool::PointTemplate &point : service.points) {
                const QString dataRef = point.dataRef().trimmed();
                const QString shortName = pointShortName(point);
                if (!shortName.isEmpty() && !dataRef.isEmpty() && !pointsByShortName.contains(shortName)) {
                    pointsByShortName.insert(shortName, dataRef);
                }
            }
        }
        return pointsByShortName;
    };

    auto detectInstances = [&]() {
        const auto context = selectedContext();
        if (!context.sourceDevice || !context.targetModel) {
            return QList<DerivedInstance>();
        }

        QStringList targetShortNames = targetPointMap(*context.targetModel).keys();
        std::sort(targetShortNames.begin(), targetShortNames.end(), [](const QString &left, const QString &right) {
            return left.size() > right.size();
        });

        QHash<QString, DerivedInstance> instancesByPrefix;
        const QRegularExpression numberPattern(QStringLiteral("(\\d+)"));
        for (const configtool::PointBinding &binding : context.sourceDevice->bindings) {
            const QString sourceShortName = pointShortNameFromDataRef(binding.dataRef);
            for (const QString &targetShortName : targetShortNames) {
                if (!sourceShortName.endsWith(targetShortName, Qt::CaseInsensitive)) {
                    continue;
                }
                const QString prefix = sourceShortName.left(sourceShortName.size() - targetShortName.size());
                if (prefix.isEmpty() || instancesByPrefix.contains(prefix)) {
                    break;
                }
                QString targetDeviceId;
                const QRegularExpressionMatch match = numberPattern.match(prefix);
                if (match.hasMatch()) {
                    targetDeviceId = match.captured(1);
                }
                DerivedInstance instance;
                instance.prefix = prefix;
                instance.targetDeviceId = targetDeviceId;
                instance.description = targetDeviceId.isEmpty()
                    ? QStringLiteral("派生设备")
                    : QStringLiteral("派生设备%1").arg(targetDeviceId);
                instancesByPrefix.insert(prefix, instance);
                break;
            }
        }

        QList<DerivedInstance> instances = instancesByPrefix.values();
        std::sort(instances.begin(), instances.end(), [](const DerivedInstance &left, const DerivedInstance &right) {
            return left.prefix < right.prefix;
        });
        return instances;
    };

    auto refreshPreview = [&]() {
        previews.clear();
        const auto context = selectedContext();
        if (!context.sourceDevice || !context.targetModel) {
            previewTable->setRowCount(0);
            previewSummaryLabel->setText(QStringLiteral("请选择有效的源设备和目标模型"));
            return;
        }

        const QList<DerivedInstance> instances = collectInstances();
        const QHash<QString, QString> targetPoints = targetPointMap(*context.targetModel);
        const configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;

        for (const DerivedInstance &instance : instances) {
            const bool targetDeviceExists = findDeviceById(m_configProjectManager.project(), instance.targetDeviceId) != nullptr;
            for (const configtool::PointBinding &binding : context.sourceDevice->bindings) {
                const QString sourceShortName = pointShortNameFromDataRef(binding.dataRef);
                if (!sourceShortName.startsWith(instance.prefix, Qt::CaseInsensitive)) {
                    continue;
                }

                DerivedPreview preview;
                preview.sourceDataRef = binding.dataRef;
                preview.targetDeviceId = instance.targetDeviceId;
                const QString targetShortName = sourceShortName.mid(instance.prefix.size());
                preview.targetDataRef = targetPoints.value(targetShortName);
                if (preview.targetDataRef.isEmpty()) {
                    preview.status = QStringLiteral("目标模型无点 %1").arg(targetShortName);
                } else if (!targetDeviceExists && !createMissingDeviceCheck->isChecked()) {
                    preview.status = QStringLiteral("目标设备不存在");
                } else {
                    preview.valid = true;
                    preview.action = QStringLiteral("新增");
                    for (const configtool::LogicComputationPoint &existing : logic.computationPoints) {
                        if (existing.deviceId.trimmed() == preview.targetDeviceId
                            && existing.dataRef.trimmed() == preview.targetDataRef) {
                            preview.action = QStringLiteral("更新");
                            break;
                        }
                    }
                    preview.status = targetDeviceExists ? QStringLiteral("可生成")
                                                        : QStringLiteral("将创建设备后生成");
                }
                previews.append(preview);
            }
        }

        previewTable->setRowCount(previews.size());
        int validCount = 0;
        for (int row = 0; row < previews.size(); ++row) {
            const DerivedPreview &preview = previews.at(row);
            if (preview.valid) {
                ++validCount;
            }
            const QColor color = preview.valid ? QColor() : QColor(170, 0, 0);
            const QStringList values = {
                QStringLiteral("%1#%2").arg(context.sourceDevice->deviceId, preview.sourceDataRef),
                preview.targetDeviceId,
                preview.targetDataRef,
                preview.action,
                preview.status
            };
            for (int column = 0; column < values.size(); ++column) {
                auto *item = new QTableWidgetItem(values.at(column));
                if (color.isValid()) {
                    item->setForeground(color);
                }
                previewTable->setItem(row, column, item);
            }
        }
        previewSummaryLabel->setText(QStringLiteral("可生成 %1 条，预览共 %2 条").arg(validCount).arg(previews.size()));
    };

    auto addInstanceRow = [&](const DerivedInstance &instance) {
        const int row = instanceTable->rowCount();
        instanceTable->insertRow(row);
        instanceTable->setItem(row, 0, new QTableWidgetItem(instance.prefix));
        instanceTable->setItem(row, 1, new QTableWidgetItem(instance.targetDeviceId));
        instanceTable->setItem(row, 2, new QTableWidgetItem(instance.description));
        instanceTable->selectRow(row);
    };

    connect(detectPrefixBtn, &QPushButton::clicked, &dialog, [&]() {
        const QList<DerivedInstance> instances = detectInstances();
        if (instances.isEmpty()) {
            QMessageBox::information(&dialog,
                                     QStringLiteral("识别前缀"),
                                     QStringLiteral("未能从源点中识别出可匹配目标模型的前缀。"));
            return;
        }
        setInstanceRows(instances);
        refreshPreview();
    });
    connect(addInstanceBtn, &QPushButton::clicked, &dialog, [&]() {
        const int index = instanceTable->rowCount() + 1;
        addInstanceRow({QStringLiteral("Inv%1_").arg(index),
                        QString::number(index),
                        QStringLiteral("派生设备%1").arg(index)});
    });
    connect(removeInstanceBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = instanceTable->currentRow();
        if (row >= 0) {
            instanceTable->removeRow(row);
            refreshPreview();
        }
    });
    connect(refreshPreviewBtn, &QPushButton::clicked, &dialog, refreshPreview);
    connect(sourceDeviceCombo, &QComboBox::currentTextChanged, &dialog, [&]() {
        setInstanceRows(detectInstances());
        refreshPreview();
    });
    connect(targetModelCombo, &QComboBox::currentTextChanged, &dialog, [&]() {
        setInstanceRows(detectInstances());
        refreshPreview();
    });
    connect(createMissingDeviceCheck, &QCheckBox::toggled, &dialog, [&]() {
        refreshPreview();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        refreshPreview();
        bool hasValid = false;
        for (const DerivedPreview &preview : previews) {
            if (preview.valid) {
                hasValid = true;
                break;
            }
        }
        if (!hasValid) {
            QMessageBox::warning(&dialog, QStringLiteral("派生设备"), QStringLiteral("没有可生成的映射。"));
            return;
        }
        dialog.accept();
    });

    setInstanceRows(detectInstances());
    refreshPreview();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto context = selectedContext();
    if (!context.sourceDevice || !context.targetModel) {
        return;
    }

    const configtool::ProtocolDeviceInstance sourceDevice = *context.sourceDevice;
    const configtool::ModelTemplate targetModel = *context.targetModel;
    const QList<DerivedInstance> instances = collectInstances();
    const bool createMissingDevices = createMissingDeviceCheck->isChecked();
    const bool generateOnlineLinks = generateOnlineLinkCheck->isChecked();

    pushConfigUndoSnapshot();
    configtool::ConfigProject &mutableProject = m_configProjectManager.project();
    configtool::LogicCenterConfig &logic = mutableProject.logicCenter;

    int createdDeviceCount = 0;
    if (createMissingDevices) {
        for (const DerivedInstance &instance : instances) {
            if (findDeviceById(mutableProject, instance.targetDeviceId)) {
                continue;
            }
            configtool::ProtocolDeviceInstance device;
            device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            device.appType = sourceDevice.appType;
            device.protocol = sourceDevice.protocol;
            device.deviceId = instance.targetDeviceId;
            device.deviceDesc = instance.description.isEmpty() ? instance.targetDeviceId : instance.description;
            device.modelId = targetModel.modelId;
            for (const configtool::ServiceTemplate &service : targetModel.services) {
                for (const configtool::PointTemplate &point : service.points) {
                    configtool::PointBinding binding;
                    binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    binding.pointRef = point.pointRef(targetModel.modelId);
                    binding.dataRef = point.dataRef();
                    binding.descriptionOverride = point.description;
                    binding.enabled = true;
                    device.bindings.append(binding);
                }
            }
            mutableProject.devices.append(device);
            ++createdDeviceCount;
        }
    }

    int insertedCount = 0;
    int updatedCount = 0;
    for (const DerivedPreview &preview : previews) {
        if (!preview.valid) {
            continue;
        }
        configtool::LogicComputationPoint point;
        point.deviceId = preview.targetDeviceId;
        point.dataRef = preview.targetDataRef;
        point.formula = QStringLiteral("{1}");
        point.dropOperands = true;
        point.description = QStringLiteral("派生设备映射: %1#%2 -> %3#%4")
            .arg(sourceDevice.deviceId, preview.sourceDataRef, preview.targetDeviceId, preview.targetDataRef);
        configtool::LogicOperand operand;
        operand.deviceId = sourceDevice.deviceId;
        operand.dataRef = preview.sourceDataRef;
        point.operands.append(operand);
        if (configtool::upsertLogicComputationPoint(logic, point)) {
            ++insertedCount;
        } else {
            ++updatedCount;
        }
    }

    int onlineLinkCount = 0;
    if (generateOnlineLinks) {
        for (const DerivedInstance &instance : instances) {
            bool exists = false;
            for (const configtool::LogicOnlineStatusLink &link : logic.onlineStatusLinks) {
                if (link.deviceId == instance.targetDeviceId && link.linkToDeviceId == sourceDevice.deviceId) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                configtool::LogicOnlineStatusLink link;
                link.deviceId = instance.targetDeviceId;
                link.linkToDeviceId = sourceDevice.deviceId;
                logic.onlineStatusLinks.append(link);
                ++onlineLinkCount;
            }
        }
    }

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    refreshLogicOnlineLinkPage();
    statusBar()->showMessage(QStringLiteral("派生设备生成完成：新增 %1 条，更新 %2 条，创建设备 %3 个，在线联动 %4 条")
                                 .arg(insertedCount)
                                 .arg(updatedCount)
                                 .arg(createdDeviceCount)
                                 .arg(onlineLinkCount),
                             6000);
}

void MainWindow::generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType type,
                                                   const QString &templateName,
                                                   const QString &operatorText,
                                                   const QString &titleText,
                                                   const QString &defaultOutputDataRef)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("%1 模板").arg(templateName));
    dialog.resize(1080, 420);

    QList<configtool::LogicOperand> inputPoints;
    for (int i = 0; i < 3; ++i) {
        inputPoints.append(configtool::LogicOperand());
    }
    configtool::LogicOperand outputPoint;

    auto pointText = [](const QString &title, const configtool::LogicOperand &point) {
        if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
            return QStringLiteral("%1\n点击选择").arg(title);
        }
        return QStringLiteral("%1\n%2\n%3").arg(title, point.deviceId, point.dataRef);
    };

    auto selectStatusPoint = [this](const QString &title, configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Status);
        selector.setWindowTitle(title);
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(titleText, &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *topLayout = new QHBoxLayout();
    auto *addInputBtn = new QPushButton(QStringLiteral("+"), &dialog);
    auto *removeInputBtn = new QPushButton(QStringLiteral("-"), &dialog);
    addInputBtn->setFixedSize(52, 40);
    removeInputBtn->setFixedSize(52, 40);
    topLayout->addWidget(addInputBtn);
    topLayout->addWidget(removeInputBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(10);

    auto *inputHost = new QWidget(&dialog);
    auto *inputLayout = new QHBoxLayout(inputHost);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);

    auto *inputScroll = new QScrollArea(&dialog);
    inputScroll->setWidgetResizable(true);
    inputScroll->setMinimumHeight(120);
    inputScroll->setWidget(inputHost);
    sceneLayout->addWidget(inputScroll, 1);

    auto *arrowLabel = new QLabel(QStringLiteral("=>"), &dialog);
    arrowLabel->setAlignment(Qt::AlignCenter);
    QFont arrowFont = arrowLabel->font();
    arrowFont.setBold(true);
    arrowFont.setPointSize(14);
    arrowLabel->setFont(arrowFont);
    sceneLayout->addWidget(arrowLabel);

    auto *outputBtn = new QPushButton(&dialog);
    outputBtn->setMinimumSize(190, 96);
    outputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(outputBtn);
    mainLayout->addLayout(sceneLayout, 1);

    std::function<void()> rebuildInputs;
    auto updateOutput = [&]() {
        outputBtn->setText(pointText(QStringLiteral("输出点"), outputPoint));
    };

    rebuildInputs = [&]() {
        while (QLayoutItem *child = inputLayout->takeAt(0)) {
            if (QWidget *widget = child->widget()) {
                widget->deleteLater();
            }
            delete child;
        }

        for (int index = 0; index < inputPoints.size(); ++index) {
            if (index > 0) {
                auto *orLabel = new QLabel(operatorText, &dialog);
                orLabel->setAlignment(Qt::AlignCenter);
                QFont orFont = orLabel->font();
                orFont.setBold(true);
                orFont.setPointSize(12);
                orLabel->setFont(orFont);
                orLabel->setMinimumWidth(28);
                inputLayout->addWidget(orLabel);
            }
            auto *button = new QPushButton(pointText(QStringLiteral("输入点 %1").arg(index + 1), inputPoints.at(index)), &dialog);
            button->setMinimumSize(150, 90);
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            inputLayout->addWidget(button);
            connect(button, &QPushButton::clicked, &dialog, [&, index]() {
                if (selectStatusPoint(QStringLiteral("选择输入点 %1").arg(index + 1), inputPoints[index])) {
                    rebuildInputs();
                }
            });
        }
        inputLayout->addStretch();
        removeInputBtn->setEnabled(inputPoints.size() > 1);
    };

    connect(addInputBtn, &QPushButton::clicked, &dialog, [&]() {
        inputPoints.append(configtool::LogicOperand());
        rebuildInputs();
    });
    connect(removeInputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (inputPoints.size() <= 1) {
            return;
        }
        inputPoints.removeLast();
        rebuildInputs();
    });
    connect(outputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectStatusPoint(QStringLiteral("选择输出点"), outputPoint)) {
            updateOutput();
        }
    });

    updateOutput();
    rebuildInputs();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (outputPoint.deviceId.trimmed().isEmpty() || outputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, templateName, QStringLiteral("请先选择输出点。"));
            return;
        }
        for (int index = 0; index < inputPoints.size(); ++index) {
            const configtool::LogicOperand &point = inputPoints.at(index);
            if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
                QMessageBox::information(&dialog,
                                         templateName,
                                         QStringLiteral("请先选择输入点 %1。").arg(index + 1));
                return;
            }
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationTemplateRequest request;
    request.type = type;
    request.outputDeviceId = outputPoint.deviceId;
    request.outputDataRef = outputPoint.dataRef;
    request.operands = inputPoints;
    request.dropOperands = true;
    request.description = QStringLiteral("可视化模板生成: %1").arg(templateName);
    upsertLogicTemplatePoint(request);
}

bool MainWindow::editLogicComputationOperands(const QString &title,
                                              const QString &formula,
                                              QList<configtool::LogicOperand> &operands)
{
    const int operandCount = formulaOperandCount(formula);
    if (operandCount <= 0) {
        QMessageBox::information(this,
                                 title,
                                 QStringLiteral("公式中没有 {1} 这类源点占位符，不需要配置源点。"));
        operands.clear();
        return true;
    }

    while (operands.size() < operandCount) {
        operands.append(configtool::LogicOperand());
    }
    while (operands.size() > operandCount) {
        operands.removeLast();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(820, 420);

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *formulaLabel = new QLabel(QStringLiteral("公式: %1").arg(formula.trimmed()), &dialog);
    formulaLabel->setWordWrap(true);
    mainLayout->addWidget(formulaLabel);

    auto *hintLabel = new QLabel(QStringLiteral("按公式中的 {1}、{2} 顺序配置源点。"), &dialog);
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    auto *table = new QTableWidget(operandCount, 3, &dialog);
    table->setHorizontalHeaderLabels({
        QStringLiteral("占位符"),
        QStringLiteral("源设备"),
        QStringLiteral("源点")
    });
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 90);
    table->setColumnWidth(1, 160);
    mainLayout->addWidget(table, 1);

    auto refreshRow = [&](int row) {
        const configtool::LogicOperand &operand = operands.at(row);
        auto *placeholderItem = new QTableWidgetItem(QStringLiteral("{%1}").arg(row + 1));
        auto *deviceItem = new QTableWidgetItem(operand.deviceId);
        auto *dataRefItem = new QTableWidgetItem(operand.dataRef);
        placeholderItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        deviceItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        dataRefItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        table->setItem(row, 0, placeholderItem);
        table->setItem(row, 1, deviceItem);
        table->setItem(row, 2, dataRefItem);
    };

    for (int row = 0; row < operandCount; ++row) {
        refreshRow(row);
    }

    auto selectOperand = [&](int row) {
        if (row < 0 || row >= operands.size()) {
            return;
        }

        PointSelectorDialog selector(&dialog);
        selector.setProject(&m_configProjectManager.project());
        selector.setWindowTitle(QStringLiteral("选择 %1").arg(table->item(row, 0)->text()));
        if (selector.exec() != QDialog::Accepted) {
            return;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return;
        }

        operands[row].deviceId = selected.deviceId;
        operands[row].dataRef = selected.dataRef;
        refreshRow(row);
        table->selectRow(row);
    };

    auto *toolbar = new QHBoxLayout();
    auto *selectBtn = new QPushButton(QStringLiteral("选择源点"), &dialog);
    auto *clearBtn = new QPushButton(QStringLiteral("清空本行"), &dialog);
    toolbar->addWidget(selectBtn);
    toolbar->addWidget(clearBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    connect(selectBtn, &QPushButton::clicked, &dialog, [&]() {
        selectOperand(table->currentRow());
    });
    connect(clearBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = table->currentRow();
        if (row < 0 || row >= operands.size()) {
            return;
        }

        operands[row] = configtool::LogicOperand();
        refreshRow(row);
        table->selectRow(row);
    });
    connect(table, &QTableWidget::cellDoubleClicked, &dialog, [&](int row, int) {
        selectOperand(row);
    });
    if (operandCount > 0) {
        table->selectRow(0);
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        for (int row = 0; row < operands.size(); ++row) {
            const configtool::LogicOperand &operand = operands.at(row);
            if (operand.deviceId.trimmed().isEmpty() || operand.dataRef.trimmed().isEmpty()) {
                QMessageBox::information(&dialog,
                                         title,
                                         QStringLiteral("请先配置 %1 的源点。").arg(table->item(row, 0)->text()));
                table->selectRow(row);
                return;
            }
        }
        dialog.accept();
    });

    return dialog.exec() == QDialog::Accepted;
}

void MainWindow::onAddLogicComputationPointClicked()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    PointSelectorDialog outputSelector(this);
    outputSelector.setProject(&m_configProjectManager.project());
    outputSelector.setWindowTitle(QStringLiteral("选择计算点输出点"));
    if (outputSelector.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint output = outputSelector.selectedPoint();
    if (!output.valid) {
        return;
    }
    for (const configtool::LogicComputationPoint &existing : logic.computationPoints) {
        if (existing.deviceId.trimmed() == output.deviceId.trimmed()
            && existing.dataRef.trimmed() == output.dataRef.trimmed()) {
            QMessageBox::warning(this,
                                 QStringLiteral("新增计算点"),
                                 QStringLiteral("计算点 %1#%2 已存在，请选择其他输出点或直接编辑现有行。")
                                     .arg(output.deviceId, output.dataRef));
            return;
        }
    }

    bool ok = false;
    const QString formula = QInputDialog::getText(this,
                                                  QStringLiteral("新增计算点"),
                                                  QStringLiteral("公式:"),
                                                  QLineEdit::Normal,
                                                  QStringLiteral("{1}"),
                                                  &ok).trimmed();
    if (!ok) {
        return;
    }
    if (formula.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("新增计算点"), QStringLiteral("公式不能为空。"));
        return;
    }
    if (formulaOperandCount(formula) <= 0) {
        QMessageBox::warning(this,
                             QStringLiteral("新增计算点"),
                             QStringLiteral("公式至少需要包含一个 {1} 这类源点占位符。"));
        return;
    }

    configtool::LogicComputationPoint point;
    point.deviceId = output.deviceId;
    point.dataRef = output.dataRef;
    point.formula = formula;
    point.dropOperands = true;
    point.description = QStringLiteral("手工新增计算点");
    if (!editLogicComputationOperands(QStringLiteral("配置计算点源点"), point.formula, point.operands)) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.computationPoints.append(point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    const int row = logic.computationPoints.size() - 1;
    if (row >= 0) {
        m_logicComputationPointTable->selectRow(row);
    }
    statusBar()->showMessage(QStringLiteral("已新增计算点 %1#%2").arg(point.deviceId, point.dataRef), 5000);
}

void MainWindow::onCopyLogicComputationPointClicked()
{
    if (!m_logicComputationPointTable) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    const int row = m_logicComputationPointTable->currentRow();
    if (row < 0 || row >= logic.computationPoints.size()) {
        QMessageBox::information(this, QStringLiteral("复制计算点"), QStringLiteral("请先选择要复制的计算点。"));
        return;
    }

    configtool::LogicComputationPoint point = logic.computationPoints.at(row);
    PointSelectorDialog outputSelector(this);
    outputSelector.setProject(&m_configProjectManager.project());
    outputSelector.setWindowTitle(QStringLiteral("选择复制后的输出点"));
    if (outputSelector.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint output = outputSelector.selectedPoint();
    if (!output.valid) {
        return;
    }
    for (int index = 0; index < logic.computationPoints.size(); ++index) {
        const configtool::LogicComputationPoint &existing = logic.computationPoints.at(index);
        if (existing.deviceId.trimmed() == output.deviceId.trimmed()
            && existing.dataRef.trimmed() == output.dataRef.trimmed()) {
            QMessageBox::warning(this,
                                 QStringLiteral("复制计算点"),
                                 QStringLiteral("计算点 %1#%2 已存在，请选择其他输出点。")
                                     .arg(output.deviceId, output.dataRef));
            return;
        }
    }

    point.deviceId = output.deviceId;
    point.dataRef = output.dataRef;
    point.description = point.description.trimmed().isEmpty()
        ? QStringLiteral("复制计算点")
        : QStringLiteral("%1 (复制)").arg(point.description);

    pushConfigUndoSnapshot();
    logic.computationPoints.append(point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    const int newRow = logic.computationPoints.size() - 1;
    if (newRow >= 0) {
        m_logicComputationPointTable->selectRow(newRow);
    }
    statusBar()->showMessage(QStringLiteral("已复制计算点 %1#%2").arg(point.deviceId, point.dataRef), 5000);
}

void MainWindow::onDeleteLogicComputationPointClicked()
{
    if (!m_logicComputationPointTable) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    const int row = m_logicComputationPointTable->currentRow();
    if (row < 0 || row >= logic.computationPoints.size()) {
        QMessageBox::information(this, QStringLiteral("删除计算点"), QStringLiteral("请先选择要删除的计算点。"));
        return;
    }

    const configtool::LogicComputationPoint point = logic.computationPoints.at(row);
    const QString objectId = QStringLiteral("%1#%2").arg(point.deviceId, point.dataRef);
    if (QMessageBox::question(this,
                              QStringLiteral("删除计算点"),
                              QStringLiteral("确定删除计算点 %1 吗？").arg(objectId),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.computationPoints.removeAt(row);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();

    if (!logic.computationPoints.isEmpty()) {
        m_logicComputationPointTable->selectRow(qMin(row, logic.computationPoints.size() - 1));
    }

    statusBar()->showMessage(QStringLiteral("已删除计算点 %1").arg(objectId), 5000);
}

void MainWindow::onLogicComputationPointItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicComputationPointPage || m_restoringConfigUndo || !item) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    const int row = item->row();
    if (row < 0 || row >= logic.computationPoints.size()) {
        return;
    }

    configtool::LogicComputationPoint &point = logic.computationPoints[row];
    const int column = item->column();
    bool refreshTable = false;

    pushConfigUndoSnapshot();
    if (column == LogicComputationColumnFormula) {
        point.formula = item->text().trimmed();
        const int operandCount = formulaOperandCount(point.formula);
        while (point.operands.size() < operandCount) {
            point.operands.append(configtool::LogicOperand());
        }
        while (point.operands.size() > operandCount) {
            point.operands.removeLast();
        }
        refreshTable = true;
    } else if (column == LogicComputationColumnDropOperands) {
        point.dropOperands = item->checkState() == Qt::Checked;
    } else if (column == LogicComputationColumnDescription) {
        point.description = item->text();
    } else {
        return;
    }

    refreshLogicCenterOverview();
    if (refreshTable) {
        refreshLogicComputationPointPage();
        if (row < m_logicComputationPointTable->rowCount()) {
            m_logicComputationPointTable->setCurrentCell(row, column);
        }
    }
}

void MainWindow::onLogicComputationPointCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }

    if (column == LogicComputationColumnOutputDevice || column == LogicComputationColumnOutputPoint) {
        selectLogicComputationOutputPoint(row);
    } else if (column == LogicComputationColumnOperands) {
        selectLogicComputationOperands(row);
    }
}

void MainWindow::selectLogicComputationOutputPoint(int row)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.computationPoints.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setWindowTitle(QStringLiteral("选择计算点输出点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint selected = dialog.selectedPoint();
    if (!selected.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicComputationPoint &point = logic.computationPoints[row];
    point.deviceId = selected.deviceId;
    point.dataRef = selected.dataRef;
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    m_logicComputationPointTable->setCurrentCell(row, LogicComputationColumnOutputPoint);
}

void MainWindow::selectLogicComputationOperands(int row)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.computationPoints.size()) {
        return;
    }

    QList<configtool::LogicOperand> operands = logic.computationPoints.at(row).operands;
    if (!editLogicComputationOperands(QStringLiteral("编辑计算点源点"),
                                      logic.computationPoints.at(row).formula,
                                      operands)) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.computationPoints[row].operands = operands;
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    m_logicComputationPointTable->setCurrentCell(row, LogicComputationColumnOperands);
}

int MainWindow::currentLogicControlRuleIndex() const
{
    if (!m_logicControlRuleTable || !m_logicControlRuleTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_logicControlRuleTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void MainWindow::refreshLogicControlRulePage()
{
    if (!m_logicControlRuleTable) {
        return;
    }

    const int previousRow = currentLogicControlRuleIndex();
    const QList<configtool::LogicControlRule> &rules =
        m_configProjectManager.project().logicCenter.controlRules;

    m_updatingLogicControlRulePage = true;
    m_logicControlRuleTable->setRowCount(rules.size());
    for (int row = 0; row < rules.size(); ++row) {
        const configtool::LogicControlRule &rule = rules.at(row);
        const QStringList values = {
            rule.matchDeviceId,
            rule.matchDataRef,
            QString::number(rule.targets.size()),
            rule.description
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == LogicControlRuleColumnTargetCount) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            m_logicControlRuleTable->setItem(row, column, item);
        }
    }
    m_updatingLogicControlRulePage = false;

    if (previousRow >= 0 && previousRow < rules.size()) {
        m_logicControlRuleTable->selectRow(previousRow);
    } else if (!rules.isEmpty()) {
        m_logicControlRuleTable->selectRow(0);
    } else {
        refreshLogicControlTargetTable();
    }
    refreshLogicControlTargetTable();
}

void MainWindow::refreshLogicControlTargetTable()
{
    if (!m_logicControlTargetTable) {
        return;
    }

    const int ruleIndex = currentLogicControlRuleIndex();
    const QList<configtool::LogicControlRule> &rules =
        m_configProjectManager.project().logicCenter.controlRules;

    m_updatingLogicControlRulePage = true;
    if (ruleIndex < 0 || ruleIndex >= rules.size()) {
        m_logicControlTargetTable->setRowCount(0);
        m_updatingLogicControlRulePage = false;
        refreshLogicControlPreview();
        return;
    }

    const QList<configtool::LogicControlTarget> &targets = rules.at(ruleIndex).targets;
    m_logicControlTargetTable->setRowCount(targets.size());
    for (int row = 0; row < targets.size(); ++row) {
        const configtool::LogicControlTarget &target = targets.at(row);
        const QString targetType = target.targetType.trimmed().isEmpty()
            ? QStringLiteral("ctrlcmd")
            : target.targetType.trimmed().toLower();
        const QString preview = expandedLogicControlExpression(
            target.expr,
            m_logicControlPreviewValueEdit ? m_logicControlPreviewValueEdit->text() : QStringLiteral("1"));
        const QStringList values = {
            targetType,
            target.deviceId,
            target.dataRef,
            target.expr,
            preview
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == LogicControlTargetColumnPreview) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            if (isLogicControlTotalTarget(target)) {
                item->setToolTip(QStringLiteral("该目标会进入 AGC/AVC 总控分配逻辑。"));
            }
            m_logicControlTargetTable->setItem(row, column, item);
        }
    }
    m_updatingLogicControlRulePage = false;
    refreshLogicControlPreview();
}

void MainWindow::onLogicControlRuleSelectionChanged()
{
    refreshLogicControlTargetTable();
}

void MainWindow::onAddLogicControlRuleClicked()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();

    configtool::LogicControlRule rule;
    rule.matchDeviceId = QStringLiteral("device_%1").arg(logic.controlRules.size() + 1);
    rule.matchDataRef = QStringLiteral("PROT.SlrInvCtlGGIO.1.TotalP_Ctrl");
    rule.description = QStringLiteral("手工新增控制转换");

    configtool::LogicControlTarget target;
    target.deviceId = rule.matchDeviceId;
    target.dataRef = rule.matchDataRef;
    target.expr = QStringLiteral("{x}");
    target.targetType = QStringLiteral("ctrlcmd");
    rule.targets.append(target);

    logic.controlRules.append(rule);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(logic.controlRules.size() - 1);
}

void MainWindow::onCopyLogicControlRuleClicked()
{
    const int row = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要复制的控制转换规则。"));
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlRule rule = logic.controlRules.at(row);
    rule.matchDataRef += QStringLiteral("_copy");
    rule.description = rule.description.trimmed().isEmpty()
        ? QStringLiteral("复制的控制转换")
        : QStringLiteral("%1 (复制)").arg(rule.description);
    logic.controlRules.insert(row + 1, rule);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(row + 1);
}

void MainWindow::onDeleteLogicControlRuleClicked()
{
    const int row = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的控制转换规则。"));
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules.removeAt(row);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    if (!logic.controlRules.isEmpty()) {
        m_logicControlRuleTable->selectRow(qMin(row, logic.controlRules.size() - 1));
    }
}

void MainWindow::onAddLogicControlTargetClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()) {
        onAddLogicControlRuleClicked();
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget target;
    target.deviceId = logic.controlRules.at(ruleIndex).matchDeviceId;
    target.dataRef = logic.controlRules.at(ruleIndex).matchDataRef;
    target.expr = QStringLiteral("{x}");
    target.targetType = QStringLiteral("ctrlcmd");
    logic.controlRules[ruleIndex].targets.append(target);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(logic.controlRules.at(ruleIndex).targets.size() - 1);
}

void MainWindow::onDeleteLogicControlTargetClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable ? m_logicControlTargetTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || targetRow < 0 || targetRow >= logic.controlRules.at(ruleIndex).targets.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的目标动作。"));
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules[ruleIndex].targets.removeAt(targetRow);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
}

void MainWindow::onLogicControlRuleItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicControlRulePage || m_restoringConfigUndo || !item) {
        return;
    }

    const int row = item->row();
    const int column = item->column();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlRule &rule = logic.controlRules[row];
    const QString text = item->text().trimmed();
    if (column == LogicControlRuleColumnDevice) {
        rule.matchDeviceId = text;
    } else if (column == LogicControlRuleColumnPoint) {
        rule.matchDataRef = text;
    } else if (column == LogicControlRuleColumnDescription) {
        rule.description = text;
    } else {
        return;
    }

    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    if (row < m_logicControlRuleTable->rowCount()) {
        m_logicControlRuleTable->setCurrentCell(row, column);
    }
}

void MainWindow::onLogicControlTargetItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicControlRulePage || m_restoringConfigUndo || !item) {
        return;
    }

    const int ruleIndex = currentLogicControlRuleIndex();
    const int row = item->row();
    const int column = item->column();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || row < 0 || row >= logic.controlRules.at(ruleIndex).targets.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[row];
    const QString text = item->text().trimmed();
    if (column == LogicControlTargetColumnType) {
        target.targetType = text.isEmpty() ? QStringLiteral("ctrlcmd") : text.toLower();
    } else if (column == LogicControlTargetColumnDevice) {
        target.deviceId = text;
    } else if (column == LogicControlTargetColumnPoint) {
        target.dataRef = text;
    } else if (column == LogicControlTargetColumnExpr) {
        target.expr = text;
    } else {
        return;
    }

    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    if (row < m_logicControlTargetTable->rowCount()) {
        m_logicControlTargetTable->setCurrentCell(row, column);
    }
}

void MainWindow::onLogicControlRuleCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }
    if (column == LogicControlRuleColumnDevice || column == LogicControlRuleColumnPoint) {
        selectLogicControlMatchPoint(row);
    }
}

void MainWindow::onLogicControlTargetCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }
    if (column == LogicControlTargetColumnDevice || column == LogicControlTargetColumnPoint) {
        selectLogicControlTargetPoint(row);
    }
}

void MainWindow::selectLogicControlMatchPoint(int row)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setServiceTypeFilter(configtool::ModelServiceType::Control);
    dialog.setWindowTitle(QStringLiteral("选择源遥控/遥调点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules[row].matchDeviceId = point.deviceId;
    logic.controlRules[row].matchDataRef = point.dataRef;
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(row);
}

void MainWindow::selectLogicControlTargetPoint(int row)
{
    const int ruleIndex = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || row < 0 || row >= logic.controlRules.at(ruleIndex).targets.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setServiceTypeFilter(configtool::ModelServiceType::Control);
    dialog.setWindowTitle(QStringLiteral("选择目标遥控/遥调点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[row];
    target.deviceId = point.deviceId;
    target.dataRef = point.dataRef;
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(row);
}

void MainWindow::onLogicControlPreviewEdited()
{
    refreshLogicControlTargetTable();
}

void MainWindow::refreshLogicControlPreview()
{
    if (!m_logicControlPreviewLabel || !m_logicControlTargetTable) {
        return;
    }

    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable->currentRow();
    const QList<configtool::LogicControlRule> &rules =
        m_configProjectManager.project().logicCenter.controlRules;
    if (ruleIndex < 0 || ruleIndex >= rules.size()
        || targetRow < 0 || targetRow >= rules.at(ruleIndex).targets.size()) {
        m_logicControlPreviewLabel->setText(QStringLiteral("选择目标动作后显示表达式展开结果。"));
        return;
    }

    const configtool::LogicControlTarget &target = rules.at(ruleIndex).targets.at(targetRow);
    QString text = QStringLiteral("%1/%2: %3 => %4")
                       .arg(target.deviceId,
                            target.dataRef,
                            target.expr,
                            expandedLogicControlExpression(target.expr, m_logicControlPreviewValueEdit->text()));
    if (isLogicControlTotalTarget(target)) {
        text += QStringLiteral("；该目标会进入 AGC/AVC 分配逻辑");
    }
    m_logicControlPreviewLabel->setText(text);
}

void MainWindow::onLogicControlTemplateClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable ? m_logicControlTargetTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || targetRow < 0 || targetRow >= logic.controlRules.at(ruleIndex).targets.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个目标动作。"));
        return;
    }

    QString expr;
    QObject *senderObject = sender();
    if (senderObject == m_logicControlTemplateOriginalBtn) {
        expr = QStringLiteral("{x}");
    } else if (senderObject == m_logicControlTemplateInvertBtn) {
        expr = QStringLiteral("1 - {x}");
    } else if (senderObject == m_logicControlTemplateScaleBtn) {
        expr = QStringLiteral("{x} * 1.0");
    } else if (senderObject == m_logicControlTemplateFixedBtn) {
        expr = QStringLiteral("1");
    } else {
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules[ruleIndex].targets[targetRow].expr = expr;
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(targetRow);
}

void MainWindow::onInsertLogicControlRealtimeRefClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable ? m_logicControlTargetTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || targetRow < 0 || targetRow >= logic.controlRules.at(ruleIndex).targets.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个目标动作。"));
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setWindowTitle(QStringLiteral("选择实时引用点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[targetRow];
    const QString ref = QStringLiteral("{rt:%1#%2}").arg(point.deviceId, point.dataRef);
    if (target.expr.trimmed().isEmpty()) {
        target.expr = QStringLiteral("{x} + %1").arg(ref);
    } else {
        target.expr += QStringLiteral(" + %1").arg(ref);
    }

    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(targetRow);
}

void MainWindow::refreshLogicOnlineLinkPage()
{
    if (!m_logicOnlineLinkTable) {
        return;
    }

    m_updatingLogicOnlineLinkPage = true;
    const QList<configtool::LogicOnlineStatusLink> &links =
        m_configProjectManager.project().logicCenter.onlineStatusLinks;
    m_logicOnlineLinkTable->setRowCount(links.size());
    for (int row = 0; row < links.size(); ++row) {
        const configtool::LogicOnlineStatusLink &link = links.at(row);
        m_logicOnlineLinkTable->setItem(row,
                                        LogicOnlineLinkColumnDevice,
                                        new QTableWidgetItem(link.deviceId));
        m_logicOnlineLinkTable->setItem(row,
                                        LogicOnlineLinkColumnFollowDevice,
                                        new QTableWidgetItem(link.linkToDeviceId));
    }
    m_updatingLogicOnlineLinkPage = false;
}

void MainWindow::onAddLogicOnlineLinkClicked()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();

    configtool::LogicOnlineStatusLink link;
    link.deviceId = QStringLiteral("virtual_device_%1").arg(logic.onlineStatusLinks.size() + 1);
    link.linkToDeviceId = QStringLiteral("real_device_%1").arg(logic.onlineStatusLinks.size() + 1);
    logic.onlineStatusLinks.append(link);

    refreshLogicCenterOverview();
    refreshLogicOnlineLinkPage();
    m_logicOnlineLinkTable->selectRow(logic.onlineStatusLinks.size() - 1);
}

void MainWindow::onDeleteLogicOnlineLinkClicked()
{
    const int row = m_logicOnlineLinkTable ? m_logicOnlineLinkTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.onlineStatusLinks.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的在线联动。"));
        return;
    }

    pushConfigUndoSnapshot();
    logic.onlineStatusLinks.removeAt(row);
    refreshLogicCenterOverview();
    refreshLogicOnlineLinkPage();
    if (!logic.onlineStatusLinks.isEmpty()) {
        m_logicOnlineLinkTable->selectRow(qMin(row, logic.onlineStatusLinks.size() - 1));
    }
}

void MainWindow::onGenerateLogicVirtualOnlineLinksClicked()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    QList<configtool::LogicOnlineStatusLink> generatedLinks;

    for (const configtool::AgcAvcGroup &group : logic.agcAvcGroups) {
        const QString virtualDeviceId = group.virtualDeviceId.trimmed();
        if (virtualDeviceId.isEmpty() || group.devices.isEmpty()) {
            continue;
        }

        const QString followDeviceId = group.devices.first().deviceId.trimmed();
        if (followDeviceId.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const configtool::LogicOnlineStatusLink &link : logic.onlineStatusLinks) {
            if (link.deviceId == virtualDeviceId && link.linkToDeviceId == followDeviceId) {
                exists = true;
                break;
            }
        }
        for (const configtool::LogicOnlineStatusLink &link : generatedLinks) {
            if (link.deviceId == virtualDeviceId && link.linkToDeviceId == followDeviceId) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        configtool::LogicOnlineStatusLink link;
        link.deviceId = virtualDeviceId;
        link.linkToDeviceId = followDeviceId;
        generatedLinks.append(link);
    }

    if (generatedLinks.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("在线联动"),
                                 QStringLiteral("没有可生成的 AGC/AVC 虚拟设备联动。请先配置虚拟设备和南向设备，或检查是否已存在。"));
        return;
    }

    pushConfigUndoSnapshot();
    for (const configtool::LogicOnlineStatusLink &link : generatedLinks) {
        logic.onlineStatusLinks.append(link);
    }
    refreshLogicCenterOverview();
    refreshLogicOnlineLinkPage();
    statusBar()->showMessage(QStringLiteral("已生成 %1 条虚拟设备在线联动").arg(generatedLinks.size()), 5000);
}

void MainWindow::onGenerateLogicDerivedOnlineLinkClicked()
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    QStringList deviceChoices;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (!device.deviceId.trimmed().isEmpty()) {
            deviceChoices.append(deviceChoiceText(device));
        }
    }
    deviceChoices.removeDuplicates();

    if (deviceChoices.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("在线联动"), QStringLiteral("当前工程没有可选择的设备。"));
        return;
    }

    bool ok = false;
    const QString derivedChoice = QInputDialog::getItem(this,
                                                        QStringLiteral("派生设备跟随真实设备"),
                                                        QStringLiteral("被联动设备:"),
                                                        deviceChoices,
                                                        0,
                                                        false,
                                                        &ok);
    if (!ok) {
        return;
    }
    const QString followChoice = QInputDialog::getItem(this,
                                                       QStringLiteral("派生设备跟随真实设备"),
                                                       QStringLiteral("跟随设备:"),
                                                       deviceChoices,
                                                       0,
                                                       false,
                                                       &ok);
    if (!ok) {
        return;
    }

    const QString derivedDeviceId = deviceIdFromChoiceText(derivedChoice);
    const QString followDeviceId = deviceIdFromChoiceText(followChoice);
    if (derivedDeviceId.isEmpty() || followDeviceId.isEmpty()) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    for (int index = 0; index < logic.onlineStatusLinks.size(); ++index) {
        const configtool::LogicOnlineStatusLink &existing = logic.onlineStatusLinks.at(index);
        if (existing.deviceId == derivedDeviceId && existing.linkToDeviceId == followDeviceId) {
            refreshLogicOnlineLinkPage();
            m_logicOnlineLinkTable->selectRow(index);
            statusBar()->showMessage(QStringLiteral("该在线联动已存在"), 5000);
            return;
        }
    }

    pushConfigUndoSnapshot();
    configtool::LogicOnlineStatusLink link;
    link.deviceId = derivedDeviceId;
    link.linkToDeviceId = followDeviceId;
    logic.onlineStatusLinks.append(link);
    refreshLogicCenterOverview();
    refreshLogicOnlineLinkPage();
    m_logicOnlineLinkTable->selectRow(logic.onlineStatusLinks.size() - 1);
}

void MainWindow::onLogicOnlineLinkItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicOnlineLinkPage || m_restoringConfigUndo || !item) {
        return;
    }

    const int row = item->row();
    const int column = item->column();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.onlineStatusLinks.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicOnlineStatusLink &link = logic.onlineStatusLinks[row];
    const QString text = item->text().trimmed();
    if (column == LogicOnlineLinkColumnDevice) {
        link.deviceId = text;
    } else if (column == LogicOnlineLinkColumnFollowDevice) {
        link.linkToDeviceId = text;
    } else {
        return;
    }

    refreshLogicCenterOverview();
    refreshLogicOnlineLinkPage();
    if (row < m_logicOnlineLinkTable->rowCount()) {
        m_logicOnlineLinkTable->setCurrentCell(row, column);
    }
}

void MainWindow::onLogicOnlineLinkCellDoubleClicked(int row, int column)
{
    selectLogicOnlineLinkDevice(row, column);
}

void MainWindow::selectLogicOnlineLinkDevice(int row, int column)
{
    if (column != LogicOnlineLinkColumnDevice && column != LogicOnlineLinkColumnFollowDevice) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.onlineStatusLinks.size()) {
        return;
    }

    QStringList deviceChoices;
    for (const configtool::ProtocolDeviceInstance &device : m_configProjectManager.project().devices) {
        if (!device.deviceId.trimmed().isEmpty()) {
            deviceChoices.append(deviceChoiceText(device));
        }
    }
    deviceChoices.removeDuplicates();

    if (deviceChoices.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("选择设备"), QStringLiteral("当前工程没有可选择的设备。"));
        return;
    }

    const QString currentDeviceId = column == LogicOnlineLinkColumnDevice
        ? logic.onlineStatusLinks.at(row).deviceId
        : logic.onlineStatusLinks.at(row).linkToDeviceId;
    int currentIndex = 0;
    for (int index = 0; index < deviceChoices.size(); ++index) {
        if (deviceIdFromChoiceText(deviceChoices.at(index)) == currentDeviceId) {
            currentIndex = index;
            break;
        }
    }

    bool ok = false;
    const QString choice = QInputDialog::getItem(this,
                                                 QStringLiteral("选择设备"),
                                                 column == LogicOnlineLinkColumnDevice
                                                     ? QStringLiteral("被联动设备:")
                                                     : QStringLiteral("跟随设备:"),
                                                 deviceChoices,
                                                 currentIndex,
                                                 false,
                                                 &ok);
    if (!ok) {
        return;
    }

    const QString selectedDeviceId = deviceIdFromChoiceText(choice);
    if (selectedDeviceId.isEmpty()) {
        return;
    }

    pushConfigUndoSnapshot();
    if (column == LogicOnlineLinkColumnDevice) {
        logic.onlineStatusLinks[row].deviceId = selectedDeviceId;
    } else {
        logic.onlineStatusLinks[row].linkToDeviceId = selectedDeviceId;
    }

    refreshLogicCenterOverview();
    refreshLogicOnlineLinkPage();
    m_logicOnlineLinkTable->setCurrentCell(row, column);
}

void MainWindow::onGenerateLogicAgcAvcTotalPClicked()
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    const QString outputDeviceId = group->virtualDeviceId.trimmed();
    if (outputDeviceId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成 TotalP"), QStringLiteral("请先填写虚拟设备 ID。"));
        return;
    }

    const QList<configtool::LogicOperand> operands =
        collectLogicTemplateOperands(configtool::ModelServiceType::Measurement,
                                     QStringLiteral("生成 TotalP"),
                                     1);
    if (operands.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString outputDataRef = QInputDialog::getText(this,
                                                        QStringLiteral("生成 TotalP"),
                                                        QStringLiteral("输出点号:"),
                                                        QLineEdit::Normal,
                                                        QStringLiteral("TotalP"),
                                                        &ok).trimmed();
    if (!ok) {
        return;
    }

    configtool::LogicComputationTemplateRequest request;
    request.type = configtool::LogicComputationTemplateType::Sum;
    request.outputDeviceId = outputDeviceId;
    request.outputDataRef = outputDataRef;
    request.operands = operands;
    request.dropOperands = true;
    request.description = QStringLiteral("AGC/AVC 模板生成: TotalP");
    upsertLogicTemplatePoint(request);
}

void MainWindow::onGenerateLogicAgcAvcTotalQClicked()
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    const QString outputDeviceId = group->virtualDeviceId.trimmed();
    if (outputDeviceId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成 TotalQ"), QStringLiteral("请先填写虚拟设备 ID。"));
        return;
    }

    const QList<configtool::LogicOperand> operands =
        collectLogicTemplateOperands(configtool::ModelServiceType::Measurement,
                                     QStringLiteral("生成 TotalQ"),
                                     1);
    if (operands.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString outputDataRef = QInputDialog::getText(this,
                                                        QStringLiteral("生成 TotalQ"),
                                                        QStringLiteral("输出点号:"),
                                                        QLineEdit::Normal,
                                                        QStringLiteral("TotalQ"),
                                                        &ok).trimmed();
    if (!ok) {
        return;
    }

    configtool::LogicComputationTemplateRequest request;
    request.type = configtool::LogicComputationTemplateType::Sum;
    request.outputDeviceId = outputDeviceId;
    request.outputDataRef = outputDataRef;
    request.operands = operands;
    request.dropOperands = true;
    request.description = QStringLiteral("AGC/AVC 模板生成: TotalQ");
    upsertLogicTemplatePoint(request);
}

void MainWindow::onGenerateLogicAgcAvcCosClicked()
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    const QString outputDeviceId = group->virtualDeviceId.trimmed();
    if (outputDeviceId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成 Cos"), QStringLiteral("请先填写虚拟设备 ID。"));
        return;
    }

    bool ok = false;
    const QString outputDataRef = QInputDialog::getText(this,
                                                        QStringLiteral("生成 Cos"),
                                                        QStringLiteral("输出点号:"),
                                                        QLineEdit::Normal,
                                                        QStringLiteral("Cos"),
                                                        &ok).trimmed();
    if (!ok) {
        return;
    }

    const QString totalPDataRef = QInputDialog::getText(this,
                                                        QStringLiteral("生成 Cos"),
                                                        QStringLiteral("有功源点号:"),
                                                        QLineEdit::Normal,
                                                        QStringLiteral("TotalP"),
                                                        &ok).trimmed();
    if (!ok) {
        return;
    }

    const QString totalQDataRef = QInputDialog::getText(this,
                                                        QStringLiteral("生成 Cos"),
                                                        QStringLiteral("无功源点号:"),
                                                        QLineEdit::Normal,
                                                        QStringLiteral("TotalQ"),
                                                        &ok).trimmed();
    if (!ok) {
        return;
    }

    configtool::LogicOperand totalP;
    totalP.deviceId = outputDeviceId;
    totalP.dataRef = totalPDataRef;
    configtool::LogicOperand totalQ;
    totalQ.deviceId = outputDeviceId;
    totalQ.dataRef = totalQDataRef;

    configtool::LogicComputationTemplateRequest request;
    request.type = configtool::LogicComputationTemplateType::PowerFactor;
    request.outputDeviceId = outputDeviceId;
    request.outputDataRef = outputDataRef;
    request.operands = {totalP, totalQ};
    request.dropOperands = true;
    request.description = QStringLiteral("AGC/AVC 模板生成: Cos");
    upsertLogicTemplatePoint(request);
}

void MainWindow::onLogicAgcAvcBasicEdited()
{
    if (m_updatingLogicAgcAvcPage || m_restoringConfigUndo) {
        return;
    }

    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    pushConfigUndoSnapshot();
    group->groupId = m_logicAgcAvcGroupIdEdit->text().trimmed();
    group->virtualDeviceId = m_logicAgcAvcVirtualDeviceIdEdit->text().trimmed();
    group->measurementScale.totalP = m_logicMeasurementTotalPEdit->value();
    group->measurementScale.totalQ = m_logicMeasurementTotalQEdit->value();

    group->gateReverse.enable = m_logicGateEnableReverseCheck->isChecked();
    group->gateReverse.distant = m_logicGateDistantReverseCheck->isChecked();
    group->gateReverse.lock = m_logicGateLockReverseCheck->isChecked();
    group->gateReverse.uplock = m_logicGateUplockReverseCheck->isChecked();
    group->gateReverse.downlock = m_logicGateDownlockReverseCheck->isChecked();
    group->gateReverse.openloop = m_logicGateOpenloopReverseCheck->isChecked();

    group->agcFollow.enable = m_logicAgcFollowEnableCheck->isChecked();
    group->agcFollow.periodMs = m_logicAgcFollowPeriodEdit->value();
    group->agcFollow.step = m_logicAgcFollowStepEdit->value();
    group->agcFollow.tolerance = m_logicAgcFollowToleranceEdit->value();
    group->avcFollow.enable = m_logicAvcFollowEnableCheck->isChecked();
    group->avcFollow.periodMs = m_logicAvcFollowPeriodEdit->value();
    group->avcFollow.step = m_logicAvcFollowStepEdit->value();
    group->avcFollow.tolerance = m_logicAvcFollowToleranceEdit->value();

    refreshLogicCenterOverview();
}

void MainWindow::onLogicAgcAvcDeviceItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicAgcAvcPage || m_restoringConfigUndo || !item) {
        return;
    }

    configtool::AgcAvcGroup *group = currentLogicAgcAvcGroup();
    if (!group || item->row() < 0 || item->row() >= group->devices.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::AgcAvcDevice &device = group->devices[item->row()];
    const QString text = item->text().trimmed();
    switch (item->column()) {
    case LogicAgcAvcDeviceColumnDeviceId:
        device.deviceId = text;
        if (device.onlineDeviceId.trimmed().isEmpty()) {
            device.onlineDeviceId = text;
        }
        break;
    case LogicAgcAvcDeviceColumnCtrlP:
        device.ctrlDataRefP = text;
        break;
    case LogicAgcAvcDeviceColumnCtrlQ:
        device.ctrlDataRefQ = text;
        break;
    case LogicAgcAvcDeviceColumnOnlineDevice:
        device.onlineDeviceId = text;
        break;
    case LogicAgcAvcDeviceColumnOnlinePoint:
        device.onlineDataRef = text;
        break;
    case LogicAgcAvcDeviceColumnOnlineOk:
        device.onlineOkValue = uiTextToInt(text, device.onlineOkValue);
        break;
    case LogicAgcAvcDeviceColumnPMin:
        device.pMin = uiTextToDouble(text, device.pMin);
        break;
    case LogicAgcAvcDeviceColumnPMax:
        device.pMax = uiTextToDouble(text, device.pMax);
        break;
    case LogicAgcAvcDeviceColumnQMin:
        device.qMin = uiTextToDouble(text, device.qMin);
        break;
    case LogicAgcAvcDeviceColumnQMax:
        device.qMax = uiTextToDouble(text, device.qMax);
        break;
    case LogicAgcAvcDeviceColumnScaleP:
        device.scaleP = uiTextToDouble(text, device.scaleP);
        break;
    case LogicAgcAvcDeviceColumnScaleQ:
        device.scaleQ = uiTextToDouble(text, device.scaleQ);
        break;
    default:
        break;
    }

    refreshLogicCenterOverview();
}

void MainWindow::onAddLogicAgcAvcDeviceClicked()
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    pushConfigUndoSnapshot();
    configtool::AgcAvcDevice device;
    device.deviceId = QStringLiteral("device_%1").arg(group->devices.size() + 1);
    device.onlineDeviceId = device.deviceId;
    device.ctrlDataRefP = QStringLiteral("PROT.SlrInvCtlGGIO.1.TotalP_Ctrl");
    device.ctrlDataRefQ = QStringLiteral("PROT.SlrInvCtlGGIO.1.TotalQ_Ctrl");
    device.pMax = 1000.0;
    device.pMin = 0.0;
    device.qMax = 1000.0;
    device.qMin = -1000.0;
    group->devices.append(device);
    refreshLogicAgcAvcPage();
    refreshLogicCenterOverview();
    m_logicAgcAvcDeviceTable->selectRow(group->devices.size() - 1);
}

void MainWindow::onLogicAgcAvcDeviceCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }

    m_logicAgcAvcDeviceTable->setCurrentCell(row, column);
    if (column == LogicAgcAvcDeviceColumnCtrlP) {
        selectLogicAgcAvcPointForColumn(LogicAgcAvcDeviceColumnCtrlP,
                                        configtool::ModelServiceType::Control,
                                        false);
    } else if (column == LogicAgcAvcDeviceColumnCtrlQ) {
        selectLogicAgcAvcPointForColumn(LogicAgcAvcDeviceColumnCtrlQ,
                                        configtool::ModelServiceType::Control,
                                        false);
    } else if (column == LogicAgcAvcDeviceColumnOnlinePoint) {
        selectLogicAgcAvcPointForColumn(LogicAgcAvcDeviceColumnOnlinePoint,
                                        configtool::ModelServiceType::Status,
                                        true);
    }
}

void MainWindow::onDeleteLogicAgcAvcDeviceClicked()
{
    configtool::AgcAvcGroup *group = currentLogicAgcAvcGroup();
    if (!group) {
        return;
    }
    const int row = m_logicAgcAvcDeviceTable->currentRow();
    if (row < 0 || row >= group->devices.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的南向设备。"));
        return;
    }

    pushConfigUndoSnapshot();
    group->devices.removeAt(row);
    refreshLogicAgcAvcPage();
    refreshLogicCenterOverview();
}

void MainWindow::selectLogicAgcAvcPointForColumn(int dataRefColumn,
                                                 configtool::ModelServiceType preferredType,
                                                 bool updateOnlineDevice)
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    int row = m_logicAgcAvcDeviceTable->currentRow();
    if (row < 0) {
        onAddLogicAgcAvcDeviceClicked();
        row = m_logicAgcAvcDeviceTable->currentRow();
    }
    if (row < 0 || row >= group->devices.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setServiceTypeFilter(preferredType);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::AgcAvcDevice &device = group->devices[row];
    device.deviceId = point.deviceId;
    if (dataRefColumn == LogicAgcAvcDeviceColumnCtrlP) {
        device.ctrlDataRefP = point.dataRef;
    } else if (dataRefColumn == LogicAgcAvcDeviceColumnCtrlQ) {
        device.ctrlDataRefQ = point.dataRef;
    } else if (dataRefColumn == LogicAgcAvcDeviceColumnOnlinePoint) {
        device.onlineDataRef = point.dataRef;
        if (updateOnlineDevice) {
            device.onlineDeviceId = point.deviceId;
        }
    }
    if (device.onlineDeviceId.trimmed().isEmpty()) {
        device.onlineDeviceId = device.deviceId;
    }

    refreshLogicAgcAvcPage();
    refreshLogicCenterOverview();
    m_logicAgcAvcDeviceTable->setCurrentCell(row, dataRefColumn);
}

int MainWindow::currentConfigModelIndex() const
{
    if (!m_configModelTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_configModelTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

int MainWindow::currentConfigDeviceIndex() const
{
    if (!m_configDeviceTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_configDeviceTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QPair<int, int> MainWindow::currentModelPointLocation() const
{
    if (!m_modelPointsTable->selectionModel()) {
        return qMakePair(-1, -1);
    }

    int selectedRow = m_modelPointsTable->currentRow();
    const QModelIndexList indexes = m_modelPointsTable->selectionModel()->selectedIndexes();
    if (!indexes.isEmpty()) {
        selectedRow = indexes.first().row();
    }

    if (selectedRow < 0) {
        return qMakePair(-1, -1);
    }

    QTableWidgetItem *categoryItem = m_modelPointsTable->item(selectedRow, 0);
    if (!categoryItem) {
        return qMakePair(-1, -1);
    }

    return qMakePair(categoryItem->data(Qt::UserRole).toInt(),
                     categoryItem->data(Qt::UserRole + 1).toInt());
}

QSet<QString> MainWindow::duplicateDataRefsForModel(const configtool::ModelTemplate &model) const
{
    QSet<QString> seenRefs;
    QSet<QString> duplicateRefs;

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            const QString ref = point.dataRef();
            if (ref.isEmpty()) {
                continue;
            }

            if (seenRefs.contains(ref)) {
                duplicateRefs.insert(ref);
            } else {
                seenRefs.insert(ref);
            }
        }
    }

    return duplicateRefs;
}

QSet<QString> MainWindow::duplicateBindingAddresses(const configtool::ProtocolDeviceInstance &device) const
{
    QSet<QString> seenAddresses;
    QSet<QString> duplicateAddresses;

    for (const configtool::PointBinding &binding : device.bindings) {
        if (!binding.enabled) {
            continue;
        }

        const QString address = binding.address.trimmed();
        if (address.isEmpty()) {
            continue;
        }

        if (seenAddresses.contains(address)) {
            duplicateAddresses.insert(address);
        } else {
            seenAddresses.insert(address);
        }
    }

    return duplicateAddresses;
}
