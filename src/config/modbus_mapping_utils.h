#ifndef CONFIG_MODBUS_MAPPING_UTILS_H
#define CONFIG_MODBUS_MAPPING_UTILS_H

#include "config/config_domain.h"

#include <QString>

namespace configtool {

bool isModbusDevice(const ProtocolDeviceInstance &device);
int modbusTypeRegisterCount(const QString &dataType);
int modbusMaxReadQuantity(int funCode);
QString normalizedModbusKind(const QString &value,
                             const QString &fallback = QStringLiteral("yc"));
QString modbusKindDisplayName(const QString &kind);
ModbusPointKind modbusKindFromString(const QString &value);
bool isModbusSetKind(const QString &kind);
QString modbusBindingKind(const PointBinding &binding);
int modbusBindingInt(const PointBinding &binding,
                     const QString &key,
                     int fallback = 0);
QString modbusBindingString(const PointBinding &binding,
                            const QString &key,
                            const QString &fallback = QString());
uint buildModbusDataIndex(int groupNo, int entryNo);

} // namespace configtool

#endif
