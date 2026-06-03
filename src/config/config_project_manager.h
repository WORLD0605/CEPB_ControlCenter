#ifndef CONFIG_CONFIG_PROJECT_MANAGER_H
#define CONFIG_CONFIG_PROJECT_MANAGER_H

#include <QList>
#include <QString>

#include "config/config_domain.h"

namespace configtool {

enum class ImportIssueSeverity {
    Warning,
    Error
};

struct ImportIssue {
    ImportIssueSeverity severity = ImportIssueSeverity::Error;
    QString filePath;
    QString message;
};

struct ImportReport {
    QList<ImportIssue> issues;
    int importedModelCount = 0;
    int importedDeviceCount = 0;

    bool hasErrors() const;
    void addIssue(ImportIssueSeverity severity,
                  const QString &filePath,
                  const QString &message);
};

struct ExportReport {
    QList<ImportIssue> issues;
    int exportedModelCount = 0;
    int exportedDeviceCount = 0;

    bool hasErrors() const;
    void addIssue(ImportIssueSeverity severity,
                  const QString &filePath,
                  const QString &message);
};

class Iec104ConfigImporter
{
public:
    bool importAppDirectory(const QString &appDir,
                            ConfigProject &project,
                            ImportReport &report) const;

private:
    bool importModelDirectory(const QString &modelDir,
                              ConfigProject &project,
                              ImportReport &report) const;
    bool importDeviceDirectory(const QString &deviceDir,
                               ConfigProject &project,
                               ImportReport &report) const;
    bool importModelFile(const QString &filePath,
                         ConfigProject &project,
                         ImportReport &report) const;
    bool importDeviceFile(const QString &filePath,
                          ConfigProject &project,
                          ImportReport &report) const;
};

class ModbusConfigImporter
{
public:
    bool importAppDirectory(const QString &appDir,
                            ConfigProject &project,
                            ImportReport &report) const;

private:
    bool importModelDirectory(const QString &modelDir,
                              ConfigProject &project,
                              ImportReport &report) const;
    bool importDeviceDirectory(const QString &deviceDir,
                               const QString &iniPath,
                               ConfigProject &project,
                               ImportReport &report) const;
    bool importModelFile(const QString &filePath,
                         ConfigProject &project,
                         ImportReport &report) const;
    bool importDeviceFile(const QString &filePath,
                          const QString &iniPath,
                          ConfigProject &project,
                          ImportReport &report) const;
    ModbusGlobalConfig importGlobalIniConfig(const QString &iniPath) const;
    ModbusDeviceConfig importDeviceIniConfig(const QString &iniPath,
                                             const QString &deviceId,
                                             ImportReport &report) const;
};

class ConfigProjectManager
{
public:
    ConfigProjectManager();

    void createEmptyProject(const QString &projectName,
                            const QString &sourceRoot);
    bool importIec104AppDirectory(const QString &appDir,
                                  ImportReport &report);
    bool importModbusAppDirectory(const QString &appDir,
                                  ImportReport &report);
    bool exportIec104AppDirectory(const QString &appDir,
                                  ExportReport &report) const;

    ConfigProject &project();
    const ConfigProject &project() const;

private:
    ConfigProject m_project;
    Iec104ConfigImporter m_iec104Importer;
    ModbusConfigImporter m_modbusImporter;
};

} // namespace configtool

#endif
