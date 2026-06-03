#include "ui/point_selector_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

constexpr int TypeRole = Qt::UserRole;
constexpr int HasTypeRole = Qt::UserRole + 1;

QString pointKey(const QString &deviceId, const QString &dataRef)
{
    return deviceId.trimmed() + QLatin1Char('#') + dataRef.trimmed();
}

} // namespace

PointSelectorDialog::PointSelectorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("选择点位"));
    resize(920, 560);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(QStringLiteral("设备:"), this));
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(220);
    filterLayout->addWidget(m_deviceCombo);

    filterLayout->addWidget(new QLabel(QStringLiteral("类型:"), this));
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("全部"), -1);
    m_typeCombo->addItem(QStringLiteral("遥测"), static_cast<int>(configtool::ModelServiceType::Measurement));
    m_typeCombo->addItem(QStringLiteral("遥信"), static_cast<int>(configtool::ModelServiceType::Status));
    m_typeCombo->addItem(QStringLiteral("控制"), static_cast<int>(configtool::ModelServiceType::Control));
    filterLayout->addWidget(m_typeCombo);

    filterLayout->addWidget(new QLabel(QStringLiteral("搜索:"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(QStringLiteral("DeviceId / DataRef / 描述 / 模型"));
    filterLayout->addWidget(m_searchEdit, 1);
    mainLayout->addLayout(filterLayout);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("设备"),
        QStringLiteral("设备描述"),
        QStringLiteral("模型"),
        QStringLiteral("类型"),
        QStringLiteral("DataRef"),
        QStringLiteral("描述")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setColumnWidth(0, 140);
    m_table->setColumnWidth(1, 160);
    m_table->setColumnWidth(2, 140);
    m_table->setColumnWidth(3, 70);
    m_table->setColumnWidth(4, 300);
    mainLayout->addWidget(m_table, 1);

    auto *bottomLayout = new QHBoxLayout();
    m_summaryLabel = new QLabel(QStringLiteral("未加载配置工程"), this);
    bottomLayout->addWidget(m_summaryLabel, 1);
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("选择"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    bottomLayout->addWidget(m_buttonBox);
    mainLayout->addLayout(bottomLayout);

    connect(m_deviceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PointSelectorDialog::refreshPointTable);
    connect(m_typeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PointSelectorDialog::refreshPointTable);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &PointSelectorDialog::refreshPointTable);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &PointSelectorDialog::updateSelectionState);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { acceptSelection(); });
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &PointSelectorDialog::acceptSelection);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void PointSelectorDialog::setProject(const configtool::ConfigProject *project)
{
    m_project = project;
    rebuildDeviceFilter();
    refreshPointTable();
}

void PointSelectorDialog::setServiceTypeFilter(configtool::ModelServiceType type)
{
    const int index = m_typeCombo->findData(static_cast<int>(type));
    if (index >= 0) {
        m_typeCombo->setCurrentIndex(index);
    }
}

PointSelectorDialog::SelectedPoint PointSelectorDialog::selectedPoint() const
{
    return m_selectedPoint;
}

void PointSelectorDialog::rebuildDeviceFilter()
{
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    m_deviceCombo->addItem(QStringLiteral("全部设备"), QString());
    if (m_project) {
        for (const configtool::ProtocolDeviceInstance &device : m_project->devices) {
            const QString label = device.deviceDesc.trimmed().isEmpty()
                ? device.deviceId
                : QStringLiteral("%1  %2").arg(device.deviceId, device.deviceDesc);
            m_deviceCombo->addItem(label, device.deviceId);
        }
    }
    m_deviceCombo->blockSignals(false);
}

QList<PointSelectorDialog::PointRow> PointSelectorDialog::collectPointRows() const
{
    QList<PointRow> rows;
    if (!m_project) {
        return rows;
    }

    QSet<QString> seenKeys;
    for (const configtool::ProtocolDeviceInstance &device : m_project->devices) {
        const configtool::ModelTemplate *model = findModelById(device.modelId);
        if (model) {
            for (const configtool::ServiceTemplate &service : model->services) {
                for (const configtool::PointTemplate &point : service.points) {
                    const QString dataRef = point.dataRef().trimmed();
                    if (dataRef.isEmpty()) {
                        continue;
                    }
                    PointRow row;
                    row.deviceId = device.deviceId;
                    row.deviceDesc = device.deviceDesc;
                    row.modelId = device.modelId;
                    row.dataRef = dataRef;
                    row.description = point.description;
                    row.serviceType = service.type;
                    row.hasServiceType = true;
                    rows.append(row);
                    seenKeys.insert(pointKey(row.deviceId, row.dataRef));
                }
            }
        }

        for (const configtool::PointBinding &binding : device.bindings) {
            const QString dataRef = binding.dataRef.trimmed();
            if (dataRef.isEmpty() || seenKeys.contains(pointKey(device.deviceId, dataRef))) {
                continue;
            }
            PointRow row;
            row.deviceId = device.deviceId;
            row.deviceDesc = device.deviceDesc;
            row.modelId = device.modelId;
            row.dataRef = dataRef;
            row.description = binding.descriptionOverride;
            row.hasServiceType = false;
            rows.append(row);
            seenKeys.insert(pointKey(row.deviceId, row.dataRef));
        }
    }

    return rows;
}

const configtool::ModelTemplate *PointSelectorDialog::findModelById(const QString &modelId) const
{
    if (!m_project) {
        return nullptr;
    }

    for (const configtool::ModelTemplate &model : m_project->models) {
        if (model.modelId == modelId) {
            return &model;
        }
    }
    return nullptr;
}

QString PointSelectorDialog::serviceTypeDisplayName(configtool::ModelServiceType type, bool hasServiceType)
{
    if (!hasServiceType) {
        return QStringLiteral("绑定");
    }

    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return QStringLiteral("遥测");
    case configtool::ModelServiceType::Status:
        return QStringLiteral("遥信");
    case configtool::ModelServiceType::Control:
        return QStringLiteral("控制");
    }

    return QStringLiteral("遥测");
}

bool PointSelectorDialog::rowMatchesSearch(const PointRow &row, const QString &keyword)
{
    if (keyword.trimmed().isEmpty()) {
        return true;
    }

    const QString needle = keyword.trimmed();
    return row.deviceId.contains(needle, Qt::CaseInsensitive)
        || row.deviceDesc.contains(needle, Qt::CaseInsensitive)
        || row.modelId.contains(needle, Qt::CaseInsensitive)
        || row.dataRef.contains(needle, Qt::CaseInsensitive)
        || row.description.contains(needle, Qt::CaseInsensitive);
}

void PointSelectorDialog::refreshPointTable()
{
    m_selectedPoint = SelectedPoint();
    m_table->setRowCount(0);

    const QString selectedDeviceId = m_deviceCombo->currentData().toString();
    const int selectedType = m_typeCombo->currentData().toInt();
    const QString keyword = m_searchEdit->text();
    const QList<PointRow> rows = collectPointRows();

    int visibleRow = 0;
    for (const PointRow &row : rows) {
        if (!selectedDeviceId.isEmpty() && row.deviceId != selectedDeviceId) {
            continue;
        }
        if (selectedType >= 0
            && (!row.hasServiceType || static_cast<int>(row.serviceType) != selectedType)) {
            continue;
        }
        if (!rowMatchesSearch(row, keyword)) {
            continue;
        }

        m_table->insertRow(visibleRow);
        auto *deviceItem = new QTableWidgetItem(row.deviceId);
        deviceItem->setData(TypeRole, static_cast<int>(row.serviceType));
        deviceItem->setData(HasTypeRole, row.hasServiceType);
        m_table->setItem(visibleRow, 0, deviceItem);
        m_table->setItem(visibleRow, 1, new QTableWidgetItem(row.deviceDesc));
        m_table->setItem(visibleRow, 2, new QTableWidgetItem(row.modelId));
        m_table->setItem(visibleRow, 3, new QTableWidgetItem(serviceTypeDisplayName(row.serviceType, row.hasServiceType)));
        m_table->setItem(visibleRow, 4, new QTableWidgetItem(row.dataRef));
        m_table->setItem(visibleRow, 5, new QTableWidgetItem(row.description));
        ++visibleRow;
    }

    m_summaryLabel->setText(QStringLiteral("显示 %1 个点位，工程总设备 %2 个")
        .arg(m_table->rowCount())
        .arg(m_project ? m_project->devices.size() : 0));
    updateSelectionState();
}

void PointSelectorDialog::acceptSelection()
{
    updateSelectionState();
    if (!m_selectedPoint.valid) {
        return;
    }
    accept();
}

void PointSelectorDialog::updateSelectionState()
{
    m_selectedPoint = SelectedPoint();

    const int row = m_table->currentRow();
    if (row >= 0) {
        QTableWidgetItem *deviceItem = m_table->item(row, 0);
        QTableWidgetItem *modelItem = m_table->item(row, 2);
        QTableWidgetItem *dataRefItem = m_table->item(row, 4);
        QTableWidgetItem *descriptionItem = m_table->item(row, 5);
        if (deviceItem && dataRefItem) {
            m_selectedPoint.deviceId = deviceItem->text();
            m_selectedPoint.modelId = modelItem ? modelItem->text() : QString();
            m_selectedPoint.dataRef = dataRefItem->text();
            m_selectedPoint.description = descriptionItem ? descriptionItem->text() : QString();
            m_selectedPoint.serviceType = static_cast<configtool::ModelServiceType>(
                deviceItem->data(TypeRole).toInt());
            m_selectedPoint.valid = !m_selectedPoint.deviceId.trimmed().isEmpty()
                && !m_selectedPoint.dataRef.trimmed().isEmpty();
        }
    }

    if (m_buttonBox) {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_selectedPoint.valid);
    }
}
