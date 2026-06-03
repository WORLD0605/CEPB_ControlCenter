#ifndef CONFIG_CONFIG_PROJECT_MANAGER_P_H
#define CONFIG_CONFIG_PROJECT_MANAGER_P_H

#include "config/config_domain.h"
#include "config/config_project_manager.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QString>

class QSettings;

namespace configtool::detail {

SourceInfo makeSourceInfo(const QString &filePath);
PointSignalType signalTypeForService(ModelServiceType type);
ModelServiceType serviceTypeFromIndex(int index);
ModelServiceType inferServiceType(const QJsonObject &pointObject);
PointTemplate parsePointTemplate(const QJsonObject &pointObject,
                                 ModelServiceType serviceType,
                                 const SourceInfo &source);
bool loadJsonDocument(const QString &filePath,
                      QJsonDocument &document,
                      QString &errorMessage);
QString safeFileSegment(const QString &value,
                        const QString &fallback);
QString jsonValueToString(const QJsonValue &value);
int parseModbusLinePart(const QString &line, int sectionIndex);
bool isCompleteModbusLine(const QString &line, int expectedParts);
QJsonObject qSettingsGroupRawExtra(QSettings &settings,
                                   const QSet<QString> &knownKeys);
void annotateModbusBinding(PointBinding &binding,
                           const ModbusDeviceConfig &modbus);
bool isDeviceForProtocol(const ProtocolDeviceInstance &device,
                         ProtocolType protocol);
QList<ProtocolDeviceInstance> devicesForProtocol(const ConfigProject &project,
                                                 ProtocolType protocol);
QSet<QString> modelIdsUsedByDevices(const QList<ProtocolDeviceInstance> &devices);
QList<ModelTemplate> modelsForDeviceSet(const ConfigProject &project,
                                        const QSet<QString> &modelIds);
QString modbusKindString(ModbusPointKind kind);
QString modbusPollKeyPrefix(ModbusPointKind kind);
QString modbusSetKeyPrefix(ModbusPointKind kind);
QString modbusIniPollLine(const ModbusPollGroup &group);
QString modbusIniSetLine(const ModbusSetPoint &setPoint);
QString modelFileNameForExport(const ModelTemplate &model);
QString deviceFileNameForExport(const ProtocolDeviceInstance &device);
QJsonObject serializeModel(const ModelTemplate &model);
const ModelTemplate *findModelById(const ConfigProject &project,
                                   const QString &modelId);
QSet<QString> duplicateDataRefsForModel(const ModelTemplate &model);
QSet<QString> duplicateBindingAddresses(const ProtocolDeviceInstance &device);
ImportIssueSeverity importSeverityForConfigIssue(ConfigIssueSeverity severity);
QString formatConfigIssue(const ConfigIssue &issue);
void appendConfigIssuesToReport(const QList<ConfigIssue> &issues,
                                const QString &filePath,
                                ImportReport &report);
void appendConfigIssuesToReport(const QList<ConfigIssue> &issues,
                                const QString &filePath,
                                ExportReport &report);
QHash<QString, QString> buildDataRefDescriptionMap(const ModelTemplate *model);
QHash<QString, int> buildModelPointOrderMap(const ModelTemplate *model);
QJsonObject serializeDevice(const ProtocolDeviceInstance &device,
                            const ModelTemplate *model);
QJsonObject serializeModbusDevice(const ProtocolDeviceInstance &device,
                                  const ModelTemplate *model);
QList<PointBinding> bindingsForExport(const ProtocolDeviceInstance &device,
                                      const QHash<QString, int> &orderMap);
bool writeJsonFile(const QString &filePath,
                   const QJsonObject &object,
                   QString &errorMessage);
bool removeJsonFilesInDirectory(const QString &dirPath,
                                QString &errorMessage);
bool writeTextFile(const QString &filePath,
                   const QString &content,
                   QString &errorMessage);
QString buildModbusIniContent(const ConfigProject &project,
                              const QList<ProtocolDeviceInstance> &devices);

} // namespace configtool::detail

#endif
