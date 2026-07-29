#ifndef UI_DATA_UPLOAD_POLICY_EDITOR_H
#define UI_DATA_UPLOAD_POLICY_EDITOR_H

#include <QJsonObject>
#include <QWidget>

#include "config/config_domain.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTabBar;
class QTableWidget;

class DataUploadPolicyEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DataUploadPolicyEditor(QWidget *parent = nullptr);

    void setProject(const configtool::ConfigProject *project,
                    const QJsonObject &savedPolicies);
    QJsonObject policies() const;

    static QJsonObject importBusinessPolicies(const QJsonObject &subData);
    static QJsonObject buildSubDataConfig(const configtool::ConfigProject &project,
                                          const QJsonObject &savedPolicies);
    static int candidateCount(const configtool::ConfigProject &project);

signals:
    void policiesChanged(const QJsonObject &policies);

private slots:
    void applyFilter();
    void notifyPoliciesChanged();
    void toggleAllSpont();
    void setAllPeriods();
    void setAllDeadZoneTypes();
    void setAllDeadZoneValues();

private:
    void populateRows();
    void updateBulkButtonText();

    const configtool::ConfigProject *m_project = nullptr;
    QJsonObject m_savedPolicies;
    QLineEdit *m_filterEdit = nullptr;
    QPushButton *m_spontToggleButton = nullptr;
    QTabBar *m_typeTabBar = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_summaryLabel = nullptr;
    bool m_populating = false;
};

#endif
