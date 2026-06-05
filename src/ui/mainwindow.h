#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPair>
#include <QSet>
#include <QHash>
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onQuickCommandClicked();
    void onAppSelectionChanged(int index);
    void onDeviceFilterChanged(int index);
    void onDataRefFilterTextChanged(const QString &text);
    void onDescriptionFilterTextChanged(const QString &text);
    void onAutoRefreshIntervalChanged(int index);
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
    void onGenerateLogicDerivedOnlineLinkClicked();
    void onLogicOnlineLinkItemChanged(QTableWidgetItem *item);
    void onLogicOnlineLinkCellDoubleClicked(int row, int column);

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
    AppConfig currentAppConfig() const;
    void requestServiceChannelData(bool logRequest = true);
    QList<ServiceChannelDataItem> parseServiceChannelDataReply(const QString &reply) const;
    void populateServiceChannelTable(const QList<ServiceChannelDataItem> &items);
    void refreshDeviceFilterOptions();
    void applyServiceChannelFilter();
    void updateAutoRefreshTimer();
    void updateHighlightRefreshTimer();
    void copySelectedTableCells();
    void pasteClipboardIntoModelPointsTable();
    void pasteClipboardIntoDeviceBindingsTable();
    void applyModelPointCellText(int row, int column, const QString &text);
    void applyDeviceBindingCellText(int row, int column, const QString &text);
    void rebuildModbusDeviceConfig(configtool::ProtocolDeviceInstance &device);
    int applyIec104SameChannelDerivedDeviceMappings();
    void pushConfigUndoSnapshot();
    void undoLastConfigEdit();
    QString serviceChannelItemKey(const ServiceChannelDataItem &item) const;
    QString normalizedConfigProjectRoot(const QString &selectedPath) const;
    QString resolveIec104AppDir(const QString &projectRoot) const;
    QString resolveModbusAppDir(const QString &projectRoot) const;
    QString resolveLogicCenterAppDir(const QString &projectRoot) const;
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
    void generateLogicDerivedDeviceMappingVisual();
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

    DebugConsoleClient *m_client = nullptr;
    configtool::ConfigProjectManager m_configProjectManager;
    QTimer *m_autoRefreshTimer = nullptr;
    QTimer *m_highlightRefreshTimer = nullptr;
    QList<AppConfig> m_appConfigs;
    QList<ServiceChannelDataItem> m_serviceChannelItems;
    QHash<QString, ServiceChannelDataItem> m_previousServiceChannelItemMap;
    QHash<QString, QDateTime> m_timeHighlightUntilMap;
    QHash<QString, QDateTime> m_valueHighlightUntilMap;
    bool m_updatingModelPointsTable = false;
    bool m_updatingModelPointCategory = false;
    bool m_updatingDeviceBindingsTable = false;
    bool m_updatingLogicAgcAvcPage = false;
    bool m_updatingLogicComputationPointPage = false;
    bool m_updatingLogicControlRulePage = false;
    bool m_updatingLogicOnlineLinkPage = false;
    bool m_restoringConfigUndo = false;
    bool m_lastConfigExportOk = false;
    int m_logicComputationDragRow = -1;
    int m_modelPointDragRow = -1;
    QFrame *m_modelPointDropLine = nullptr;
    QFrame *m_logicComputationDropLine = nullptr;
    QList<configtool::ConfigProject> m_configUndoStack;

    QLineEdit *m_ipEdit = nullptr;
    QLineEdit *m_configImportDirEdit = nullptr;
    QComboBox *m_appCombo = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
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
    QLineEdit *m_dataRefFilterEdit = nullptr;
    QLineEdit *m_descriptionFilterEdit = nullptr;
    QComboBox *m_autoRefreshCombo = nullptr;
    QPushButton *m_refreshDataBtn = nullptr;
    QTableWidget *m_dataTable = nullptr;
    QTableWidget *m_configModelTable = nullptr;
    QTableWidget *m_configDeviceTable = nullptr;
    QTableWidget *m_modelPointsTable = nullptr;
    QTableWidget *m_deviceBindingsTable = nullptr;
    QTabWidget *m_mainTabWidget = nullptr;
    QTabBar *m_modelPointFilterTabBar = nullptr;
    QCheckBox *m_showAutoDerivedModelPointsCheck = nullptr;
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
    QPushButton *m_generateLogicDerivedOnlineLinkBtn = nullptr;
    QTableWidget *m_logicOnlineLinkTable = nullptr;
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
    QLabel *m_statusLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
};

#endif
