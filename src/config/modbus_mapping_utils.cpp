#include "config/modbus_mapping_utils.h"

#include <QJsonValue>

namespace configtool {

bool isModbusDevice(const ProtocolDeviceInstance &device)
{
    return device.protocol == ProtocolType::Modbus
        || device.appType.compare(QStringLiteral("South_Modbus"), Qt::CaseInsensitive) == 0
        || device.appType.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0;
}

int modbusTypeRegisterCount(const QString &dataType)
{
    const QString normalized = dataType.trimmed().toUpper();
    if (normalized == QStringLiteral("DWORD")
        || normalized == QStringLiteral("DWORD_L")
        || normalized == QStringLiteral("FLOAT")) {
        return 2;
    }
    if (normalized == QStringLiteral("FLOAT_L")) {
        return 4;
    }
    return 1;
}

int modbusMaxReadQuantity(int funCode)
{
    if (funCode == 1 || funCode == 2) {
        return 2000;
    }
    if (funCode == 3 || funCode == 4) {
        return 125;
    }
    return 125;
}

QString normalizedModbusKind(const QString &value,
                             const QString &fallback)
{
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("yx") || lower == QStringLiteral("yc")
        || lower == QStringLiteral("yk") || lower == QStringLiteral("yt")) {
        return lower;
    }
    const QString trimmed = value.trimmed();
    if (trimmed == QStringLiteral("遥信")) {
        return QStringLiteral("yx");
    }
    if (trimmed == QStringLiteral("遥测")) {
        return QStringLiteral("yc");
    }
    if (trimmed == QStringLiteral("遥控")) {
        return QStringLiteral("yk");
    }
    if (trimmed == QStringLiteral("遥调")) {
        return QStringLiteral("yt");
    }
    return fallback;
}

QString modbusKindDisplayName(const QString &kind)
{
    const QString normalized = normalizedModbusKind(kind);
    if (normalized == QStringLiteral("yx")) {
        return QStringLiteral("遥信");
    }
    if (normalized == QStringLiteral("yk")) {
        return QStringLiteral("遥控");
    }
    if (normalized == QStringLiteral("yt")) {
        return QStringLiteral("遥调");
    }
    return QStringLiteral("遥测");
}

ModbusPointKind modbusKindFromString(const QString &value)
{
    const QString kind = normalizedModbusKind(value);
    if (kind == QStringLiteral("yx")) {
        return ModbusPointKind::Yx;
    }
    if (kind == QStringLiteral("yk")) {
        return ModbusPointKind::Yk;
    }
    if (kind == QStringLiteral("yt")) {
        return ModbusPointKind::Yt;
    }
    return ModbusPointKind::Yc;
}

bool isModbusSetKind(const QString &kind)
{
    const QString normalized = normalizedModbusKind(kind);
    return normalized == QStringLiteral("yk") || normalized == QStringLiteral("yt");
}

QString modbusBindingKind(const PointBinding &binding)
{
    return normalizedModbusKind(binding.extensions.value(QStringLiteral("modbusKind")).toString());
}

int modbusBindingInt(const PointBinding &binding,
                     const QString &key,
                     int fallback)
{
    const QJsonValue value = binding.extensions.value(key);
    if (value.isDouble()) {
        return value.toInt();
    }

    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok, 0);
    return ok ? parsed : fallback;
}

QString modbusBindingString(const PointBinding &binding,
                            const QString &key,
                            const QString &fallback)
{
    const QJsonValue value = binding.extensions.value(key);
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    return fallback;
}

uint buildModbusDataIndex(int groupNo, int entryNo)
{
    return (static_cast<uint>(groupNo) << 16) + static_cast<uint>(entryNo);
}

} // namespace configtool
