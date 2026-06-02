#include "config/config_domain.h"

namespace configtool {

namespace {

int modelServiceOrder(ModelServiceType type)
{
    switch (type) {
    case ModelServiceType::Measurement:
        return 0;
    case ModelServiceType::Status:
        return 1;
    case ModelServiceType::Control:
        return 2;
    }

    return 99;
}

ServiceTemplate makeService(ModelServiceType type)
{
    ServiceTemplate service;
    service.type = type;
    service.serviceId = modelServiceTypeId(type);
    service.displayName = modelServiceTypeDisplayName(type);
    return service;
}

} // namespace

QString PointTemplate::dataRef() const
{
    return buildDataRef(ldName, lnType, lnInst, doName);
}

QString PointTemplate::pointRef(const QString &modelId) const
{
    return modelId + QLatin1Char('#') + dataRef();
}

bool PointTemplate::hasCompleteAddressParts() const
{
    return !ldName.isEmpty() && !lnType.isEmpty() && !lnInst.isEmpty() && !doName.isEmpty();
}

void ModelTemplate::ensureDefaultServices()
{
    QList<ServiceTemplate> normalized;
    normalized.reserve(3);

    for (ModelServiceType type : {ModelServiceType::Measurement, ModelServiceType::Status, ModelServiceType::Control}) {
        const ServiceTemplate *existing = findService(type);
        normalized.append(existing ? *existing : makeService(type));
    }

    services = normalized;
}

ServiceTemplate *ModelTemplate::findService(ModelServiceType type)
{
    for (ServiceTemplate &service : services) {
        if (service.type == type) {
            return &service;
        }
    }

    return nullptr;
}

const ServiceTemplate *ModelTemplate::findService(ModelServiceType type) const
{
    for (const ServiceTemplate &service : services) {
        if (service.type == type) {
            return &service;
        }
    }

    return nullptr;
}

QString ProtocolDeviceInstance::deviceKey() const
{
    return protocolTypeId(protocol) + QLatin1Char('@') + appType + QLatin1Char('#') + deviceId;
}

QString buildDataRef(const QString &ldName,
                     const QString &lnType,
                     const QString &lnInst,
                     const QString &doName)
{
    return ldName + QLatin1Char('.')
        + lnType + QLatin1Char('.')
        + lnInst + QLatin1Char('.')
        + doName;
}

QString modelServiceTypeId(ModelServiceType type)
{
    switch (type) {
    case ModelServiceType::Measurement:
        return QStringLiteral("measurement");
    case ModelServiceType::Status:
        return QStringLiteral("status");
    case ModelServiceType::Control:
        return QStringLiteral("control");
    }

    return QStringLiteral("measurement");
}

QString modelServiceTypeDisplayName(ModelServiceType type)
{
    switch (type) {
    case ModelServiceType::Measurement:
        return QStringLiteral("遥测");
    case ModelServiceType::Status:
        return QStringLiteral("遥信");
    case ModelServiceType::Control:
        return QStringLiteral("控制");
    }

    return QStringLiteral("遥测");
}

QString pointSignalTypeId(PointSignalType type)
{
    switch (type) {
    case PointSignalType::Yc:
        return QStringLiteral("yc");
    case PointSignalType::Yx:
        return QStringLiteral("yx");
    case PointSignalType::Ctrl:
        return QStringLiteral("ctrl");
    }

    return QStringLiteral("yc");
}

QString controlKindId(ControlKind kind)
{
    switch (kind) {
    case ControlKind::None:
        return QStringLiteral("none");
    case ControlKind::RemoteControl:
        return QStringLiteral("remote_control");
    case ControlKind::RemoteAdjust:
        return QStringLiteral("remote_adjust");
    }

    return QStringLiteral("none");
}

QString protocolTypeId(ProtocolType type)
{
    switch (type) {
    case ProtocolType::Unknown:
        return QStringLiteral("unknown");
    case ProtocolType::Iec104:
        return QStringLiteral("iec104");
    case ProtocolType::Dlt645:
        return QStringLiteral("dlt645");
    case ProtocolType::Modbus:
        return QStringLiteral("modbus");
    }

    return QStringLiteral("unknown");
}

QList<ServiceTemplate> createDefaultModelServices()
{
    QList<ServiceTemplate> services;
    services.reserve(3);

    for (ModelServiceType type : {ModelServiceType::Measurement, ModelServiceType::Status, ModelServiceType::Control}) {
        services.append(makeService(type));
    }

    return services;
}

} // namespace configtool