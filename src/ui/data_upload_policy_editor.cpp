#include "ui/data_upload_policy_editor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleValidator>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

constexpr int DeviceIdColumn = 0;
constexpr int DeviceDescColumn = 1;
constexpr int ServiceColumn = 2;
constexpr int DataRefColumn = 3;
constexpr int PointDescColumn = 4;
constexpr int PeriodColumn = 5;
constexpr int SpontColumn = 6;
constexpr int DzTypeColumn = 7;
constexpr int DzValueColumn = 8;
constexpr int ColumnCount = 9;

struct UploadPoint {
    QString deviceId;
    QString deviceDesc;
    QString serviceId;
    configtool::ModelServiceType serviceType = configtool::ModelServiceType::Measurement;
    QString dataRef;
    QString pointDesc;
    QString deadZoneType;
    QString deadZoneValue;
};

QString uploadKey(const QString &deviceId, const QString &dataRef)
{
    return deviceId.trimmed() + QLatin1Char('#') + dataRef.trimmed();
}

QString exportedServiceId(const configtool::ServiceTemplate &service)
{
    const QString configured = service.serviceId.trimmed().toLower();
    if (!configured.isEmpty()
        && configured != QStringLiteral("measurement")
        && configured != QStringLiteral("status")) {
        return configured;
    }
    return service.type == configtool::ModelServiceType::Status
        ? QStringLiteral("discrete")
        : QStringLiteral("analog");
}

const configtool::ModelTemplate *findModel(const configtool::ConfigProject &project,
                                           const QString &modelId)
{
    for (const configtool::ModelTemplate &model : project.models) {
        if (model.modelId.trimmed() == modelId.trimmed()) {
            return &model;
        }
    }
    return nullptr;
}

QList<UploadPoint> collectUploadPoints(const configtool::ConfigProject &project)
{
    QList<UploadPoint> result;
    QSet<QString> seen;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        const QString deviceId = device.deviceId.trimmed();
        const configtool::ModelTemplate *model = findModel(project, device.modelId);
        if (deviceId.isEmpty() || !model || !model->northVisible) {
            continue;
        }

        QHash<QString, QString> bindingDescriptions;
        QSet<QString> enabledBindingRefs;
        for (const configtool::PointBinding &binding : device.bindings) {
            const QString dataRef = binding.dataRef.trimmed();
            if (!binding.enabled || dataRef.isEmpty()) {
                continue;
            }
            enabledBindingRefs.insert(dataRef);
            if (!binding.descriptionOverride.trimmed().isEmpty()) {
                bindingDescriptions.insert(dataRef, binding.descriptionOverride.trimmed());
            }
        }

        for (const configtool::ServiceTemplate &service : model->services) {
            if (service.type == configtool::ModelServiceType::Control) {
                continue;
            }
            const QString serviceId = exportedServiceId(service);
            for (const configtool::PointTemplate &point : service.points) {
                const QString dataRef = point.dataRef().trimmed();
                if (!point.northVisible || dataRef.isEmpty()) {
                    continue;
                }
                if (!device.bindings.isEmpty() && !enabledBindingRefs.contains(dataRef)) {
                    continue;
                }
                const QString key = uploadKey(deviceId, dataRef);
                if (seen.contains(key)) {
                    continue;
                }
                seen.insert(key);

                UploadPoint item;
                item.deviceId = deviceId;
                item.deviceDesc = device.deviceDesc.trimmed();
                item.serviceId = serviceId;
                item.serviceType = service.type;
                item.dataRef = dataRef;
                item.pointDesc = bindingDescriptions.value(dataRef, point.description).trimmed();
                item.deadZoneType = point.deadZoneType.trimmed().isEmpty()
                    ? QStringLiteral("1") : point.deadZoneType.trimmed();
                item.deadZoneValue = point.deadZoneValue.trimmed().isEmpty()
                    ? QStringLiteral("0") : point.deadZoneValue.trimmed();
                result.append(item);
            }
        }
    }
    return result;
}

QJsonObject normalizedPolicies(const configtool::ConfigProject &project,
                               const QJsonObject &savedPolicies)
{
    QJsonObject result;
    const QList<UploadPoint> points = collectUploadPoints(project);
    for (const UploadPoint &point : points) {
        const QString key = uploadKey(point.deviceId, point.dataRef);
        const QJsonObject saved = savedPolicies.value(key).toObject();
        QJsonObject policy;
        policy.insert(QStringLiteral("DeviceId"), point.deviceId);
        policy.insert(QStringLiteral("DataRefer"), point.dataRef);
        // 服务类型由模型点所属服务唯一决定，不接受配置文件或界面覆盖。
        policy.insert(QStringLiteral("ServiceId"), point.serviceId);
        policy.insert(QStringLiteral("Period"),
                      saved.value(QStringLiteral("Period")).toString(QStringLiteral("0")));
        policy.insert(QStringLiteral("Spont"),
                      saved.value(QStringLiteral("Spont")).toString(QStringLiteral("0")));
        policy.insert(QStringLiteral("DZType"),
                      saved.value(QStringLiteral("DZType")).toString(point.deadZoneType));
        policy.insert(QStringLiteral("DZVal"),
                      saved.value(QStringLiteral("DZVal")).toString(point.deadZoneValue));
        result.insert(key, policy);
    }
    return result;
}

} // namespace

DataUploadPolicyEditor::DataUploadPolicyEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 6);
    layout->setSpacing(6);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("数据上送策略（根据当前设备、模型和点表自动生成）"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    header->addWidget(title);

    m_typeTabBar = new QTabBar(this);
    m_typeTabBar->setExpanding(false);
    m_typeTabBar->addTab(QStringLiteral("遥测"));
    m_typeTabBar->addTab(QStringLiteral("遥信"));
    header->addWidget(m_typeTabBar);
    header->addStretch();

    m_spontToggleButton = new QPushButton(QStringLiteral("突变全部启用"), this);
    m_spontToggleButton->setToolTip(QStringLiteral("一次设置全部遥测和遥信点位的突变上送开关"));
    header->addWidget(m_spontToggleButton);

    auto *setPeriodButton = new QPushButton(QStringLiteral("统一设置周期..."), this);
    setPeriodButton->setToolTip(
        QStringLiteral("统一设置当前“遥测”或“遥信”页签全部点位的 Period"));
    header->addWidget(setPeriodButton);

    auto *setDzTypeButton = new QPushButton(QStringLiteral("统一死区类型..."), this);
    setDzTypeButton->setToolTip(
        QStringLiteral("统一设置当前“遥测”或“遥信”页签全部点位的 DZType"));
    header->addWidget(setDzTypeButton);

    auto *setDzValueButton = new QPushButton(QStringLiteral("统一死区值..."), this);
    setDzValueButton->setToolTip(
        QStringLiteral("统一设置当前“遥测”或“遥信”页签全部点位的 DZVal"));
    header->addWidget(setDzValueButton);

    header->addWidget(new QLabel(QStringLiteral("筛选:"), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMinimumWidth(280);
    m_filterEdit->setPlaceholderText(QStringLiteral("设备 ID、DataRefer 或描述"));
    header->addWidget(m_filterEdit);
    layout->addLayout(header);

    auto *hint = new QLabel(
        QStringLiteral("Period=0 表示不周期上送；关闭突变后 Spont=0。"
                       "CheckStamp、OldValue 等运行字段在导出时自动生成。"),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("设备"),
        QStringLiteral("设备描述"),
        QStringLiteral("服务类型"),
        QStringLiteral("点号 (DataRefer)"),
        QStringLiteral("点位描述"),
        QStringLiteral("周期 Period(s)"),
        QStringLiteral("突变 Spont"),
        QStringLiteral("死区类型 DZType"),
        QStringLiteral("死区值 DZVal")
    });
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(DeviceIdColumn, 100);
    m_table->setColumnWidth(DeviceDescColumn, 130);
    m_table->setColumnWidth(ServiceColumn, 145);
    m_table->setColumnWidth(DataRefColumn, 270);
    m_table->setColumnWidth(PointDescColumn, 170);
    m_table->setColumnWidth(PeriodColumn, 120);
    m_table->setColumnWidth(SpontColumn, 90);
    m_table->setColumnWidth(DzTypeColumn, 155);
    layout->addWidget(m_table, 1);

    m_summaryLabel = new QLabel(QStringLiteral("尚未加载配置工程"), this);
    layout->addWidget(m_summaryLabel);

    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &DataUploadPolicyEditor::applyFilter);
    connect(m_typeTabBar, &QTabBar::currentChanged,
            this, &DataUploadPolicyEditor::applyFilter);
    connect(m_spontToggleButton, &QPushButton::clicked,
            this, &DataUploadPolicyEditor::toggleAllSpont);
    connect(setPeriodButton, &QPushButton::clicked,
            this, &DataUploadPolicyEditor::setAllPeriods);
    connect(setDzTypeButton, &QPushButton::clicked,
            this, &DataUploadPolicyEditor::setAllDeadZoneTypes);
    connect(setDzValueButton, &QPushButton::clicked,
            this, &DataUploadPolicyEditor::setAllDeadZoneValues);
}

void DataUploadPolicyEditor::setProject(const configtool::ConfigProject *project,
                                      const QJsonObject &savedPolicies)
{
    m_project = project;
    m_savedPolicies = project ? normalizedPolicies(*project, savedPolicies) : QJsonObject();
    populateRows();
}

void DataUploadPolicyEditor::populateRows()
{
    m_populating = true;
    m_table->setRowCount(0);
    const QList<UploadPoint> points = m_project
        ? collectUploadPoints(*m_project) : QList<UploadPoint>();
    QHash<QString, UploadPoint> pointByKey;
    for (const UploadPoint &point : points) {
        pointByKey.insert(uploadKey(point.deviceId, point.dataRef), point);
    }

    for (auto it = m_savedPolicies.constBegin(); it != m_savedPolicies.constEnd(); ++it) {
        const QJsonObject policy = it.value().toObject();
        const UploadPoint point = pointByKey.value(it.key());
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto addReadOnly = [&](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(row, column, item);
        };
        addReadOnly(DeviceIdColumn, point.deviceId);
        addReadOnly(DeviceDescColumn, point.deviceDesc);
        addReadOnly(DataRefColumn, point.dataRef);
        addReadOnly(PointDescColumn, point.pointDesc);

        const QString serviceId = policy.value(QStringLiteral("ServiceId")).toString().toLower();
        QString serviceText;
        if (serviceId == QStringLiteral("discrete")) {
            serviceText = QStringLiteral("遥信 (discrete)");
        } else if (serviceId == QStringLiteral("accumulator")) {
            serviceText = QStringLiteral("累计量 (accumulator)");
        } else if (serviceId == QStringLiteral("control")) {
            serviceText = QStringLiteral("控制 (control)");
        } else {
            serviceText = QStringLiteral("遥测 (analog)");
        }
        auto *serviceItem = new QTableWidgetItem(serviceText);
        serviceItem->setData(Qt::UserRole, serviceId);
        serviceItem->setData(Qt::UserRole + 1, static_cast<int>(point.serviceType));
        serviceItem->setFlags(serviceItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ServiceColumn, serviceItem);

        auto *periodSpin = new QSpinBox(m_table);
        periodSpin->setRange(0, 2147483647);
        periodSpin->setValue(policy.value(QStringLiteral("Period")).toString().toInt());
        periodSpin->setSpecialValueText(QStringLiteral("0 (关闭)"));
        m_table->setCellWidget(row, PeriodColumn, periodSpin);

        auto *spontCheck = new QCheckBox(QStringLiteral("启用"), m_table);
        spontCheck->setChecked(policy.value(QStringLiteral("Spont")).toString() == QStringLiteral("1"));
        m_table->setCellWidget(row, SpontColumn, spontCheck);

        auto *dzTypeCombo = new QComboBox(m_table);
        dzTypeCombo->addItem(QStringLiteral("相对量程 (0)"), QStringLiteral("0"));
        dzTypeCombo->addItem(QStringLiteral("绝对值 (1)"), QStringLiteral("1"));
        const int dzTypeIndex = dzTypeCombo->findData(
            policy.value(QStringLiteral("DZType")).toString());
        dzTypeCombo->setCurrentIndex(dzTypeIndex >= 0 ? dzTypeIndex : 1);
        m_table->setCellWidget(row, DzTypeColumn, dzTypeCombo);

        auto *dzValueEdit = new QLineEdit(
            policy.value(QStringLiteral("DZVal")).toString(), m_table);
        auto *validator = new QDoubleValidator(0.0, 1.0e100, 12, dzValueEdit);
        validator->setNotation(QDoubleValidator::ScientificNotation);
        dzValueEdit->setValidator(validator);
        m_table->setCellWidget(row, DzValueColumn, dzValueEdit);

        connect(periodSpin, qOverload<int>(&QSpinBox::valueChanged),
                this, &DataUploadPolicyEditor::notifyPoliciesChanged);
        connect(spontCheck, &QCheckBox::toggled,
                this, &DataUploadPolicyEditor::notifyPoliciesChanged);
        connect(dzTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &DataUploadPolicyEditor::notifyPoliciesChanged);
        connect(dzValueEdit, &QLineEdit::editingFinished,
                this, &DataUploadPolicyEditor::notifyPoliciesChanged);
    }
    m_populating = false;
    updateBulkButtonText();
    applyFilter();
}

QJsonObject DataUploadPolicyEditor::policies() const
{
    QJsonObject result;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString deviceId = m_table->item(row, DeviceIdColumn)->text().trimmed();
        const QString dataRef = m_table->item(row, DataRefColumn)->text().trimmed();
        const auto *periodSpin = qobject_cast<QSpinBox *>(m_table->cellWidget(row, PeriodColumn));
        const auto *spontCheck = qobject_cast<QCheckBox *>(m_table->cellWidget(row, SpontColumn));
        const auto *dzTypeCombo = qobject_cast<QComboBox *>(m_table->cellWidget(row, DzTypeColumn));
        const auto *dzValueEdit = qobject_cast<QLineEdit *>(m_table->cellWidget(row, DzValueColumn));
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem || !periodSpin || !spontCheck || !dzTypeCombo || !dzValueEdit) {
            continue;
        }

        QJsonObject policy;
        policy.insert(QStringLiteral("DeviceId"), deviceId);
        policy.insert(QStringLiteral("DataRefer"), dataRef);
        policy.insert(QStringLiteral("ServiceId"), serviceItem->data(Qt::UserRole).toString());
        policy.insert(QStringLiteral("Period"), QString::number(periodSpin->value()));
        policy.insert(QStringLiteral("Spont"), spontCheck->isChecked()
                          ? QStringLiteral("1") : QStringLiteral("0"));
        policy.insert(QStringLiteral("DZType"), dzTypeCombo->currentData().toString());
        policy.insert(QStringLiteral("DZVal"), dzValueEdit->text().trimmed().isEmpty()
                          ? QStringLiteral("0") : dzValueEdit->text().trimmed());
        result.insert(uploadKey(deviceId, dataRef), policy);
    }
    return result;
}

void DataUploadPolicyEditor::notifyPoliciesChanged()
{
    if (m_populating) {
        return;
    }
    m_savedPolicies = policies();
    updateBulkButtonText();
    emit policiesChanged(m_savedPolicies);
}

void DataUploadPolicyEditor::toggleAllSpont()
{
    bool allEnabled = m_table->rowCount() > 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *check = qobject_cast<QCheckBox *>(m_table->cellWidget(row, SpontColumn));
        if (!check || !check->isChecked()) {
            allEnabled = false;
            break;
        }
    }

    m_populating = true;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (auto *check = qobject_cast<QCheckBox *>(m_table->cellWidget(row, SpontColumn))) {
            check->setChecked(!allEnabled);
        }
    }
    m_populating = false;
    notifyPoliciesChanged();
}

void DataUploadPolicyEditor::setAllPeriods()
{
    const auto selectedType = m_typeTabBar->currentIndex() == 1
        ? configtool::ModelServiceType::Status
        : configtool::ModelServiceType::Measurement;
    const QString typeName = selectedType == configtool::ModelServiceType::Status
        ? QStringLiteral("遥信")
        : QStringLiteral("遥测");

    int currentPeriod = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem
            || static_cast<configtool::ModelServiceType>(
                   serviceItem->data(Qt::UserRole + 1).toInt()) != selectedType) {
            continue;
        }
        if (const auto *spin = qobject_cast<QSpinBox *>(
                m_table->cellWidget(row, PeriodColumn))) {
            currentPeriod = spin->value();
            break;
        }
    }

    bool accepted = false;
    const int period = QInputDialog::getInt(
        this,
        QStringLiteral("统一设置%1周期").arg(typeName),
        QStringLiteral("%1周期 Period（秒，0 表示不周期上送）:").arg(typeName),
        currentPeriod,
        0,
        2147483647,
        1,
        &accepted);
    if (!accepted) {
        return;
    }

    m_populating = true;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem
            || static_cast<configtool::ModelServiceType>(
                   serviceItem->data(Qt::UserRole + 1).toInt()) != selectedType) {
            continue;
        }
        if (auto *spin = qobject_cast<QSpinBox *>(m_table->cellWidget(row, PeriodColumn))) {
            spin->setValue(period);
        }
    }
    m_populating = false;
    notifyPoliciesChanged();
}

void DataUploadPolicyEditor::setAllDeadZoneTypes()
{
    const auto selectedType = m_typeTabBar->currentIndex() == 1
        ? configtool::ModelServiceType::Status
        : configtool::ModelServiceType::Measurement;
    const QString typeName = selectedType == configtool::ModelServiceType::Status
        ? QStringLiteral("遥信")
        : QStringLiteral("遥测");

    int currentIndex = 1;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem
            || static_cast<configtool::ModelServiceType>(
                   serviceItem->data(Qt::UserRole + 1).toInt()) != selectedType) {
            continue;
        }
        if (const auto *combo = qobject_cast<QComboBox *>(
                m_table->cellWidget(row, DzTypeColumn))) {
            currentIndex = combo->currentData().toString() == QStringLiteral("0") ? 0 : 1;
            break;
        }
    }

    const QStringList choices{
        QStringLiteral("相对量程 (0)"),
        QStringLiteral("绝对值 (1)")
    };
    bool accepted = false;
    const QString choice = QInputDialog::getItem(
        this,
        QStringLiteral("统一设置%1死区类型").arg(typeName),
        QStringLiteral("%1死区类型 DZType:").arg(typeName),
        choices,
        currentIndex,
        false,
        &accepted);
    if (!accepted) {
        return;
    }
    const QString dzType = choice == choices.first()
        ? QStringLiteral("0")
        : QStringLiteral("1");

    m_populating = true;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem
            || static_cast<configtool::ModelServiceType>(
                   serviceItem->data(Qt::UserRole + 1).toInt()) != selectedType) {
            continue;
        }
        if (auto *combo = qobject_cast<QComboBox *>(
                m_table->cellWidget(row, DzTypeColumn))) {
            combo->setCurrentIndex(combo->findData(dzType));
        }
    }
    m_populating = false;
    notifyPoliciesChanged();
}

void DataUploadPolicyEditor::setAllDeadZoneValues()
{
    const auto selectedType = m_typeTabBar->currentIndex() == 1
        ? configtool::ModelServiceType::Status
        : configtool::ModelServiceType::Measurement;
    const QString typeName = selectedType == configtool::ModelServiceType::Status
        ? QStringLiteral("遥信")
        : QStringLiteral("遥测");

    QString currentValue = QStringLiteral("0");
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem
            || static_cast<configtool::ModelServiceType>(
                   serviceItem->data(Qt::UserRole + 1).toInt()) != selectedType) {
            continue;
        }
        if (const auto *edit = qobject_cast<QLineEdit *>(
                m_table->cellWidget(row, DzValueColumn))) {
            currentValue = edit->text();
            break;
        }
    }

    QInputDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("统一设置%1死区值").arg(typeName));
    dialog.setLabelText(QStringLiteral("%1死区值 DZVal:").arg(typeName));
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextValue(currentValue);
    if (auto *edit = dialog.findChild<QLineEdit *>()) {
        auto *validator = new QDoubleValidator(0.0, 1.0e100, 12, edit);
        validator->setNotation(QDoubleValidator::ScientificNotation);
        edit->setValidator(validator);
    }
    if (!dialog.exec()) {
        return;
    }
    const QString dzValue = dialog.textValue().trimmed();

    m_populating = true;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        if (!serviceItem
            || static_cast<configtool::ModelServiceType>(
                   serviceItem->data(Qt::UserRole + 1).toInt()) != selectedType) {
            continue;
        }
        if (auto *edit = qobject_cast<QLineEdit *>(
                m_table->cellWidget(row, DzValueColumn))) {
            edit->setText(dzValue);
        }
    }
    m_populating = false;
    notifyPoliciesChanged();
}

void DataUploadPolicyEditor::updateBulkButtonText()
{
    if (!m_spontToggleButton) {
        return;
    }
    bool allEnabled = m_table->rowCount() > 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *check = qobject_cast<QCheckBox *>(m_table->cellWidget(row, SpontColumn));
        if (!check || !check->isChecked()) {
            allEnabled = false;
            break;
        }
    }
    m_spontToggleButton->setText(
        allEnabled ? QStringLiteral("取消全部突变") : QStringLiteral("突变全部启用"));
    m_spontToggleButton->setEnabled(m_table->rowCount() > 0);
}

void DataUploadPolicyEditor::applyFilter()
{
    const QString keyword = m_filterEdit->text().trimmed();
    const auto selectedType = m_typeTabBar->currentIndex() == 1
        ? configtool::ModelServiceType::Status
        : configtool::ModelServiceType::Measurement;
    int visible = 0;
    int typeCount = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *serviceItem = m_table->item(row, ServiceColumn);
        const auto rowType = serviceItem
            ? static_cast<configtool::ModelServiceType>(
                  serviceItem->data(Qt::UserRole + 1).toInt())
            : configtool::ModelServiceType::Control;
        const bool matchesType = rowType == selectedType;
        if (matchesType) {
            ++typeCount;
        }
        QString searchable;
        for (int column : {DeviceIdColumn, DeviceDescColumn, DataRefColumn, PointDescColumn}) {
            if (const QTableWidgetItem *item = m_table->item(row, column)) {
                searchable += item->text() + QLatin1Char(' ');
            }
        }
        const bool matches = matchesType
            && (keyword.isEmpty() || searchable.contains(keyword, Qt::CaseInsensitive));
        m_table->setRowHidden(row, !matches);
        if (matches) {
            ++visible;
        }
    }
    m_summaryLabel->setText(
        m_project
            ? QStringLiteral("显示 %1 / %2 个%3点位；控制点位不参与上送策略；"
                             "导出键使用 DeviceId#DataRefer")
                  .arg(visible)
                  .arg(typeCount)
                  .arg(selectedType == configtool::ModelServiceType::Status
                           ? QStringLiteral("遥信")
                           : QStringLiteral("遥测"))
            : QStringLiteral("尚未加载配置工程"));
}

QJsonObject DataUploadPolicyEditor::importBusinessPolicies(const QJsonObject &subData)
{
    QJsonObject result;
    for (auto it = subData.constBegin(); it != subData.constEnd(); ++it) {
        const QJsonObject runtime = it.value().toObject();
        const QString deviceId = runtime.value(QStringLiteral("DeviceId")).toString().trimmed();
        QString dataRef = it.key();
        const QString expectedPrefix = deviceId + QLatin1Char('#');
        if (!deviceId.isEmpty() && dataRef.startsWith(expectedPrefix)) {
            dataRef = dataRef.mid(expectedPrefix.size());
        }
        if (deviceId.isEmpty() || dataRef.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject policy;
        policy.insert(QStringLiteral("DeviceId"), deviceId);
        policy.insert(QStringLiteral("DataRefer"), dataRef.trimmed());
        policy.insert(QStringLiteral("ServiceId"),
                      runtime.value(QStringLiteral("ServiceId")).toString(QStringLiteral("analog")));
        policy.insert(QStringLiteral("Period"),
                      runtime.value(QStringLiteral("Period")).toString(QStringLiteral("0")));
        policy.insert(QStringLiteral("Spont"),
                      runtime.value(QStringLiteral("Spont")).toString(QStringLiteral("0")));
        policy.insert(QStringLiteral("DZType"),
                      runtime.value(QStringLiteral("DZType")).toString(QStringLiteral("1")));
        policy.insert(QStringLiteral("DZVal"),
                      runtime.value(QStringLiteral("DZVal")).toString(QStringLiteral("0")));
        result.insert(uploadKey(deviceId, dataRef), policy);
    }
    return result;
}

QJsonObject DataUploadPolicyEditor::buildSubDataConfig(
    const configtool::ConfigProject &project,
    const QJsonObject &savedPolicies)
{
    QJsonObject result;
    const QJsonObject policies = normalizedPolicies(project, savedPolicies);
    const QString checkStamp = QDateTime::currentDateTime().toString(
        QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    for (auto it = policies.constBegin(); it != policies.constEnd(); ++it) {
        const QJsonObject policy = it.value().toObject();
        QJsonObject runtime;
        runtime.insert(QStringLiteral("CheckStamp"), checkStamp);
        runtime.insert(QStringLiteral("DZType"), policy.value(QStringLiteral("DZType")).toString());
        runtime.insert(QStringLiteral("DZVal"), policy.value(QStringLiteral("DZVal")).toString());
        runtime.insert(QStringLiteral("DeviceId"), policy.value(QStringLiteral("DeviceId")).toString());
        runtime.insert(QStringLiteral("OldValue"), QStringLiteral("0"));
        runtime.insert(QStringLiteral("Period"), policy.value(QStringLiteral("Period")).toString());
        runtime.insert(QStringLiteral("ServiceId"), policy.value(QStringLiteral("ServiceId")).toString());
        runtime.insert(QStringLiteral("Spont"), policy.value(QStringLiteral("Spont")).toString());
        result.insert(it.key(), runtime);
    }
    return result;
}

int DataUploadPolicyEditor::candidateCount(const configtool::ConfigProject &project)
{
    return collectUploadPoints(project).size();
}
