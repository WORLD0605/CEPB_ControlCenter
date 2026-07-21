#include "mainwindow_config_p.h"

#include <QIntValidator>

using namespace cepb_config_helpers;

// ============================================================
// IEC104 配置页面 — 槽函数 & 序列化
// ============================================================

void MainWindow::setupIec104ConfigPage()
{
    m_iec104ConfigPage = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(m_iec104ConfigPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(6);

    auto *basicFrame = new QFrame(this);
    basicFrame->setFrameShape(QFrame::StyledPanel);
    auto *basicGrid = new QGridLayout(basicFrame);
    basicGrid->setContentsMargins(8, 6, 8, 6);
    basicGrid->setHorizontalSpacing(24);
    basicGrid->setVerticalSpacing(6);

    m_iec104CodeIpEdit = new QLineEdit(QStringLiteral("0.0.0.0"), this);
    m_iec104CodeIpEdit->setPlaceholderText(QStringLiteral("0.0.0.0"));
    basicGrid->addWidget(new QLabel(QStringLiteral("监听 IP:"), this), 0, 0);
    basicGrid->addWidget(m_iec104CodeIpEdit, 0, 1);

    m_iec104CodePortEdit = new QLineEdit(QStringLiteral("2404"), this);
    m_iec104CodePortEdit->setValidator(new QIntValidator(1, 65535, m_iec104CodePortEdit));
    basicGrid->addWidget(new QLabel(QStringLiteral("监听端口:"), this), 0, 2);
    basicGrid->addWidget(m_iec104CodePortEdit, 0, 3);

    m_iec104ComAddrEdit = new QLineEdit(QStringLiteral("1"), this);
    m_iec104ComAddrEdit->setValidator(new QIntValidator(1, 65535, m_iec104ComAddrEdit));
    basicGrid->addWidget(new QLabel(QStringLiteral("公共地址:"), this), 1, 0);
    basicGrid->addWidget(m_iec104ComAddrEdit, 1, 1);
    basicGrid->setColumnStretch(1, 1);
    basicGrid->setColumnStretch(3, 1);
    pageLayout->addWidget(basicFrame);

    auto *protocolFrame = new QFrame(this);
    protocolFrame->setFrameShape(QFrame::StyledPanel);
    auto *protocolGrid = new QGridLayout(protocolFrame);
    protocolGrid->setContentsMargins(8, 6, 8, 6);
    protocolGrid->setHorizontalSpacing(24);
    protocolGrid->setVerticalSpacing(6);

    auto addLengthCombo = [this](std::initializer_list<int> values, int current) {
        auto *combo = new QComboBox(this);
        for (const int value : values) {
            combo->addItem(QString::number(value), value);
        }
        combo->setCurrentIndex(qMax(0, combo->findData(current)));
        return combo;
    };
    m_iec104CotCombo = addLengthCombo({1, 2}, 2);
    m_iec104CaCombo = addLengthCombo({1, 2}, 2);
    m_iec104IoaCombo = addLengthCombo({1, 2, 3}, 3);
    m_iec104LinkAddrCombo = addLengthCombo({1, 2}, 2);
    protocolGrid->addWidget(new QLabel(QStringLiteral("传送原因 COT 字节数:"), this), 0, 0);
    protocolGrid->addWidget(m_iec104CotCombo, 0, 1);
    protocolGrid->addWidget(new QLabel(QStringLiteral("公共地址 CA 字节数:"), this), 0, 2);
    protocolGrid->addWidget(m_iec104CaCombo, 0, 3);
    protocolGrid->addWidget(new QLabel(QStringLiteral("信息对象地址 IOA 字节数:"), this), 1, 0);
    protocolGrid->addWidget(m_iec104IoaCombo, 1, 1);
    protocolGrid->addWidget(new QLabel(QStringLiteral("链路地址字节数:"), this), 1, 2);
    protocolGrid->addWidget(m_iec104LinkAddrCombo, 1, 3);

    m_iec104TelecontrolTypeCombo = new QComboBox(this);
    m_iec104TelecontrolTypeCombo->addItem(QStringLiteral("单命令"), QStringLiteral("单命令"));
    m_iec104TelecontrolTypeCombo->addItem(QStringLiteral("双命令"), QStringLiteral("双命令"));
    protocolGrid->addWidget(new QLabel(QStringLiteral("遥控类型:"), this), 2, 0);
    protocolGrid->addWidget(m_iec104TelecontrolTypeCombo, 2, 1);

    m_iec104TelemetryTypeCombo = new QComboBox(this);
    m_iec104TelemetryTypeCombo->addItem(QStringLiteral("归一化值"), QStringLiteral("归一化值"));
    m_iec104TelemetryTypeCombo->addItem(QStringLiteral("短浮点数"), QStringLiteral("短浮点数"));
    m_iec104TelemetryTypeCombo->addItem(QStringLiteral("标度化值"), QStringLiteral("标度化值"));
    m_iec104TelemetryTypeCombo->setCurrentIndex(1);
    protocolGrid->addWidget(new QLabel(QStringLiteral("遥测类型:"), this), 2, 2);
    protocolGrid->addWidget(m_iec104TelemetryTypeCombo, 2, 3);

    m_iec104SequenceCombo = new QComboBox(this);
    m_iec104SequenceCombo->addItem(QStringLiteral("0 — 逐点发送"), 0);
    m_iec104SequenceCombo->addItem(QStringLiteral("1 — 连续地址批量打包"), 1);
    protocolGrid->addWidget(new QLabel(QStringLiteral("总召唤模式:"), this), 3, 0);
    protocolGrid->addWidget(m_iec104SequenceCombo, 3, 1);
    m_iec104YxUseDoubleValueCombo = new QComboBox(this);
    m_iec104YxUseDoubleValueCombo->addItem(QStringLiteral("0 — 不转换"), 0);
    m_iec104YxUseDoubleValueCombo->addItem(QStringLiteral("1 — 转为双点标准格式"), 1);
    protocolGrid->addWidget(new QLabel(QStringLiteral("遥信双点转换:"), this), 3, 2);
    protocolGrid->addWidget(m_iec104YxUseDoubleValueCombo, 3, 3);
    m_iec104YxAllSTransDFlagCombo = new QComboBox(this);
    m_iec104YxAllSTransDFlagCombo->addItem(QStringLiteral("0 — 不转换"), 0);
    m_iec104YxAllSTransDFlagCombo->addItem(QStringLiteral("1 — 双命令值转双点格式"), 1);
    protocolGrid->addWidget(new QLabel(QStringLiteral("双命令遥信转换:"), this), 4, 0);
    protocolGrid->addWidget(m_iec104YxAllSTransDFlagCombo, 4, 1);
    protocolGrid->setColumnStretch(1, 1);
    protocolGrid->setColumnStretch(3, 1);
    pageLayout->addWidget(protocolFrame);

    auto *linkFrame = new QFrame(this);
    linkFrame->setFrameShape(QFrame::StyledPanel);
    auto *linkGrid = new QGridLayout(linkFrame);
    linkGrid->setContentsMargins(8, 6, 8, 6);
    linkGrid->setHorizontalSpacing(24);
    linkGrid->setVerticalSpacing(6);
    auto makePositiveEdit = [this](const QString &value) {
        auto *edit = new QLineEdit(value, this);
        edit->setValidator(new QIntValidator(1, 86400, edit));
        return edit;
    };
    m_iec104T0Edit = makePositiveEdit(QStringLiteral("10"));
    m_iec104T1Edit = makePositiveEdit(QStringLiteral("15"));
    m_iec104T2Edit = makePositiveEdit(QStringLiteral("10"));
    m_iec104T3Edit = makePositiveEdit(QStringLiteral("20"));
    m_iec104KEdit = makePositiveEdit(QStringLiteral("12"));
    m_iec104WEdit = makePositiveEdit(QStringLiteral("8"));
    const QList<QPair<QString, QLineEdit *>> linkFields = {
        {QStringLiteral("T0 (秒):"), m_iec104T0Edit}, {QStringLiteral("T1 (秒):"), m_iec104T1Edit},
        {QStringLiteral("T2 (秒):"), m_iec104T2Edit}, {QStringLiteral("T3 (秒):"), m_iec104T3Edit},
        {QStringLiteral("K (发送窗口):"), m_iec104KEdit}, {QStringLiteral("W (接收确认):"), m_iec104WEdit}
    };
    for (int i = 0; i < linkFields.size(); ++i) {
        const int row = i / 3;
        const int col = (i % 3) * 2;
        linkGrid->addWidget(new QLabel(linkFields.at(i).first, this), row, col);
        linkGrid->addWidget(linkFields.at(i).second, row, col + 1);
        linkGrid->setColumnStretch(col + 1, 1);
    }
    pageLayout->addWidget(linkFrame);

    auto *pointsFrame = new QFrame(this);
    pointsFrame->setFrameShape(QFrame::StyledPanel);
    auto *pointsLayout = new QVBoxLayout(pointsFrame);
    pointsLayout->setContentsMargins(8, 8, 8, 6);
    pointsLayout->setSpacing(6);
    auto *pointsHeader = new QHBoxLayout();
    auto *pointsTitle = new QLabel(QStringLiteral("点表（根据北向104点表填写）"), this);
    QFont boldFont = pointsTitle->font();
    boldFont.setBold(true);
    pointsTitle->setFont(boldFont);
    pointsHeader->addWidget(pointsTitle);
    pointsHeader->addStretch();
    m_sortIec104PointsBtn = new QPushButton(QStringLiteral("按104地址排序"), this);
    m_refreshIec104PointsBtn = new QPushButton(QStringLiteral("从设备刷新点位"), this);
    pointsHeader->addWidget(m_sortIec104PointsBtn);
    pointsHeader->addWidget(m_refreshIec104PointsBtn);
    pointsLayout->addLayout(pointsHeader);

    auto *filterRow = new QHBoxLayout();
    m_iec104PointFilterTabBar = new QTabBar(this);
    for (const QString &label : {QStringLiteral("全部"), QStringLiteral("遥测"), QStringLiteral("遥信"), QStringLiteral("控制")}) {
        m_iec104PointFilterTabBar->addTab(label);
    }
    m_iec104PointFilterTabBar->setExpanding(false);
    filterRow->addWidget(m_iec104PointFilterTabBar);
    filterRow->addStretch();
    filterRow->addWidget(new QLabel(QStringLiteral("搜索:"), this));
    m_iec104PointDataRefFilterEdit = new QLineEdit(this);
    m_iec104PointDataRefFilterEdit->setPlaceholderText(QStringLiteral("DataRef / Description"));
    m_iec104PointDataRefFilterEdit->setClearButtonEnabled(true);
    m_iec104PointDataRefFilterEdit->setFixedWidth(300);
    filterRow->addWidget(m_iec104PointDataRefFilterEdit);
    pointsLayout->addLayout(filterRow);

    m_iec104PointsTable = new QTableWidget(0, 8, this);
    m_iec104PointsTable->setHorizontalHeaderLabels({QString(), QStringLiteral("启用"), QStringLiteral("DeviceId"),
        QStringLiteral("DataRef"), QStringLiteral("Description"), QStringLiteral("北向104地址"),
        QStringLiteral("死区类型"), QStringLiteral("死区值")});
    m_iec104PointsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_iec104PointsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_iec104PointsTable->setSelectionMode(QAbstractItemView::ContiguousSelection);
    m_iec104PointsTable->setAlternatingRowColors(true);
    m_iec104PointsTable->verticalHeader()->setVisible(false);
    m_iec104PointsTable->horizontalHeader()->setStretchLastSection(true);
    m_iec104PointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const QList<int> widths = {28, 50, 130, 220, 150, 100, 130};
    for (int column = 0; column < widths.size(); ++column) m_iec104PointsTable->setColumnWidth(column, widths.at(column));
    pointsLayout->addWidget(m_iec104PointsTable, 1);
    m_iec104ValidationLabel = new QLabel(QStringLiteral("✓ 当前地址分配未发现问题。"), this);
    m_iec104ValidationLabel->setWordWrap(true);
    m_iec104ValidationLabel->setStyleSheet(QStringLiteral("QLabel { color: #2e7d32; }"));
    pointsLayout->addWidget(m_iec104ValidationLabel);
    pageLayout->addWidget(pointsFrame, 1);

    connect(m_refreshIec104PointsBtn, &QPushButton::clicked, this, &MainWindow::onRefreshIec104PointsClicked);
    connect(m_sortIec104PointsBtn, &QPushButton::clicked, this, &MainWindow::onSortIec104PointsClicked);
    connect(m_iec104PointsTable, &QTableWidget::itemChanged, this, &MainWindow::onIec104PointItemChanged);
    connect(m_iec104PointFilterTabBar, &QTabBar::currentChanged, this, &MainWindow::onIec104PointFilterChanged);
    connect(m_iec104PointDataRefFilterEdit, &QLineEdit::textChanged, this, &MainWindow::onIec104PointFilterTextChanged);
}

void MainWindow::onRefreshIec104PointsClicked()
{
    refreshIec104PointsFromDevices();
}

void MainWindow::refreshIec104PointsFromDevices(const QHash<QString, QJsonObject> &savedPointSettings,
                                                const QStringList &savedPointOrder)
{
    if (!m_iec104PointsTable) {
        return;
    }

    // 1. 收集当前表中已有的设置（保留用户编辑的值）
    struct PointSettings {
        bool enabled = true;
        QString deviceaddr;
        QString deathzoneType = QStringLiteral("0");
        QString deathzone = QStringLiteral("0.2");
    };
    QHash<QString, PointSettings> existingSettings;
    QStringList existingPointOrder;
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        const QString deviceId = m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
        const QString dataRef = m_iec104PointsTable->item(row, Iec101PointColumnDataRef)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
        if (deviceId.isEmpty() || dataRef.isEmpty()) {
            continue;
        }
        const QString key = deviceId + QStringLiteral("|") + dataRef;
        existingPointOrder.append(key);
        PointSettings s;
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        s.enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        if (m_iec104PointsTable->item(row, Iec101PointColumnAddress)) {
            s.deviceaddr = m_iec104PointsTable->item(row, Iec101PointColumnAddress)->text().trimmed();
        }
        QWidget *w = m_iec104PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            s.deathzoneType = combo->currentData().toString();
        }
        if (m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)) {
            s.deathzone = m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)->text().trimmed();
        }
        existingSettings[key] = s;
    }

    // 2. 合并 savedPointSettings（来自 localhost.json）；仅当表中无对应记录时才写入
    for (auto it = savedPointSettings.begin(); it != savedPointSettings.end(); ++it) {
        if (!existingSettings.contains(it.key())) {
            const QJsonObject &pt = it.value();
            PointSettings s;
            s.deviceaddr = pt.value(QStringLiteral("deviceaddr")).toString();
            s.deathzoneType = pt.value(QStringLiteral("deathzone_type")).toString(QStringLiteral("0"));
            s.deathzone = pt.value(QStringLiteral("deathzone")).toString(QStringLiteral("0.2"));
            s.enabled = true;
            existingSettings[it.key()] = s;
        }
    }

    // 3. 从当前工程的所有南向设备中收集点位
    const configtool::ConfigProject &project = m_configProjectManager.project();
    struct DevicePoint {
        QString deviceId;
        QString dataRef;
        QString description;
        int category = 0; // ModelServiceType: 0=Measurement, 1=Status, 2=Control
    };
    QHash<QString, DevicePoint> devicePointsByKey;
    QStringList devicePointOrder;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        // 预解析设备模型，建立 pointRef → category 映射
        QHash<QString, int> pointCategoryMap;
        for (const configtool::ModelTemplate &model : project.models) {
            if (model.modelId != device.modelId) {
                continue;
            }
            for (const configtool::ServiceTemplate &service : model.services) {
                for (const configtool::PointTemplate &pt : service.points) {
                    pointCategoryMap[pt.pointRef(model.modelId)] = static_cast<int>(pt.category);
                }
            }
        }

        for (const configtool::PointBinding &binding : device.bindings) {
            if (!binding.enabled) {
                continue;
            }
            DevicePoint dp;
            dp.deviceId = device.deviceId;
            dp.dataRef = binding.dataRef;
            dp.description = binding.descriptionOverride.isEmpty()
                ? binding.dataRef : binding.descriptionOverride;
            dp.category = pointCategoryMap.value(binding.pointRef, 0);
            const QString key = dp.deviceId + QStringLiteral("|") + dp.dataRef;
            if (!devicePointsByKey.contains(key)) {
                devicePointOrder.append(key);
            }
            devicePointsByKey.insert(key, dp);
        }
    }

    // 已有表格顺序优先；首次加载时恢复 localhost.json 中 meas_points 的数组顺序。
    const QStringList preferredOrder = existingPointOrder.isEmpty()
        ? savedPointOrder
        : existingPointOrder;
    QStringList mergedPointOrder;
    QSet<QString> appendedKeys;
    for (const QString &key : preferredOrder) {
        if (devicePointsByKey.contains(key) && !appendedKeys.contains(key)) {
            mergedPointOrder.append(key);
            appendedKeys.insert(key);
        }
    }
    // 刷新发现的新点位只追加到末尾，不打乱用户已经调整过的顺序。
    for (const QString &key : devicePointOrder) {
        if (!appendedKeys.contains(key)) {
            mergedPointOrder.append(key);
            appendedKeys.insert(key);
        }
    }

    // 4. 重建表格
    m_iec104PointsTable->blockSignals(true);
    m_iec104PointsTable->setRowCount(0);

    for (const QString &key : mergedPointOrder) {
        const DevicePoint &dp = devicePointsByKey[key];
        const int row = m_iec104PointsTable->rowCount();
        m_iec104PointsTable->insertRow(row);

        QString deviceaddr;
        QString deathzoneType = QStringLiteral("0");
        QString deathzone = QStringLiteral("0.2");
        bool enabled = true;
        if (existingSettings.contains(key)) {
            const PointSettings &s = existingSettings[key];
            enabled = s.enabled;
            deviceaddr = s.deviceaddr;
            deathzoneType = s.deathzoneType.isEmpty() ? QStringLiteral("0") : s.deathzoneType;
            deathzone = s.deathzone.isEmpty() ? QStringLiteral("0.2") : s.deathzone;
        }

        // Col 0: 拖动手柄
        auto *handleItem = new QTableWidgetItem(QStringLiteral("⋮"));
        handleItem->setTextAlignment(Qt::AlignCenter);
        handleItem->setToolTip(QStringLiteral("拖动调整顺序"));
        handleItem->setForeground(QColor(QStringLiteral("#9a9a9a")));
        handleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDragHandle, handleItem);

        // Col 1: 启用 (checkbox)
        auto *checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
        checkItem->setData(Qt::UserRole, dp.category);
        m_iec104PointsTable->setItem(row, Iec101PointColumnEnabled, checkItem);

        // Col 2: DeviceId (read-only)
        auto *devIdItem = new QTableWidgetItem(dp.deviceId);
        devIdItem->setFlags(devIdItem->flags() & ~Qt::ItemIsEditable);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDeviceId, devIdItem);

        // Col 3: DataRef (read-only)
        auto *dataRefItem = new QTableWidgetItem(dp.dataRef);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDataRef, dataRefItem);

        // Col 4: Description (read-only)
        auto *descItem = new QTableWidgetItem(dp.description);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDescription, descItem);

        // Col 5: 北向104地址
        m_iec104PointsTable->setItem(row, Iec101PointColumnAddress, new QTableWidgetItem(deviceaddr));

        // Col 6: 死区类型 (QComboBox via setCellWidget)
        auto *dzTypeCombo = new QComboBox();
        configureTableCellCombo(dzTypeCombo, this);
        dzTypeCombo->addItem(QStringLiteral("0 — 百分比"), QStringLiteral("0"));
        dzTypeCombo->addItem(QStringLiteral("1 — 固定值"), QStringLiteral("1"));
        const int dzTypeIdx = dzTypeCombo->findData(deathzoneType);
        dzTypeCombo->setCurrentIndex(dzTypeIdx >= 0 ? dzTypeIdx : 0);
        m_iec104PointsTable->setCellWidget(row, Iec101PointColumnDeadzoneType, dzTypeCombo);

        // Col 7: 死区值
        m_iec104PointsTable->setItem(row, Iec101PointColumnDeadzone, new QTableWidgetItem(deathzone));
    }

    m_iec104PointsTable->blockSignals(false);
    applyIec104PointsFilter();
    highlightIec104DuplicateAddresses();
}

void MainWindow::rebuildIec104PointRowsInOrder(const QList<int> &sourceRows)
{
    if (!m_iec104PointsTable || sourceRows.size() != m_iec104PointsTable->rowCount()) {
        return;
    }

    struct RowSnapshot {
        QString deviceId;
        QString dataRef;
        QString description;
        int category = 0;
        bool enabled = true;
        QString deviceaddr;
        QString deathzoneType = QStringLiteral("0");
        QString deathzone = QStringLiteral("0.2");
    };
    QList<RowSnapshot> allRows;
    allRows.reserve(m_iec104PointsTable->rowCount());
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        RowSnapshot rs;
        rs.deviceId = m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
        rs.dataRef = m_iec104PointsTable->item(row, Iec101PointColumnDataRef)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
        rs.description = m_iec104PointsTable->item(row, Iec101PointColumnDescription)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDescription)->text().trimmed() : QString();
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        rs.category = checkItem ? checkItem->data(Qt::UserRole).toInt() : 0;
        rs.enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        rs.deviceaddr = m_iec104PointsTable->item(row, Iec101PointColumnAddress)
            ? m_iec104PointsTable->item(row, Iec101PointColumnAddress)->text().trimmed() : QString();
        if (auto *combo = qobject_cast<QComboBox *>(
                m_iec104PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType))) {
            rs.deathzoneType = combo->currentData().toString();
        }
        rs.deathzone = m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)->text().trimmed()
            : QStringLiteral("0.2");
        allRows.append(rs);
    }

    m_iec104PointsTable->blockSignals(true);
    m_iec104PointsTable->setRowCount(0);
    for (const int sourceRow : sourceRows) {
        if (sourceRow < 0 || sourceRow >= allRows.size()) {
            continue;
        }
        const RowSnapshot &rs = allRows[sourceRow];
        const int row = m_iec104PointsTable->rowCount();
        m_iec104PointsTable->insertRow(row);

        auto *handleItem = new QTableWidgetItem(QStringLiteral("⋮"));
        handleItem->setTextAlignment(Qt::AlignCenter);
        handleItem->setToolTip(QStringLiteral("拖动调整顺序"));
        handleItem->setForeground(QColor(QStringLiteral("#9a9a9a")));
        handleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDragHandle, handleItem);

        auto *checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkItem->setCheckState(rs.enabled ? Qt::Checked : Qt::Unchecked);
        checkItem->setData(Qt::UserRole, rs.category);
        m_iec104PointsTable->setItem(row, Iec101PointColumnEnabled, checkItem);

        auto *devIdItem = new QTableWidgetItem(rs.deviceId);
        devIdItem->setFlags(devIdItem->flags() & ~Qt::ItemIsEditable);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDeviceId, devIdItem);

        auto *dataRefItem = new QTableWidgetItem(rs.dataRef);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDataRef, dataRefItem);

        auto *descItem = new QTableWidgetItem(rs.description);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDescription, descItem);
        m_iec104PointsTable->setItem(row, Iec101PointColumnAddress, new QTableWidgetItem(rs.deviceaddr));

        auto *dzTypeCombo = new QComboBox();
        configureTableCellCombo(dzTypeCombo, this);
        dzTypeCombo->addItem(QStringLiteral("0 — 百分比"), QStringLiteral("0"));
        dzTypeCombo->addItem(QStringLiteral("1 — 固定值"), QStringLiteral("1"));
        const int dzTypeIdx = dzTypeCombo->findData(rs.deathzoneType);
        dzTypeCombo->setCurrentIndex(dzTypeIdx >= 0 ? dzTypeIdx : 0);
        m_iec104PointsTable->setCellWidget(row, Iec101PointColumnDeadzoneType, dzTypeCombo);
        m_iec104PointsTable->setItem(row, Iec101PointColumnDeadzone, new QTableWidgetItem(rs.deathzone));
    }
    m_iec104PointsTable->blockSignals(false);
    applyIec104PointsFilter();
    highlightIec104DuplicateAddresses();
}

void MainWindow::onIec104PointItemChanged(QTableWidgetItem *item)
{
    if (!item) {
        return;
    }
    // 当地址列或启用列变化时，刷新重复地址高亮
    if (item->column() == Iec101PointColumnEnabled || item->column() == Iec101PointColumnAddress) {
        highlightIec104DuplicateAddresses();
    }
}

void MainWindow::onIec104PointFilterChanged(int /*index*/)
{
    applyIec104PointsFilter();
}

void MainWindow::onIec104PointFilterTextChanged()
{
    applyIec104PointsFilter();
}

void MainWindow::applyIec104PointsFilter()
{
    if (!m_iec104PointsTable) {
        return;
    }

    const int filterTab = m_iec104PointFilterTabBar ? m_iec104PointFilterTabBar->currentIndex() : 0;
    const QString keyword = m_iec104PointDataRefFilterEdit
        ? m_iec104PointDataRefFilterEdit->text().trimmed() : QString();

    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        bool visible = true;

        // 分类筛选：tab 0=全部, 1=遥测(Measurement), 2=遥信(Status), 3=控制(Control)
        if (filterTab > 0) {
            QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
            const int category = checkItem ? checkItem->data(Qt::UserRole).toInt() : 0;
            // filterTab: 1→Measurement(0), 2→Status(1), 3→Control(2)
            if (category != filterTab - 1) {
                visible = false;
            }
        }

        // DataRef / Description 文本筛选
        if (visible && !keyword.isEmpty()) {
            QTableWidgetItem *dataRefItem = m_iec104PointsTable->item(row, Iec101PointColumnDataRef);
            const QString dataRef = dataRefItem ? dataRefItem->text() : QString();
            QTableWidgetItem *descItem = m_iec104PointsTable->item(row, Iec101PointColumnDescription);
            const QString desc = descItem ? descItem->text() : QString();
            if (!dataRef.contains(keyword, Qt::CaseInsensitive)
                && !desc.contains(keyword, Qt::CaseInsensitive)) {
                visible = false;
            }
        }

        m_iec104PointsTable->setRowHidden(row, !visible);
    }
}

void MainWindow::pasteClipboardIntoIec104PointsTable()
{
    const QList<QStringList> clipboardRows = parseClipboardTable(QApplication::clipboard()->text());
    if (clipboardRows.isEmpty()) {
        return;
    }

    QModelIndexList targets = sortedEditableTargetIndexes(m_iec104PointsTable);
    const bool useSelectedCells = targets.size() > 1;
    const int startRow = useSelectedCells ? targets.first().row() : m_iec104PointsTable->currentRow();
    const int startColumn = useSelectedCells ? targets.first().column() : m_iec104PointsTable->currentColumn();
    if (startRow < 0 || startColumn < 0) {
        return;
    }

    pushIec104PointsUndoSnapshot();
    m_iec104PointsTable->blockSignals(true);

    if (useSelectedCells && clipboardRows.size() == 1 && clipboardRows.first().size() == 1) {
        // 单值 → 填充所有选中可编辑格
        const QString value = clipboardRows.first().first();
        for (const QModelIndex &target : targets) {
            if (QTableWidgetItem *item = m_iec104PointsTable->item(target.row(), target.column());
                item && (item->flags() & Qt::ItemIsEditable)) {
                item->setText(value);
            }
        }
    } else if (useSelectedCells && clipboardRows.size() * clipboardRows.first().size() == targets.size()) {
        // 形状匹配 → 一一对应填充
        int valueIndex = 0;
        for (const QStringList &clipboardRow : clipboardRows) {
            for (const QString &value : clipboardRow) {
                const QModelIndex target = targets.at(valueIndex++);
                if (QTableWidgetItem *item = m_iec104PointsTable->item(target.row(), target.column());
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(value);
                }
            }
        }
    } else {
        // 从起始格开始逐行逐列粘贴
        for (int rowOffset = 0; rowOffset < clipboardRows.size(); ++rowOffset) {
            const int row = startRow + rowOffset;
            if (row >= m_iec104PointsTable->rowCount()) {
                break;
            }
            for (int columnOffset = 0; columnOffset < clipboardRows.at(rowOffset).size(); ++columnOffset) {
                const int column = startColumn + columnOffset;
                if (column >= m_iec104PointsTable->columnCount()) {
                    break;
                }
                if (QTableWidgetItem *item = m_iec104PointsTable->item(row, column);
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(clipboardRows.at(rowOffset).at(columnOffset));
                }
            }
        }
    }

    m_iec104PointsTable->blockSignals(false);
    highlightIec104DuplicateAddresses();
}

void MainWindow::clearSelectedIec104PointCells()
{
    QModelIndexList targets = sortedEditableTargetIndexes(m_iec104PointsTable);
    if (targets.isEmpty()) {
        return;
    }

    bool hasEditableText = false;
    for (const QModelIndex &target : targets) {
        QTableWidgetItem *item = m_iec104PointsTable->item(target.row(), target.column());
        if (item && (item->flags() & Qt::ItemIsEditable) && !item->text().isEmpty()) {
            hasEditableText = true;
            break;
        }
    }
    if (!hasEditableText) {
        return;
    }

    pushIec104PointsUndoSnapshot();
    m_iec104PointsTable->blockSignals(true);
    for (const QModelIndex &target : targets) {
        if (QTableWidgetItem *item = m_iec104PointsTable->item(target.row(), target.column());
            item && (item->flags() & Qt::ItemIsEditable) && !item->text().isEmpty()) {
            item->setText(QString());
        }
    }
    m_iec104PointsTable->blockSignals(false);
    highlightIec104DuplicateAddresses();
    statusBar()->showMessage(QStringLiteral("已清空选中单元格"), 2000);
}

void MainWindow::pushIec104PointsUndoSnapshot()
{
    if (!m_iec104PointsTable) {
        return;
    }

    QJsonObject snapshot;
    QJsonArray rows;
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        QJsonObject rowObj;
        rowObj[QStringLiteral("deviceId")] = m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
        rowObj[QStringLiteral("dataRef")] = m_iec104PointsTable->item(row, Iec101PointColumnDataRef)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        rowObj[QStringLiteral("enabled")] = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        rowObj[QStringLiteral("deviceaddr")] = m_iec104PointsTable->item(row, Iec101PointColumnAddress)
            ? m_iec104PointsTable->item(row, Iec101PointColumnAddress)->text() : QString();
        QWidget *w = m_iec104PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            rowObj[QStringLiteral("deathzone_type")] = combo->currentData().toString();
        }
        rowObj[QStringLiteral("deathzone")] = m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)->text() : QString();
        rows.append(rowObj);
    }
    snapshot[QStringLiteral("rows")] = rows;
    m_iec104PointsUndoStack.append(snapshot);

    constexpr int maxUndo = 50;
    while (m_iec104PointsUndoStack.size() > maxUndo) {
        m_iec104PointsUndoStack.removeFirst();
    }
}

void MainWindow::undoIec104PointsLastEdit()
{
    if (m_iec104PointsUndoStack.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有可撤回的编辑"), 2000);
        return;
    }
    if (!m_iec104PointsTable) {
        return;
    }

    const QJsonObject snapshot = m_iec104PointsUndoStack.takeLast();
    const QJsonArray rows = snapshot.value(QStringLiteral("rows")).toArray();

    // 排序和拖动也使用同一套撤回栈：先按点位身份恢复行序，再恢复各行编辑值。
    QHash<QString, int> currentRowsByKey;
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        const QString deviceId = m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
        const QString dataRef = m_iec104PointsTable->item(row, Iec101PointColumnDataRef)
            ? m_iec104PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
        currentRowsByKey.insert(deviceId + QStringLiteral("|") + dataRef, row);
    }
    QList<int> sourceRows;
    sourceRows.reserve(rows.size());
    bool canRestoreOrder = rows.size() == m_iec104PointsTable->rowCount();
    for (const QJsonValue &rowValue : rows) {
        const QJsonObject rowObj = rowValue.toObject();
        const QString key = rowObj.value(QStringLiteral("deviceId")).toString().trimmed()
            + QStringLiteral("|")
            + rowObj.value(QStringLiteral("dataRef")).toString().trimmed();
        if (!currentRowsByKey.contains(key)) {
            canRestoreOrder = false;
            break;
        }
        sourceRows.append(currentRowsByKey.value(key));
    }
    if (!canRestoreOrder) {
        statusBar()->showMessage(QStringLiteral("点位集合已变化，无法撤回此前的 IEC104 点表编辑"), 5000);
        return;
    }
    rebuildIec104PointRowsInOrder(sourceRows);

    m_iec104PointsTable->blockSignals(true);
    for (int i = 0; i < rows.size() && i < m_iec104PointsTable->rowCount(); ++i) {
        const QJsonObject rowObj = rows.at(i).toObject();

        QTableWidgetItem *checkItem = m_iec104PointsTable->item(i, Iec101PointColumnEnabled);
        if (checkItem) {
            checkItem->setCheckState(rowObj.value(QStringLiteral("enabled")).toBool(true)
                ? Qt::Checked : Qt::Unchecked);
        }

        if (m_iec104PointsTable->item(i, Iec101PointColumnAddress)) {
            m_iec104PointsTable->item(i, Iec101PointColumnAddress)
                ->setText(rowObj.value(QStringLiteral("deviceaddr")).toString());
        }

        QWidget *w = m_iec104PointsTable->cellWidget(i, Iec101PointColumnDeadzoneType);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            const int idx = combo->findData(rowObj.value(QStringLiteral("deathzone_type")).toString(QStringLiteral("0")));
            if (idx >= 0) combo->setCurrentIndex(idx);
        }

        if (m_iec104PointsTable->item(i, Iec101PointColumnDeadzone)) {
            m_iec104PointsTable->item(i, Iec101PointColumnDeadzone)
                ->setText(rowObj.value(QStringLiteral("deathzone")).toString(QStringLiteral("0.2")));
        }
    }
    m_iec104PointsTable->blockSignals(false);
    highlightIec104DuplicateAddresses();

    statusBar()->showMessage(QStringLiteral("已撤回 IEC104 点表编辑"), 3000);
}

QJsonObject MainWindow::serializeIec104LocalhostConfig() const
{
    QJsonObject root;

    // ---- 基本设置 ----
    root[QStringLiteral("communication_mode")] = QStringLiteral("104");
    root[QStringLiteral("com_addr")] = m_iec104ComAddrEdit->text().trimmed();
    root[QStringLiteral("code_ip")] = m_iec104CodeIpEdit->text().trimmed();
    root[QStringLiteral("code_port")] = m_iec104CodePortEdit->text().trimmed();

    // ---- 协议参数 ----
    root[QStringLiteral("3_transmit_reason")] = m_iec104CotCombo->currentData().toString();
    root[QStringLiteral("3_pubilc_length")] = m_iec104CaCombo->currentData().toString();
    root[QStringLiteral("3_address_length")] = m_iec104IoaCombo->currentData().toString();
    root[QStringLiteral("3_link_addr_length")] = m_iec104LinkAddrCombo->currentData().toString();
    root[QStringLiteral("3_telecontrol_type")] = m_iec104TelecontrolTypeCombo->currentData().toString();
    root[QStringLiteral("3_telemetry_type")] = m_iec104TelemetryTypeCombo->currentData().toString();
    root[QStringLiteral("sequence")] = m_iec104SequenceCombo->currentData().toString();
    root[QStringLiteral("YX_use_double_value")] = m_iec104YxUseDoubleValueCombo->currentData().toString();
    root[QStringLiteral("YX_all_s_trans_d_flag")] = m_iec104YxAllSTransDFlagCombo->currentData().toString();
    root[QStringLiteral("t0")] = m_iec104T0Edit->text().trimmed();
    root[QStringLiteral("t1")] = m_iec104T1Edit->text().trimmed();
    root[QStringLiteral("t2")] = m_iec104T2Edit->text().trimmed();
    root[QStringLiteral("t3")] = m_iec104T3Edit->text().trimmed();
    root[QStringLiteral("k")] = m_iec104KEdit->text().trimmed();
    root[QStringLiteral("w")] = m_iec104WEdit->text().trimmed();

    // ---- 点表 meas_points ----
    QJsonArray pointsArray;
    if (m_iec104PointsTable) {
        for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
            // 检查启用标志
            QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
            if (checkItem && checkItem->checkState() != Qt::Checked) {
                continue;
            }

            const QString deviceId = m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)
                ? m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
            const QString dataRef = m_iec104PointsTable->item(row, Iec101PointColumnDataRef)
                ? m_iec104PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
            const QString description = m_iec104PointsTable->item(row, Iec101PointColumnDescription)
                ? m_iec104PointsTable->item(row, Iec101PointColumnDescription)->text().trimmed() : QString();
            const QString deviceaddr = m_iec104PointsTable->item(row, Iec101PointColumnAddress)
                ? m_iec104PointsTable->item(row, Iec101PointColumnAddress)->text().trimmed() : QString();

            if (dataRef.isEmpty() || deviceaddr.isEmpty()) {
                continue;
            }

            // 死区类型 — 从 cellWidget (QComboBox) 读取
            QString deathzoneType = QStringLiteral("0");
            QWidget *w = m_iec104PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
            if (auto *combo = qobject_cast<QComboBox *>(w)) {
                deathzoneType = combo->currentData().toString();
            }

            // 死区值
            const QString deathzone = m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)
                ? m_iec104PointsTable->item(row, Iec101PointColumnDeadzone)->text().trimmed() : QStringLiteral("0.2");

            QJsonObject point;
            point[QStringLiteral("datafrom")] = deviceId;
            point[QStringLiteral("dataRef")] = dataRef;
            point[QStringLiteral("deviceaddr")] = deviceaddr;
            if (!description.isEmpty()) {
                point[QStringLiteral("description")] = description;
            }
            point[QStringLiteral("deathzone_type")] = deathzoneType;
            point[QStringLiteral("deathzone")] = deathzone;

            pointsArray.append(point);
        }
    }
    root[QStringLiteral("meas_points")] = pointsArray;

    return root;
}

void MainWindow::clearIec104ConfigPage()
{
    if (!m_iec104ConfigPage) {
        return;
    }

    // 基本设置
    m_iec104CodeIpEdit->setText(QStringLiteral("0.0.0.0"));
    m_iec104ComAddrEdit->setText(QStringLiteral("1"));
    m_iec104CodePortEdit->setText(QStringLiteral("2404"));

    // 协议参数
    m_iec104CotCombo->setCurrentIndex(m_iec104CotCombo->findData(2));
    m_iec104CaCombo->setCurrentIndex(m_iec104CaCombo->findData(2));
    m_iec104IoaCombo->setCurrentIndex(m_iec104IoaCombo->findData(3));
    m_iec104LinkAddrCombo->setCurrentIndex(m_iec104LinkAddrCombo->findData(2));
    m_iec104TelecontrolTypeCombo->setCurrentIndex(0);
    m_iec104TelemetryTypeCombo->setCurrentIndex(1);
    m_iec104SequenceCombo->setCurrentIndex(0);
    m_iec104YxUseDoubleValueCombo->setCurrentIndex(0);
    m_iec104YxAllSTransDFlagCombo->setCurrentIndex(0);
    m_iec104T0Edit->setText(QStringLiteral("10"));
    m_iec104T1Edit->setText(QStringLiteral("15"));
    m_iec104T2Edit->setText(QStringLiteral("10"));
    m_iec104T3Edit->setText(QStringLiteral("20"));
    m_iec104KEdit->setText(QStringLiteral("12"));
    m_iec104WEdit->setText(QStringLiteral("8"));

    // 点表
    m_iec104PointsTable->setRowCount(0);
}

void MainWindow::loadIec104LocalhostConfigFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        clearIec104ConfigPage();
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        clearIec104ConfigPage();
        return;
    }

    loadIec104LocalhostConfigFromJson(doc.object());
}

void MainWindow::loadIec104LocalhostConfigFromJson(const QJsonObject &root)
{
    // ---- 基本设置 ----
    m_iec104ComAddrEdit->setText(root.value(QStringLiteral("com_addr")).toString());
    const QString codeIp = root.value(QStringLiteral("code_ip")).toString();
    m_iec104CodeIpEdit->setText(codeIp.isEmpty() ? QStringLiteral("0.0.0.0") : codeIp);
    const QString codePort = root.value(QStringLiteral("code_port")).toString();
    m_iec104CodePortEdit->setText(codePort.isEmpty() ? QStringLiteral("2404") : codePort);

    // ---- 协议参数 ----
    auto setComboByIntData = [](QComboBox *combo, int value) {
        const int idx = combo->findData(value);
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    setComboByIntData(m_iec104CotCombo, root.value(QStringLiteral("3_transmit_reason")).toString().toInt());
    setComboByIntData(m_iec104CaCombo, root.value(QStringLiteral("3_pubilc_length")).toString().toInt());
    setComboByIntData(m_iec104IoaCombo, root.value(QStringLiteral("3_address_length")).toString().toInt());
    setComboByIntData(m_iec104LinkAddrCombo, root.value(QStringLiteral("3_link_addr_length")).toString().toInt());

    auto setComboByStringData = [](QComboBox *combo, const QString &value) {
        const int idx = combo->findData(value);
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    setComboByStringData(m_iec104TelecontrolTypeCombo, root.value(QStringLiteral("3_telecontrol_type")).toString());
    setComboByStringData(m_iec104TelemetryTypeCombo, root.value(QStringLiteral("3_telemetry_type")).toString());

    setComboByIntData(m_iec104SequenceCombo, root.value(QStringLiteral("sequence")).toString().toInt());
    setComboByIntData(m_iec104YxUseDoubleValueCombo, root.value(QStringLiteral("YX_use_double_value")).toString().toInt());
    setComboByIntData(m_iec104YxAllSTransDFlagCombo, root.value(QStringLiteral("YX_all_s_trans_d_flag")).toString().toInt());
    auto setTextOrDefault = [&root](QLineEdit *edit, const QString &key, const QString &defaultValue) {
        const QString value = root.value(key).toString();
        edit->setText(value.isEmpty() ? defaultValue : value);
    };
    setTextOrDefault(m_iec104T0Edit, QStringLiteral("t0"), QStringLiteral("10"));
    setTextOrDefault(m_iec104T1Edit, QStringLiteral("t1"), QStringLiteral("15"));
    setTextOrDefault(m_iec104T2Edit, QStringLiteral("t2"), QStringLiteral("10"));
    setTextOrDefault(m_iec104T3Edit, QStringLiteral("t3"), QStringLiteral("20"));
    setTextOrDefault(m_iec104KEdit, QStringLiteral("k"), QStringLiteral("12"));
    setTextOrDefault(m_iec104WEdit, QStringLiteral("w"), QStringLiteral("8"));

    // ---- 点表：收集 JSON 中的已保存配置，再从设备刷新 ----
    QHash<QString, QJsonObject> savedSettings;
    QStringList savedPointOrder;
    const QJsonArray pointsArray = root.value(QStringLiteral("meas_points")).toArray();
    for (const QJsonValue &pointVal : pointsArray) {
        if (!pointVal.isObject()) {
            continue;
        }
        const QJsonObject pt = pointVal.toObject();
        QString deviceId = pt.value(QStringLiteral("deviceId")).toString();
        if (deviceId.isEmpty()) {
            deviceId = pt.value(QStringLiteral("datafrom")).toString();
        }
        const QString dataRef = pt.value(QStringLiteral("dataRef")).toString();
        if (dataRef.isEmpty()) {
            continue;
        }
        // key = "deviceId|dataRef"；兼容旧格式无 deviceId 的情况
        const QString key = deviceId.isEmpty()
            ? (QStringLiteral("|") + dataRef)
            : (deviceId + QStringLiteral("|") + dataRef);
        savedSettings[key] = pt;
        savedPointOrder.append(key);
    }

    // 清空后从设备重建（JSON 中保存的设置会被合并）
    m_iec104PointsTable->setRowCount(0);
    refreshIec104PointsFromDevices(savedSettings, savedPointOrder);

}

QSet<QString> MainWindow::checkIec104DuplicateAddresses() const
{
    QSet<QString> duplicates;
    if (!m_iec104PointsTable) {
        return duplicates;
    }

    QHash<QString, int> addressCount;
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        // 只检查启用的行
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        if (!checkItem || checkItem->checkState() != Qt::Checked) {
            continue;
        }
        // 读取北向104地址
        QTableWidgetItem *addrItem = m_iec104PointsTable->item(row, Iec101PointColumnAddress);
        const QString addr = addrItem ? addrItem->text().trimmed() : QString();
        if (addr.isEmpty()) {
            continue;
        }
        ++addressCount[addr];
    }

    for (auto it = addressCount.begin(); it != addressCount.end(); ++it) {
        if (it.value() > 1) {
            duplicates.insert(it.key());
        }
    }
    return duplicates;
}

static int iec104ParseAddress(const QString &text, bool *ok = nullptr)
{
    if (ok) *ok = false;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    // 支持 0x 前缀的十六进制
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("0X"), Qt::CaseInsensitive)) {
        const int val = trimmed.mid(2).toInt(ok, 16);
        return val;
    }
    // 纯数字按十进制解析
    const int val = trimmed.toInt(ok, 10);
    return val;
}

void MainWindow::onSortIec104PointsClicked()
{
    if (!m_iec104PointsTable || m_iec104PointsTable->rowCount() < 2) {
        return;
    }

    QList<int> sourceRows;
    sourceRows.reserve(m_iec104PointsTable->rowCount());
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        sourceRows.append(row);
    }

    std::stable_sort(sourceRows.begin(), sourceRows.end(), [this](int leftRow, int rightRow) {
        const QTableWidgetItem *leftItem = m_iec104PointsTable->item(leftRow, Iec101PointColumnAddress);
        const QTableWidgetItem *rightItem = m_iec104PointsTable->item(rightRow, Iec101PointColumnAddress);
        bool leftOk = false;
        bool rightOk = false;
        const int leftAddress = iec104ParseAddress(leftItem ? leftItem->text() : QString(), &leftOk);
        const int rightAddress = iec104ParseAddress(rightItem ? rightItem->text() : QString(), &rightOk);
        if (leftOk != rightOk) {
            return leftOk; // 有效地址在前，空地址或格式错误的地址在后。
        }
        if (!leftOk || leftAddress == rightAddress) {
            return false; // 稳定保留空地址、错误地址及相同地址的原有相对顺序。
        }
        return leftAddress < rightAddress;
    });

    bool changed = false;
    for (int row = 0; row < sourceRows.size(); ++row) {
        if (sourceRows[row] != row) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        statusBar()->showMessage(QStringLiteral("IEC104 点表已按北向104地址升序排列"), 5000);
        return;
    }

    pushIec104PointsUndoSnapshot();
    rebuildIec104PointRowsInOrder(sourceRows);
    statusBar()->showMessage(QStringLiteral("已按北向104地址升序排列 IEC104 点表"), 5000);
}

static QString iec104CategoryDisplayName(int category)
{
    switch (category) {
    case 0: return QStringLiteral("遥测");
    case 1: return QStringLiteral("遥信");
    case 2: return QStringLiteral("遥控/遥调");
    default: return QStringLiteral("未知");
    }
}

// 根据点位类型获取期望的地址范围：
//   遥信       0x0001 - 0x3FFF
//   遥测       0x4001 - 0x5FFF
//   遥控/遥调  0x6001 - 0x7FFF
static void iec104ExpectedAddressRange(int category, int *minAddr, int *maxAddr)
{
    switch (category) {
    case 1: // 遥信
        *minAddr = 0x0001;
        *maxAddr = 0x3FFF;
        break;
    case 0: // 遥测
        *minAddr = 0x4001;
        *maxAddr = 0x5FFF;
        break;
    case 2: // 遥控/遥调
        *minAddr = 0x6001;
        *maxAddr = 0x7FFF;
        break;
    default:
        *minAddr = 0;
        *maxAddr = 0x7FFF;
        break;
    }
}

QStringList MainWindow::checkIec104AddressRangeErrors() const
{
    QStringList errors;
    if (!m_iec104PointsTable) {
        return errors;
    }

    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        // 只检查启用的行
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        if (!checkItem || checkItem->checkState() != Qt::Checked) {
            continue;
        }
        const int category = checkItem->data(Qt::UserRole).toInt();

        // 读取北向104地址
        QTableWidgetItem *addrItem = m_iec104PointsTable->item(row, Iec101PointColumnAddress);
        const QString addrText = addrItem ? addrItem->text().trimmed() : QString();
        if (addrText.isEmpty()) {
            continue;
        }

        bool ok = false;
        const int addr = iec104ParseAddress(addrText, &ok);
        if (!ok) {
            continue;
        }

        int minAddr = 0;
        int maxAddr = 0x7FFF;
        iec104ExpectedAddressRange(category, &minAddr, &maxAddr);

        if (addr < minAddr || addr > maxAddr) {
            const QString deviceId = m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)
                ? m_iec104PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
            const QString dataRef = m_iec104PointsTable->item(row, Iec101PointColumnDataRef)
                ? m_iec104PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
            const QString catName = iec104CategoryDisplayName(category);
            errors.append(QStringLiteral("%1/%2 地址 %3（0x%4）不在%5期望范围 0x%6 - 0x%7")
                .arg(deviceId.isEmpty() ? QStringLiteral("?") : deviceId,
                     dataRef.isEmpty() ? QStringLiteral("?") : dataRef,
                     addrText,
                     QString::number(addr, 16).toUpper().rightJustified(4, QLatin1Char('0')),
                     catName,
                     QString::number(minAddr, 16).toUpper().rightJustified(4, QLatin1Char('0')),
                     QString::number(maxAddr, 16).toUpper().rightJustified(4, QLatin1Char('0'))));
        }
    }
    return errors;
}

void MainWindow::highlightIec104DuplicateAddresses()
{
    if (!m_iec104PointsTable) {
        return;
    }

    const QSet<QString> duplicates = checkIec104DuplicateAddresses();
    const QStringList rangeErrors = checkIec104AddressRangeErrors();
    const QColor duplicateColor(QStringLiteral("#c0392b"));
    const QColor rangeErrorColor(QStringLiteral("#b9770e"));
    const QColor normalColor = m_iec104PointsTable->palette().text().color();

    // 扫描每行，收集范围错误行号
    QSet<int> rangeErrorRows;
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        if (!checkItem || checkItem->checkState() != Qt::Checked) {
            continue;
        }
        const int category = checkItem->data(Qt::UserRole).toInt();
        QTableWidgetItem *addrItem = m_iec104PointsTable->item(row, Iec101PointColumnAddress);
        const QString addrText = addrItem ? addrItem->text().trimmed() : QString();
        if (addrText.isEmpty()) {
            continue;
        }
        bool ok = false;
        const int addr = iec104ParseAddress(addrText, &ok);
        if (!ok) {
            continue;
        }
        int minAddr = 0;
        int maxAddr = 0x7FFF;
        iec104ExpectedAddressRange(category, &minAddr, &maxAddr);
        if (addr < minAddr || addr > maxAddr) {
            rangeErrorRows.insert(row);
        }
    }

    // 遍历所有行，着色
    for (int row = 0; row < m_iec104PointsTable->rowCount(); ++row) {
        QTableWidgetItem *checkItem = m_iec104PointsTable->item(row, Iec101PointColumnEnabled);
        const bool enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        QTableWidgetItem *addrItem = m_iec104PointsTable->item(row, Iec101PointColumnAddress);
        const QString addr = addrItem ? addrItem->text().trimmed() : QString();
        const bool isDuplicate = enabled && !addr.isEmpty() && duplicates.contains(addr);
        const bool isRangeError = rangeErrorRows.contains(row);

        // 着色北向104地址列：重复 > 范围错误 > 正常
        if (addrItem) {
            if (isDuplicate) {
                addrItem->setForeground(duplicateColor);
            } else if (isRangeError) {
                addrItem->setForeground(rangeErrorColor);
            } else {
                addrItem->setForeground(normalColor);
            }
        }
    }

    // 更新校验标签
    if (m_iec104ValidationLabel) {
        QStringList messages;
        if (!duplicates.isEmpty()) {
            messages << QStringLiteral("重复地址：%1").arg(
                QStringList(duplicates.begin(), duplicates.end()).join(QStringLiteral("，")));
        }
        if (!rangeErrors.isEmpty()) {
            messages << QStringLiteral("地址范围异常（%1处）").arg(rangeErrors.size());
        }

        if (!messages.isEmpty()) {
            m_iec104ValidationLabel->setStyleSheet(QStringLiteral("QLabel { color: #c0392b; }"));
            m_iec104ValidationLabel->setText(
                QStringLiteral("⚠ %1。请修正后再导出。").arg(messages.join(QStringLiteral("；"))));
        } else {
            m_iec104ValidationLabel->setStyleSheet(QStringLiteral("QLabel { color: #2e7d32; }"));
            m_iec104ValidationLabel->setText(QStringLiteral("✓ 当前地址分配未发现问题。"));
        }
    }
}
