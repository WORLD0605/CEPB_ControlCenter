#ifndef CONFIG_CONFIG_DOMAIN_H
#define CONFIG_CONFIG_DOMAIN_H

#include <QJsonObject>
#include <QList>
#include <QString>

namespace configtool {

enum class ModelServiceType {
    Measurement,
    Status,
    Control
};

enum class PointSignalType {
    Yc,
    Yx,
    Ctrl
};

enum class ControlKind {
    None,
    RemoteControl,
    RemoteAdjust
};

enum class ProtocolType {
    Unknown,
    Iec104,
    Dlt645,
    Modbus
};

struct SourceInfo {
    QString filePath;
    QString fileName;
    QJsonObject rawExtra;
};

struct PointTemplate {
    QString pointId;
    ModelServiceType category = ModelServiceType::Measurement;
    PointSignalType signalType = PointSignalType::Yc;
    ControlKind controlKind = ControlKind::None;
    QString name;
    QString description;
    QString ldName;
    QString lnType;
    QString lnInst;
    QString doName;
    QString doType;
    QString dataType;
    QString unit;
    QString deadZoneType;
    QString deadZoneValue;
    QString min;
    QString max;
    QString step;
    QString maxLength;
    QJsonObject extensions;
    SourceInfo source;

    QString dataRef() const;
    QString pointRef(const QString &modelId) const;
    bool hasCompleteAddressParts() const;
};

struct ServiceTemplate {
    ModelServiceType type = ModelServiceType::Measurement;
    QString serviceId;
    QString displayName;
    QList<PointTemplate> points;
};

struct ModelTemplate {
    QString modelId;
    QString name;
    QString displayName;
    QString deviceType;
    QString manufacturerId;
    QString manufacturerDesc;
    QString version;
    QString schema;
    QList<ServiceTemplate> services;
    QJsonObject extensions;
    SourceInfo source;

    void ensureDefaultServices();
    ServiceTemplate *findService(ModelServiceType type);
    const ServiceTemplate *findService(ModelServiceType type) const;
};

struct DeviceTransportConfig {
    QString primaryAddress;
    QString ip;
    QString port;
    QString channel;
    QString stationAddress;
    QJsonObject serial;
    QJsonObject protocolOptions;
    SourceInfo source;
};

struct PointBinding {
    QString bindingId;
    QString pointRef;
    QString dataRef;
    QString descriptionOverride;
    bool enabled = true;
    QString address;
    QString initValue;
    QString selfSignalFlag;
    QJsonObject qualityRule;
    QJsonObject extensions;
    SourceInfo source;
};

struct ProtocolDeviceInstance {
    QString deviceUid;
    QString appType;
    ProtocolType protocol = ProtocolType::Unknown;
    QString deviceId;
    QString deviceDesc;
    QString modelId;
    DeviceTransportConfig transport;
    QList<PointBinding> bindings;
    QJsonObject extensions;
    SourceInfo source;

    QString deviceKey() const;
};

struct ConfigProject {
    QString projectId;
    QString projectName;
    QString sourceRoot;
    QString northApp;
    QString logicApp;
    QList<QString> southApps;
    QList<ModelTemplate> models;
    QList<ProtocolDeviceInstance> devices;
    QJsonObject logic;
    QJsonObject metadata;
};

QString buildDataRef(const QString &ldName,
                     const QString &lnType,
                     const QString &lnInst,
                     const QString &doName);

QString modelServiceTypeId(ModelServiceType type);
QString modelServiceTypeDisplayName(ModelServiceType type);
QString pointSignalTypeId(PointSignalType type);
QString controlKindId(ControlKind kind);
QString protocolTypeId(ProtocolType type);

QList<ServiceTemplate> createDefaultModelServices();

} // namespace configtool

#endif