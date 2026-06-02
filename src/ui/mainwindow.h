#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QList>
#include "config/config_project_manager.h"
#include "network/debug_console_client.h"

class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QLabel;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class QTimer;

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
    QString serviceChannelItemKey(const ServiceChannelDataItem &item) const;
    void refreshConfigImportSummary(const configtool::ImportReport &report);

    DebugConsoleClient *m_client = nullptr;
    configtool::ConfigProjectManager m_configProjectManager;
    QTimer *m_autoRefreshTimer = nullptr;
    QTimer *m_highlightRefreshTimer = nullptr;
    QList<AppConfig> m_appConfigs;
    QList<ServiceChannelDataItem> m_serviceChannelItems;
    QHash<QString, ServiceChannelDataItem> m_previousServiceChannelItemMap;
    QHash<QString, QDateTime> m_timeHighlightUntilMap;
    QHash<QString, QDateTime> m_valueHighlightUntilMap;

    QLineEdit *m_ipEdit = nullptr;
    QLineEdit *m_configImportDirEdit = nullptr;
    QComboBox *m_appCombo = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QPushButton *m_browseConfigImportDirBtn = nullptr;
    QPushButton *m_importIec104ConfigBtn = nullptr;
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
    QTabWidget *m_mainTabWidget = nullptr;
    QLabel *m_configProjectNameValueLabel = nullptr;
    QLabel *m_configSourceRootValueLabel = nullptr;
    QLabel *m_configModelCountValueLabel = nullptr;
    QLabel *m_configDeviceCountValueLabel = nullptr;
    QLabel *m_configIssueCountValueLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif
