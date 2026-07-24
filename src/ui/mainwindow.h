#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPair>
#include <QSet>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include "config/config_project_manager.h"
#include "network/debug_console_client.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QLabel;
class QGroupBox;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;
class QTabWidget;
class QTabBar;
class QTimer;
class QThread;
class QSplitter;
class QEvent;
class QFrame;
class QProgressDialog;
class QProgressBar;
class QWidget;
class ProgramControlSshWorker;

enum class AppViewMode {
    Terminal,
    DataTable,
    LogicAgcAvcTable
};

struct AppConfig {
    QString name;
    quint16 port = 0;
    QString prompt;
    AppViewMode viewMode = AppViewMode::Terminal;
};

struct DeviceLogFileInfo {
    QString appName;
    QString logType;
    QString remotePath;
    QString fileName;
    QString logDate;
    QString modifiedTime;
    qint64 sizeBytes = 0;
};

struct ServiceChannelDataItem {
    QString deviceId;
    QString dataRef;
    QString serviceId;
    QString description;
    QString dataTime;
    QString value;
};

struct LogicAgcAvcStatusItem {
    QString groupId;
    QString virtualDeviceId;
    QString totalDevices;
    QString onlineDevices;
    QString offlineDevices;
    QString agcParams;
    QString avcParams;
    QString agcTarget;
    QString avcTarget;
    QString totalP;
    QString totalQ;
    QString offlineList;
};

struct DebugAppSession {
    DebugConsoleClient *client = nullptr;
    int appIndex = -1;
    QList<ServiceChannelDataItem> serviceChannelItems;
    QHash<QString, ServiceChannelDataItem> previousServiceChannelItemMap;
    QHash<QString, QDateTime> timeHighlightUntilMap;
    QHash<QString, QDateTime> valueHighlightUntilMap;
    QHash<QString, QString> controlStatusTextMap;
    QHash<QString, QColor> controlStatusColorMap;
    QList<LogicAgcAvcStatusItem> logicAgcAvcItems;
    QString pendingDataTableCommand;
    bool waitingControlResponse = false;
    QString pendingControlDeviceId;
    QString pendingControlDataRef;
    QString pendingControlValue;
    int pendingControlType = -1;
    QString pendingDataWriteDeviceId;
    QString pendingDataWriteDataRef;
    QString pendingDataWriteValue;
    QString pendingDataWriteQuality;
    QString pendingDataFreezeMode;
    bool serviceChannelDataFrozen = false;
    QJsonObject northConnectionStatus;
    QString northConnectionStatusError;
    bool showSouthConnectionStatusDialog = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    enum class ThemeMode {
        Light,
        Dark
    };

    enum class ProgramControlCommandKind {
        Verify,
        RefreshStatus,
        Start,
        Stop,
        StartAll,
        StopAll,
        ForceStop,
        Restart,
        EnableAutostart,
        DisableAutostart,
        Install,
        Upgrade
    };

    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onQuickCommandClicked();
    void onAppSelectionChanged(int index);
    void onDeviceFilterChanged(int index);
    void onServiceTypeFilterChanged(int index);
    void onDataRefFilterTextChanged(const QString &text);
    void onAutoRefreshIntervalChanged(int index);
    void onSendControlClicked();
    void onDataTableCellDoubleClicked(int row, int column);
    void onDataTableSelectionChanged();
    void onBrowseConfigImportDirClicked();
    void onOpenConfigDirClicked();
    void onImportIec104ConfigClicked();
    void onExportIec104ConfigClicked();
    void onCheckConfigIssuesClicked();
    void onUploadConfigClicked();
    void onDownloadConfigClicked();
    void onConfigModelSelectionChanged();
    void onConfigDeviceSelectionChanged();
    void onConfigModelActivated(int row, int column);
    void onConfigDeviceActivated(int row, int column);
    void onModelEditorSelectionChanged(int index);
    void onDeviceEditorSelectionChanged(int index);
    void onConfigIssueActivated(int row, int column);
    void onNewModelClicked();
    void onModelFieldEdited();
    void onAddPointClicked();
    void onCopyPointClicked();
    void onDeletePointClicked();
    void onModelPointItemChanged(QTableWidgetItem *item);
    void onModelPointCategoryChanged(int index);
    void onModelPointFilterChanged(int index);
    void onModelPointDataRefFilterTextChanged(const QString &text);
    void onCreateDeviceFromModelClicked();
    void openCreateDeviceDialog(int preselectedModelIndex);
    void onCopyDeviceClicked();
    void onDeleteModelClicked();
    void onDeleteDeviceClicked();
    void onDeviceFieldEdited();
    void onDeviceBindingFilterChanged(int index);
    void onDeviceBindingDataRefFilterTextChanged(const QString &text);
    void onDeviceBindingItemChanged(QTableWidgetItem *item);
    void onDeviceOnlineLinkEnabledChanged(bool checked);
    void onDeviceOnlineLinkTargetChanged(int index);
    void onLogicAgcAvcBasicEdited();
    void onLogicAgcAvcDeviceItemChanged(QTableWidgetItem *item);
    void onLogicAgcAvcDeviceCellDoubleClicked(int row, int column);
    void onAddLogicAgcAvcDeviceClicked();
    void onDeleteLogicAgcAvcDeviceClicked();
    void onAddLogicComputationPointClicked();
    void onCopyLogicComputationPointClicked();
    void onDeleteLogicComputationPointClicked();
    void onLogicComputationPointItemChanged(QTableWidgetItem *item);
    void onLogicComputationPointCellDoubleClicked(int row, int column);
    void onAddLogicControlRuleClicked();
    void onCopyLogicControlRuleClicked();
    void onDeleteLogicControlRuleClicked();
    void onAddLogicControlTargetClicked();
    void onDeleteLogicControlTargetClicked();
    void onLogicControlRuleSelectionChanged();
    void onLogicControlRuleItemChanged(QTableWidgetItem *item);
    void onLogicControlTargetItemChanged(QTableWidgetItem *item);
    void onLogicControlRuleCellDoubleClicked(int row, int column);
    void onLogicControlTargetCellDoubleClicked(int row, int column);
    void onLogicControlPreviewEdited();
    void onLogicControlTemplateClicked();
    void onInsertLogicControlRealtimeRefClicked();
    void onRefreshProgramStatusClicked();
    void onConnectProgramControlClicked();
    void onDisconnectProgramControlClicked();
    void onStartProgramClicked();
    void onStopProgramClicked();
    void onStartAllProgramsClicked();
    void onStopAllProgramsClicked();
    void onForceStopProgramClicked();
    void onRestartProgramClicked();
    void onToggleProgramAutostartClicked();
    void onInstallProgramClicked();
    void onUpgradeProgramClicked();
    void onIec101CommModeChanged(int index);
    void onRefreshIec101PointsClicked();
    void onSortIec101PointsClicked();
    void onIec101PointItemChanged(QTableWidgetItem *item);
    void onIec101PointFilterChanged(int index);
    void onIec101PointFilterTextChanged();
    void onRefreshIec104PointsClicked();
    void onSortIec104PointsClicked();
    void onIec104PointItemChanged(QTableWidgetItem *item);
    void onIec104PointFilterChanged(int index);
    void onIec104PointFilterTextChanged();
    void onOpenRawFrameLogClicked();
    void onReadNetworkConfigClicked();
    void onApplyNetworkConfigClicked();
    void onAddNetworkRouteClicked();
    void onDeleteNetworkRouteClicked();
    void onPingNetworkTargetClicked();

    void onConnected();
    void onDisconnected();
    void onError(const QString &err);
    void onLogLine(const QString &line);
    void onCommandReply(const QString &reply);

private:
    void appendSystem(const QString &text, const QString &color = "#ff8c00");
    void appendLog(const QString &text);
    void appendReply(const QString &text);
    void updateUIState(bool connected);
    void applyCurrentAppView();
    QPair<int, int> currentNavigationState() const;
    void recordNavigationState();
    void applyNavigationState(const QPair<int, int> &state);
    void navigateBack();
    void navigateForward();
    void showNorthConfigPage(QWidget *page);
    AppConfig currentAppConfig() const;
    AppConfig appConfigForSession(const DebugAppSession *session) const;
    DebugAppSession *currentDebugSession() const;
    DebugAppSession *debugSessionForClient(QObject *client) const;
    DebugAppSession *debugSessionForAppName(const QString &appName) const;
    DebugConsoleClient *currentDebugClient() const;
    bool anyDebugClientConnected() const;
    void updateDebugAppTabText(DebugAppSession *session);
    void requestNorthConnectionStatus(DebugAppSession *session = nullptr, bool logRequest = false);
    void updateNorthConnectionStatusUi();
    void updateNorthConnectionStatusTimer();
    void showSouthDeviceStatusDialog(DebugAppSession *session);
    void requestServiceChannelData(bool logRequest = true);
    void scheduleLogicCenterStatusRefresh(DebugAppSession *session,
                                          int delayMs = 600,
                                          int remainingRetries = 3);
    QList<LogicAgcAvcStatusItem> parseLogicAgcAvcReply(const QString &reply) const;
    void populateLogicAgcAvcTable(const QList<LogicAgcAvcStatusItem> &items);
    void configureDataTableForCurrentApp();
    QList<ServiceChannelDataItem> parseServiceChannelDataReply(const QString &reply) const;
    void populateServiceChannelTable(const QList<ServiceChannelDataItem> &items);
    void refreshDeviceFilterOptions();
    void applyServiceChannelFilter();
    void updateAutoRefreshTimer();
    void updateHighlightRefreshTimer();
    void copySelectedTableCells(QTableWidget *table = nullptr);
    void clearSelectedEditableTableCells(QTableWidget *table);
    void updateControlCommandUi();
    void updateServiceChannelDataFreezeUi();
    void openControlCommandDialog(int row);
    void openDataWriteDialog(int row);
    bool handleControlResponseLogLine(const QString &line);
    bool handleControlResponseLogLine(const QString &line, DebugAppSession *session);
    void handleControlResponseTimeout();
    QString controlCommandKey(const QString &deviceId, const QString &dataRef) const;
    void setControlStatus(const QString &deviceId,
                          const QString &dataRef,
                          const QString &statusText,
                          const QColor &color);
    bool isServiceChannelControlRow(int row) const;
    bool isServiceChannelControlPoint(const ServiceChannelDataItem &item,
                                      configtool::ControlKind *controlKind = nullptr) const;
    bool isServiceChannelDataWriteRow(int row) const;
    bool isServiceChannelDataWritePoint(const ServiceChannelDataItem &item) const;
    configtool::ControlKind inferControlKindFromText(const ServiceChannelDataItem &item) const;
    void sendServiceChannelControlCommand(const ServiceChannelDataItem &item,
                                          const QString &ctrlVal,
                                          int ctrlType);
    void sendServiceChannelDataWriteCommand(const ServiceChannelDataItem &item,
                                            const QString &value,
                                            const QString &quality);
    bool sendLogicCenterDataWriteCommand(const QString &appName,
                                         const QString &deviceId,
                                         const QString &dataRef,
                                         const QString &value);
    void sendServiceChannelDataFreezeCommand(const QString &mode);
    void pasteClipboardIntoModelPointsTable();
    void pasteClipboardIntoDeviceBindingsTable();
    void pasteClipboardIntoIec101PointsTable();
    void pasteClipboardIntoIec104PointsTable();
    void clearSelectedModelPointCells();
    void clearSelectedDeviceBindingCells();
    void clearSelectedIec101PointCells();
    void clearSelectedIec104PointCells();
    void pushIec101PointsUndoSnapshot();
    void undoIec101PointsLastEdit();
    void pushIec104PointsUndoSnapshot();
    void undoIec104PointsLastEdit();
    void applyModelPointCellText(int row, int column, const QString &text);
    void applyDeviceBindingCellText(int row, int column, const QString &text);
    int syncModelPointCategoryToDeviceBindings(const QString &modelId,
                                               const configtool::PointTemplate &point);
    void rebuildModbusDeviceConfig(configtool::ProtocolDeviceInstance &device);
    void rebuildDlt645DeviceConfig(configtool::ProtocolDeviceInstance &device);
    void autoMergeDlt645FfPollGroups();
    void pushConfigUndoSnapshot();
    void undoLastConfigEdit();
    void tryAutoOpenLastConfig();
    QString serviceChannelItemKey(const ServiceChannelDataItem &item) const;
    QString normalizedConfigProjectRoot(const QString &selectedPath) const;
    QString resolveNorthCepAppDir(const QString &projectRoot) const;
    QString resolveNorthMqttAppDir(const QString &projectRoot) const;
    QString resolveIec104AppDir(const QString &projectRoot) const;
    QString resolveModbusAppDir(const QString &projectRoot) const;
    QString resolveDlt645AppDir(const QString &projectRoot) const;
    QString resolveLogicCenterAppDir(const QString &projectRoot) const;
    QString resolveIec101ServiceChannelAppDir(const QString &projectRoot) const;
    QString resolveIec104ServiceChannelAppDir(const QString &projectRoot) const;
    void clearNorthCepConfigPage();
    void loadNorthCepMainstationConfig(const QString &filePath, configtool::ImportReport &report);
    void loadNorthCepSystemConfig(const QString &filePath, configtool::ImportReport &report);
    bool validateNorthCepConfig(QString *errorMessage = nullptr) const;
    bool writeNorthCepSystemConfig(const QString &projectRoot,
                                   configtool::ExportReport &report) const;
    void clearNorthMqttConfigPage();
    void loadNorthMqttMainstationConfig(const QString &filePath, configtool::ImportReport &report);
    bool validateNorthMqttConfig(QString *errorMessage = nullptr) const;
    QJsonObject serializeIec101LocalhostConfig() const;
    void loadIec101LocalhostConfigFromFile(const QString &filePath);
    void loadIec101LocalhostConfigFromJson(const QJsonObject &root);
    void clearIec101ConfigPage();
    void refreshIec101PointsFromDevices(const QHash<QString, QJsonObject> &savedPointSettings = {},
                                        const QStringList &savedPointOrder = {});
    void rebuildIec101PointRowsInOrder(const QList<int> &sourceRows);
    void applyIec101PointsFilter();
    QSet<QString> checkIec101DuplicateAddresses() const;
    QStringList checkIec101AddressRangeErrors() const;
    void highlightIec101DuplicateAddresses();
    void setupIec104ConfigPage();
    QJsonObject serializeIec104LocalhostConfig() const;
    void loadIec104LocalhostConfigFromFile(const QString &filePath);
    void loadIec104LocalhostConfigFromJson(const QJsonObject &root);
    void clearIec104ConfigPage();
    void refreshIec104PointsFromDevices(const QHash<QString, QJsonObject> &savedPointSettings = {},
                                        const QStringList &savedPointOrder = {});
    void rebuildIec104PointRowsInOrder(const QList<int> &sourceRows);
    void applyIec104PointsFilter();
    QSet<QString> checkIec104DuplicateAddresses() const;
    QStringList checkIec104AddressRangeErrors() const;
    void highlightIec104DuplicateAddresses();
    QString configBrowseStartDir() const;
    QStringList configTransferRelativePaths(const QString &projectRoot) const;
    QString deviceHost() const;
    QString fixedRemoteUser() const;
    QString fixedRemotePassword() const;
    QString fixedRemoteSshPort() const;
    QString configRemoteTarget() const;
    QString configRemoteBaseDir() const;
    bool clearLocalConfigTransferPaths(const QString &projectRoot, QString *errorMessage = nullptr) const;
    bool runConfigTransferProcess(const QString &program,
                                  const QStringList &arguments,
                                  const QString &title,
                                  QString *output = nullptr,
                                  QProgressDialog *progress = nullptr);
    void refreshConfigIssueTable(const QList<configtool::ImportIssue> &issues,
                                 const QString &source);
    QList<configtool::ImportIssue> collectCurrentConfigIssues() const;
    void navigateToConfigIssue(int row);
    void refreshConfigImportSummary(const configtool::ImportReport &report);
    void refreshConfigObjectViews();
    void refreshLogicCenterOverview();
    void refreshLogicAgcAvcPage();
    void refreshLogicComputationPointPage();
    void refreshLogicControlRulePage();
    void refreshSelectionOverview();
    void refreshEditorNavigationCombos();
    void refreshModelDetail(int modelIndex);
    void refreshModelOverview(int modelIndex);
    void refreshDeviceDetail(int deviceIndex);
    void refreshDeviceEditor(int deviceIndex);
    void refreshDeviceOnlineLinkPanel(int deviceIndex);
    void setCurrentDeviceOnlineLinkTarget(const QString &targetDeviceId);
    int renameModelReferences(const QString &oldModelId, const QString &newModelId);
    int renameModelPointReferences(const QString &modelId,
                                   const QString &oldDataRef,
                                   const QString &newDataRef);
    int syncModelPointDescriptionToDeviceBindings(const QString &modelId,
                                                  const configtool::PointTemplate &point,
                                                  const QString &oldDescription,
                                                  const QString &newDescription);
    int syncDeviceBindingsForModel(const QString &modelId);
    int removeDeviceBindingsForModelPoints(const QString &modelId,
                                           const QSet<QString> &pointRefs,
                                           const QSet<QString> &dataRefs);
    int renameLogicDeviceReferences(const QString &oldDeviceId, const QString &newDeviceId);
    void selectModelPointById(const QString &pointId);
    bool selectConfigDeviceByIndex(int deviceIndex);
    int currentConfigModelIndex() const;
    int currentConfigDeviceIndex() const;
    QPair<int, int> currentModelPointLocation() const;
    QSet<QString> duplicateDataRefsForModel(const configtool::ModelTemplate &model) const;
    QSet<QString> duplicateBindingAddresses(const configtool::ProtocolDeviceInstance &device) const;
    QSet<QString> duplicateIec104ChannelBindingAddresses(const configtool::ConfigProject &project,
                                                         const configtool::ProtocolDeviceInstance &device) const;
    configtool::AgcAvcGroup *ensureLogicAgcAvcGroup();
    configtool::AgcAvcGroup *currentLogicAgcAvcGroup();
    void selectLogicAgcAvcPointForColumn(int dataRefColumn,
                                         configtool::ModelServiceType preferredType,
                                         bool updateOnlineDevice);
    QList<configtool::LogicOperand> collectLogicTemplateOperands(configtool::ModelServiceType preferredType,
                                                                 const QString &title,
                                                                 int minimumCount);
    void upsertLogicTemplatePoint(const configtool::LogicComputationTemplateRequest &request);
    void generateLogicComputationTemplate(int templateIndex);
    void generateLogicSinglePointTemplateVisual();
    void generateLogicSourcePointScaleTemplateVisual();
    void generateLogicStatusOrTemplateVisual();
    void generateLogicStatusAndTemplateVisual();
    void generateLogicMultiPointSumTemplateVisual();
    void generateLogicPowerFactorTemplateVisual();
    void generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType type,
                                           const QString &templateName,
                                           const QString &operatorText,
                                           const QString &titleText,
                                           const QString &defaultOutputDataRef);
    bool editLogicComputationOperands(const QString &title,
                                      const QString &formula,
                                      QList<configtool::LogicOperand> &operands);
    void selectLogicComputationOutputPoint(int row);
    void selectLogicComputationOperands(int row);
    void selectLogicControlMatchPoint(int row);
    void selectLogicControlTargetPoint(int row);
    int currentLogicControlRuleIndex() const;
    void refreshLogicControlTargetTable();
    void refreshLogicControlPreview();
    QStringList managedProgramAppNames() const;
    QString programServiceName(const QString &appName) const;
    QString localProgramBinaryPath(const QString &appName) const;
    QString localProgramServicePath(const QString &appName) const;
    QString remoteProgramBinaryPath(const QString &appName) const;
    bool upgradeProgramBinary(const QString &appName,
                              QString *output,
                              QProgressDialog *progress = nullptr);
    bool startProgramControlCommand(const QString &command,
                                    const QString &title,
                                    ProgramControlCommandKind kind,
                                    const QString &appName = QString());
    void handleProgramControlShellReadyRead();
    void finishProgramControlCommand(int exitCode, const QString &output);
    void clearProgramControlCommandState();
    void closeProgramControlShell();
    QString programControlShellKey() const;
    QString programControlRemoteTarget() const;
    void startOpenProgramControlShell(const QString &title,
                                      QProgressDialog *progress);
    void handleProgramControlConnected(quint64 serial, const QString &key);
    void handleProgramControlConnectFailed(quint64 serial, const QString &message);
    void handleProgramControlCommandFinished(quint64 serial,
                                             int kind,
                                             const QString &title,
                                             const QString &appName,
                                             int exitCode,
                                             const QString &output);
    void handleProgramControlUpgradeProgress(quint64 serial,
                                             const QString &appName,
                                             qint64 sentBytes,
                                             qint64 totalBytes);
    void updateProgramControlConnectionUi(bool connected);
    void updateProgramControlBusyUi(bool busy);
    void startProgramStatusRefresh();
    void refreshProgramControlTable(const QString &statusOutput);
    void setDeviceStorageQueryState(const QString &statusText);
    void updateDeviceStorageDisplay(QProgressBar *progressBar,
                                    QLabel *valueLabel,
                                    const QString &fileSystem,
                                    const QString &mountPoint,
                                    qint64 totalBytes,
                                    qint64 usedBytes,
                                    qint64 availableBytes,
                                    int usedPercent);
    void setProgramControlRowPending(const QString &appName, const QString &statusText);
    ThemeMode loadThemeMode() const;
    void saveThemeMode(ThemeMode mode) const;
    void applyTheme(ThemeMode mode);
    void onThemeToggleClicked();
    void setupNetworkPage();
    bool validateNetworkConfig(QString *errorMessage = nullptr) const;
    QByteArray serializeNetworkConfig() const;
    bool loadNetworkConfigData(const QByteArray &data, QString *errorMessage = nullptr);
    bool saveNetworkConfigToProject(QString *errorMessage = nullptr) const;
    void loadNetworkProjectIfAvailable();
    QString localNetworkSupportFile(const QString &relativePath) const;
    void setupLogManagementPage();
    void resetLogRetentionDays();
    bool loadLogRetentionConfigFromProject(const QString &projectRoot,
                                           QString *errorMessage = nullptr);
    bool saveLogRetentionConfigToProject(QString *errorMessage = nullptr);
    void clearDeviceLogFileList(const QString &summaryText = QString());
    void queryDeviceLogFiles();
    void startDeviceLogFileQuery(bool automatic);
    void finishDeviceLogFileQuery(quint64 queryId,
                                  const QString &queriedHost,
                                  bool automatic,
                                  bool ok,
                                  const QString &output,
                                  const QString &error);
    void refreshDeviceLogDateFilter();
    void refreshDeviceLogFileTable();
    void chooseLogDownloadDirectory();
    void downloadDeviceLogFiles(bool selectedOnly);
    QColor serviceChannelDefaultTextColor() const;
    QColor serviceChannelChangedTextColor() const;

    QHash<QString, DebugAppSession *> m_debugSessions;
    configtool::ConfigProjectManager m_configProjectManager;
    QTimer *m_autoRefreshTimer = nullptr;
    QTimer *m_northConnectionStatusTimer = nullptr;
    QTimer *m_highlightRefreshTimer = nullptr;
    QTimer *m_controlResponseTimer = nullptr;
    QList<AppConfig> m_appConfigs;
    bool m_updatingModelPointsTable = false;
    bool m_updatingModelPointCategory = false;
    bool m_updatingDeviceBindingsTable = false;
    bool m_updatingDeviceOnlineLinkPanel = false;
    bool m_updatingLogicAgcAvcPage = false;
    bool m_updatingLogicComputationPointPage = false;
    bool m_updatingLogicControlRulePage = false;
    bool m_updatingEditorNavigationCombos = false;
    bool m_updatingConfigObjectViews = false;
    bool m_restoringConfigUndo = false;
    bool m_restoringNavigation = false;
    bool m_lastConfigExportOk = false;
    ThemeMode m_themeMode = ThemeMode::Light;
    QPair<int, int> m_navigationCurrentState = qMakePair(-1, -1);
    QList<QPair<int, int>> m_navigationBackStack;
    QList<QPair<int, int>> m_navigationForwardStack;
    int m_logicComputationDragRow = -1;
    int m_modelPointDragRow = -1;
    int m_iec101PointDragRow = -1;
    int m_iec104PointDragRow = -1;
    QFrame *m_modelPointDropLine = nullptr;
    QFrame *m_logicComputationDropLine = nullptr;
    QFrame *m_iec101PointDropLine = nullptr;
    QFrame *m_iec104PointDropLine = nullptr;
    QList<configtool::ConfigProject> m_configUndoStack;
    QList<QJsonObject> m_iec101PointsUndoStack;
    QList<QJsonObject> m_iec104PointsUndoStack;
    QThread *m_programControlThread = nullptr;
    ProgramControlSshWorker *m_programControlWorker = nullptr;
    QProgressDialog *m_programControlUpgradeProgress = nullptr;
    quint64 m_programControlConnectSerial = 0;
    QString m_programControlShellKey;
    bool m_programControlConnected = false;
    quint64 m_programControlCommandSerial = 0;
    bool m_programControlCommandRunning = false;
    ProgramControlCommandKind m_programControlCommandKind = ProgramControlCommandKind::RefreshStatus;
    QString m_programControlCommandToken;
    QString m_programControlCommandTitle;
    QString m_programControlCommandAppName;
    QByteArray m_programControlCommandBuffer;

    QLineEdit *m_ipEdit = nullptr;
    QLineEdit *m_configImportDirEdit = nullptr;
    QTabBar *m_appTabBar = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QPushButton *m_northConnectionStatusBtn = nullptr;
    QPushButton *m_selectConfigImportDirBtn = nullptr;
    QPushButton *m_browseConfigImportDirBtn = nullptr;
    QPushButton *m_openConfigDirBtn = nullptr;
    QPushButton *m_exportIec104ConfigBtn = nullptr;
    QPushButton *m_checkConfigIssuesBtn = nullptr;
    QPushButton *m_uploadConfigBtn = nullptr;
    QPushButton *m_downloadConfigBtn = nullptr;
    QPushButton *m_openNetworkConfigBtn = nullptr;
    QPushButton *m_openLogManagementBtn = nullptr;
    QLineEdit *m_configRemoteBaseDirEdit = nullptr;
    QPushButton *m_newModelBtn = nullptr;
    QPushButton *m_createDeviceBtn = nullptr;
    QPushButton *m_copyDeviceBtn = nullptr;
    QPushButton *m_deleteModelBtn = nullptr;
    QPushButton *m_deleteDeviceBtn = nullptr;
    QPushButton *m_addPointBtn = nullptr;
    QPushButton *m_copyPointBtn = nullptr;
    QPushButton *m_deletePointBtn = nullptr;
    QPushButton *m_autoMergeDlt645FfBtn = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QTextEdit *m_logView = nullptr;
    QLineEdit *m_cmdEdit = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QComboBox *m_deviceFilterCombo = nullptr;
    QTabBar *m_serviceTypeFilterTabBar = nullptr;
    QLineEdit *m_dataRefFilterEdit = nullptr;
    QComboBox *m_autoRefreshCombo = nullptr;
    QLabel *m_controlStatusLabel = nullptr;
    QPushButton *m_refreshDataBtn = nullptr;
    QPushButton *m_sendControlBtn = nullptr;
    QPushButton *m_dataFreezeBtn = nullptr;
    QPushButton *m_rawFrameLogBtn = nullptr;
    QWidget *m_serviceDataFilterWidget = nullptr;
    QStackedWidget *m_dataViewStack = nullptr;
    QWidget *m_serviceDataViewPage = nullptr;
    QTabWidget *m_logicAgcAvcStatusTabs = nullptr;
    QTableWidget *m_dataTable = nullptr;
    QTableWidget *m_configModelTable = nullptr;
    QTableWidget *m_configDeviceTable = nullptr;
    QTableWidget *m_modelPointsTable = nullptr;
    QTableWidget *m_deviceBindingsTable = nullptr;
    QTabWidget *m_mainTabWidget = nullptr;
    QTabWidget *m_northConfigTabWidget = nullptr;
    QTabBar *m_modelPointFilterTabBar = nullptr;
    QTabBar *m_deviceBindingFilterTabBar = nullptr;
    QLineEdit *m_modelPointDataRefFilterEdit = nullptr;
    QLineEdit *m_deviceBindingDataRefFilterEdit = nullptr;
    QGroupBox *m_deviceOnlineLinkGroupBox = nullptr;
    QWidget *m_deviceOnlineLinkContent = nullptr;
    QComboBox *m_deviceOnlineLinkTargetCombo = nullptr;
    QLabel *m_deviceOnlineLinkHintLabel = nullptr;
    QWidget *m_configPage = nullptr;
    QWidget *m_configIssuePage = nullptr;
    QWidget *m_logicAgcAvcPage = nullptr;
    QWidget *m_logicComputationPointPage = nullptr;
    QWidget *m_logicControlRulePage = nullptr;
    QWidget *m_programControlPage = nullptr;
    QWidget *m_networkConfigPage = nullptr;
    QWidget *m_logManagementPage = nullptr;
    QWidget *m_northConfigPage = nullptr;
    QWidget *m_northCepConfigPage = nullptr;
    QWidget *m_northMqttConfigPage = nullptr;
    QWidget *m_iec101ConfigPage = nullptr;
    QWidget *m_iec104ConfigPage = nullptr;
    QWidget *m_modelEditorPage = nullptr;
    QWidget *m_deviceEditorPage = nullptr;
    QGroupBox *m_modelGroupBox = nullptr;
    QGroupBox *m_deviceGroupBox = nullptr;
    QLabel *m_configProjectNameValueLabel = nullptr;
    QLabel *m_configSourceRootValueLabel = nullptr;
    QLabel *m_configModelCountValueLabel = nullptr;
    QLabel *m_configDeviceCountValueLabel = nullptr;
    QLabel *m_configIssueCountValueLabel = nullptr;
    QTableWidget *m_configIssueTable = nullptr;
    QPushButton *m_addLogicComputationPointBtn = nullptr;
    QPushButton *m_copyLogicComputationPointBtn = nullptr;
    QPushButton *m_deleteLogicComputationPointBtn = nullptr;
    QTableWidget *m_logicComputationPointTable = nullptr;
    QPushButton *m_addLogicControlRuleBtn = nullptr;
    QPushButton *m_copyLogicControlRuleBtn = nullptr;
    QPushButton *m_deleteLogicControlRuleBtn = nullptr;
    QPushButton *m_addLogicControlTargetBtn = nullptr;
    QPushButton *m_deleteLogicControlTargetBtn = nullptr;
    QPushButton *m_logicControlTemplateOriginalBtn = nullptr;
    QPushButton *m_logicControlTemplateInvertBtn = nullptr;
    QPushButton *m_logicControlTemplateScaleBtn = nullptr;
    QPushButton *m_logicControlTemplateFixedBtn = nullptr;
    QPushButton *m_insertLogicControlRealtimeRefBtn = nullptr;
    QTableWidget *m_logicControlRuleTable = nullptr;
    QTableWidget *m_logicControlTargetTable = nullptr;
    QLineEdit *m_logicControlPreviewValueEdit = nullptr;
    QLabel *m_logicControlPreviewLabel = nullptr;
    QPushButton *m_refreshProgramStatusBtn = nullptr;
    QPushButton *m_connectProgramControlBtn = nullptr;
    QPushButton *m_disconnectProgramControlBtn = nullptr;
    QPushButton *m_startAllProgramsBtn = nullptr;
    QPushButton *m_stopAllProgramsBtn = nullptr;
    QProgressBar *m_systemStorageProgress = nullptr;
    QLabel *m_systemStorageValueLabel = nullptr;
    QProgressBar *m_appStorageProgress = nullptr;
    QLabel *m_appStorageValueLabel = nullptr;
    QTableWidget *m_programControlTable = nullptr;
    QTableWidget *m_networkInterfaceTable = nullptr;
    QTableWidget *m_networkRouteTable = nullptr;
    QPushButton *m_readNetworkConfigBtn = nullptr;
    QPushButton *m_applyNetworkConfigBtn = nullptr;
    QPushButton *m_addNetworkRouteBtn = nullptr;
    QPushButton *m_deleteNetworkRouteBtn = nullptr;
    QLineEdit *m_networkPingTargetEdit = nullptr;
    QPushButton *m_networkPingBtn = nullptr;
    QTextEdit *m_networkPingOutput = nullptr;
    QString m_loadedNetworkProjectRoot;
    QTableWidget *m_logRetentionTable = nullptr;
    QSpinBox *m_globalLogRetentionDaysSpin = nullptr;
    QSpinBox *m_globalMsgRetentionDaysSpin = nullptr;
    QPushButton *m_logAdvancedRetentionBtn = nullptr;
    QWidget *m_logAdvancedRetentionWidget = nullptr;
    QPushButton *m_saveLogRetentionBtn = nullptr;
    QLabel *m_logRetentionProjectLabel = nullptr;
    QString m_loadedLogRetentionProjectRoot;
    QComboBox *m_logDownloadAppCombo = nullptr;
    QComboBox *m_logDownloadDateCombo = nullptr;
    QComboBox *m_logDownloadTypeCombo = nullptr;
    QLineEdit *m_logDownloadDirectoryEdit = nullptr;
    QPushButton *m_queryDeviceLogsBtn = nullptr;
    QPushButton *m_downloadSelectedLogsBtn = nullptr;
    QPushButton *m_downloadVisibleLogsBtn = nullptr;
    QTableWidget *m_deviceLogFileTable = nullptr;
    QLabel *m_logDownloadSummaryLabel = nullptr;
    QList<DeviceLogFileInfo> m_deviceLogFiles;
    bool m_deviceLogFilesLoaded = false;
    bool m_deviceLogQueryInProgress = false;
    quint64 m_deviceLogQueryId = 0;
    QLineEdit *m_logicAgcAvcGroupIdEdit = nullptr;
    QLineEdit *m_logicAgcAvcVirtualDeviceIdEdit = nullptr;
    QDoubleSpinBox *m_logicMeasurementTotalPEdit = nullptr;
    QDoubleSpinBox *m_logicMeasurementTotalQEdit = nullptr;
    QCheckBox *m_logicGateEnableReverseCheck = nullptr;
    QCheckBox *m_logicGateDistantReverseCheck = nullptr;
    QCheckBox *m_logicGateLockReverseCheck = nullptr;
    QCheckBox *m_logicGateUplockReverseCheck = nullptr;
    QCheckBox *m_logicGateDownlockReverseCheck = nullptr;
    QCheckBox *m_logicGateOpenloopReverseCheck = nullptr;
    QCheckBox *m_logicAgcFollowEnableCheck = nullptr;
    QSpinBox *m_logicAgcFollowPeriodEdit = nullptr;
    QDoubleSpinBox *m_logicAgcFollowStepEdit = nullptr;
    QDoubleSpinBox *m_logicAgcFollowToleranceEdit = nullptr;
    QCheckBox *m_logicAvcFollowEnableCheck = nullptr;
    QSpinBox *m_logicAvcFollowPeriodEdit = nullptr;
    QDoubleSpinBox *m_logicAvcFollowStepEdit = nullptr;
    QDoubleSpinBox *m_logicAvcFollowToleranceEdit = nullptr;
    QTableWidget *m_logicAgcAvcDeviceTable = nullptr;
    QPushButton *m_addLogicAgcAvcDeviceBtn = nullptr;
    QPushButton *m_deleteLogicAgcAvcDeviceBtn = nullptr;
    QLabel *m_modelOverviewIdLabel = nullptr;
    QLabel *m_modelOverviewDisplayNameLabel = nullptr;
    QLabel *m_modelOverviewDeviceTypeLabel = nullptr;
    QLabel *m_modelOverviewVersionLabel = nullptr;
    QLabel *m_modelOverviewPointCountLabel = nullptr;
    QLabel *m_deviceDetailTitleLabel = nullptr;
    QLabel *m_deviceDetailIdLabel = nullptr;
    QLabel *m_deviceDetailModelLabel = nullptr;
    QLabel *m_deviceDetailProtocolLabel = nullptr;
    QLabel *m_deviceDetailAddressLabel = nullptr;
    QLabel *m_deviceDetailIpLabel = nullptr;
    QLabel *m_deviceDetailPortLabel = nullptr;
    QLabel *m_deviceDetailBindingCountLabel = nullptr;
    QLabel *m_modelValidationLabel = nullptr;
    QLabel *m_deviceValidationLabel = nullptr;
    QComboBox *m_modelEditorCombo = nullptr;
    QComboBox *m_deviceEditorCombo = nullptr;
    QLineEdit *m_modelIdEdit = nullptr;
    QComboBox *m_newPointCategoryCombo = nullptr;
    QLineEdit *m_modelDisplayNameEdit = nullptr;
    QLineEdit *m_modelDeviceTypeEdit = nullptr;
    QLineEdit *m_modelVersionEdit = nullptr;
    QLineEdit *m_modelManufacturerIdEdit = nullptr;
    QLineEdit *m_modelManufacturerDescEdit = nullptr;
    QLineEdit *m_modelSchemaEdit = nullptr;
    QCheckBox *m_modelNorthVisibleCheck = nullptr;
    QLineEdit *m_deviceIdEdit = nullptr;
    QLineEdit *m_deviceDescEdit = nullptr;
    QLineEdit *m_deviceModelEdit = nullptr;
    QLineEdit *m_deviceStationAddressEdit = nullptr;
    QLineEdit *m_deviceIpEdit = nullptr;
    QLineEdit *m_devicePortEdit = nullptr;
    QGroupBox *m_modbusGlobalParamsGroupBox = nullptr;
    QLineEdit *m_modbusFrameIntervalEdit = nullptr;
    QComboBox *m_modbusTypeCombo = nullptr;
    QComboBox *m_modbusSerialPortCombo = nullptr;
    QComboBox *m_modbusHwVariantCombo = nullptr;
    QComboBox *m_modbusBaudCombo = nullptr;
    QComboBox *m_modbusDataBitsCombo = nullptr;
    QComboBox *m_modbusStopBitsCombo = nullptr;
    QComboBox *m_modbusParityCombo = nullptr;
    QLineEdit *m_modbusResponseTimeoutEdit = nullptr;
    QCheckBox *m_modbusDebugCheck = nullptr;
    QGroupBox *m_modbusParamsGroupBox = nullptr;
    QComboBox *m_dlt645SerialPortCombo = nullptr;
    QComboBox *m_dlt645HwVariantCombo = nullptr;
    QComboBox *m_dlt645BaudCombo = nullptr;
    QComboBox *m_dlt645DataBitsCombo = nullptr;
    QComboBox *m_dlt645StopBitsCombo = nullptr;
    QComboBox *m_dlt645ParityCombo = nullptr;
    QLineEdit *m_dlt645FrameIntervalEdit = nullptr;
    QLineEdit *m_dlt645UserIdEdit = nullptr;
    QLineEdit *m_dlt645PasswordEdit = nullptr;
    QGroupBox *m_dlt645ParamsGroupBox = nullptr;
    // North_CEP 配置页面控件
    QLineEdit *m_northCepGatewayIdEdit = nullptr;
    QLineEdit *m_northCepGatewayNameEdit = nullptr;
    QLineEdit *m_northCepManagementPortEdit = nullptr;
    QLineEdit *m_northCepDataPortEdit = nullptr;
    // North_Mqtt 配置页面控件
    QLineEdit *m_northMqttGatewayIdEdit = nullptr;
    QLineEdit *m_northMqttBrokerIpEdit = nullptr;
    QLineEdit *m_northMqttPortEdit = nullptr;
    QLineEdit *m_northMqttUsernameEdit = nullptr;
    QLineEdit *m_northMqttPasswordEdit = nullptr;
    // IEC101 配置页面控件
    QComboBox *m_iec101CommModeCombo = nullptr;
    QLineEdit *m_iec101ComAddrEdit = nullptr;
    QLineEdit *m_iec101CodePortEdit = nullptr;
    // 串口连接参数
    QGroupBox *m_iec101SerialParamsGroup = nullptr;
    QLineEdit *m_iec101UsartNameEdit = nullptr;
    QComboBox *m_iec101BaudrateCombo = nullptr;
    QComboBox *m_iec101DataBitCombo = nullptr;
    QComboBox *m_iec101StopBitCombo = nullptr;
    QComboBox *m_iec101ParityCombo = nullptr;
    // 协议参数
    QComboBox *m_iec101CotCombo = nullptr;
    QComboBox *m_iec101CaCombo = nullptr;
    QComboBox *m_iec101IoaCombo = nullptr;
    QComboBox *m_iec101LinkAddrCombo = nullptr;
    QComboBox *m_iec101TelecontrolTypeCombo = nullptr;
    QComboBox *m_iec101TelemetryTypeCombo = nullptr;
    QComboBox *m_iec101SequenceCombo = nullptr;
    QComboBox *m_iec101YxUseDoubleValueCombo = nullptr;
    QComboBox *m_iec101YxAllSTransDFlagCombo = nullptr;
    // 点表
    QTabBar *m_iec101PointFilterTabBar = nullptr;
    QLineEdit *m_iec101PointDataRefFilterEdit = nullptr;
    QTableWidget *m_iec101PointsTable = nullptr;
    QPushButton *m_refreshIec101PointsBtn = nullptr;
    QPushButton *m_sortIec101PointsBtn = nullptr;
    QLabel *m_iec101ValidationLabel = nullptr;
    // 北向 IEC104 配置页面控件
    QLineEdit *m_iec104CodeIpEdit = nullptr;
    QLineEdit *m_iec104CodePortEdit = nullptr;
    QLineEdit *m_iec104ComAddrEdit = nullptr;
    QComboBox *m_iec104CotCombo = nullptr;
    QComboBox *m_iec104CaCombo = nullptr;
    QComboBox *m_iec104IoaCombo = nullptr;
    QComboBox *m_iec104LinkAddrCombo = nullptr;
    QComboBox *m_iec104TelecontrolTypeCombo = nullptr;
    QComboBox *m_iec104TelemetryTypeCombo = nullptr;
    QComboBox *m_iec104SequenceCombo = nullptr;
    QComboBox *m_iec104YxUseDoubleValueCombo = nullptr;
    QComboBox *m_iec104YxAllSTransDFlagCombo = nullptr;
    QLineEdit *m_iec104T0Edit = nullptr;
    QLineEdit *m_iec104T1Edit = nullptr;
    QLineEdit *m_iec104T2Edit = nullptr;
    QLineEdit *m_iec104T3Edit = nullptr;
    QLineEdit *m_iec104KEdit = nullptr;
    QLineEdit *m_iec104WEdit = nullptr;
    QTabBar *m_iec104PointFilterTabBar = nullptr;
    QLineEdit *m_iec104PointDataRefFilterEdit = nullptr;
    QTableWidget *m_iec104PointsTable = nullptr;
    QPushButton *m_refreshIec104PointsBtn = nullptr;
    QPushButton *m_sortIec104PointsBtn = nullptr;
    QLabel *m_iec104ValidationLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_themeToggleBtn = nullptr;
    QLabel *m_versionLabel = nullptr;
};

#endif
