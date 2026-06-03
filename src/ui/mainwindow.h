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
class QPushButton;
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
    void onConfigModelSelectionChanged();
    void onConfigDeviceSelectionChanged();
    void onConfigModelActivated(int row, int column);
    void onConfigDeviceActivated(int row, int column);
    void onNewModelClicked();
    void onModelFieldEdited();
    void onAddPointClicked();
    void onCopyPointClicked();
    void onDeletePointClicked();
    void onModelPointItemChanged(QTableWidgetItem *item);
    void onModelPointCategoryChanged(int index);
    void onModelPointFilterChanged(int index);
    void onCreateDeviceFromModelClicked();
    void onDeviceFieldEdited();
    void onDeviceBindingItemChanged(QTableWidgetItem *item);

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
    void pushConfigUndoSnapshot();
    void undoLastConfigEdit();
    QString serviceChannelItemKey(const ServiceChannelDataItem &item) const;
    QString normalizedConfigProjectRoot(const QString &selectedPath) const;
    QString resolveIec104AppDir(const QString &projectRoot) const;
    QString resolveModbusAppDir(const QString &projectRoot) const;
    QString configBrowseStartDir() const;
    void refreshConfigImportSummary(const configtool::ImportReport &report);
    void refreshConfigObjectViews();
    void refreshSelectionOverview();
    void refreshModelDetail(int modelIndex);
    void refreshModelOverview(int modelIndex);
    void refreshDeviceDetail(int deviceIndex);
    void refreshDeviceEditor(int deviceIndex);
    void selectModelPointById(const QString &pointId);
    int currentConfigModelIndex() const;
    int currentConfigDeviceIndex() const;
    QPair<int, int> currentModelPointLocation() const;
    QSet<QString> duplicateDataRefsForModel(const configtool::ModelTemplate &model) const;
    QSet<QString> duplicateBindingAddresses(const configtool::ProtocolDeviceInstance &device) const;

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
    bool m_restoringConfigUndo = false;
    QList<configtool::ConfigProject> m_configUndoStack;

    QLineEdit *m_ipEdit = nullptr;
    QLineEdit *m_configImportDirEdit = nullptr;
    QComboBox *m_appCombo = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QPushButton *m_browseConfigImportDirBtn = nullptr;
    QPushButton *m_exportIec104ConfigBtn = nullptr;
    QPushButton *m_newModelBtn = nullptr;
    QPushButton *m_createDeviceFromModelBtn = nullptr;
    QPushButton *m_addPointBtn = nullptr;
    QPushButton *m_copyPointBtn = nullptr;
    QPushButton *m_deletePointBtn = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QTextEdit *m_logView = nullptr;
    QTextEdit *m_configImportReportView = nullptr;
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
    QWidget *m_configPage = nullptr;
    QWidget *m_modelEditorPage = nullptr;
    QWidget *m_deviceEditorPage = nullptr;
    QGroupBox *m_modelGroupBox = nullptr;
    QGroupBox *m_deviceGroupBox = nullptr;
    QLabel *m_configProjectNameValueLabel = nullptr;
    QLabel *m_configSourceRootValueLabel = nullptr;
    QLabel *m_configModelCountValueLabel = nullptr;
    QLabel *m_configDeviceCountValueLabel = nullptr;
    QLabel *m_configIssueCountValueLabel = nullptr;
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
    QLabel *m_deviceCompatIpbLabel = nullptr;
    QLineEdit *m_modelIdEdit = nullptr;
    QComboBox *m_newPointCategoryCombo = nullptr;
    QLineEdit *m_modelDisplayNameEdit = nullptr;
    QLineEdit *m_modelDeviceTypeEdit = nullptr;
    QLineEdit *m_modelVersionEdit = nullptr;
    QLineEdit *m_modelManufacturerIdEdit = nullptr;
    QLineEdit *m_modelManufacturerDescEdit = nullptr;
    QLineEdit *m_modelSchemaEdit = nullptr;
    QLineEdit *m_deviceIdEdit = nullptr;
    QLineEdit *m_deviceDescEdit = nullptr;
    QLineEdit *m_deviceModelEdit = nullptr;
    QLineEdit *m_deviceStationAddressEdit = nullptr;
    QLineEdit *m_deviceIpEdit = nullptr;
    QLineEdit *m_devicePortEdit = nullptr;
    QLineEdit *m_deviceChannelEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
};

#endif
