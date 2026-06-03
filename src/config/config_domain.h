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

enum class ModbusPointKind {
    Yx,
    Yc,
    Yk,
    Yt
};

struct ModbusGlobalConfig {
    QString frameInterval;
    QString hwVariant;
    QString yxUploadPeriod;
    QString ycUploadPeriod;
    QJsonObject rawExtra;
};

struct ModbusPollGroup {
    ModbusPointKind kind = ModbusPointKind::Yc;
    int order = 0;
    int groupNo = 0;
    int funCode = 0;
    int startAddr = 0;
    int regNum = 0;
    QString dataType;
    QString scale;
    QJsonObject rawExtra;
};

struct ModbusSetPoint {
    ModbusPointKind kind = ModbusPointKind::Yt;
    int order = 0;
    int groupNo = 0;
    int entryNo = 0;
    int funCode = 0;
    int regAddr = 0;
    QString dataType;
    QString scale;
    QJsonObject rawExtra;
};

struct ModbusDeviceConfig {
    QString yxType;
    QString ycType;
    QString ytType;
    QString ycScale;
    QString ytScale;
    QList<ModbusPollGroup> pollGroups;
    QList<ModbusSetPoint> setPoints;
    QJsonObject rawExtra;
};

struct LogicOperand {
    QString deviceId;
    QString dataRef;
    QJsonObject rawExtra;
};

struct LogicComputationPoint {
    QString deviceId;
    QString dataRef;
    QString formula;
    QString description;
    bool dropOperands = true;
    QList<LogicOperand> operands;
    QJsonObject rawExtra;
};

struct LogicControlTarget {
    QString deviceId;
    QString dataRef;
    QString expr;
    QString targetType = QStringLiteral("ctrlcmd");
    QJsonObject rawExtra;
};

struct LogicControlRule {
    QString matchDeviceId;
    QString matchDataRef;
    QString matchCtrlType;
    QList<LogicControlTarget> targets;
    QJsonObject rawExtra;
    QJsonObject matchRawExtra;
};

struct AgcAvcFollowConfig {
    bool enable = false;
    int periodMs = 5000;
    double step = 100.0;
    double tolerance = 0.1;
    QJsonObject rawExtra;
};

struct AgcAvcGateReverseConfig {
    bool enable = false;
    bool distant = false;
    bool lock = false;
    bool uplock = false;
    bool downlock = false;
    bool openloop = false;
    QJsonObject rawExtra;
};

struct AgcAvcMeasurementScale {
    double totalP = 1.0;
    double totalQ = 1.0;
    QJsonObject rawExtra;
};

struct AgcAvcDevice {
    QString deviceId;
    QString ctrlDataRefP;
    QString ctrlDataRefQ;
    QString onlineDeviceId;
    QString onlineDataRef;
    int onlineOkValue = 1;
    double pMax = 0.0;
    double pMin = 0.0;
    double qMax = 0.0;
    double qMin = 0.0;
    double scaleP = 1.0;
    double scaleQ = 1.0;
    QJsonObject rawExtra;
};

struct AgcAvcGroup {
    QString groupId = QStringLiteral("default");
    QString virtualDeviceId = QStringLiteral("999");
    QList<AgcAvcDevice> devices;
    AgcAvcMeasurementScale measurementScale;
    AgcAvcGateReverseConfig gateReverse;
    AgcAvcFollowConfig agcFollow;
    AgcAvcFollowConfig avcFollow;
    QJsonObject rawExtra;
};

struct LogicOnlineStatusLink {
    QString deviceId;
    QString linkToDeviceId;
    QJsonObject rawExtra;
};

struct AgcAvcDebugConfig {
    bool enable = false;
    int intervalMs = 5000;
    int maxList = 50;
    QJsonObject rawExtra;
};

struct LogicCenterConfig {
    QList<LogicComputationPoint> computationPoints;
    QList<LogicControlRule> controlRules;
    QList<AgcAvcGroup> agcAvcGroups;
    QList<LogicOnlineStatusLink> onlineStatusLinks;
    AgcAvcDebugConfig debug;
    QJsonObject rawExtra;
    bool hasDebug = false;
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
    ModbusDeviceConfig modbus;
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
    ModbusGlobalConfig modbus;
    LogicCenterConfig logicCenter;
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
QString modbusPointKindId(ModbusPointKind kind);

QList<ServiceTemplate> createDefaultModelServices();

LogicCenterConfig parseLogicCenterConfig(const QJsonObject &object);
QJsonObject serializeLogicCenterConfig(const LogicCenterConfig &config);

} // namespace configtool

#endif
