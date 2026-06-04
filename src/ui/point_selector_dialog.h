#ifndef UI_POINT_SELECTOR_DIALOG_H
#define UI_POINT_SELECTOR_DIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

#include "config/config_domain.h"

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;

class PointSelectorDialog : public QDialog
{
    Q_OBJECT

public:
    struct SelectedPoint {
        QString deviceId;
        QString dataRef;
        QString description;
        QString modelId;
        configtool::ModelServiceType serviceType = configtool::ModelServiceType::Measurement;
        bool valid = false;
    };

    explicit PointSelectorDialog(QWidget *parent = nullptr);

    void setProject(const configtool::ConfigProject *project);
    void setServiceTypeFilter(configtool::ModelServiceType type);
    SelectedPoint selectedPoint() const;

private slots:
    void refreshPointTable();
    void acceptSelection();
    void updateSelectionState();

private:
    struct PointRow {
        QString deviceId;
        QString deviceDesc;
        QString modelId;
        QString dataRef;
        QString description;
        configtool::ModelServiceType serviceType = configtool::ModelServiceType::Measurement;
        bool hasServiceType = false;
    };

    void rebuildDeviceFilter();
    void restoreFilterState();
    void saveFilterState() const;
    QList<PointRow> collectPointRows() const;
    const configtool::ModelTemplate *findModelById(const QString &modelId) const;
    static QString serviceTypeDisplayName(configtool::ModelServiceType type, bool hasServiceType);
    static bool rowMatchesSearch(const PointRow &row, const QString &keyword);

    const configtool::ConfigProject *m_project = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    SelectedPoint m_selectedPoint;
};

#endif
