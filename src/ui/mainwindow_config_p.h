#pragma once

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
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
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
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace cepb_config_helpers {

inline constexpr int Iec104BindingColumnCount = 6;
inline constexpr int ModbusBindingColumnCount = 12;
inline constexpr int ModbusColumnEnabled = 0;
inline constexpr int ModbusColumnKind = 1;
inline constexpr int ModbusColumnDataRef = 2;
inline constexpr int ModbusColumnDescription = 3;
inline constexpr int ModbusColumnFunCode = 4;
inline constexpr int ModbusColumnRegister = 5;
inline constexpr int ModbusColumnDataType = 6;
inline constexpr int ModbusColumnScale = 7;
inline constexpr int ModbusColumnGroupNo = 8;
inline constexpr int ModbusColumnEntryNo = 9;
inline constexpr int ModbusColumnDataIndex = 10;
inline constexpr int ModbusColumnSelfSignal = 11;
inline constexpr int LogicAgcAvcDeviceColumnDeviceId = 0;
inline constexpr int LogicAgcAvcDeviceColumnCtrlP = 1;
inline constexpr int LogicAgcAvcDeviceColumnCtrlQ = 2;
inline constexpr int LogicAgcAvcDeviceColumnOnlineDevice = 3;
inline constexpr int LogicAgcAvcDeviceColumnOnlinePoint = 4;
inline constexpr int LogicAgcAvcDeviceColumnOnlineOk = 5;
inline constexpr int LogicAgcAvcDeviceColumnPMin = 6;
inline constexpr int LogicAgcAvcDeviceColumnPMax = 7;
inline constexpr int LogicAgcAvcDeviceColumnQMin = 8;
inline constexpr int LogicAgcAvcDeviceColumnQMax = 9;
inline constexpr int LogicAgcAvcDeviceColumnScaleP = 10;
inline constexpr int LogicAgcAvcDeviceColumnScaleQ = 11;
inline constexpr int ModelPointColumnDragHandle = 0;
inline constexpr int ModelPointColumnNorthVisible = 1;
inline constexpr int ModelPointColumnCategory = 2;
inline constexpr int ModelPointColumnDoName = 3;
inline constexpr int ModelPointColumnDescription = 4;
inline constexpr int ModelPointColumnLdName = 5;
inline constexpr int ModelPointColumnLnType = 6;
inline constexpr int ModelPointColumnLnInst = 7;
inline constexpr int ModelPointColumnDataRef = 8;
inline constexpr int ModelPointColumnDataType = 9;
inline constexpr int ModelPointColumnUnit = 10;
inline constexpr int LogicComputationColumnDragHandle = 0;
inline constexpr int LogicComputationColumnOutputDevice = 1;
inline constexpr int LogicComputationColumnOutputPoint = 2;
inline constexpr int Iec101PointColumnDragHandle = 0;
inline constexpr int Iec101PointColumnEnabled = 1;
inline constexpr int Iec101PointColumnDeviceId = 2;
inline constexpr int Iec101PointColumnDataRef = 3;
inline constexpr int Iec101PointColumnDescription = 4;
inline constexpr int Iec101PointColumnAddress = 5;
inline constexpr int Iec101PointColumnDeadzoneType = 6;
inline constexpr int Iec101PointColumnDeadzone = 7;
inline constexpr int LogicComputationColumnFormula = 3;
inline constexpr int LogicComputationColumnDropOperands = 4;
inline constexpr int LogicComputationColumnOperands = 5;
inline constexpr int LogicComputationColumnDescription = 6;
inline constexpr int LogicControlRuleColumnDevice = 0;
inline constexpr int LogicControlRuleColumnPoint = 1;
inline constexpr int LogicControlRuleColumnTargetCount = 2;
inline constexpr int LogicControlRuleColumnDescription = 3;
inline constexpr int LogicControlTargetColumnType = 0;
inline constexpr int LogicControlTargetColumnDevice = 1;
inline constexpr int LogicControlTargetColumnPoint = 2;
inline constexpr int LogicControlTargetColumnExpr = 3;
inline constexpr int LogicControlTargetColumnPreview = 4;
inline constexpr int LogicOnlineLinkColumnDevice = 0;
inline constexpr int LogicOnlineLinkColumnFollowDevice = 1;
inline constexpr int ConfigIssueRoleTargetType = Qt::UserRole + 1;
inline constexpr int ConfigIssueRoleTargetKey = Qt::UserRole + 2;
inline constexpr const char *ModelEditorOriginalModelIdProperty = "originalModelId";
inline constexpr const char *DeviceEditorOriginalDeviceIdProperty = "originalDeviceId";

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

inline QString iec104ChannelKey(const configtool::ProtocolDeviceInstance &device)
{
    return QStringLiteral("%1|%2|%3")
        .arg(device.transport.ip.trimmed().toLower(),
             device.transport.port.trimmed(),
             device.transport.stationAddress.trimmed());
}

inline QString iec104ChannelDisplayName(const configtool::ProtocolDeviceInstance &device)
{
    return QStringLiteral("IP=%1，端口=%2，104公共地址=%3")
        .arg(device.transport.ip.trimmed(),
             device.transport.port.trimmed(),
             device.transport.stationAddress.trimmed());
}

inline QHash<QString, QSet<QString>> duplicateIec104BindingAddressesByChannel(
    const QList<configtool::ProtocolDeviceInstance> &devices)
{
    QHash<QString, QSet<QString>> seenAddressesByChannel;
    QHash<QString, QSet<QString>> duplicateAddressesByChannel;

    for (const configtool::ProtocolDeviceInstance &device : devices) {
        if (device.protocol != configtool::ProtocolType::Iec104) {
            continue;
        }

        const QString channelKey = iec104ChannelKey(device);
        QSet<QString> &seenAddresses = seenAddressesByChannel[channelKey];
        QSet<QString> &duplicateAddresses = duplicateAddressesByChannel[channelKey];

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
    }

    return duplicateAddressesByChannel;
}

inline QString modbusTcpChannelKey(const configtool::ProtocolDeviceInstance &device)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(QStringLiteral("TCP"),
             device.transport.ip.trimmed().toLower(),
             device.transport.port.trimmed(),
             device.transport.stationAddress.trimmed());
}

inline QString modbusTcpChannelDisplayName(const configtool::ProtocolDeviceInstance &device)
{
    return QStringLiteral("IP=%1，端口=%2，协议地址=%3")
        .arg(device.transport.ip.trimmed(),
             device.transport.port.trimmed(),
             device.transport.stationAddress.trimmed());
}

inline QHash<QString, QSet<QString>> duplicateModbusRegisterAddressesByTcpChannel(
    const QList<configtool::ProtocolDeviceInstance> &devices)
{
    QHash<QString, QSet<QString>> seenAddressesByChannel;
    QHash<QString, QSet<QString>> duplicateAddressesByChannel;

    for (const configtool::ProtocolDeviceInstance &device : devices) {
        if (!isModbusDevice(device)) {
            continue;
        }

        const QString type = device.transport.protocolOptions
                                 .value(QStringLiteral("type"))
                                 .toString(QStringLiteral("TCP"))
                                 .trimmed()
                                 .toUpper();
        if (type != QStringLiteral("TCP")) {
            continue;
        }

        const QString channelKey = modbusTcpChannelKey(device);
        QSet<QString> &seenAddresses = seenAddressesByChannel[channelKey];
        QSet<QString> &duplicateAddresses = duplicateAddressesByChannel[channelKey];

        for (const configtool::PointBinding &binding : device.bindings) {
            if (!binding.enabled || !binding.extensions.contains(QStringLiteral("modbusRegisterAddress"))) {
                continue;
            }

            const QJsonValue value = binding.extensions.value(QStringLiteral("modbusRegisterAddress"));
            bool ok = value.isDouble();
            const int registerAddress = value.isDouble()
                ? value.toInt()
                : value.toString().trimmed().toInt(&ok, 0);
            if (!ok) {
                continue;
            }

            const QString address = QString::number(registerAddress);
            if (seenAddresses.contains(address)) {
                duplicateAddresses.insert(address);
            } else {
                seenAddresses.insert(address);
            }
        }
    }

    return duplicateAddressesByChannel;
}

inline bool selfSignalFlagChecked(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("1")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("on");
}

inline QString uiJsonValueToString(const QJsonValue &value)
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

inline int modelPointFilterTabIndex(configtool::ModelServiceType type)
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

inline configtool::ModelServiceType modelServiceTypeFromTabIndex(int index)
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

inline configtool::PointSignalType signalTypeForModelService(configtool::ModelServiceType type)
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

inline QString defaultLnTypeForModelService(configtool::ModelServiceType type)
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

inline QStringList modelDataTypeOptions()
{
    return {
        QStringLiteral("Boolean"),
        QStringLiteral("DBool"),
        QStringLiteral("Int"),
        QStringLiteral("Uint"),
        QStringLiteral("Long"),
        QStringLiteral("ULong"),
        QStringLiteral("Float"),
        QStringLiteral("Double"),
        QStringLiteral("String"),
        QStringLiteral("OcterString"),
        QStringLiteral("Hex")
    };
}

inline QString defaultModelDataTypeForService(configtool::ModelServiceType type)
{
    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return QStringLiteral("Float");
    case configtool::ModelServiceType::Status:
    case configtool::ModelServiceType::Control:
        return QStringLiteral("Boolean");
    }

    return QStringLiteral("Float");
}

inline QString normalizedModelDataType(configtool::ModelServiceType type, const QString &dataType)
{
    const QString trimmed = dataType.trimmed();
    if (trimmed.compare(QStringLiteral("Bool"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Boolean");
    }

    const QStringList options = modelDataTypeOptions();
    for (const QString &option : options) {
        if (option.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return option;
        }
    }
    return defaultModelDataTypeForService(type);
}

inline void updateModelPointControlKind(configtool::PointTemplate &point)
{
    if (point.category != configtool::ModelServiceType::Control) {
        point.controlKind = configtool::ControlKind::None;
        return;
    }

    const QString dataType = point.dataType.trimmed().toLower();
    point.controlKind = dataType == QStringLiteral("boolean")
        || dataType == QStringLiteral("dbool")
        ? configtool::ControlKind::RemoteControl
        : configtool::ControlKind::RemoteAdjust;
}

inline QStringList modbusDataTypeOptionsForKind(const QString &kind)
{
    const QString normalizedKind = normalizedModbusKind(kind);
    if (normalizedKind == QStringLiteral("yx")) {
        return {QStringLiteral("BIT"), QStringLiteral("POCKETBIT")};
    }
    if (normalizedKind == QStringLiteral("yt")) {
        return {
            QStringLiteral("WORD"),
            QStringLiteral("FLOAT"),
            QStringLiteral("FLOAT_L"),
            QStringLiteral("DWORD"),
            QStringLiteral("DWORD_L")
        };
    }
    if (normalizedKind == QStringLiteral("yk")) {
        return {QStringLiteral("WORD")};
    }
    return {
        QStringLiteral("WORD"),
        QStringLiteral("UNSIGNED_WORD"),
        QStringLiteral("FLOAT"),
        QStringLiteral("FLOAT_L"),
        QStringLiteral("DWORD"),
        QStringLiteral("DWORD_L")
    };
}

inline QString defaultModbusDataTypeForKind(const QString &kind)
{
    return modbusDataTypeOptionsForKind(kind).value(0, QStringLiteral("WORD"));
}

inline QString normalizedModbusDataTypeForKind(const QString &kind, const QString &dataType)
{
    const QString trimmed = dataType.trimmed();
    const QStringList options = modbusDataTypeOptionsForKind(kind);
    for (const QString &option : options) {
        if (option.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return option;
        }
    }
    return defaultModbusDataTypeForKind(kind);
}

inline QStringList splitClipboardLine(const QString &line)
{
    return line.split('\t');
}

inline QString deviceChoiceText(const configtool::ProtocolDeviceInstance &device)
{
    const QString description = device.deviceDesc.trimmed();
    return description.isEmpty()
        ? device.deviceId
        : QStringLiteral("%1 - %2").arg(device.deviceId, description);
}

inline QString deviceIdFromChoiceText(const QString &text)
{
    return text.section(QStringLiteral(" - "), 0, 0).trimmed();
}

inline QString modelChoiceText(const configtool::ModelTemplate &model)
{
    const QString name = model.displayName.trimmed();
    return name.isEmpty()
        ? model.modelId
        : QStringLiteral("%1 - %2").arg(model.modelId, name);
}

inline QString modelIdFromChoiceText(const QString &text)
{
    return text.section(QStringLiteral(" - "), 0, 0).trimmed();
}

inline QString pointShortNameFromDataRef(const QString &dataRef)
{
    return dataRef.section(QLatin1Char('.'), -1).trimmed();
}

inline QString pointShortName(const configtool::PointTemplate &point)
{
    const QString name = point.name.trimmed();
    if (!name.isEmpty()) {
        return name;
    }
    const QString doName = point.doName.trimmed();
    return doName.isEmpty() ? pointShortNameFromDataRef(point.dataRef()) : doName;
}

inline const configtool::ModelTemplate *findModelById(const configtool::ConfigProject &project,
                                               const QString &modelId)
{
    for (const configtool::ModelTemplate &model : project.models) {
        if (model.modelId == modelId) {
            return &model;
        }
    }
    return nullptr;
}

inline const configtool::ProtocolDeviceInstance *findDeviceById(const configtool::ConfigProject &project,
                                                         const QString &deviceId)
{
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.deviceId == deviceId) {
            return &device;
        }
    }
    return nullptr;
}

inline QString remoteShellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(quoted);
}

inline QString remotePathJoin(const QString &baseDir, const QString &relativePath)
{
    QString base = baseDir.trimmed();
    while (base.endsWith(QLatin1Char('/')) && base.size() > 1) {
        base.chop(1);
    }
    return QStringLiteral("%1/%2").arg(base, relativePath);
}

inline QStringList configTransferPathList()
{
    return {
        QStringLiteral("cepiec104/model"),
        QStringLiteral("cepiec104/dev"),
        QStringLiteral("cepmodbus/model"),
        QStringLiteral("cepmodbus/dev"),
        QStringLiteral("cepmodbus/etc"),
        QStringLiteral("cepLogicCenter/etc"),
        QStringLiteral("IEC101ServiceChannel/config")
    };
}

inline QList<QStringList> parseClipboardTable(const QString &text)
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

inline QModelIndexList sortedEditableTargetIndexes(QTableWidget *table)
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

inline QString configIssueSeverityText(configtool::ConfigIssueSeverity severity)
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

inline QColor configIssueSeverityColor(configtool::ConfigIssueSeverity severity)
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

inline QString importIssueSeverityText(configtool::ImportIssueSeverity severity)
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

inline QColor importIssueSeverityColor(configtool::ImportIssueSeverity severity)
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

inline int importIssueProblemCount(const QList<configtool::ImportIssue> &issues)
{
    int count = 0;
    for (const configtool::ImportIssue &issue : issues) {
        if (issue.severity != configtool::ImportIssueSeverity::Info) {
            ++count;
        }
    }
    return count;
}

inline configtool::ImportIssueSeverity importSeverityForConfigIssue(configtool::ConfigIssueSeverity severity)
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

inline QString formatConfigIssueForIssueTable(const configtool::ConfigIssue &issue)
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

inline void appendConfigIssueForIssueTable(const configtool::ConfigIssue &issue,
                                    const QString &filePath,
                                    QList<configtool::ImportIssue> &issues)
{
    configtool::ImportIssue importIssue;
    importIssue.severity = importSeverityForConfigIssue(issue.severity);
    importIssue.filePath = filePath;
    importIssue.message = formatConfigIssueForIssueTable(issue);
    issues.append(importIssue);
}

inline bool resolveLogicIssueTarget(const QString &message, QString *targetType, QString *targetKey)
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

inline QString doubleToUiText(double value)
{
    return QString::number(value, 'g', 12);
}

inline double uiTextToDouble(const QString &text, double fallback)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    return ok ? value : fallback;
}

inline int uiTextToInt(const QString &text, int fallback)
{
    bool ok = false;
    const int value = text.trimmed().toInt(&ok);
    return ok ? value : fallback;
}

inline bool isLogicControlTotalTarget(const configtool::LogicControlTarget &target)
{
    if (target.targetType.trimmed().compare(QStringLiteral("ctrlcmd"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    return target.dataRef.contains(QStringLiteral("TotalP_Ctrl"), Qt::CaseInsensitive)
        || target.dataRef.contains(QStringLiteral("TotalQ_Ctrl"), Qt::CaseInsensitive);
}

inline QString expandedLogicControlExpression(const QString &expr, const QString &xValue)
{
    QString expanded = expr;
    expanded.replace(QStringLiteral("{x}"), xValue.trimmed().isEmpty() ? QStringLiteral("0") : xValue.trimmed());
    static const QRegularExpression realtimeRefPattern(QStringLiteral("\\{rt:([^{}]+)\\}"));
    return expanded.replace(realtimeRefPattern, QStringLiteral("<实时:$1>"));
}

// formulaOperandCount was originally defined in the middle of the file;
// it is used by logic template functions so it lives here.
inline int formulaOperandCount(const QString &formula)
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

} // namespace cepb_config_helpers
