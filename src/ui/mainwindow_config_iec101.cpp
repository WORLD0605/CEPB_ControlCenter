#include "mainwindow_config_p.h"

using namespace cepb_config_helpers;

// ============================================================
// IEC101 配置页面 — 槽函数 & 序列化
// ============================================================

void MainWindow::onIec101CommModeChanged(int index)
{
    const int mode = m_iec101CommModeCombo->itemData(index).toInt();
    const bool needSerial = (mode == 0 || mode == 2);
    m_iec101SerialParamsGroup->setVisible(needSerial);
}

void MainWindow::onRefreshIec101PointsClicked()
{
    refreshIec101PointsFromDevices();
}

void MainWindow::refreshIec101PointsFromDevices(const QHash<QString, QJsonObject> &savedPointSettings)
{
    if (!m_iec101PointsTable) {
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
    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        const QString deviceId = m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)
            ? m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
        const QString dataRef = m_iec101PointsTable->item(row, Iec101PointColumnDataRef)
            ? m_iec101PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
        if (deviceId.isEmpty() || dataRef.isEmpty()) {
            continue;
        }
        const QString key = deviceId + QStringLiteral("|") + dataRef;
        PointSettings s;
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
        s.enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        if (m_iec101PointsTable->item(row, Iec101PointColumnAddress)) {
            s.deviceaddr = m_iec101PointsTable->item(row, Iec101PointColumnAddress)->text().trimmed();
        }
        QWidget *w = m_iec101PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            s.deathzoneType = combo->currentData().toString();
        }
        if (m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)) {
            s.deathzone = m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)->text().trimmed();
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
    QList<DevicePoint> devicePoints;
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
            devicePoints.append(dp);
        }
    }

    // 4. 重建表格
    m_iec101PointsTable->blockSignals(true);
    m_iec101PointsTable->setRowCount(0);

    for (const DevicePoint &dp : devicePoints) {
        const QString key = dp.deviceId + QStringLiteral("|") + dp.dataRef;
        const int row = m_iec101PointsTable->rowCount();
        m_iec101PointsTable->insertRow(row);

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
        m_iec101PointsTable->setItem(row, Iec101PointColumnDragHandle, handleItem);

        // Col 1: 启用 (checkbox)
        auto *checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
        checkItem->setData(Qt::UserRole, dp.category);
        m_iec101PointsTable->setItem(row, Iec101PointColumnEnabled, checkItem);

        // Col 2: DeviceId (read-only)
        auto *devIdItem = new QTableWidgetItem(dp.deviceId);
        devIdItem->setFlags(devIdItem->flags() & ~Qt::ItemIsEditable);
        m_iec101PointsTable->setItem(row, Iec101PointColumnDeviceId, devIdItem);

        // Col 3: DataRef (read-only)
        auto *dataRefItem = new QTableWidgetItem(dp.dataRef);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
        m_iec101PointsTable->setItem(row, Iec101PointColumnDataRef, dataRefItem);

        // Col 4: Description (read-only)
        auto *descItem = new QTableWidgetItem(dp.description);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_iec101PointsTable->setItem(row, Iec101PointColumnDescription, descItem);

        // Col 5: 北向101地址
        m_iec101PointsTable->setItem(row, Iec101PointColumnAddress, new QTableWidgetItem(deviceaddr));

        // Col 6: 死区类型 (QComboBox via setCellWidget)
        auto *dzTypeCombo = new QComboBox();
        configureTableCellCombo(dzTypeCombo, this);
        dzTypeCombo->addItem(QStringLiteral("0 — 百分比"), QStringLiteral("0"));
        dzTypeCombo->addItem(QStringLiteral("1 — 固定值"), QStringLiteral("1"));
        const int dzTypeIdx = dzTypeCombo->findData(deathzoneType);
        dzTypeCombo->setCurrentIndex(dzTypeIdx >= 0 ? dzTypeIdx : 0);
        m_iec101PointsTable->setCellWidget(row, Iec101PointColumnDeadzoneType, dzTypeCombo);

        // Col 7: 死区值
        m_iec101PointsTable->setItem(row, Iec101PointColumnDeadzone, new QTableWidgetItem(deathzone));
    }

    m_iec101PointsTable->blockSignals(false);
    applyIec101PointsFilter();
    highlightIec101DuplicateAddresses();
}

void MainWindow::onIec101PointItemChanged(QTableWidgetItem *item)
{
    if (!item) {
        return;
    }
    // 当地址列或启用列变化时，刷新重复地址高亮
    if (item->column() == Iec101PointColumnEnabled || item->column() == Iec101PointColumnAddress) {
        highlightIec101DuplicateAddresses();
    }
}

void MainWindow::onIec101PointFilterChanged(int /*index*/)
{
    applyIec101PointsFilter();
}

void MainWindow::onIec101PointFilterTextChanged()
{
    applyIec101PointsFilter();
}

void MainWindow::applyIec101PointsFilter()
{
    if (!m_iec101PointsTable) {
        return;
    }

    const int filterTab = m_iec101PointFilterTabBar ? m_iec101PointFilterTabBar->currentIndex() : 0;
    const QString dataRefKeyword = m_iec101PointDataRefFilterEdit
        ? m_iec101PointDataRefFilterEdit->text().trimmed() : QString();
    const QString descKeyword = m_iec101PointDescriptionFilterEdit
        ? m_iec101PointDescriptionFilterEdit->text().trimmed() : QString();

    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        bool visible = true;

        // 分类筛选：tab 0=全部, 1=遥测(Measurement), 2=遥信(Status), 3=控制(Control)
        if (filterTab > 0) {
            QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
            const int category = checkItem ? checkItem->data(Qt::UserRole).toInt() : 0;
            // filterTab: 1→Measurement(0), 2→Status(1), 3→Control(2)
            if (category != filterTab - 1) {
                visible = false;
            }
        }

        // DataRef 文本筛选
        if (visible && !dataRefKeyword.isEmpty()) {
            QTableWidgetItem *dataRefItem = m_iec101PointsTable->item(row, Iec101PointColumnDataRef);
            const QString dataRef = dataRefItem ? dataRefItem->text() : QString();
            if (!dataRef.contains(dataRefKeyword, Qt::CaseInsensitive)) {
                visible = false;
            }
        }

        // Description 文本筛选
        if (visible && !descKeyword.isEmpty()) {
            QTableWidgetItem *descItem = m_iec101PointsTable->item(row, Iec101PointColumnDescription);
            const QString desc = descItem ? descItem->text() : QString();
            if (!desc.contains(descKeyword, Qt::CaseInsensitive)) {
                visible = false;
            }
        }

        m_iec101PointsTable->setRowHidden(row, !visible);
    }
}

void MainWindow::pasteClipboardIntoIec101PointsTable()
{
    const QList<QStringList> clipboardRows = parseClipboardTable(QApplication::clipboard()->text());
    if (clipboardRows.isEmpty()) {
        return;
    }

    QModelIndexList targets = sortedEditableTargetIndexes(m_iec101PointsTable);
    const bool useSelectedCells = targets.size() > 1;
    const int startRow = useSelectedCells ? targets.first().row() : m_iec101PointsTable->currentRow();
    const int startColumn = useSelectedCells ? targets.first().column() : m_iec101PointsTable->currentColumn();
    if (startRow < 0 || startColumn < 0) {
        return;
    }

    pushIec101PointsUndoSnapshot();
    m_iec101PointsTable->blockSignals(true);

    if (useSelectedCells && clipboardRows.size() == 1 && clipboardRows.first().size() == 1) {
        // 单值 → 填充所有选中可编辑格
        const QString value = clipboardRows.first().first();
        for (const QModelIndex &target : targets) {
            if (QTableWidgetItem *item = m_iec101PointsTable->item(target.row(), target.column());
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
                if (QTableWidgetItem *item = m_iec101PointsTable->item(target.row(), target.column());
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(value);
                }
            }
        }
    } else {
        // 从起始格开始逐行逐列粘贴
        for (int rowOffset = 0; rowOffset < clipboardRows.size(); ++rowOffset) {
            const int row = startRow + rowOffset;
            if (row >= m_iec101PointsTable->rowCount()) {
                break;
            }
            for (int columnOffset = 0; columnOffset < clipboardRows.at(rowOffset).size(); ++columnOffset) {
                const int column = startColumn + columnOffset;
                if (column >= m_iec101PointsTable->columnCount()) {
                    break;
                }
                if (QTableWidgetItem *item = m_iec101PointsTable->item(row, column);
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(clipboardRows.at(rowOffset).at(columnOffset));
                }
            }
        }
    }

    m_iec101PointsTable->blockSignals(false);
    highlightIec101DuplicateAddresses();
}

void MainWindow::pushIec101PointsUndoSnapshot()
{
    if (!m_iec101PointsTable) {
        return;
    }

    QJsonObject snapshot;
    QJsonArray rows;
    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        QJsonObject rowObj;
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
        rowObj[QStringLiteral("enabled")] = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        rowObj[QStringLiteral("deviceaddr")] = m_iec101PointsTable->item(row, Iec101PointColumnAddress)
            ? m_iec101PointsTable->item(row, Iec101PointColumnAddress)->text() : QString();
        QWidget *w = m_iec101PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            rowObj[QStringLiteral("deathzone_type")] = combo->currentData().toString();
        }
        rowObj[QStringLiteral("deathzone")] = m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)
            ? m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)->text() : QString();
        rows.append(rowObj);
    }
    snapshot[QStringLiteral("rows")] = rows;
    m_iec101PointsUndoStack.append(snapshot);

    constexpr int maxUndo = 50;
    while (m_iec101PointsUndoStack.size() > maxUndo) {
        m_iec101PointsUndoStack.removeFirst();
    }
}

void MainWindow::undoIec101PointsLastEdit()
{
    if (m_iec101PointsUndoStack.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有可撤回的编辑"), 2000);
        return;
    }
    if (!m_iec101PointsTable) {
        return;
    }

    const QJsonObject snapshot = m_iec101PointsUndoStack.takeLast();
    const QJsonArray rows = snapshot.value(QStringLiteral("rows")).toArray();

    m_iec101PointsTable->blockSignals(true);
    for (int i = 0; i < rows.size() && i < m_iec101PointsTable->rowCount(); ++i) {
        const QJsonObject rowObj = rows.at(i).toObject();

        QTableWidgetItem *checkItem = m_iec101PointsTable->item(i, Iec101PointColumnEnabled);
        if (checkItem) {
            checkItem->setCheckState(rowObj.value(QStringLiteral("enabled")).toBool(true)
                ? Qt::Checked : Qt::Unchecked);
        }

        if (m_iec101PointsTable->item(i, Iec101PointColumnAddress)) {
            m_iec101PointsTable->item(i, Iec101PointColumnAddress)
                ->setText(rowObj.value(QStringLiteral("deviceaddr")).toString());
        }

        QWidget *w = m_iec101PointsTable->cellWidget(i, Iec101PointColumnDeadzoneType);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            const int idx = combo->findData(rowObj.value(QStringLiteral("deathzone_type")).toString(QStringLiteral("0")));
            if (idx >= 0) combo->setCurrentIndex(idx);
        }

        if (m_iec101PointsTable->item(i, Iec101PointColumnDeadzone)) {
            m_iec101PointsTable->item(i, Iec101PointColumnDeadzone)
                ->setText(rowObj.value(QStringLiteral("deathzone")).toString(QStringLiteral("0.2")));
        }
    }
    m_iec101PointsTable->blockSignals(false);
    highlightIec101DuplicateAddresses();

    statusBar()->showMessage(QStringLiteral("已撤回 IEC101 点表编辑"), 3000);
}

QJsonObject MainWindow::serializeIec101LocalhostConfig() const
{
    QJsonObject root;

    // ---- 基本设置 ----
    const int commMode = m_iec101CommModeCombo->currentData().toInt();
    root[QStringLiteral("communication_mode")] = QString::number(commMode);
    root[QStringLiteral("com_addr")] = m_iec101ComAddrEdit->text().trimmed();
    root[QStringLiteral("code_port")] = m_iec101CodePortEdit->text().trimmed();

    // ---- 串口连接参数 (按需输出) ----
    if (commMode == 0 || commMode == 2) {
        root[QStringLiteral("code_usart_name")] = m_iec101UsartNameEdit->text().trimmed();
        root[QStringLiteral("code_baudrate")] = m_iec101BaudrateCombo->currentText().trimmed();
        root[QStringLiteral("code_starbit")] = m_iec101DataBitCombo->currentText().trimmed();
        root[QStringLiteral("code_stopbit")] = m_iec101StopBitCombo->currentText().trimmed();
        root[QStringLiteral("code_verity")] = m_iec101ParityCombo->currentText().trimmed();
    }

    // ---- 协议参数 ----
    root[QStringLiteral("3_transmit_reason")] = m_iec101CotCombo->currentData().toString();
    root[QStringLiteral("3_pubilc_length")] = m_iec101CaCombo->currentData().toString();
    root[QStringLiteral("3_address_length")] = m_iec101IoaCombo->currentData().toString();
    root[QStringLiteral("3_link_addr_length")] = m_iec101LinkAddrCombo->currentData().toString();
    root[QStringLiteral("3_telecontrol_type")] = m_iec101TelecontrolTypeCombo->currentData().toString();
    root[QStringLiteral("3_telemetry_type")] = m_iec101TelemetryTypeCombo->currentData().toString();
    root[QStringLiteral("sequence")] = m_iec101SequenceCombo->currentData().toString();
    root[QStringLiteral("YX_use_double_value")] = m_iec101YxUseDoubleValueCombo->currentData().toString();
    root[QStringLiteral("YX_all_s_trans_d_flag")] = m_iec101YxAllSTransDFlagCombo->currentData().toString();

    // ---- 3_Cmd_* 命令码（固定默认值，不在 UI 中编辑） ----
    root[QStringLiteral("3_Cmd_Read_Single_ShortF")] = 102;
    root[QStringLiteral("3_Cmd_Read_Multi_ShortF")] = 132;
    root[QStringLiteral("3_Cmd_Read_Signle_Normal")] = -1;
    root[QStringLiteral("3_Cmd_Read_Multi_Normal")] = -2;
    root[QStringLiteral("3_Cmd_Read_Signle_Scaled")] = -3;
    root[QStringLiteral("3_Cmd_Read_Multi_Scaled")] = -4;
    root[QStringLiteral("3_Cmd_Set_Single_ShortF")] = 50;
    root[QStringLiteral("3_Cmd_Set_Multi_ShortF")] = 136;
    root[QStringLiteral("3_Cmd_Set_Signle_Normal")] = -5;
    root[QStringLiteral("3_Cmd_Set_Multi_Normal")] = -6;
    root[QStringLiteral("3_Cmd_Set_Signle_Scaled")] = -7;
    root[QStringLiteral("3_Cmd_Set_Multi_Scaled")] = -8;

    // ---- 点表 meas_points ----
    QJsonArray pointsArray;
    if (m_iec101PointsTable) {
        for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
            // 检查启用标志
            QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
            if (checkItem && checkItem->checkState() != Qt::Checked) {
                continue;
            }

            const QString deviceId = m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)
                ? m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
            const QString dataRef = m_iec101PointsTable->item(row, Iec101PointColumnDataRef)
                ? m_iec101PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
            const QString description = m_iec101PointsTable->item(row, Iec101PointColumnDescription)
                ? m_iec101PointsTable->item(row, Iec101PointColumnDescription)->text().trimmed() : QString();
            const QString deviceaddr = m_iec101PointsTable->item(row, Iec101PointColumnAddress)
                ? m_iec101PointsTable->item(row, Iec101PointColumnAddress)->text().trimmed() : QString();

            if (dataRef.isEmpty() || deviceaddr.isEmpty()) {
                continue;
            }

            // 死区类型 — 从 cellWidget (QComboBox) 读取
            QString deathzoneType = QStringLiteral("0");
            QWidget *w = m_iec101PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
            if (auto *combo = qobject_cast<QComboBox *>(w)) {
                deathzoneType = combo->currentData().toString();
            }

            // 死区值
            const QString deathzone = m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)
                ? m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)->text().trimmed() : QStringLiteral("0.2");

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

void MainWindow::clearIec101ConfigPage()
{
    if (!m_iec101ConfigPage) {
        return;
    }

    // 基本设置
    m_iec101CommModeCombo->setCurrentIndex(1); // TCP
    m_iec101ComAddrEdit->clear();
    m_iec101CodePortEdit->setText(QStringLiteral("2404"));

    // 串口参数
    m_iec101UsartNameEdit->clear();
    m_iec101BaudrateCombo->setCurrentText(QStringLiteral("9600"));
    m_iec101DataBitCombo->setCurrentIndex(3); // 8
    m_iec101StopBitCombo->setCurrentIndex(0); // 1
    m_iec101ParityCombo->setCurrentIndex(0);  // None

    // 协议参数
    m_iec101CotCombo->setCurrentIndex(0);     // 1
    m_iec101CaCombo->setCurrentIndex(1);       // 2
    m_iec101IoaCombo->setCurrentIndex(1);      // 2
    m_iec101LinkAddrCombo->setCurrentIndex(0); // 1
    m_iec101TelecontrolTypeCombo->setCurrentIndex(1); // 单命令
    m_iec101TelemetryTypeCombo->setCurrentIndex(2);   // 短浮点数
    m_iec101SequenceCombo->setCurrentIndex(1);        // 1 — 连续地址批量打包
    m_iec101YxUseDoubleValueCombo->setCurrentIndex(0);
    m_iec101YxAllSTransDFlagCombo->setCurrentIndex(0);

    // 点表
    m_iec101PointsTable->setRowCount(0);
}

void MainWindow::loadIec101LocalhostConfigFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        clearIec101ConfigPage();
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        clearIec101ConfigPage();
        return;
    }

    loadIec101LocalhostConfigFromJson(doc.object());
}

void MainWindow::loadIec101LocalhostConfigFromJson(const QJsonObject &root)
{
    // ---- 基本设置 ----
    const int commMode = root.value(QStringLiteral("communication_mode")).toString().toInt();
    const int commModeIdx = m_iec101CommModeCombo->findData(commMode);
    if (commModeIdx >= 0) {
        m_iec101CommModeCombo->setCurrentIndex(commModeIdx);
    }
    m_iec101ComAddrEdit->setText(root.value(QStringLiteral("com_addr")).toString());
    const QString codePort = root.value(QStringLiteral("code_port")).toString();
    m_iec101CodePortEdit->setText(codePort.isEmpty() ? QStringLiteral("2404") : codePort);

    // ---- 串口连接参数 ----
    m_iec101UsartNameEdit->setText(root.value(QStringLiteral("code_usart_name")).toString());
    const QString baudrate = root.value(QStringLiteral("code_baudrate")).toString();
    if (!baudrate.isEmpty()) {
        m_iec101BaudrateCombo->setCurrentText(baudrate);
    }
    const QString starbit = root.value(QStringLiteral("code_starbit")).toString();
    if (!starbit.isEmpty()) {
        const int sbIdx = m_iec101DataBitCombo->findText(starbit);
        if (sbIdx >= 0) m_iec101DataBitCombo->setCurrentIndex(sbIdx);
    }
    const QString stopbit = root.value(QStringLiteral("code_stopbit")).toString();
    if (!stopbit.isEmpty()) {
        const int spIdx = m_iec101StopBitCombo->findText(stopbit);
        if (spIdx >= 0) m_iec101StopBitCombo->setCurrentIndex(spIdx);
    }
    const QString verity = root.value(QStringLiteral("code_verity")).toString();
    if (!verity.isEmpty()) {
        const int vIdx = m_iec101ParityCombo->findText(verity);
        if (vIdx >= 0) m_iec101ParityCombo->setCurrentIndex(vIdx);
    }

    // ---- 协议参数 ----
    auto setComboByIntData = [](QComboBox *combo, int value) {
        const int idx = combo->findData(value);
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    setComboByIntData(m_iec101CotCombo, root.value(QStringLiteral("3_transmit_reason")).toString().toInt());
    setComboByIntData(m_iec101CaCombo, root.value(QStringLiteral("3_pubilc_length")).toString().toInt());
    setComboByIntData(m_iec101IoaCombo, root.value(QStringLiteral("3_address_length")).toString().toInt());
    setComboByIntData(m_iec101LinkAddrCombo, root.value(QStringLiteral("3_link_addr_length")).toString().toInt());

    auto setComboByStringData = [](QComboBox *combo, const QString &value) {
        const int idx = combo->findData(value);
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    setComboByStringData(m_iec101TelecontrolTypeCombo, root.value(QStringLiteral("3_telecontrol_type")).toString());
    setComboByStringData(m_iec101TelemetryTypeCombo, root.value(QStringLiteral("3_telemetry_type")).toString());

    setComboByIntData(m_iec101SequenceCombo, root.value(QStringLiteral("sequence")).toString().toInt());
    setComboByIntData(m_iec101YxUseDoubleValueCombo, root.value(QStringLiteral("YX_use_double_value")).toString().toInt());
    setComboByIntData(m_iec101YxAllSTransDFlagCombo, root.value(QStringLiteral("YX_all_s_trans_d_flag")).toString().toInt());

    // ---- 点表：收集 JSON 中的已保存配置，再从设备刷新 ----
    QHash<QString, QJsonObject> savedSettings;
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
    }

    // 清空后从设备重建（JSON 中保存的设置会被合并）
    m_iec101PointsTable->setRowCount(0);
    refreshIec101PointsFromDevices(savedSettings);

    // 更新串口参数显隐
    onIec101CommModeChanged(m_iec101CommModeCombo->currentIndex());
}

QSet<QString> MainWindow::checkIec101DuplicateAddresses() const
{
    QSet<QString> duplicates;
    if (!m_iec101PointsTable) {
        return duplicates;
    }

    QHash<QString, int> addressCount;
    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        // 只检查启用的行
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
        if (!checkItem || checkItem->checkState() != Qt::Checked) {
            continue;
        }
        // 读取北向101地址
        QTableWidgetItem *addrItem = m_iec101PointsTable->item(row, Iec101PointColumnAddress);
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

static int iec101ParseAddress(const QString &text, bool *ok = nullptr)
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

static QString iec101CategoryDisplayName(int category)
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
static void iec101ExpectedAddressRange(int category, int *minAddr, int *maxAddr)
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

QStringList MainWindow::checkIec101AddressRangeErrors() const
{
    QStringList errors;
    if (!m_iec101PointsTable) {
        return errors;
    }

    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        // 只检查启用的行
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
        if (!checkItem || checkItem->checkState() != Qt::Checked) {
            continue;
        }
        const int category = checkItem->data(Qt::UserRole).toInt();

        // 读取北向101地址
        QTableWidgetItem *addrItem = m_iec101PointsTable->item(row, Iec101PointColumnAddress);
        const QString addrText = addrItem ? addrItem->text().trimmed() : QString();
        if (addrText.isEmpty()) {
            continue;
        }

        bool ok = false;
        const int addr = iec101ParseAddress(addrText, &ok);
        if (!ok) {
            continue;
        }

        int minAddr = 0;
        int maxAddr = 0x7FFF;
        iec101ExpectedAddressRange(category, &minAddr, &maxAddr);

        if (addr < minAddr || addr > maxAddr) {
            const QString deviceId = m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)
                ? m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
            const QString dataRef = m_iec101PointsTable->item(row, Iec101PointColumnDataRef)
                ? m_iec101PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
            const QString catName = iec101CategoryDisplayName(category);
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

void MainWindow::highlightIec101DuplicateAddresses()
{
    if (!m_iec101PointsTable) {
        return;
    }

    const QSet<QString> duplicates = checkIec101DuplicateAddresses();
    const QStringList rangeErrors = checkIec101AddressRangeErrors();
    const QColor duplicateColor(QStringLiteral("#c0392b"));
    const QColor rangeErrorColor(QStringLiteral("#b9770e"));
    const QColor normalColor = m_iec101PointsTable->palette().text().color();

    // 扫描每行，收集范围错误行号
    QSet<int> rangeErrorRows;
    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
        if (!checkItem || checkItem->checkState() != Qt::Checked) {
            continue;
        }
        const int category = checkItem->data(Qt::UserRole).toInt();
        QTableWidgetItem *addrItem = m_iec101PointsTable->item(row, Iec101PointColumnAddress);
        const QString addrText = addrItem ? addrItem->text().trimmed() : QString();
        if (addrText.isEmpty()) {
            continue;
        }
        bool ok = false;
        const int addr = iec101ParseAddress(addrText, &ok);
        if (!ok) {
            continue;
        }
        int minAddr = 0;
        int maxAddr = 0x7FFF;
        iec101ExpectedAddressRange(category, &minAddr, &maxAddr);
        if (addr < minAddr || addr > maxAddr) {
            rangeErrorRows.insert(row);
        }
    }

    // 遍历所有行，着色
    for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
        const bool enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        QTableWidgetItem *addrItem = m_iec101PointsTable->item(row, Iec101PointColumnAddress);
        const QString addr = addrItem ? addrItem->text().trimmed() : QString();
        const bool isDuplicate = enabled && !addr.isEmpty() && duplicates.contains(addr);
        const bool isRangeError = rangeErrorRows.contains(row);

        // 着色北向101地址列：重复 > 范围错误 > 正常
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
    if (m_iec101ValidationLabel) {
        QStringList messages;
        if (!duplicates.isEmpty()) {
            messages << QStringLiteral("重复地址：%1").arg(
                QStringList(duplicates.begin(), duplicates.end()).join(QStringLiteral("，")));
        }
        if (!rangeErrors.isEmpty()) {
            messages << QStringLiteral("地址范围异常（%1处）").arg(rangeErrors.size());
        }

        if (!messages.isEmpty()) {
            m_iec101ValidationLabel->setStyleSheet(QStringLiteral("QLabel { color: #c0392b; }"));
            m_iec101ValidationLabel->setText(
                QStringLiteral("⚠ %1。请修正后再导出。").arg(messages.join(QStringLiteral("；"))));
        } else {
            m_iec101ValidationLabel->setStyleSheet(QStringLiteral("QLabel { color: #2e7d32; }"));
            m_iec101ValidationLabel->setText(QStringLiteral("✓ 当前地址分配未发现问题。"));
        }
    }
}
