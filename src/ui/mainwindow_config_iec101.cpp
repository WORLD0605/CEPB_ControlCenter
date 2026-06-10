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
        const QString deviceId = m_iec101PointsTable->item(row, 1)
            ? m_iec101PointsTable->item(row, 1)->text().trimmed() : QString();
        const QString dataRef = m_iec101PointsTable->item(row, 2)
            ? m_iec101PointsTable->item(row, 2)->text().trimmed() : QString();
        if (deviceId.isEmpty() || dataRef.isEmpty()) {
            continue;
        }
        const QString key = deviceId + QStringLiteral("|") + dataRef;
        PointSettings s;
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, 0);
        s.enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        if (m_iec101PointsTable->item(row, 4)) {
            s.deviceaddr = m_iec101PointsTable->item(row, 4)->text().trimmed();
        }
        QWidget *w = m_iec101PointsTable->cellWidget(row, 5);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            s.deathzoneType = combo->currentData().toString();
        }
        if (m_iec101PointsTable->item(row, 6)) {
            s.deathzone = m_iec101PointsTable->item(row, 6)->text().trimmed();
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

        // Col 0: 启用 (checkbox)
        auto *checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
        checkItem->setData(Qt::UserRole, dp.category);
        m_iec101PointsTable->setItem(row, 0, checkItem);

        // Col 1: DeviceId (read-only)
        auto *devIdItem = new QTableWidgetItem(dp.deviceId);
        devIdItem->setFlags(devIdItem->flags() & ~Qt::ItemIsEditable);
        m_iec101PointsTable->setItem(row, 1, devIdItem);

        // Col 2: DataRef (read-only)
        auto *dataRefItem = new QTableWidgetItem(dp.dataRef);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
        m_iec101PointsTable->setItem(row, 2, dataRefItem);

        // Col 3: Description (read-only)
        auto *descItem = new QTableWidgetItem(dp.description);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_iec101PointsTable->setItem(row, 3, descItem);

        // Col 4: 北向101地址
        m_iec101PointsTable->setItem(row, 4, new QTableWidgetItem(deviceaddr));

        // Col 5: 死区类型 (QComboBox via setCellWidget)
        auto *dzTypeCombo = new QComboBox();
        dzTypeCombo->addItem(QStringLiteral("0 — 百分比"), QStringLiteral("0"));
        dzTypeCombo->addItem(QStringLiteral("1 — 固定值"), QStringLiteral("1"));
        const int dzTypeIdx = dzTypeCombo->findData(deathzoneType);
        dzTypeCombo->setCurrentIndex(dzTypeIdx >= 0 ? dzTypeIdx : 0);
        m_iec101PointsTable->setCellWidget(row, 5, dzTypeCombo);

        // Col 6: 死区值
        m_iec101PointsTable->setItem(row, 6, new QTableWidgetItem(deathzone));
    }

    m_iec101PointsTable->blockSignals(false);
    applyIec101PointsFilter();
}

void MainWindow::onIec101PointItemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item);
    // checkbox 通过 checkState 读取，combo 通过 cellWidget 读取，无需自动联动
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
            QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, 0);
            const int category = checkItem ? checkItem->data(Qt::UserRole).toInt() : 0;
            // filterTab: 1→Measurement(0), 2→Status(1), 3→Control(2)
            if (category != filterTab - 1) {
                visible = false;
            }
        }

        // DataRef 文本筛选
        if (visible && !dataRefKeyword.isEmpty()) {
            QTableWidgetItem *dataRefItem = m_iec101PointsTable->item(row, 2);
            const QString dataRef = dataRefItem ? dataRefItem->text() : QString();
            if (!dataRef.contains(dataRefKeyword, Qt::CaseInsensitive)) {
                visible = false;
            }
        }

        // Description 文本筛选
        if (visible && !descKeyword.isEmpty()) {
            QTableWidgetItem *descItem = m_iec101PointsTable->item(row, 3);
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
        QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, 0);
        rowObj[QStringLiteral("enabled")] = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
        rowObj[QStringLiteral("deviceaddr")] = m_iec101PointsTable->item(row, 4)
            ? m_iec101PointsTable->item(row, 4)->text() : QString();
        QWidget *w = m_iec101PointsTable->cellWidget(row, 5);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            rowObj[QStringLiteral("deathzone_type")] = combo->currentData().toString();
        }
        rowObj[QStringLiteral("deathzone")] = m_iec101PointsTable->item(row, 6)
            ? m_iec101PointsTable->item(row, 6)->text() : QString();
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

        QTableWidgetItem *checkItem = m_iec101PointsTable->item(i, 0);
        if (checkItem) {
            checkItem->setCheckState(rowObj.value(QStringLiteral("enabled")).toBool(true)
                ? Qt::Checked : Qt::Unchecked);
        }

        if (m_iec101PointsTable->item(i, 4)) {
            m_iec101PointsTable->item(i, 4)->setText(rowObj.value(QStringLiteral("deviceaddr")).toString());
        }

        QWidget *w = m_iec101PointsTable->cellWidget(i, 5);
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            const int idx = combo->findData(rowObj.value(QStringLiteral("deathzone_type")).toString(QStringLiteral("0")));
            if (idx >= 0) combo->setCurrentIndex(idx);
        }

        if (m_iec101PointsTable->item(i, 6)) {
            m_iec101PointsTable->item(i, 6)->setText(rowObj.value(QStringLiteral("deathzone")).toString(QStringLiteral("0.2")));
        }
    }
    m_iec101PointsTable->blockSignals(false);

    statusBar()->showMessage(QStringLiteral("已撤回 IEC101 点表编辑"), 3000);
}

QJsonObject MainWindow::serializeIec101LocalhostConfig() const
{
    QJsonObject root;

    // ---- 基本设置 ----
    const int commMode = m_iec101CommModeCombo->currentData().toInt();
    root[QStringLiteral("communication_mode")] = QString::number(commMode);
    root[QStringLiteral("com_addr")] = m_iec101ComAddrEdit->text().trimmed();

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
            QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, 0);
            if (checkItem && checkItem->checkState() != Qt::Checked) {
                continue;
            }

            const QString deviceId = m_iec101PointsTable->item(row, 1)
                ? m_iec101PointsTable->item(row, 1)->text().trimmed() : QString();
            const QString dataRef = m_iec101PointsTable->item(row, 2)
                ? m_iec101PointsTable->item(row, 2)->text().trimmed() : QString();
            const QString description = m_iec101PointsTable->item(row, 3)
                ? m_iec101PointsTable->item(row, 3)->text().trimmed() : QString();
            const QString deviceaddr = m_iec101PointsTable->item(row, 4)
                ? m_iec101PointsTable->item(row, 4)->text().trimmed() : QString();

            if (dataRef.isEmpty() || deviceaddr.isEmpty()) {
                continue;
            }

            // 死区类型 — 从 cellWidget (QComboBox) 读取
            QString deathzoneType = QStringLiteral("0");
            QWidget *w = m_iec101PointsTable->cellWidget(row, 5);
            if (auto *combo = qobject_cast<QComboBox *>(w)) {
                deathzoneType = combo->currentData().toString();
            }

            // 死区值
            const QString deathzone = m_iec101PointsTable->item(row, 6)
                ? m_iec101PointsTable->item(row, 6)->text().trimmed() : QStringLiteral("0.2");

            QJsonObject point;
            point[QStringLiteral("datafrom")] = QStringLiteral("4");
            point[QStringLiteral("deviceId")] = deviceId;
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
        const QString deviceId = pt.value(QStringLiteral("deviceId")).toString();
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
