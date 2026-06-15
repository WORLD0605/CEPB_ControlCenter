#include "config/config_domain.h"

#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

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

QJsonObject rawExtraWithout(const QJsonObject &object, const QSet<QString> &knownKeys)
{
    QJsonObject extra;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!knownKeys.contains(it.key())) {
            extra.insert(it.key(), it.value());
        }
    }
    return extra;
}

QString stringValue(const QJsonObject &object, const QString &primaryKey, const QString &fallbackKey = QString())
{
    const QJsonValue primary = object.value(primaryKey);
    if (primary.isString()) {
        return primary.toString();
    }
    if (!fallbackKey.isEmpty()) {
        const QJsonValue fallback = object.value(fallbackKey);
        if (fallback.isString()) {
            return fallback.toString();
        }
    }
    return QString();
}

double doubleValue(const QJsonObject &object,
                   const QString &primaryKey,
                   const QString &fallbackKey,
                   double defaultValue)
{
    const QJsonValue primary = object.value(primaryKey);
    if (primary.isDouble()) {
        return primary.toDouble();
    }
    if (!fallbackKey.isEmpty()) {
        const QJsonValue fallback = object.value(fallbackKey);
        if (fallback.isDouble()) {
            return fallback.toDouble();
        }
    }
    return defaultValue;
}

int intValue(const QJsonObject &object,
             const QString &primaryKey,
             const QString &fallbackKey,
             int defaultValue)
{
    const QJsonValue primary = object.value(primaryKey);
    if (primary.isDouble()) {
        return primary.toInt();
    }
    if (!fallbackKey.isEmpty()) {
        const QJsonValue fallback = object.value(fallbackKey);
        if (fallback.isDouble()) {
            return fallback.toInt();
        }
    }
    return defaultValue;
}

bool boolValue(const QJsonObject &object, const QString &key, bool defaultValue)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : defaultValue;
}

LogicOperand parseLogicOperand(const QJsonObject &object)
{
    LogicOperand operand;
    operand.deviceId = object.value(QStringLiteral("DeviceId")).toString();
    operand.dataRef = object.value(QStringLiteral("dataRef")).toString();
    operand.rawExtra = rawExtraWithout(object, {QStringLiteral("DeviceId"), QStringLiteral("dataRef")});
    return operand;
}

QJsonObject serializeLogicOperand(const LogicOperand &operand)
{
    QJsonObject object = operand.rawExtra;
    object.insert(QStringLiteral("DeviceId"), operand.deviceId);
    object.insert(QStringLiteral("dataRef"), operand.dataRef);
    return object;
}

LogicComputationPoint parseLogicComputationPoint(const QJsonObject &object)
{
    LogicComputationPoint point;
    point.deviceId = object.value(QStringLiteral("DeviceId")).toString();
    point.dataRef = object.value(QStringLiteral("dataRef")).toString();
    point.formula = object.value(QStringLiteral("formula")).toString();
    point.description = object.value(QStringLiteral("description")).toString();
    point.dropOperands = object.value(QStringLiteral("drop_operands")).isBool()
        ? object.value(QStringLiteral("drop_operands")).toBool()
        : true;
    const QJsonArray operands = object.value(QStringLiteral("operands")).toArray();
    for (const QJsonValue &value : operands) {
        if (value.isObject()) {
            point.operands.append(parseLogicOperand(value.toObject()));
        }
    }
    point.rawExtra = rawExtraWithout(object, {
        QStringLiteral("DeviceId"),
        QStringLiteral("dataRef"),
        QStringLiteral("formula"),
        QStringLiteral("description"),
        QStringLiteral("drop_operands"),
        QStringLiteral("operands")
    });
    return point;
}

QJsonObject serializeLogicComputationPoint(const LogicComputationPoint &point)
{
    QJsonObject object = point.rawExtra;
    QJsonArray operands;
    for (const LogicOperand &operand : point.operands) {
        operands.append(serializeLogicOperand(operand));
    }
    object.insert(QStringLiteral("DeviceId"), point.deviceId);
    object.insert(QStringLiteral("dataRef"), point.dataRef);
    object.insert(QStringLiteral("formula"), point.formula);
    object.insert(QStringLiteral("description"), point.description);
    object.insert(QStringLiteral("drop_operands"), point.dropOperands);
    object.insert(QStringLiteral("operands"), operands);
    return object;
}

QString logicOperandPlaceholder(int index)
{
    return QStringLiteral("{%1}").arg(index + 1);
}

QString joinLogicOperandPlaceholders(int count, const QString &op)
{
    QStringList parts;
    for (int index = 0; index < count; ++index) {
        parts.append(logicOperandPlaceholder(index));
    }
    return parts.join(op);
}

QString buildLogicStatusOrFormula(int count)
{
    QString formula = QStringLiteral("1");
    for (int index = 0; index < count; ++index) {
        formula += QStringLiteral("*(1-%1)").arg(logicOperandPlaceholder(index));
    }
    return QStringLiteral("1-(%1)").arg(formula);
}

LogicControlTarget parseLogicControlTarget(const QJsonObject &object)
{
    LogicControlTarget target;
    target.deviceId = object.value(QStringLiteral("DeviceId")).toString();
    target.dataRef = object.value(QStringLiteral("dataRef")).toString();
    target.expr = object.value(QStringLiteral("expr")).toString();
    target.targetType = stringValue(object, QStringLiteral("targetType"), QStringLiteral("target_type"));
    if (target.targetType.trimmed().isEmpty()) {
        target.targetType = QStringLiteral("ctrlcmd");
    }
    target.rawExtra = rawExtraWithout(object, {
        QStringLiteral("DeviceId"),
        QStringLiteral("dataRef"),
        QStringLiteral("expr"),
        QStringLiteral("targetType"),
        QStringLiteral("target_type")
    });
    return target;
}

QJsonObject serializeLogicControlTarget(const LogicControlTarget &target)
{
    QJsonObject object = target.rawExtra;
    object.insert(QStringLiteral("DeviceId"), target.deviceId);
    object.insert(QStringLiteral("dataRef"), target.dataRef);
    object.insert(QStringLiteral("expr"), target.expr);
    object.insert(QStringLiteral("targetType"), target.targetType.trimmed().isEmpty()
        ? QStringLiteral("ctrlcmd")
        : target.targetType);
    return object;
}

LogicControlRule parseLogicControlRule(const QJsonObject &object)
{
    LogicControlRule rule;
    const QJsonObject match = object.value(QStringLiteral("match")).toObject();
    rule.matchDeviceId = match.value(QStringLiteral("DeviceId")).toString();
    rule.matchDataRef = match.value(QStringLiteral("dataRef")).toString();
    rule.matchCtrlType = match.value(QStringLiteral("CtrlType")).toString();
    rule.description = stringValue(match, QStringLiteral("Description"), QStringLiteral("description"));
    rule.matchRawExtra = rawExtraWithout(match, {
        QStringLiteral("DeviceId"),
        QStringLiteral("dataRef"),
        QStringLiteral("CtrlType"),
        QStringLiteral("Description"),
        QStringLiteral("description")
    });
    const QJsonArray targets = object.value(QStringLiteral("targets")).toArray();
    for (const QJsonValue &value : targets) {
        if (value.isObject()) {
            rule.targets.append(parseLogicControlTarget(value.toObject()));
        }
    }
    rule.rawExtra = rawExtraWithout(object, {
        QStringLiteral("match"),
        QStringLiteral("targets")
    });
    return rule;
}

QJsonObject serializeLogicControlRule(const LogicControlRule &rule)
{
    QJsonObject match = rule.matchRawExtra;
    match.insert(QStringLiteral("DeviceId"), rule.matchDeviceId);
    match.insert(QStringLiteral("dataRef"), rule.matchDataRef);
    match.remove(QStringLiteral("CtrlType"));
    if (!rule.description.trimmed().isEmpty()) {
        match.insert(QStringLiteral("Description"), rule.description);
    } else {
        match.remove(QStringLiteral("Description"));
    }
    match.remove(QStringLiteral("description"));

    QJsonArray targets;
    for (const LogicControlTarget &target : rule.targets) {
        targets.append(serializeLogicControlTarget(target));
    }

    QJsonObject object = rule.rawExtra;
    object.insert(QStringLiteral("match"), match);
    object.insert(QStringLiteral("targets"), targets);
    return object;
}

AgcAvcFollowConfig parseAgcAvcFollowConfig(const QJsonObject &object)
{
    AgcAvcFollowConfig follow;
    follow.enable = boolValue(object, QStringLiteral("enable"), false);
    follow.periodMs = intValue(object, QStringLiteral("period_ms"), QString(), 5000);
    follow.step = doubleValue(object, QStringLiteral("step"), QString(), 100.0);
    follow.tolerance = doubleValue(object, QStringLiteral("tolerance"), QString(), 0.1);
    follow.rawExtra = rawExtraWithout(object, {
        QStringLiteral("enable"),
        QStringLiteral("period_ms"),
        QStringLiteral("step"),
        QStringLiteral("tolerance")
    });
    return follow;
}

QJsonObject serializeAgcAvcFollowConfig(const AgcAvcFollowConfig &follow)
{
    QJsonObject object = follow.rawExtra;
    object.insert(QStringLiteral("enable"), follow.enable);
    object.insert(QStringLiteral("period_ms"), follow.periodMs);
    object.insert(QStringLiteral("step"), follow.step);
    object.insert(QStringLiteral("tolerance"), follow.tolerance);
    return object;
}

AgcAvcGateReverseConfig parseAgcAvcGateReverseConfig(const QJsonObject &object)
{
    AgcAvcGateReverseConfig gate;
    gate.enable = boolValue(object, QStringLiteral("enable"), false);
    gate.distant = boolValue(object, QStringLiteral("distant"), false);
    gate.lock = boolValue(object, QStringLiteral("lock"), false);
    gate.uplock = boolValue(object, QStringLiteral("uplock"), false);
    gate.downlock = boolValue(object, QStringLiteral("downlock"), false);
    gate.openloop = boolValue(object, QStringLiteral("openloop"), false);
    gate.rawExtra = rawExtraWithout(object, {
        QStringLiteral("enable"),
        QStringLiteral("distant"),
        QStringLiteral("lock"),
        QStringLiteral("uplock"),
        QStringLiteral("downlock"),
        QStringLiteral("openloop")
    });
    return gate;
}

QJsonObject serializeAgcAvcGateReverseConfig(const AgcAvcGateReverseConfig &gate)
{
    QJsonObject object = gate.rawExtra;
    object.insert(QStringLiteral("enable"), gate.enable);
    object.insert(QStringLiteral("distant"), gate.distant);
    object.insert(QStringLiteral("lock"), gate.lock);
    object.insert(QStringLiteral("uplock"), gate.uplock);
    object.insert(QStringLiteral("downlock"), gate.downlock);
    object.insert(QStringLiteral("openloop"), gate.openloop);
    return object;
}

AgcAvcMeasurementScale parseAgcAvcMeasurementScale(const QJsonObject &object)
{
    AgcAvcMeasurementScale scale;
    scale.totalP = doubleValue(object, QStringLiteral("totalP"), QStringLiteral("total_p"), 1.0);
    scale.totalQ = doubleValue(object, QStringLiteral("totalQ"), QStringLiteral("total_q"), 1.0);
    scale.rawExtra = rawExtraWithout(object, {
        QStringLiteral("totalP"),
        QStringLiteral("totalQ"),
        QStringLiteral("total_p"),
        QStringLiteral("total_q")
    });
    return scale;
}

QJsonObject serializeAgcAvcMeasurementScale(const AgcAvcMeasurementScale &scale)
{
    QJsonObject object = scale.rawExtra;
    object.insert(QStringLiteral("totalP"), scale.totalP);
    object.insert(QStringLiteral("totalQ"), scale.totalQ);
    return object;
}

AgcAvcDevice parseAgcAvcDevice(const QJsonObject &object)
{
    AgcAvcDevice device;
    device.deviceId = object.value(QStringLiteral("DeviceId")).toString();
    device.ctrlDataRefP = stringValue(object, QStringLiteral("ctrlDataRefP"), QStringLiteral("ctrl_dataref_p"));
    device.ctrlDataRefQ = stringValue(object, QStringLiteral("ctrlDataRefQ"), QStringLiteral("ctrl_dataref_q"));
    device.onlineDeviceId = stringValue(object, QStringLiteral("onlineDeviceId"), QStringLiteral("online_device_id"));
    device.onlineDataRef = stringValue(object, QStringLiteral("onlineDataRef"), QStringLiteral("online_dataref"));
    device.onlineOkValue = intValue(object, QStringLiteral("onlineOkValue"), QStringLiteral("online_ok_value"), 1);
    device.pMax = doubleValue(object, QStringLiteral("pMax"), QString(), 0.0);
    device.pMin = doubleValue(object, QStringLiteral("pMin"), QString(), 0.0);
    device.qMax = doubleValue(object, QStringLiteral("qMax"), QString(), 0.0);
    device.qMin = doubleValue(object, QStringLiteral("qMin"), QString(), 0.0);
    const double legacyScale = doubleValue(object, QStringLiteral("scale"), QString(), 1.0);
    device.scaleP = doubleValue(object, QStringLiteral("scaleP"), QStringLiteral("scale_p"), legacyScale);
    device.scaleQ = doubleValue(object, QStringLiteral("scaleQ"), QStringLiteral("scale_q"), legacyScale);
    device.rawExtra = rawExtraWithout(object, {
        QStringLiteral("DeviceId"),
        QStringLiteral("ctrlDataRefP"),
        QStringLiteral("ctrlDataRefQ"),
        QStringLiteral("ctrl_dataref_p"),
        QStringLiteral("ctrl_dataref_q"),
        QStringLiteral("onlineDeviceId"),
        QStringLiteral("onlineDataRef"),
        QStringLiteral("online_device_id"),
        QStringLiteral("online_dataref"),
        QStringLiteral("onlineOkValue"),
        QStringLiteral("online_ok_value"),
        QStringLiteral("pMax"),
        QStringLiteral("pMin"),
        QStringLiteral("qMax"),
        QStringLiteral("qMin"),
        QStringLiteral("scale"),
        QStringLiteral("scaleP"),
        QStringLiteral("scaleQ"),
        QStringLiteral("scale_p"),
        QStringLiteral("scale_q")
    });
    return device;
}

QJsonObject serializeAgcAvcDevice(const AgcAvcDevice &device)
{
    QJsonObject object = device.rawExtra;
    object.insert(QStringLiteral("DeviceId"), device.deviceId);
    object.insert(QStringLiteral("ctrlDataRefP"), device.ctrlDataRefP);
    object.insert(QStringLiteral("ctrlDataRefQ"), device.ctrlDataRefQ);
    if (!device.onlineDeviceId.trimmed().isEmpty()) {
        object.insert(QStringLiteral("onlineDeviceId"), device.onlineDeviceId);
    }
    if (!device.onlineDataRef.trimmed().isEmpty()) {
        object.insert(QStringLiteral("onlineDataRef"), device.onlineDataRef);
    }
    object.insert(QStringLiteral("onlineOkValue"), device.onlineOkValue);
    object.insert(QStringLiteral("pMax"), device.pMax);
    object.insert(QStringLiteral("pMin"), device.pMin);
    object.insert(QStringLiteral("qMax"), device.qMax);
    object.insert(QStringLiteral("qMin"), device.qMin);
    object.insert(QStringLiteral("scaleP"), device.scaleP);
    object.insert(QStringLiteral("scaleQ"), device.scaleQ);
    return object;
}

AgcAvcGroup parseAgcAvcGroup(const QJsonObject &object)
{
    AgcAvcGroup group;
    group.groupId = stringValue(object, QStringLiteral("groupId"), QStringLiteral("group_id"));
    if (group.groupId.trimmed().isEmpty()) {
        group.groupId = QStringLiteral("default");
    }
    group.virtualDeviceId = stringValue(object, QStringLiteral("virtualDeviceId"), QStringLiteral("virtual_device_id"));
    if (group.virtualDeviceId.trimmed().isEmpty()) {
        group.virtualDeviceId = QStringLiteral("999");
    }
    const QJsonArray devices = object.value(QStringLiteral("devicelist")).toArray();
    for (const QJsonValue &value : devices) {
        if (value.isObject()) {
            group.devices.append(parseAgcAvcDevice(value.toObject()));
        }
    }
    group.measurementScale = parseAgcAvcMeasurementScale(object.value(QStringLiteral("measurement_scale")).toObject());
    group.gateReverse = parseAgcAvcGateReverseConfig(object.value(QStringLiteral("gate_reverse")).toObject());
    const QJsonObject legacyFollow = object.value(QStringLiteral("follow")).toObject();
    group.agcFollow = object.value(QStringLiteral("agc_follow")).isObject()
        ? parseAgcAvcFollowConfig(object.value(QStringLiteral("agc_follow")).toObject())
        : parseAgcAvcFollowConfig(legacyFollow);
    group.avcFollow = object.value(QStringLiteral("avc_follow")).isObject()
        ? parseAgcAvcFollowConfig(object.value(QStringLiteral("avc_follow")).toObject())
        : parseAgcAvcFollowConfig(legacyFollow);
    group.rawExtra = rawExtraWithout(object, {
        QStringLiteral("groupId"),
        QStringLiteral("group_id"),
        QStringLiteral("virtualDeviceId"),
        QStringLiteral("virtual_device_id"),
        QStringLiteral("devicelist"),
        QStringLiteral("measurement_scale"),
        QStringLiteral("gate_reverse"),
        QStringLiteral("follow"),
        QStringLiteral("agc_follow"),
        QStringLiteral("avc_follow")
    });
    return group;
}

QJsonObject serializeAgcAvcGroup(const AgcAvcGroup &group)
{
    QJsonArray devices;
    for (const AgcAvcDevice &device : group.devices) {
        devices.append(serializeAgcAvcDevice(device));
    }

    QJsonObject object = group.rawExtra;
    object.insert(QStringLiteral("groupId"), group.groupId);
    object.insert(QStringLiteral("virtualDeviceId"), group.virtualDeviceId);
    object.insert(QStringLiteral("devicelist"), devices);
    object.insert(QStringLiteral("measurement_scale"), serializeAgcAvcMeasurementScale(group.measurementScale));
    object.insert(QStringLiteral("gate_reverse"), serializeAgcAvcGateReverseConfig(group.gateReverse));
    object.insert(QStringLiteral("agc_follow"), serializeAgcAvcFollowConfig(group.agcFollow));
    object.insert(QStringLiteral("avc_follow"), serializeAgcAvcFollowConfig(group.avcFollow));
    return object;
}

LogicOnlineStatusLink parseLogicOnlineStatusLink(const QJsonObject &object)
{
    LogicOnlineStatusLink link;
    link.deviceId = object.value(QStringLiteral("DeviceId")).toString();
    link.linkToDeviceId = object.value(QStringLiteral("LinkToDeviceId")).toString();
    link.rawExtra = rawExtraWithout(object, {
        QStringLiteral("DeviceId"),
        QStringLiteral("LinkToDeviceId")
    });
    return link;
}

QJsonObject serializeLogicOnlineStatusLink(const LogicOnlineStatusLink &link)
{
    QJsonObject object = link.rawExtra;
    object.insert(QStringLiteral("DeviceId"), link.deviceId);
    object.insert(QStringLiteral("LinkToDeviceId"), link.linkToDeviceId);
    return object;
}

AgcAvcDebugConfig parseAgcAvcDebugConfig(const QJsonObject &object)
{
    AgcAvcDebugConfig debug;
    debug.enable = boolValue(object, QStringLiteral("enable"), false);
    debug.intervalMs = intValue(object, QStringLiteral("interval_ms"), QString(), 5000);
    debug.maxList = intValue(object, QStringLiteral("max_list"), QString(), 50);
    debug.rawExtra = rawExtraWithout(object, {
        QStringLiteral("enable"),
        QStringLiteral("interval_ms"),
        QStringLiteral("max_list")
    });
    return debug;
}

QJsonObject serializeAgcAvcDebugConfig(const AgcAvcDebugConfig &debug)
{
    QJsonObject object = debug.rawExtra;
    object.insert(QStringLiteral("enable"), debug.enable);
    object.insert(QStringLiteral("interval_ms"), debug.intervalMs);
    object.insert(QStringLiteral("max_list"), debug.maxList);
    return object;
}

QString pointKey(const QString &deviceId, const QString &dataRef)
{
    return deviceId.trimmed() + QLatin1Char('#') + dataRef.trimmed();
}

ConfigIssue makeConfigIssue(ConfigIssueSeverity severity,
                            const QString &module,
                            const QString &objectId,
                            const QString &message)
{
    ConfigIssue issue;
    issue.severity = severity;
    issue.module = module;
    issue.objectId = objectId;
    issue.message = message;
    return issue;
}

QSet<QString> buildProjectDeviceIds(const ConfigProject *project)
{
    QSet<QString> deviceIds;
    if (!project) {
        return deviceIds;
    }

    for (const ProtocolDeviceInstance &device : project->devices) {
        if (!device.deviceId.trimmed().isEmpty()) {
            deviceIds.insert(device.deviceId.trimmed());
        }
    }
    return deviceIds;
}

QSet<QString> buildProjectPointKeys(const ConfigProject *project)
{
    QSet<QString> pointKeys;
    if (!project) {
        return pointKeys;
    }

    QHash<QString, const ModelTemplate*> modelsById;
    for (const ModelTemplate &model : project->models) {
        if (!model.modelId.trimmed().isEmpty()) {
            modelsById.insert(model.modelId.trimmed(), &model);
        }
    }

    for (const ProtocolDeviceInstance &device : project->devices) {
        for (const PointBinding &binding : device.bindings) {
            if (!device.deviceId.trimmed().isEmpty() && !binding.dataRef.trimmed().isEmpty()) {
                pointKeys.insert(pointKey(device.deviceId, binding.dataRef));
            }
        }

        const ModelTemplate *model = modelsById.value(device.modelId.trimmed(), nullptr);
        if (!model || device.deviceId.trimmed().isEmpty()) {
            continue;
        }
        for (const ServiceTemplate &service : model->services) {
            for (const PointTemplate &point : service.points) {
                const QString dataRef = point.dataRef().trimmed();
                if (!dataRef.isEmpty()) {
                    pointKeys.insert(pointKey(device.deviceId, dataRef));
                }
            }
        }
    }

    return pointKeys;
}

QSet<QString> buildProjectControlPointKeys(const ConfigProject *project)
{
    QSet<QString> pointKeys;
    if (!project) {
        return pointKeys;
    }

    QHash<QString, const ModelTemplate*> modelsById;
    for (const ModelTemplate &model : project->models) {
        if (!model.modelId.trimmed().isEmpty()) {
            modelsById.insert(model.modelId.trimmed(), &model);
        }
    }

    for (const ProtocolDeviceInstance &device : project->devices) {
        const ModelTemplate *model = modelsById.value(device.modelId.trimmed(), nullptr);
        if (!model || device.deviceId.trimmed().isEmpty()) {
            continue;
        }
        for (const ServiceTemplate &service : model->services) {
            if (service.type != ModelServiceType::Control) {
                continue;
            }
            for (const PointTemplate &point : service.points) {
                const QString dataRef = point.dataRef().trimmed();
                if (!dataRef.isEmpty()) {
                    pointKeys.insert(pointKey(device.deviceId, dataRef));
                }
            }
        }
    }

    return pointKeys;
}

QString placeholderName(int index)
{
    return QStringLiteral("{%1}").arg(index);
}

void validateLogicFormula(const QString &module,
                          const QString &objectId,
                          const QString &formula,
                          int operandCount,
                          QList<ConfigIssue> &issues)
{
    if (formula.trimmed().isEmpty()) {
        issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                      module,
                                      objectId,
                                      QStringLiteral("公式不能为空")));
        return;
    }

    QSet<int> usedOperandIndexes;
    const QRegularExpression placeholderPattern(QStringLiteral("\\{(\\d+)\\}"));
    QRegularExpressionMatchIterator it = placeholderPattern.globalMatch(formula);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        bool ok = false;
        const int operandIndex = match.captured(1).toInt(&ok);
        if (!ok) {
            continue;
        }
        usedOperandIndexes.insert(operandIndex);
        if (operandIndex < 1 || operandIndex > operandCount) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          module,
                                          objectId,
                                          QStringLiteral("公式引用 %1 超出源点数量 %2")
                                              .arg(placeholderName(operandIndex))
                                              .arg(operandCount)));
        }
    }

    if (operandCount > 0 && usedOperandIndexes.isEmpty()) {
        issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                      module,
                                      objectId,
                                      QStringLiteral("公式没有引用任何源点占位符")));
    }

    for (int index = 1; index <= operandCount; ++index) {
        if (!usedOperandIndexes.contains(index)) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                          module,
                                          objectId,
                                          QStringLiteral("源点 %1 未在公式中使用").arg(placeholderName(index))));
        }
    }
}

void validateLogicControlExpression(const QString &objectId,
                                    const QString &targetId,
                                    const QString &expr,
                                    QList<ConfigIssue> &issues)
{
    if (expr.trimmed().isEmpty()) {
        issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                      QStringLiteral("控制转换"),
                                      objectId,
                                      QStringLiteral("目标 %1 的 expr 不能为空").arg(targetId)));
        return;
    }

    static const QRegularExpression placeholderPattern(QStringLiteral("\\{([^{}]+)\\}"));
    QRegularExpressionMatchIterator placeholderIt = placeholderPattern.globalMatch(expr);
    while (placeholderIt.hasNext()) {
        const QRegularExpressionMatch match = placeholderIt.next();
        const QString token = match.captured(1).trimmed();
        if (token == QStringLiteral("x")) {
            continue;
        }
        if (token.startsWith(QStringLiteral("rt:")) && token.mid(3).contains(QLatin1Char('#'))) {
            continue;
        }
        issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                      QStringLiteral("控制转换"),
                                      objectId,
                                      QStringLiteral("目标 %1 的 expr 包含不支持的占位符 %2，控制转换仅支持 {x} 和 {rt:DeviceId#DataRefer}")
                                          .arg(targetId, match.captured(0))));
    }

    static const QRegularExpression functionPattern(QStringLiteral("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*\\("));
    static const QSet<QString> supportedFunctions = {
        QStringLiteral("sqrt"),
        QStringLiteral("sqr"),
        QStringLiteral("square"),
        QStringLiteral("pow")
    };
    QRegularExpressionMatchIterator functionIt = functionPattern.globalMatch(expr);
    while (functionIt.hasNext()) {
        const QRegularExpressionMatch match = functionIt.next();
        const QString name = match.captured(1);
        if (!supportedFunctions.contains(name)) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("控制转换"),
                                          objectId,
                                          QStringLiteral("目标 %1 的 expr 使用了不支持的函数 %2")
                                              .arg(targetId, name)));
        }
    }
}

bool projectDeviceExists(const QSet<QString> &deviceIds, const QString &deviceId)
{
    return deviceIds.contains(deviceId.trimmed());
}

bool projectPointExists(const QSet<QString> &pointKeys, const QString &deviceId, const QString &dataRef)
{
    return pointKeys.contains(pointKey(deviceId, dataRef));
}

bool projectControlPointExists(const QSet<QString> &controlPointKeys, const QString &deviceId, const QString &dataRef)
{
    return controlPointKeys.contains(pointKey(deviceId, dataRef));
}

} // namespace

LogicComputationPoint buildLogicComputationPointFromTemplate(const LogicComputationTemplateRequest &request)
{
    LogicComputationPoint point;
    point.deviceId = request.outputDeviceId.trimmed();
    point.dataRef = request.outputDataRef.trimmed();
    point.description = request.description;
    point.dropOperands = request.dropOperands;
    point.operands = request.operands;

    switch (request.type) {
    case LogicComputationTemplateType::Sum:
        point.formula = joinLogicOperandPlaceholders(point.operands.size(), QStringLiteral("+"));
        break;
    case LogicComputationTemplateType::PowerFactor:
        if (point.operands.size() >= 2) {
            point.formula = QStringLiteral("{1}/sqrt(sqr({1})+sqr({2}))");
        }
        break;
    case LogicComputationTemplateType::SinglePoint:
        point.formula = point.operands.isEmpty() ? QString() : QStringLiteral("{1}");
        break;
    case LogicComputationTemplateType::StatusOr:
        point.formula = buildLogicStatusOrFormula(point.operands.size());
        break;
    case LogicComputationTemplateType::StatusAnd:
        point.formula = joinLogicOperandPlaceholders(point.operands.size(), QStringLiteral("*"));
        break;
    }

    return point;
}

bool upsertLogicComputationPoint(LogicCenterConfig &config, const LogicComputationPoint &point)
{
    const QString deviceId = point.deviceId.trimmed();
    const QString dataRef = point.dataRef.trimmed();
    for (LogicComputationPoint &existing : config.computationPoints) {
        if (existing.deviceId.trimmed() == deviceId && existing.dataRef.trimmed() == dataRef) {
            existing = point;
            return false;
        }
    }

    config.computationPoints.append(point);
    return true;
}

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

QString modbusPointKindId(ModbusPointKind kind)
{
    switch (kind) {
    case ModbusPointKind::Yx:
        return QStringLiteral("yx");
    case ModbusPointKind::Yc:
        return QStringLiteral("yc");
    case ModbusPointKind::Yk:
        return QStringLiteral("yk");
    case ModbusPointKind::Yt:
        return QStringLiteral("yt");
    }

    return QStringLiteral("yc");
}

QString dlt645PointKindId(Dlt645PointKind kind)
{
    switch (kind) {
    case Dlt645PointKind::Yx:
        return QStringLiteral("yx");
    case Dlt645PointKind::Yc:
        return QStringLiteral("yc");
    case Dlt645PointKind::Yk:
        return QStringLiteral("yk");
    case Dlt645PointKind::Yt:
        return QStringLiteral("yt");
    }

    return QStringLiteral("yc");
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

LogicCenterConfig parseLogicCenterConfig(const QJsonObject &object)
{
    LogicCenterConfig config;

    const QJsonArray computationPoints = object.value(QStringLiteral("computation_points")).toArray();
    for (const QJsonValue &value : computationPoints) {
        if (value.isObject()) {
            config.computationPoints.append(parseLogicComputationPoint(value.toObject()));
        }
    }

    const QJsonArray controlRules = object.value(QStringLiteral("control_rules")).toArray();
    for (const QJsonValue &value : controlRules) {
        if (value.isObject()) {
            config.controlRules.append(parseLogicControlRule(value.toObject()));
        }
    }

    const QJsonArray groups = object.value(QStringLiteral("AgcAvcGroups")).toArray();
    for (const QJsonValue &value : groups) {
        if (value.isObject()) {
            config.agcAvcGroups.append(parseAgcAvcGroup(value.toObject()));
        }
    }
    if (config.agcAvcGroups.isEmpty() && object.value(QStringLiteral("AgcAvc")).isObject()) {
        config.agcAvcGroups.append(parseAgcAvcGroup(object.value(QStringLiteral("AgcAvc")).toObject()));
    }

    const QJsonArray onlineLinks = object.value(QStringLiteral("onlineStatus_link")).toArray();
    for (const QJsonValue &value : onlineLinks) {
        if (value.isObject()) {
            config.onlineStatusLinks.append(parseLogicOnlineStatusLink(value.toObject()));
        }
    }

    config.hasDebug = object.value(QStringLiteral("AgcAvc_debug")).isObject();
    if (config.hasDebug) {
        config.debug = parseAgcAvcDebugConfig(object.value(QStringLiteral("AgcAvc_debug")).toObject());
    }

    config.rawExtra = rawExtraWithout(object, {
        QStringLiteral("computation_points"),
        QStringLiteral("control_rules"),
        QStringLiteral("AgcAvcGroups"),
        QStringLiteral("AgcAvc"),
        QStringLiteral("onlineStatus_link"),
        QStringLiteral("AgcAvc_debug")
    });

    return config;
}

QJsonObject serializeLogicCenterConfig(const LogicCenterConfig &config)
{
    QJsonObject object = config.rawExtra;

    QJsonArray computationPoints;
    for (const LogicComputationPoint &point : config.computationPoints) {
        computationPoints.append(serializeLogicComputationPoint(point));
    }
    object.insert(QStringLiteral("computation_points"), computationPoints);

    QJsonArray controlRules;
    for (const LogicControlRule &rule : config.controlRules) {
        controlRules.append(serializeLogicControlRule(rule));
    }
    object.insert(QStringLiteral("control_rules"), controlRules);

    QJsonArray groups;
    for (const AgcAvcGroup &group : config.agcAvcGroups) {
        groups.append(serializeAgcAvcGroup(group));
    }
    object.insert(QStringLiteral("AgcAvcGroups"), groups);

    QJsonArray onlineLinks;
    for (const LogicOnlineStatusLink &link : config.onlineStatusLinks) {
        onlineLinks.append(serializeLogicOnlineStatusLink(link));
    }
    object.insert(QStringLiteral("onlineStatus_link"), onlineLinks);

    if (config.hasDebug) {
        object.insert(QStringLiteral("AgcAvc_debug"), serializeAgcAvcDebugConfig(config.debug));
    }

    return object;
}

QList<ConfigIssue> validateLogicCenterConfig(const LogicCenterConfig &config,
                                             const ConfigProject *project)
{
    QList<ConfigIssue> issues;
    const QSet<QString> projectDeviceIds = buildProjectDeviceIds(project);
    const QSet<QString> projectPointKeys = buildProjectPointKeys(project);
    const QSet<QString> projectControlPointKeys = buildProjectControlPointKeys(project);
    const bool hasProjectContext = project != nullptr;

    for (const LogicComputationPoint &point : config.computationPoints) {
        const QString objectId = pointKey(point.deviceId, point.dataRef);
        if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("计算点"),
                                          objectId,
                                          QStringLiteral("计算点输出 DeviceId 和 dataRef 不能为空")));
        }
        if (point.operands.isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("计算点"),
                                          objectId,
                                          QStringLiteral("operands 不能为空")));
        }
        validateLogicFormula(QStringLiteral("计算点"),
                             objectId,
                             point.formula,
                             point.operands.size(),
                             issues);
        if (!point.dropOperands) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Info,
                                          QStringLiteral("计算点"),
                                          objectId,
                                          QStringLiteral("drop_operands=false 会保留源点原始转发")));
        }

        if (hasProjectContext) {
            if (!point.deviceId.trimmed().isEmpty() && !projectDeviceExists(projectDeviceIds, point.deviceId)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("计算点"),
                                              objectId,
                                              QStringLiteral("输出设备在当前工程中不存在")));
            } else if (!point.dataRef.trimmed().isEmpty()
                       && !projectPointExists(projectPointKeys, point.deviceId, point.dataRef)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("计算点"),
                                              objectId,
                                              QStringLiteral("输出点在当前工程设备或模型点表中找不到")));
            }

            for (const LogicOperand &operand : point.operands) {
                const QString operandId = pointKey(operand.deviceId, operand.dataRef);
                if (operand.deviceId.trimmed().isEmpty() || operand.dataRef.trimmed().isEmpty()) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                                  QStringLiteral("计算点"),
                                                  objectId,
                                                  QStringLiteral("源点 DeviceId 和 dataRef 不能为空")));
                    continue;
                }
                if (!projectDeviceExists(projectDeviceIds, operand.deviceId)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("计算点"),
                                                  objectId,
                                                  QStringLiteral("源设备 %1 在当前工程中不存在").arg(operand.deviceId)));
                } else if (!projectPointExists(projectPointKeys, operand.deviceId, operand.dataRef)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("计算点"),
                                                  objectId,
                                                  QStringLiteral("源点 %1 在当前工程设备或模型点表中找不到").arg(operandId)));
                }
            }
        }
    }

    QHash<QString, int> controlMatchCount;
    for (const LogicControlRule &rule : config.controlRules) {
        const QString objectId = pointKey(rule.matchDeviceId, rule.matchDataRef);
        controlMatchCount[objectId] += 1;

        if (rule.matchDeviceId.trimmed().isEmpty() || rule.matchDataRef.trimmed().isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("控制转换"),
                                          objectId,
                                          QStringLiteral("match.DeviceId 和 match.dataRef 不能为空")));
        }
        if (rule.targets.isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("控制转换"),
                                          objectId,
                                          QStringLiteral("targets 不能为空")));
        }

        if (hasProjectContext) {
            if (!rule.matchDeviceId.trimmed().isEmpty() && !projectDeviceExists(projectDeviceIds, rule.matchDeviceId)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("控制转换"),
                                              objectId,
                                              QStringLiteral("源控制设备在当前工程中不存在")));
            } else if (!rule.matchDataRef.trimmed().isEmpty()
                       && !projectPointExists(projectPointKeys, rule.matchDeviceId, rule.matchDataRef)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("控制转换"),
                                              objectId,
                                              QStringLiteral("源控制点在当前工程设备或模型点表中找不到")));
            } else if (!rule.matchDataRef.trimmed().isEmpty()
                       && !projectControlPointExists(projectControlPointKeys, rule.matchDeviceId, rule.matchDataRef)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("控制转换"),
                                              objectId,
                                              QStringLiteral("源点不是模型中的控制类点位，Control_Rule 仅适用于遥控/遥调")));
            }
        }

        for (const LogicControlTarget &target : rule.targets) {
            const QString targetId = pointKey(target.deviceId, target.dataRef);
            if (target.deviceId.trimmed().isEmpty() || target.dataRef.trimmed().isEmpty()) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                              QStringLiteral("控制转换"),
                                              objectId,
                                              QStringLiteral("目标 DeviceId 和 dataRef 不能为空")));
            }
            validateLogicControlExpression(objectId, targetId, target.expr, issues);
            const QString targetType = target.targetType.trimmed();
            if (targetType != QStringLiteral("ctrlcmd") && targetType != QStringLiteral("data_write")) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                              QStringLiteral("控制转换"),
                                              objectId,
                                              QStringLiteral("目标 %1 的 targetType 非法：%2")
                                                  .arg(targetId, target.targetType)));
            }

            if (hasProjectContext) {
                if (!target.deviceId.trimmed().isEmpty() && !projectDeviceExists(projectDeviceIds, target.deviceId)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("控制转换"),
                                                  objectId,
                                                  QStringLiteral("目标设备 %1 在当前工程中不存在").arg(target.deviceId)));
                } else if (!target.dataRef.trimmed().isEmpty()
                           && !projectPointExists(projectPointKeys, target.deviceId, target.dataRef)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("控制转换"),
                                                  objectId,
                                                  QStringLiteral("目标点 %1 在当前工程设备或模型点表中找不到").arg(targetId)));
                } else if (targetType == QStringLiteral("ctrlcmd")
                           && !target.dataRef.trimmed().isEmpty()
                           && !projectControlPointExists(projectControlPointKeys, target.deviceId, target.dataRef)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("控制转换"),
                                                  objectId,
                                                  QStringLiteral("ctrlcmd 目标点 %1 不是模型中的控制类点位").arg(targetId)));
                }
            }
        }
    }
    for (auto it = controlMatchCount.constBegin(); it != controlMatchCount.constEnd(); ++it) {
        if (it.value() > 1) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                          QStringLiteral("控制转换"),
                                          it.key(),
                                          QStringLiteral("存在重复 match.DeviceId + match.dataRef，运行时会全部执行")));
        }
    }

    QHash<QString, int> virtualDeviceCount;
    for (const AgcAvcGroup &group : config.agcAvcGroups) {
        const QString objectId = group.groupId.trimmed().isEmpty()
            ? group.virtualDeviceId
            : group.groupId;
        virtualDeviceCount[group.virtualDeviceId.trimmed()] += 1;

        if (group.virtualDeviceId.trimmed().isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("AGC/AVC"),
                                          objectId,
                                          QStringLiteral("virtualDeviceId 不能为空")));
        }
        if (group.devices.isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("AGC/AVC"),
                                          objectId,
                                          QStringLiteral("devicelist 不能为空")));
        }

        double totalPCapacity = 0.0;
        double totalQCapacity = 0.0;
        for (const AgcAvcDevice &device : group.devices) {
            if (device.deviceId.trimmed().isEmpty()) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                              QStringLiteral("AGC/AVC"),
                                              objectId,
                                              QStringLiteral("南向设备 DeviceId 不能为空")));
            }
            if (device.pMax <= device.pMin) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                              QStringLiteral("AGC/AVC"),
                                              objectId,
                                              QStringLiteral("设备 %1 有功容量异常：pMax 必须大于 pMin").arg(device.deviceId)));
            } else {
                totalPCapacity += device.pMax - device.pMin;
            }
            if (device.qMax <= device.qMin) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                              QStringLiteral("AGC/AVC"),
                                              objectId,
                                              QStringLiteral("设备 %1 无功容量异常：qMax 必须大于 qMin").arg(device.deviceId)));
            } else {
                totalQCapacity += device.qMax - device.qMin;
            }

            if (hasProjectContext) {
                if (!device.deviceId.trimmed().isEmpty() && !projectDeviceExists(projectDeviceIds, device.deviceId)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("AGC/AVC"),
                                                  objectId,
                                                  QStringLiteral("南向设备 %1 在当前工程中不存在").arg(device.deviceId)));
                }
                if (!device.ctrlDataRefP.trimmed().isEmpty()
                    && !projectPointExists(projectPointKeys, device.deviceId, device.ctrlDataRefP)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("AGC/AVC"),
                                                  objectId,
                                                  QStringLiteral("设备 %1 的 P 控制点在当前工程中找不到").arg(device.deviceId)));
                }
                if (!device.ctrlDataRefQ.trimmed().isEmpty()
                    && !projectPointExists(projectPointKeys, device.deviceId, device.ctrlDataRefQ)) {
                    issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                  QStringLiteral("AGC/AVC"),
                                                  objectId,
                                                  QStringLiteral("设备 %1 的 Q 控制点在当前工程中找不到").arg(device.deviceId)));
                }
                if (!device.onlineDataRef.trimmed().isEmpty()) {
                    const QString onlineDeviceId = device.onlineDeviceId.trimmed().isEmpty()
                        ? device.deviceId
                        : device.onlineDeviceId;
                    if (!projectPointExists(projectPointKeys, onlineDeviceId, device.onlineDataRef)) {
                        issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                                      QStringLiteral("AGC/AVC"),
                                                      objectId,
                                                      QStringLiteral("设备 %1 的在线状态点在当前工程中找不到").arg(device.deviceId)));
                    }
                }
            }
        }

        if (!group.devices.isEmpty() && totalPCapacity <= 0.0) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("AGC/AVC"),
                                          objectId,
                                          QStringLiteral("AGC 组总有功容量为 0")));
        }
        if (!group.devices.isEmpty() && totalQCapacity <= 0.0) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("AGC/AVC"),
                                          objectId,
                                          QStringLiteral("AVC 组总无功容量为 0")));
        }
    }
    for (auto it = virtualDeviceCount.constBegin(); it != virtualDeviceCount.constEnd(); ++it) {
        if (!it.key().isEmpty() && it.value() > 1) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("AGC/AVC"),
                                          it.key(),
                                          QStringLiteral("virtualDeviceId 重复")));
        }
    }

    for (const LogicOnlineStatusLink &link : config.onlineStatusLinks) {
        const QString objectId = QStringLiteral("%1 -> %2").arg(link.deviceId, link.linkToDeviceId);
        if (link.deviceId.trimmed().isEmpty() || link.linkToDeviceId.trimmed().isEmpty()) {
            issues.append(makeConfigIssue(ConfigIssueSeverity::Error,
                                          QStringLiteral("在线联动"),
                                          objectId,
                                          QStringLiteral("DeviceId 和 LinkToDeviceId 不能为空")));
        }
        if (hasProjectContext) {
            if (!link.deviceId.trimmed().isEmpty() && !projectDeviceExists(projectDeviceIds, link.deviceId)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("在线联动"),
                                              objectId,
                                              QStringLiteral("被联动设备在当前工程中不存在")));
            }
            if (!link.linkToDeviceId.trimmed().isEmpty() && !projectDeviceExists(projectDeviceIds, link.linkToDeviceId)) {
                issues.append(makeConfigIssue(ConfigIssueSeverity::Warning,
                                              QStringLiteral("在线联动"),
                                              objectId,
                                              QStringLiteral("跟随设备在当前工程中不存在")));
            }
        }
    }

    return issues;
}

} // namespace configtool
