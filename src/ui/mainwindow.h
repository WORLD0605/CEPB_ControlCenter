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
class QSplitter;
class QEvent;
class QFrame;
class QProgressDialog;
class QProcess;

enum class AppViewMode {
    Terminal,
    DataTable
};

struct AppConfig {
    QString name;
    quint16 port = 0;
    QString prompt;
    AppViewMode viewMode = AppViewMode::Terminal;
};

struct ServiceChannelDataItem {
    QString deviceId;
    QString dataRef;
    QString serviceId;
    QString description;
    QString dataTime;
    QString value;
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
        ForceStop,
        Restart,
        EnableAutostart,
        DisableAutostart
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
    void onDescriptionFilterTextChanged(const QString &text);
    void onAutoRefreshIntervalChanged(int index);
    void onSendControlClicked();
    void onDataTableCellDoubleClicked(int row, int column);
    void onDataTableSelectionChanged();
    void onBrowseConfigImportDirClicked();
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
    void onModelPointDescriptionFilterTextChanged(const QString &text);
    void onCreateDeviceFromModelClicked();
    void onDeleteModelClicked();
    void onDeleteDeviceClicked();
    void onDeviceFieldEdited();
    void onDeviceBindingDataRefFilterTextChanged(const QString &text);
    void onDeviceBindingDescriptionFilterTextChanged(const QString &text);
    void onDeviceBindingItemChanged(QTableWidgetItem *item);
    void onLogicAgcAvcBasicEdited();
    void onLogicAgcAvcDeviceItemChanged(QTableWidgetItem *item);
    void onLogicAgcAvcDeviceCellDoubleClicked(int row, int column);
    void onAddLogicAgcAvcDeviceClicked();
    void onDeleteLogicAgcAvcDeviceClicked();
    void onGenerateLogicAgcAvcTotalPClicked();
    void onGenerateLogicAgcAvcTotalQClicked();
    void onGenerateLogicAgcAvcCosClicked();
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
    void onAddLogicOnlineLinkClicked();
    void onDeleteLogicOnlineLinkClicked();
    void onGenerateLogicVirtualOnlineLinksClicked();
    void onLogicOnlineLinkItemChanged(QTableWidgetItem *item);
    void onLogicOnlineLinkCellDoubleClicked(int row, int column);
    void onRefreshProgramStatusClicked();
    void onConnectProgramControlClicked();
    void onDisconnectProgramControlClicked();
    void onStartProgramClicked();
    void onStopProgramClicked();
    void onForceStopProgramClicked();
    void onRestartProgramClicked();
    void onToggleProgramAutostartClicked();
    void onIec101CommModeChanged(int index);
    void onRefreshIec101PointsClicked();
    void onIec101PointItemChanged(QTableWidgetItem *item);
    void onIec101PointFilterChanged(int index);
    void onIec101PointFilterTextChanged();

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
    AppConfig currentAppConfig() const;
    void requestServiceChannelData(bool logRequest = true);
    QList<ServiceChannelDataItem> parseServiceChannelDataReply(const QString &reply) const;
    void populateServiceChannelTable(const QList<ServiceChannelDataItem> &items);
    void refreshDeviceFilterOptions();
    void applyServiceChannelFilter();
    void updateAutoRefreshTimer();
    void updateHighlightRefreshTimer();
    void copySelectedTableCells();
    void updateControlCommandUi();
    void updateServiceChannelDataFreezeUi();
    void openControlCommandDialog(int row);
    void openDataWriteDialog(int row);
    bool handleControlResponseLogLine(const QString &line);
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
    void sendServiceChannelDataFreezeCommand(const QString &mode);
    void pasteClipboardIntoModelPointsTable();
    void pasteClipboardIntoDeviceBindingsTable();
    void pasteClipboardIntoIec101PointsTable();
    void pushIec101PointsUndoSnapshot();
    void undoIec101PointsLastEdit();
    void applyModelPointCellText(int row, int column, const QString &text);
    void applyDeviceBindingCellText(int row, int column, const QString &text);
    void rebuildModbusDeviceConfig(configtool::ProtocolDeviceInstance &device);
    void pushConfigUndoSnapshot();
    void undoLastConfigEdit();
    QString serviceChannelItemKey(const ServiceChannelDataItem &item) const;
    QString normalizedConfigProjectRoot(const QString &selectedPath) const;
    QString resolveIec104AppDir(const QString &projectRoot) const;
    QString resolveModbusAppDir(const QString &projectRoot) const;
    QString resolveLogicCenterAppDir(const QString &projectRoot) const;
    QString resolveIec101ServiceChannelAppDir(const QString &projectRoot) const;
    QJsonObject serializeIec101LocalhostConfig() const;
    void loadIec101LocalhostConfigFromFile(const QString &filePath);
    void loadIec101LocalhostConfigFromJson(const QJsonObject &root);
    void clearIec101ConfigPage();
    void refreshIec101PointsFromDevices(const QHash<QString, QJsonObject> &savedPointSettings = {});
    void applyIec101PointsFilter();
    QString configBrowseStartDir() const;
    QStringList configTransferRelativePaths(const QString &projectRoot) const;
    QString configRemoteTarget() const;
    QString configRemoteBaseDir() const;
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
    void refreshLogicOnlineLinkPage();
    void refreshSelectionOverview();
    void refreshEditorNavigationCombos();
    void refreshModelDetail(int modelIndex);
    void refreshModelOverview(int modelIndex);
    void refreshDeviceDetail(int deviceIndex);
    void refreshDeviceEditor(int deviceIndex);
    int renameModelReferences(const QString &oldModelId, const QString &newModelId);
    int renameModelPointReferences(const QString &modelId,
                                   const QString &oldDataRef,
                                   const QString &newDataRef);
    int renameLogicDeviceReferences(const QString &oldDeviceId, const QString &newDeviceId);
    void selectModelPointById(const QString &pointId);
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
    void selectLogicOnlineLinkDevice(int row, int column);
    int currentLogicControlRuleIndex() const;
    void refreshLogicControlTargetTable();
    void refreshLogicControlPreview();
    QStringList managedProgramAppNames() const;
    QString programServiceName(const QString &appName) const;
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
    bool openProgramControlShell(const QString &title,
                                 QString *output,
                                 QProgressDialog *progress);
    void updateProgramControlConnectionUi(bool connected);
    void updateProgramControlBusyUi(bool busy);
    void startProgramStatusRefresh();
    void refreshProgramControlTable(const QString &statusOutput);
    void setProgramControlRowPending(const QString &appName, const QString &statusText);
    ThemeMode loadThemeMode() const;
    void saveThemeMode(ThemeMode mode) const;
    void applyTheme(ThemeMode mode);
    void onThemeToggleClicked();
    QColor serviceChannelDefaultTextColor() const;
    QColor serviceChannelChangedTextColor() const;

    DebugConsoleClient *m_client = nullptr;
    configtool::ConfigProjectManager m_configProjectManager;
    QTimer *m_autoRefreshTimer = nullptr;
    QTimer *m_highlightRefreshTimer = nullptr;
    QTimer *m_controlResponseTimer = nullptr;
    QList<AppConfig> m_appConfigs;
    QList<ServiceChannelDataItem> m_serviceChannelItems;
    QHash<QString, ServiceChannelDataItem> m_previousServiceChannelItemMap;
    QHash<QString, QDateTime> m_timeHighlightUntilMap;
    QHash<QString, QDateTime> m_valueHighlightUntilMap;
    QHash<QString, QString> m_controlStatusTextMap;
    QHash<QString, QColor> m_controlStatusColorMap;
    bool m_updatingModelPointsTable = false;
    bool m_updatingModelPointCategory = false;
    bool m_updatingDeviceBindingsTable = false;
    bool m_updatingLogicAgcAvcPage = false;
    bool m_updatingLogicComputationPointPage = false;
    bool m_updatingLogicControlRulePage = false;
    bool m_updatingLogicOnlineLinkPage = false;
    bool m_updatingEditorNavigationCombos = false;
    bool m_restoringConfigUndo = false;
    bool m_restoringNavigation = false;
    bool m_lastConfigExportOk = false;
    ThemeMode m_themeMode = ThemeMode::Light;
    QPair<int, int> m_navigationCurrentState = qMakePair(-1, -1);
    QList<QPair<int, int>> m_navigationBackStack;
    QList<QPair<int, int>> m_navigationForwardStack;
    int m_logicComputationDragRow = -1;
    int m_modelPointDragRow = -1;
    QFrame *m_modelPointDropLine = nullptr;
    QFrame *m_logicComputationDropLine = nullptr;
    QList<configtool::ConfigProject> m_configUndoStack;
    QList<QJsonObject> m_iec101PointsUndoStack;
    QProcess *m_programControlShell = nullptr;
    QString m_programControlShellKey;
    quint64 m_programControlCommandSerial = 0;
    bool m_programControlCommandRunning = false;
    ProgramControlCommandKind m_programControlCommandKind = ProgramControlCommandKind::RefreshStatus;
    QString m_programControlCommandToken;
    QString m_programControlCommandTitle;
    QString m_programControlCommandAppName;
    QByteArray m_programControlCommandBuffer;
    QString m_pendingDataTableCommand;
    bool m_waitingControlResponse = false;
    QString m_pendingControlDeviceId;
    QString m_pendingControlDataRef;
    QString m_pendingControlValue;
    int m_pendingControlType = -1;
    QString m_pendingDataWriteDeviceId;
    QString m_pendingDataWriteDataRef;
    QString m_pendingDataWriteValue;
    QString m_pendingDataWriteQuality;
    QString m_pendingDataFreezeMode;
    bool m_serviceChannelDataFrozen = false;

    QLineEdit *m_ipEdit = nullptr;
    QLineEdit *m_configImportDirEdit = nullptr;
    QComboBox *m_appCombo = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QPushButton *m_selectConfigImportDirBtn = nullptr;
    QPushButton *m_browseConfigImportDirBtn = nullptr;
    QPushButton *m_exportIec104ConfigBtn = nullptr;
    QPushButton *m_checkConfigIssuesBtn = nullptr;
    QPushButton *m_uploadConfigBtn = nullptr;
    QPushButton *m_downloadConfigBtn = nullptr;
    QLineEdit *m_configRemoteHostEdit = nullptr;
    QLineEdit *m_configRemoteUserEdit = nullptr;
    QLineEdit *m_configRemotePasswordEdit = nullptr;
    QSpinBox *m_configRemotePortEdit = nullptr;
    QLineEdit *m_configRemoteBaseDirEdit = nullptr;
    QPushButton *m_newModelBtn = nullptr;
    QPushButton *m_createDeviceFromModelBtn = nullptr;
    QPushButton *m_deleteModelBtn = nullptr;
    QPushButton *m_deleteDeviceBtn = nullptr;
    QPushButton *m_addPointBtn = nullptr;
    QPushButton *m_copyPointBtn = nullptr;
    QPushButton *m_deletePointBtn = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QTextEdit *m_logView = nullptr;
    QLineEdit *m_cmdEdit = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QComboBox *m_deviceFilterCombo = nullptr;
    QComboBox *m_serviceTypeFilterCombo = nullptr;
    QLineEdit *m_dataRefFilterEdit = nullptr;
    QLineEdit *m_descriptionFilterEdit = nullptr;
    QComboBox *m_autoRefreshCombo = nullptr;
    QLabel *m_controlStatusLabel = nullptr;
    QPushButton *m_refreshDataBtn = nullptr;
    QPushButton *m_sendControlBtn = nullptr;
    QPushButton *m_dataFreezeBtn = nullptr;
    QTableWidget *m_dataTable = nullptr;
    QTableWidget *m_configModelTable = nullptr;
    QTableWidget *m_configDeviceTable = nullptr;
    QTableWidget *m_modelPointsTable = nullptr;
    QTableWidget *m_deviceBindingsTable = nullptr;
    QTabWidget *m_mainTabWidget = nullptr;
    QTabBar *m_modelPointFilterTabBar = nullptr;
    QLineEdit *m_modelPointDataRefFilterEdit = nullptr;
    QLineEdit *m_modelPointDescriptionFilterEdit = nullptr;
    QLineEdit *m_deviceBindingDataRefFilterEdit = nullptr;
    QLineEdit *m_deviceBindingDescriptionFilterEdit = nullptr;
    QWidget *m_configPage = nullptr;
    QWidget *m_configIssuePage = nullptr;
    QWidget *m_logicCenterPage = nullptr;
    QWidget *m_logicAgcAvcPage = nullptr;
    QWidget *m_logicComputationPointPage = nullptr;
    QWidget *m_logicControlRulePage = nullptr;
    QWidget *m_logicOnlineLinkPage = nullptr;
    QWidget *m_programControlPage = nullptr;
    QWidget *m_iec101ConfigPage = nullptr;
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
    QLabel *m_logicComputationPointCountLabel = nullptr;
    QLabel *m_logicControlRuleCountLabel = nullptr;
    QLabel *m_logicAgcAvcGroupCountLabel = nullptr;
    QLabel *m_logicOnlineLinkCountLabel = nullptr;
    QLabel *m_logicDerivedDeviceCountLabel = nullptr;
    QLabel *m_logicIssueCountLabel = nullptr;
    QLabel *m_logicExportPathLabel = nullptr;
    QTableWidget *m_logicIssueTable = nullptr;
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
    QPushButton *m_addLogicOnlineLinkBtn = nullptr;
    QPushButton *m_deleteLogicOnlineLinkBtn = nullptr;
    QPushButton *m_generateLogicVirtualOnlineLinksBtn = nullptr;
    QTableWidget *m_logicOnlineLinkTable = nullptr;
    QPushButton *m_refreshProgramStatusBtn = nullptr;
    QLineEdit *m_programRemoteHostEdit = nullptr;
    QLineEdit *m_programRemoteUserEdit = nullptr;
    QLineEdit *m_programRemotePasswordEdit = nullptr;
    QSpinBox *m_programRemotePortEdit = nullptr;
    QPushButton *m_connectProgramControlBtn = nullptr;
    QPushButton *m_disconnectProgramControlBtn = nullptr;
    QTableWidget *m_programControlTable = nullptr;
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
    QPushButton *m_generateLogicTotalPBtn = nullptr;
    QPushButton *m_generateLogicTotalQBtn = nullptr;
    QPushButton *m_generateLogicCosBtn = nullptr;
    QLabel *m_modelOverviewIdLabel = nullptr;
    QLabel *m_modelOverviewDisplayNameLabel = nullptr;
    QLabel *m_modelOverviewDeviceTypeLabel = nullptr;
    QLabel *m_modelOverviewVersionLabel = nullptr;
    QLabel *m_modelOverviewPointCountLabel = nullptr;
    QLabel *m_deviceDetailTitleLabel = nullptr;
    QLabel *m_deviceDetailModelLabel = nullptr;
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
    QComboBox *m_modbusTypeCombo = nullptr;
    QComboBox *m_modbusSerialPortCombo = nullptr;
    QComboBox *m_modbusHwVariantCombo = nullptr;
    QComboBox *m_modbusBaudCombo = nullptr;
    QComboBox *m_modbusDataBitsCombo = nullptr;
    QComboBox *m_modbusStopBitsCombo = nullptr;
    QComboBox *m_modbusParityCombo = nullptr;
    QCheckBox *m_modbusDebugCheck = nullptr;
    QGroupBox *m_modbusParamsGroupBox = nullptr;
    // IEC101 配置页面控件
    QComboBox *m_iec101CommModeCombo = nullptr;
    QLineEdit *m_iec101ComAddrEdit = nullptr;
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
    QLineEdit *m_iec101PointDescriptionFilterEdit = nullptr;
    QTableWidget *m_iec101PointsTable = nullptr;
    QPushButton *m_refreshIec101PointsBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_themeToggleBtn = nullptr;
    QLabel *m_versionLabel = nullptr;
};

#endif
