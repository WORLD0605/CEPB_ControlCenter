#include "mainwindow_config_p.h"

using namespace cepb_config_helpers;

void MainWindow::onConfigModelSelectionChanged()
{
    const int modelIndex = currentConfigModelIndex();
    if (m_deleteModelBtn) {
        m_deleteModelBtn->setEnabled(modelIndex >= 0);
    }
    refreshModelOverview(modelIndex);
    refreshModelDetail(modelIndex);
    refreshSelectionOverview();
    refreshEditorNavigationCombos();
}

void MainWindow::onConfigDeviceSelectionChanged()
{
    const int deviceIndex = currentConfigDeviceIndex();
    if (m_deleteDeviceBtn) {
        m_deleteDeviceBtn->setEnabled(deviceIndex >= 0);
    }
    refreshDeviceDetail(deviceIndex);
    if (deviceIndex >= 0) {
        refreshDeviceEditor(deviceIndex);
    } else {
        refreshDeviceEditor(-1);
    }
    refreshSelectionOverview();
    if (m_deleteModelBtn) {
        m_deleteModelBtn->setEnabled(currentConfigModelIndex() >= 0);
    }
    if (m_deleteDeviceBtn) {
        m_deleteDeviceBtn->setEnabled(currentConfigDeviceIndex() >= 0);
    }
    refreshEditorNavigationCombos();
}

void MainWindow::onConfigModelActivated(int row, int /*column*/)
{
    if (row < 0) {
        return;
    }

    m_configModelTable->selectRow(row);
    refreshModelDetail(row);
    m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
}

void MainWindow::onConfigDeviceActivated(int row, int /*column*/)
{
    if (row < 0) {
        return;
    }

    m_configDeviceTable->selectRow(row);
    refreshDeviceDetail(row);
    refreshDeviceEditor(row);
    m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
}

void MainWindow::onModelEditorSelectionChanged(int index)
{
    if (m_updatingEditorNavigationCombos || !m_modelEditorCombo || index < 0) {
        return;
    }

    bool ok = false;
    const int row = m_modelEditorCombo->itemData(index).toInt(&ok);
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (!ok || row < 0 || row >= project.models.size()) {
        return;
    }

    if (m_configModelTable) {
        m_configModelTable->selectRow(row);
    }
    refreshModelOverview(row);
    refreshModelDetail(row);
    refreshSelectionOverview();
    if (m_mainTabWidget && m_modelEditorPage) {
        m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    }
}

void MainWindow::onDeviceEditorSelectionChanged(int index)
{
    if (m_updatingEditorNavigationCombos || !m_deviceEditorCombo || index < 0) {
        return;
    }

    bool ok = false;
    const int row = m_deviceEditorCombo->itemData(index).toInt(&ok);
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (!ok || row < 0 || row >= project.devices.size()) {
        return;
    }

    if (m_configDeviceTable) {
        m_configDeviceTable->selectRow(row);
    }
    refreshDeviceDetail(row);
    refreshDeviceEditor(row);
    refreshSelectionOverview();
    if (m_mainTabWidget && m_deviceEditorPage) {
        m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    }
}

void MainWindow::onConfigIssueActivated(int row, int /*column*/)
{
    navigateToConfigIssue(row);
}

void MainWindow::navigateToConfigIssue(int row)
{
    if (!m_configIssueTable || row < 0 || row >= m_configIssueTable->rowCount()) {
        return;
    }

    QTableWidgetItem *anchorItem = m_configIssueTable->item(row, 0);
    if (!anchorItem) {
        return;
    }

    const QString targetType = anchorItem->data(ConfigIssueRoleTargetType).toString();
    const QString targetKey = anchorItem->data(ConfigIssueRoleTargetKey).toString();
    const configtool::ConfigProject &project = m_configProjectManager.project();

    if (targetType == QStringLiteral("model")) {
        for (int index = 0; index < project.models.size(); ++index) {
            if (project.models.at(index).modelId == targetKey) {
                onConfigModelActivated(index, 0);
                return;
            }
        }
    } else if (targetType == QStringLiteral("device")) {
        for (int index = 0; index < project.devices.size(); ++index) {
            if (project.devices.at(index).deviceId == targetKey) {
                onConfigDeviceActivated(index, 0);
                return;
            }
        }
    } else if (targetType == QStringLiteral("logic-computation")) {
        refreshLogicComputationPointPage();
        m_mainTabWidget->setCurrentWidget(m_logicComputationPointPage);
        const QString deviceId = targetKey.section(QLatin1Char('#'), 0, 0);
        const QString dataRef = targetKey.section(QLatin1Char('#'), 1);
        const QList<configtool::LogicComputationPoint> &points = project.logicCenter.computationPoints;
        for (int index = 0; index < points.size(); ++index) {
            if ((points.at(index).deviceId == deviceId && points.at(index).dataRef == dataRef)
                || targetKey.contains(points.at(index).deviceId + QLatin1Char('#') + points.at(index).dataRef)) {
                m_logicComputationPointTable->selectRow(index);
                return;
            }
        }
        statusBar()->showMessage(QStringLiteral("已进入计算点页面，但未找到精确对象"), 5000);
        return;
    } else if (targetType == QStringLiteral("logic-control")) {
        refreshLogicControlRulePage();
        m_mainTabWidget->setCurrentWidget(m_logicControlRulePage);
        const QString deviceId = targetKey.section(QLatin1Char('#'), 0, 0);
        const QString dataRef = targetKey.section(QLatin1Char('#'), 1);
        const QList<configtool::LogicControlRule> &rules = project.logicCenter.controlRules;
        for (int index = 0; index < rules.size(); ++index) {
            if ((rules.at(index).matchDeviceId == deviceId && rules.at(index).matchDataRef == dataRef)
                || targetKey.contains(rules.at(index).matchDeviceId + QLatin1Char('#') + rules.at(index).matchDataRef)) {
                m_logicControlRuleTable->selectRow(index);
                refreshLogicControlTargetTable();
                return;
            }
        }
        statusBar()->showMessage(QStringLiteral("已进入控制转换页面，但未找到精确对象"), 5000);
        return;
    } else if (targetType == QStringLiteral("logic-online")) {
        refreshLogicOnlineLinkPage();
        m_mainTabWidget->setCurrentWidget(m_logicOnlineLinkPage);
        const QList<configtool::LogicOnlineStatusLink> &links = project.logicCenter.onlineStatusLinks;
        for (int index = 0; index < links.size(); ++index) {
            const QString objectId = QStringLiteral("%1 -> %2").arg(links.at(index).deviceId, links.at(index).linkToDeviceId);
            if (objectId == targetKey || targetKey.contains(objectId)) {
                m_logicOnlineLinkTable->selectRow(index);
                return;
            }
        }
        statusBar()->showMessage(QStringLiteral("已进入在线联动页面，但未找到精确对象"), 5000);
        return;
    } else if (targetType == QStringLiteral("logic-agcavc")) {
        refreshLogicAgcAvcPage();
        m_mainTabWidget->setCurrentWidget(m_logicAgcAvcPage);
        statusBar()->showMessage(QStringLiteral("已进入 AGC/AVC 页面"), 5000);
        return;
    } else if (targetType == QStringLiteral("iec101")) {
        if (m_iec101ConfigPage) {
            m_mainTabWidget->setCurrentWidget(m_iec101ConfigPage);
            statusBar()->showMessage(QStringLiteral("已进入 IEC101 配置页面，请检查点表地址。"), 5000);
            return;
        }
    }

    m_mainTabWidget->setCurrentWidget(m_configIssuePage);
    statusBar()->showMessage(QStringLiteral("该问题暂时无法定位到具体编辑对象"), 5000);
}

void MainWindow::onNewModelClicked()
{
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (project.projectId.isEmpty()) {
        m_configProjectManager.createEmptyProject(QStringLiteral("本地配置工程"), QString());
    }

    configtool::ModelTemplate model;
    model.modelId = QStringLiteral("model_new_%1").arg(project.models.size() + 1);
    model.name = model.modelId;
    model.displayName = QStringLiteral("新模型%1").arg(project.models.size() + 1);
    model.deviceType = QStringLiteral("未定义设备");
    model.version = QStringLiteral("1.0");
    model.ensureDefaultServices();
    project.models.append(model);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int row = m_configModelTable->rowCount() - 1;
    if (row >= 0) {
        m_configModelTable->selectRow(row);
    }
    m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    statusBar()->showMessage(QStringLiteral("已创建模型骨架"), 4000);
}

void MainWindow::onModelFieldEdited()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    QString oldModelId = m_modelIdEdit
        ? m_modelIdEdit->property(ModelEditorOriginalModelIdProperty).toString().trimmed()
        : QString();
    if (oldModelId.isEmpty()) {
        oldModelId = model.modelId.trimmed();
    }
    const QString newModelId = m_modelIdEdit->text().trimmed();
    model.modelId = newModelId;
    model.name = model.modelId;
    model.displayName = m_modelDisplayNameEdit->text().trimmed();
    model.deviceType = m_modelDeviceTypeEdit->text().trimmed();
    model.version = m_modelVersionEdit->text().trimmed();
    model.manufacturerId = m_modelManufacturerIdEdit->text().trimmed();
    model.manufacturerDesc = m_modelManufacturerDescEdit->text().trimmed();
    model.schema = m_modelSchemaEdit->text().trimmed();
    model.northVisible = !m_modelNorthVisibleCheck || m_modelNorthVisibleCheck->isChecked();

    const int renamedReferenceCount = renameModelReferences(oldModelId, newModelId);
    if (m_modelIdEdit && !newModelId.isEmpty() && oldModelId != newModelId) {
        m_modelIdEdit->setProperty(ModelEditorOriginalModelIdProperty, newModelId);
    }
    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    if (sender() == m_modelNorthVisibleCheck) {
        refreshModelDetail(modelIndex);
    }
    if (renamedReferenceCount > 0) {
        statusBar()->showMessage(QStringLiteral("已同步更新 %1 处模型引用").arg(renamedReferenceCount), 5000);
    }
}

int MainWindow::renameModelReferences(const QString &oldModelId, const QString &newModelId)
{
    const QString oldId = oldModelId.trimmed();
    const QString newId = newModelId.trimmed();
    if (oldId.isEmpty() || newId.isEmpty() || oldId == newId) {
        return 0;
    }

    int updateCount = 0;
    configtool::ConfigProject &project = m_configProjectManager.project();
    for (configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.modelId.trimmed() == oldId) {
            device.modelId = newId;
            ++updateCount;
        }
        const QString oldPointRefPrefix = oldId + QLatin1Char('#');
        const QString newPointRefPrefix = newId + QLatin1Char('#');
        for (configtool::PointBinding &binding : device.bindings) {
            if (binding.pointRef.startsWith(oldPointRefPrefix)) {
                binding.pointRef = newPointRefPrefix + binding.pointRef.mid(oldPointRefPrefix.size());
                ++updateCount;
            }
        }
    }

    return updateCount;
}

int MainWindow::renameModelPointReferences(const QString &modelId,
                                           const QString &oldDataRef,
                                           const QString &newDataRef)
{
    const QString targetModelId = modelId.trimmed();
    const QString oldRef = oldDataRef.trimmed();
    const QString newRef = newDataRef.trimmed();
    if (targetModelId.isEmpty() || oldRef.isEmpty() || newRef.isEmpty() || oldRef == newRef) {
        return 0;
    }

    int updateCount = 0;
    QSet<QString> affectedDeviceIds;
    configtool::ConfigProject &project = m_configProjectManager.project();
    const QString oldPointRef = targetModelId + QLatin1Char('#') + oldRef;
    const QString newPointRef = targetModelId + QLatin1Char('#') + newRef;

    for (configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.modelId.trimmed() != targetModelId) {
            continue;
        }

        const QString deviceId = device.deviceId.trimmed();
        if (!deviceId.isEmpty()) {
            affectedDeviceIds.insert(deviceId);
        }
        for (configtool::PointBinding &binding : device.bindings) {
            if (binding.dataRef.trimmed() == oldRef) {
                binding.dataRef = newRef;
                ++updateCount;
            }
            if (binding.pointRef.trimmed() == oldPointRef) {
                binding.pointRef = newPointRef;
                ++updateCount;
            }
        }
    }

    if (affectedDeviceIds.isEmpty()) {
        return updateCount;
    }

    auto isAffectedDevice = [&](const QString &deviceId) {
        return affectedDeviceIds.contains(deviceId.trimmed());
    };
    auto renamePointRef = [&](const QString &deviceId, QString &dataRef) {
        if (isAffectedDevice(deviceId) && dataRef.trimmed() == oldRef) {
            dataRef = newRef;
            ++updateCount;
        }
    };
    auto renameRealtimeRefs = [&](QString &expr) {
        if (expr.isEmpty()) {
            return;
        }
        const QString before = expr;
        for (const QString &deviceId : affectedDeviceIds) {
            const QRegularExpression pattern(QStringLiteral("\\{rt:%1#%2\\}")
                                                 .arg(QRegularExpression::escape(deviceId),
                                                      QRegularExpression::escape(oldRef)));
            expr.replace(pattern, QStringLiteral("{rt:%1#%2}").arg(deviceId, newRef));
        }
        if (expr != before) {
            ++updateCount;
        }
    };

    configtool::LogicCenterConfig &logic = project.logicCenter;
    for (configtool::LogicComputationPoint &point : logic.computationPoints) {
        renamePointRef(point.deviceId, point.dataRef);
        for (configtool::LogicOperand &operand : point.operands) {
            renamePointRef(operand.deviceId, operand.dataRef);
        }
    }

    for (configtool::LogicControlRule &rule : logic.controlRules) {
        renamePointRef(rule.matchDeviceId, rule.matchDataRef);
        for (configtool::LogicControlTarget &target : rule.targets) {
            renamePointRef(target.deviceId, target.dataRef);
            renameRealtimeRefs(target.expr);
        }
    }

    for (configtool::AgcAvcGroup &group : logic.agcAvcGroups) {
        for (configtool::AgcAvcDevice &device : group.devices) {
            renamePointRef(device.deviceId, device.ctrlDataRefP);
            renamePointRef(device.deviceId, device.ctrlDataRefQ);
            const QString onlineDeviceId = device.onlineDeviceId.trimmed().isEmpty()
                ? device.deviceId
                : device.onlineDeviceId;
            renamePointRef(onlineDeviceId, device.onlineDataRef);
        }
    }

    return updateCount;
}

void MainWindow::onAddPointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个模型。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    model.ensureDefaultServices();
    configtool::ModelServiceType newPointType = static_cast<configtool::ModelServiceType>(
        m_newPointCategoryCombo->currentData().toInt());
    if (m_modelPointFilterTabBar->currentIndex() > 0) {
        newPointType = modelServiceTypeFromTabIndex(m_modelPointFilterTabBar->currentIndex());
    }

    configtool::ServiceTemplate *service = model.findService(newPointType);
    if (!service) {
        return;
    }

    configtool::PointTemplate point;
    point.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    point.category = newPointType;
    point.signalType = signalTypeForModelService(newPointType);
    point.controlKind = newPointType == configtool::ModelServiceType::Control
        ? configtool::ControlKind::RemoteControl
        : configtool::ControlKind::None;
    const int nextIndex = service->points.size() + 1;
    point.name = QStringLiteral("NewPoint%1").arg(nextIndex);
    point.description = QStringLiteral("新建点位%1").arg(nextIndex);
    point.ldName = QStringLiteral("PROT");
    point.lnType = defaultLnTypeForModelService(newPointType);
    point.lnInst = QStringLiteral("1");
    point.doName = point.name;
    point.doType = newPointType == configtool::ModelServiceType::Status
        ? QStringLiteral("SPS")
        : (newPointType == configtool::ModelServiceType::Control
            ? QStringLiteral("SPC")
            : QStringLiteral("MV"));
    point.dataType = newPointType == configtool::ModelServiceType::Measurement
        ? QStringLiteral("Float")
        : QStringLiteral("Boolean");
    service->points.append(point);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    selectModelPointById(point.pointId);
    statusBar()->showMessage(QStringLiteral("已新增模型点位"), 3000);
}

void MainWindow::onModelPointFilterChanged(int index)
{
    if (index > 0) {
        const int comboIndex = m_newPointCategoryCombo->findData(static_cast<int>(modelServiceTypeFromTabIndex(index)));
        if (comboIndex >= 0) {
            QSignalBlocker blocker(m_newPointCategoryCombo);
            m_newPointCategoryCombo->setCurrentIndex(comboIndex);
        }
    }

    refreshModelDetail(currentConfigModelIndex());
}

void MainWindow::onModelPointDataRefFilterTextChanged(const QString & /*text*/)
{
    refreshModelDetail(currentConfigModelIndex());
}

void MainWindow::onModelPointDescriptionFilterTextChanged(const QString & /*text*/)
{
    refreshModelDetail(currentConfigModelIndex());
}

void MainWindow::onModelPointCategoryChanged(int index)
{
    if (m_updatingModelPointCategory || index < 0) {
        return;
    }

    auto *combo = qobject_cast<QComboBox*>(sender());
    if (!combo) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    const int serviceIndex = combo->property("serviceIndex").toInt();
    const int pointIndex = combo->property("pointIndex").toInt();
    const configtool::ModelServiceType targetType = static_cast<configtool::ModelServiceType>(combo->itemData(index).toInt());

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &sourceService = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= sourceService.points.size()) {
        return;
    }

    configtool::PointTemplate point = sourceService.points.at(pointIndex);
    if (point.category == targetType) {
        return;
    }

    sourceService.points.removeAt(pointIndex);
    point.category = targetType;
    point.signalType = signalTypeForModelService(targetType);
    point.controlKind = targetType == configtool::ModelServiceType::Control
        ? configtool::ControlKind::RemoteControl
        : configtool::ControlKind::None;
    configtool::ServiceTemplate *targetService = model.findService(targetType);
    if (!targetService) {
        return;
    }
    targetService->points.append(point);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    selectModelPointById(point.pointId);
    statusBar()->showMessage(QStringLiteral("已调整点位类别"), 3000);
}

void MainWindow::onCopyPointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    const QPair<int, int> pointLocation = currentModelPointLocation();
    if (modelIndex < 0 || pointLocation.first < 0 || pointLocation.second < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (pointLocation.first >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[pointLocation.first];
    if (pointLocation.second >= service.points.size()) {
        return;
    }

    configtool::PointTemplate copied = service.points.at(pointLocation.second);
    copied.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copied.name += QStringLiteral("_copy");
    copied.doName = copied.name;
    copied.description += QStringLiteral("-副本");
    service.points.insert(pointLocation.second + 1, copied);

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    if (pointLocation.second + 1 < m_modelPointsTable->rowCount()) {
        m_modelPointsTable->selectRow(pointLocation.second + 1);
    }
    statusBar()->showMessage(QStringLiteral("已复制模型点位"), 3000);
}

void MainWindow::onDeletePointClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    QSet<QString> seenLocations;
    QList<QPair<int, int>> pointLocations;
    const QModelIndexList indexes = m_modelPointsTable->selectionModel()
        ? m_modelPointsTable->selectionModel()->selectedIndexes()
        : QModelIndexList();
    for (const QModelIndex &index : indexes) {
        QTableWidgetItem *handleItem = m_modelPointsTable->item(index.row(), ModelPointColumnDragHandle);
        if (!handleItem) {
            continue;
        }

        const int serviceIndex = handleItem->data(Qt::UserRole).toInt();
        const int pointIndex = handleItem->data(Qt::UserRole + 1).toInt();
        const QString key = QStringLiteral("%1:%2").arg(serviceIndex).arg(pointIndex);
        if (seenLocations.contains(key)) {
            continue;
        }
        if (serviceIndex >= 0
            && serviceIndex < model.services.size()
            && pointIndex >= 0
            && pointIndex < model.services.at(serviceIndex).points.size()) {
            seenLocations.insert(key);
            pointLocations.append(qMakePair(serviceIndex, pointIndex));
        }
    }

    if (pointLocations.isEmpty()) {
        const QPair<int, int> pointLocation = currentModelPointLocation();
        if (pointLocation.first >= 0
            && pointLocation.first < model.services.size()
            && pointLocation.second >= 0
            && pointLocation.second < model.services.at(pointLocation.first).points.size()) {
            pointLocations.append(pointLocation);
        }
    }

    if (pointLocations.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除点位"),
                              pointLocations.size() == 1
                                  ? QStringLiteral("确定删除当前选中的模型点位吗？")
                                  : QStringLiteral("确定删除选中的 %1 个模型点位吗？").arg(pointLocations.size()))
        != QMessageBox::Yes) {
        return;
    }

    std::sort(pointLocations.begin(), pointLocations.end(), [](const QPair<int, int> &left,
                                                               const QPair<int, int> &right) {
        if (left.first != right.first) {
            return left.first > right.first;
        }
        return left.second > right.second;
    });

    for (const QPair<int, int> &pointLocation : pointLocations) {
        if (pointLocation.first >= 0
            && pointLocation.first < model.services.size()
            && pointLocation.second >= 0
            && pointLocation.second < model.services.at(pointLocation.first).points.size()) {
            model.services[pointLocation.first].points.removeAt(pointLocation.second);
        }
    }

    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    statusBar()->showMessage(pointLocations.size() == 1
                                 ? QStringLiteral("已删除模型点位")
                                 : QStringLiteral("已删除 %1 个模型点位").arg(pointLocations.size()),
                             3000);
}

void MainWindow::onCreateDeviceFromModelClicked()
{
    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个模型。"));
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    bool accepted = false;
    const QString protocolName = QInputDialog::getItem(
        this,
        QStringLiteral("选择协议设备"),
        QStringLiteral("请选择要创建的协议设备:"),
        {QStringLiteral("104"), QStringLiteral("Modbus")},
        0,
        false,
        &accepted);
    if (!accepted || protocolName.isEmpty()) {
        return;
    }
    const bool createModbus = protocolName.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0;

    const QString deviceId = QInputDialog::getText(
        this,
        createModbus ? QStringLiteral("创建 Modbus 设备") : QStringLiteral("创建 104 设备"),
        QStringLiteral("请输入 DeviceId:"),
        QLineEdit::Normal,
        QStringLiteral("1"),
        &accepted).trimmed();
    if (!accepted || deviceId.isEmpty()) {
        return;
    }

    configtool::ProtocolDeviceInstance device;
    device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    device.appType = createModbus ? QStringLiteral("cepmodbus") : QStringLiteral("cepiec104");
    device.protocol = createModbus ? configtool::ProtocolType::Modbus : configtool::ProtocolType::Iec104;
    device.deviceId = deviceId;
    device.deviceDesc = model.displayName.isEmpty() ? model.modelId : model.displayName;
    device.modelId = model.modelId;
    if (createModbus) {
        device.transport.protocolOptions.insert(QStringLiteral("type"), QStringLiteral("RTU"));
        device.transport.protocolOptions.insert(QStringLiteral("debug"), QStringLiteral("off"));
        device.transport.serial.insert(QStringLiteral("serialPort"), QStringLiteral("RS485_1"));
        device.transport.serial.insert(QStringLiteral("baud"), QStringLiteral("9600"));
        device.transport.serial.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
        device.transport.serial.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
        device.transport.serial.insert(QStringLiteral("parity"), QStringLiteral("N"));
        device.modbus.yxType = QStringLiteral("BIT");
        device.modbus.ycType = QStringLiteral("WORD");
        device.modbus.ytType = QStringLiteral("WORD");
        device.modbus.ycScale = QStringLiteral("1.0");
        device.modbus.ytScale = QStringLiteral("1.0");
    }

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            configtool::PointBinding binding;
            binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            binding.pointRef = point.pointRef(model.modelId);
            binding.dataRef = point.dataRef();
            binding.descriptionOverride = point.description;
            binding.enabled = true;
            if (createModbus) {
                QString kind = QStringLiteral("yc");
                if (point.category == configtool::ModelServiceType::Status) {
                    kind = QStringLiteral("yx");
                } else if (point.category == configtool::ModelServiceType::Control) {
                    const QString dataType = point.dataType.trimmed().toLower();
                    kind = dataType == QStringLiteral("boolean") || dataType == QStringLiteral("dbool")
                        ? QStringLiteral("yk")
                        : QStringLiteral("yt");
                }
                const QString lowerDataType = point.dataType.toLower();
                const QString modbusDataType = kind == QStringLiteral("yx")
                    ? QStringLiteral("BIT")
                    : (lowerDataType.contains(QStringLiteral("float")) ? QStringLiteral("FLOAT") : QStringLiteral("WORD"));
                binding.extensions.insert(QStringLiteral("modbusKind"), kind);
                binding.extensions.insert(QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : (kind == QStringLiteral("yc") ? 3 : 6));
                binding.extensions.insert(QStringLiteral("modbusDataType"), modbusDataType);
                binding.extensions.insert(QStringLiteral("modbusScale"), QStringLiteral("1.0"));
            }
            device.bindings.append(binding);
        }
    }

    project.devices.append(device);
    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int row = m_configDeviceTable->rowCount() - 1;
    if (row >= 0) {
        m_configDeviceTable->selectRow(row);
    }
    m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    statusBar()->showMessage(QStringLiteral("已根据模型生成 104 设备绑定骨架"), 4000);
}

void MainWindow::onDeleteModelClicked()
{
    const int modelIndex = currentConfigModelIndex();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        QMessageBox::information(this, QStringLiteral("删除模型"), QStringLiteral("请先选择要删除的模型。"));
        return;
    }

    const configtool::ModelTemplate model = project.models.at(modelIndex);
    int referencedDeviceCount = 0;
    for (const configtool::ProtocolDeviceInstance &device : project.devices) {
        if (device.modelId == model.modelId) {
            ++referencedDeviceCount;
        }
    }

    QString message = QStringLiteral("确定删除模型“%1”？")
        .arg(model.displayName.isEmpty() ? model.modelId : model.displayName);
    if (referencedDeviceCount > 0) {
        message += QStringLiteral("\n\n当前有 %1 个设备使用该模型，删除模型时这些设备也会一起删除。")
            .arg(referencedDeviceCount);
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除模型"),
                              message,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    pushConfigUndoSnapshot();
    const QString deletedModelId = model.modelId;
    for (int index = project.devices.size() - 1; index >= 0; --index) {
        if (project.devices.at(index).modelId == deletedModelId) {
            project.devices.removeAt(index);
        }
    }
    project.models.removeAt(modelIndex);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int nextModelIndex = qMin(modelIndex, project.models.size() - 1);
    if (nextModelIndex >= 0) {
        m_configModelTable->selectRow(nextModelIndex);
    }
    refreshModelOverview(currentConfigModelIndex());
    refreshModelDetail(currentConfigModelIndex());
    refreshDeviceDetail(currentConfigDeviceIndex());
    refreshDeviceEditor(currentConfigDeviceIndex());
    statusBar()->showMessage(QStringLiteral("已删除模型"), 3000);
}

void MainWindow::onDeleteDeviceClicked()
{
    const int deviceIndex = currentConfigDeviceIndex();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        QMessageBox::information(this, QStringLiteral("删除设备"), QStringLiteral("请先选择要删除的设备。"));
        return;
    }

    const configtool::ProtocolDeviceInstance device = project.devices.at(deviceIndex);
    const QString deviceName = device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc;
    if (QMessageBox::question(this,
                              QStringLiteral("删除设备"),
                              QStringLiteral("确定删除设备“%1”？").arg(deviceName),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    pushConfigUndoSnapshot();
    project.devices.removeAt(deviceIndex);

    configtool::ImportReport report;
    refreshConfigImportSummary(report);
    const int nextDeviceIndex = qMin(deviceIndex, project.devices.size() - 1);
    if (nextDeviceIndex >= 0) {
        m_configDeviceTable->selectRow(nextDeviceIndex);
    }
    refreshSelectionOverview();
    refreshDeviceDetail(currentConfigDeviceIndex());
    refreshDeviceEditor(currentConfigDeviceIndex());
    statusBar()->showMessage(QStringLiteral("已删除设备"), 3000);
}

void MainWindow::onDeviceFieldEdited()
{
    const int deviceIndex = currentConfigDeviceIndex();
    if (deviceIndex < 0) {
        return;
    }

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    QString oldDeviceId = m_deviceIdEdit
        ? m_deviceIdEdit->property(DeviceEditorOriginalDeviceIdProperty).toString().trimmed()
        : QString();
    if (oldDeviceId.isEmpty()) {
        oldDeviceId = device.deviceId.trimmed();
    }
    const QString newDeviceId = m_deviceIdEdit->text().trimmed();
    device.deviceId = newDeviceId;
    device.deviceDesc = m_deviceDescEdit->text().trimmed();
    device.transport.stationAddress = m_deviceStationAddressEdit->text().trimmed();
    device.transport.ip = m_deviceIpEdit->text().trimmed();
    device.transport.port = m_devicePortEdit->text().trimmed();
    if (isModbusDevice(device)) {
        const QString type = m_modbusTypeCombo && m_modbusTypeCombo->currentIndex() >= 0
            ? m_modbusTypeCombo->currentText().trimmed().toUpper()
            : QStringLiteral("TCP");
        device.transport.protocolOptions.insert(QStringLiteral("type"), type);
        device.transport.protocolOptions.insert(QStringLiteral("debug"), m_modbusDebugCheck && m_modbusDebugCheck->isChecked()
            ? QStringLiteral("on")
            : QStringLiteral("off"));
        if (m_modbusHwVariantCombo) {
            project.modbus.hwVariant = m_modbusHwVariantCombo->currentData().toString().trimmed();
        }

        QJsonObject rtu;
        rtu.insert(QStringLiteral("serialPort"), m_modbusSerialPortCombo && m_modbusSerialPortCombo->currentIndex() >= 0
            ? m_modbusSerialPortCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("baud"), m_modbusBaudCombo && m_modbusBaudCombo->currentIndex() >= 0
            ? m_modbusBaudCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("dataBits"), m_modbusDataBitsCombo && m_modbusDataBitsCombo->currentIndex() >= 0
            ? m_modbusDataBitsCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("stopBits"), m_modbusStopBitsCombo && m_modbusStopBitsCombo->currentIndex() >= 0
            ? m_modbusStopBitsCombo->currentText().trimmed()
            : QString());
        rtu.insert(QStringLiteral("parity"), m_modbusParityCombo && m_modbusParityCombo->currentIndex() >= 0
            ? m_modbusParityCombo->currentText().trimmed()
            : QString());
        device.transport.serial = rtu;
    }

    const int renamedReferenceCount = renameLogicDeviceReferences(oldDeviceId, newDeviceId);
    if (m_deviceIdEdit && !newDeviceId.isEmpty() && oldDeviceId != newDeviceId) {
        m_deviceIdEdit->setProperty(DeviceEditorOriginalDeviceIdProperty, newDeviceId);
    }
    refreshConfigObjectViews();
    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    if (deviceIndex < m_configDeviceTable->rowCount()) {
        m_configDeviceTable->selectRow(deviceIndex);
    }
    if (renamedReferenceCount > 0) {
        statusBar()->showMessage(QStringLiteral("已同步更新 %1 处 DeviceId 引用").arg(renamedReferenceCount), 5000);
    }
}

int MainWindow::renameLogicDeviceReferences(const QString &oldDeviceId, const QString &newDeviceId)
{
    const QString oldId = oldDeviceId.trimmed();
    const QString newId = newDeviceId.trimmed();
    if (oldId.isEmpty() || newId.isEmpty() || oldId == newId) {
        return 0;
    }

    int updateCount = 0;
    auto renameDeviceId = [&](QString &deviceId) {
        if (deviceId.trimmed() != oldId) {
            return;
        }
        deviceId = newId;
        ++updateCount;
    };
    auto renameRealtimeRefs = [&](QString &expr) {
        if (expr.isEmpty()) {
            return;
        }
        const QString before = expr;
        const QRegularExpression pattern(QStringLiteral("\\{rt:%1#").arg(QRegularExpression::escape(oldId)));
        expr.replace(pattern, QStringLiteral("{rt:%1#").arg(newId));
        if (expr != before) {
            ++updateCount;
        }
    };

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    for (configtool::LogicComputationPoint &point : logic.computationPoints) {
        renameDeviceId(point.deviceId);
        for (configtool::LogicOperand &operand : point.operands) {
            renameDeviceId(operand.deviceId);
        }
    }

    for (configtool::LogicControlRule &rule : logic.controlRules) {
        renameDeviceId(rule.matchDeviceId);
        for (configtool::LogicControlTarget &target : rule.targets) {
            renameDeviceId(target.deviceId);
            renameRealtimeRefs(target.expr);
        }
    }

    for (configtool::AgcAvcGroup &group : logic.agcAvcGroups) {
        renameDeviceId(group.virtualDeviceId);
        for (configtool::AgcAvcDevice &agcDevice : group.devices) {
            renameDeviceId(agcDevice.deviceId);
            renameDeviceId(agcDevice.onlineDeviceId);
        }
    }

    for (configtool::LogicOnlineStatusLink &link : logic.onlineStatusLinks) {
        renameDeviceId(link.deviceId);
        renameDeviceId(link.linkToDeviceId);
    }

    return updateCount;
}

void MainWindow::onDeviceBindingDataRefFilterTextChanged(const QString & /*text*/)
{
    refreshDeviceEditor(currentConfigDeviceIndex());
}

void MainWindow::onDeviceBindingDescriptionFilterTextChanged(const QString & /*text*/)
{
    refreshDeviceEditor(currentConfigDeviceIndex());
}

void MainWindow::onDeviceBindingItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingDeviceBindingsTable || m_restoringConfigUndo) {
        return;
    }

    const int deviceIndex = currentConfigDeviceIndex();
    if (deviceIndex < 0) {
        return;
    }

    QTableWidgetItem *enabledItem = m_deviceBindingsTable->item(item->row(), 0);
    if (!enabledItem) {
        return;
    }

    const int bindingIndex = enabledItem->data(Qt::UserRole).toInt();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::PointBinding &binding = device.bindings[bindingIndex];
    const bool modbusDevice = isModbusDevice(device);
    const int editedRow = item->row();
    const int editedColumn = item->column();
    bool refreshEditor = true;
    if (item->column() == 0) {
        binding.enabled = item->checkState() == Qt::Checked;
    } else if ((modbusDevice && item->column() == ModbusColumnSelfSignal)
               || (!modbusDevice && item->column() == 5)) {
        binding.selfSignalFlag = item->checkState() == Qt::Checked ? QStringLiteral("1") : QString();
        refreshEditor = false;
    } else {
        applyDeviceBindingCellText(item->row(), item->column(), item->text());
    }
    if (modbusDevice && refreshEditor) {
        rebuildModbusDeviceConfig(device);
    }

    if (refreshEditor) {
        refreshDeviceDetail(deviceIndex);
        refreshDeviceEditor(deviceIndex);
        m_deviceBindingsTable->setCurrentCell(editedRow, editedColumn);
    }
}

void MainWindow::onModelPointItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingModelPointsTable || m_restoringConfigUndo) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    if (modelIndex < 0) {
        return;
    }

    QTableWidgetItem *anchorItem = m_modelPointsTable->item(item->row(), ModelPointColumnDragHandle);
    if (!anchorItem) {
        return;
    }

    const int serviceIndex = anchorItem->data(Qt::UserRole).toInt();
    const int pointIndex = anchorItem->data(Qt::UserRole + 1).toInt();

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= service.points.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    const int editedRow = item->row();
    const int editedColumn = item->column();
    const QString pointId = service.points.at(pointIndex).pointId;
    if (editedColumn == ModelPointColumnNorthVisible) {
        service.points[pointIndex].northVisible = item->checkState() == Qt::Checked;
    } else {
        applyModelPointCellText(item->row(), item->column(), item->text());
    }

    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    refreshModelDetail(modelIndex);
    selectModelPointById(pointId);
    m_modelPointsTable->setCurrentCell(editedRow, editedColumn);
}

void MainWindow::applyModelPointCellText(int row, int column, const QString &text)
{
    QTableWidgetItem *anchorItem = m_modelPointsTable->item(row, ModelPointColumnDragHandle);
    if (!anchorItem) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    const int serviceIndex = anchorItem->data(Qt::UserRole).toInt();
    const int pointIndex = anchorItem->data(Qt::UserRole + 1).toInt();

    configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        return;
    }

    configtool::ModelTemplate &model = project.models[modelIndex];
    if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
        return;
    }

    configtool::ServiceTemplate &service = model.services[serviceIndex];
    if (pointIndex < 0 || pointIndex >= service.points.size()) {
        return;
    }

    configtool::PointTemplate &point = service.points[pointIndex];
    const QString oldDataRef = point.dataRef();
    const QString value = text.trimmed();
    switch (column) {
    case ModelPointColumnDoName:
        point.name = value;
        point.doName = point.name;
        break;
    case ModelPointColumnDescription:
        point.description = value;
        break;
    case ModelPointColumnLdName:
        point.ldName = value;
        break;
    case ModelPointColumnLnType:
        point.lnType = value;
        break;
    case ModelPointColumnLnInst:
        point.lnInst = value;
        break;
    case ModelPointColumnDataType:
        point.dataType = normalizedModelDataType(point.category, value);
        updateModelPointControlKind(point);
        break;
    case ModelPointColumnUnit:
        point.unit = value;
        break;
    default:
        break;
    }
    const QString newDataRef = point.dataRef();
    const int renamedReferenceCount = renameModelPointReferences(model.modelId, oldDataRef, newDataRef);
    if (renamedReferenceCount > 0) {
        statusBar()->showMessage(QStringLiteral("已同步更新 %1 处点位引用").arg(renamedReferenceCount), 5000);
    }
}

void MainWindow::applyDeviceBindingCellText(int row, int column, const QString &text)
{
    QTableWidgetItem *enabledItem = m_deviceBindingsTable->item(row, 0);
    if (!enabledItem) {
        return;
    }

    const int deviceIndex = currentConfigDeviceIndex();
    const int bindingIndex = enabledItem->data(Qt::UserRole).toInt();
    configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        return;
    }

    configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
    if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
        return;
    }

    configtool::PointBinding &binding = device.bindings[bindingIndex];
    const QString value = text.trimmed();
    if (isModbusDevice(device)) {
        switch (column) {
        case ModbusColumnKind:
        {
            const QString kind = normalizedModbusKind(value);
            binding.extensions.insert(QStringLiteral("modbusKind"), kind);
            binding.extensions.insert(QStringLiteral("modbusDataType"), defaultModbusDataTypeForKind(kind));
            break;
        }
        case ModbusColumnDescription:
            binding.descriptionOverride = value;
            break;
        case ModbusColumnFunCode:
            if (value.isEmpty()) {
                binding.extensions.remove(QStringLiteral("modbusFunctionCode"));
            } else {
                binding.extensions.insert(QStringLiteral("modbusFunctionCode"), value.toInt());
            }
            break;
        case ModbusColumnRegister:
            if (value.isEmpty()) {
                binding.extensions.remove(QStringLiteral("modbusRegisterAddress"));
            } else {
                binding.extensions.insert(QStringLiteral("modbusRegisterAddress"), value.toInt());
            }
            break;
        case ModbusColumnDataType:
            binding.extensions.insert(QStringLiteral("modbusDataType"),
                                      normalizedModbusDataTypeForKind(modbusBindingKind(binding), value));
            break;
        case ModbusColumnScale:
            binding.extensions.insert(QStringLiteral("modbusScale"), value);
            break;
        case ModbusColumnSelfSignal:
            binding.selfSignalFlag = selfSignalFlagChecked(value) ? QStringLiteral("1") : QString();
            break;
        default:
            break;
        }
        rebuildModbusDeviceConfig(device);
        return;
    }

    switch (column) {
    case 2:
        binding.descriptionOverride = value;
        break;
    case 3:
        binding.address = value;
        break;
    case 4:
        binding.initValue = value;
        break;
    case 5:
        binding.selfSignalFlag = selfSignalFlagChecked(value) ? QStringLiteral("1") : QString();
        break;
    default:
        break;
    }
}

void MainWindow::rebuildModbusDeviceConfig(configtool::ProtocolDeviceInstance &device)
{
    if (!isModbusDevice(device)) {
        return;
    }

    device.modbus.pollGroups.clear();
    device.modbus.setPoints.clear();
    if (device.modbus.yxType.trimmed().isEmpty()) {
        device.modbus.yxType = QStringLiteral("BIT");
    }
    if (device.modbus.ycType.trimmed().isEmpty()) {
        device.modbus.ycType = QStringLiteral("WORD");
    }
    if (device.modbus.ytType.trimmed().isEmpty()) {
        device.modbus.ytType = QStringLiteral("WORD");
    }
    if (device.modbus.ycScale.trimmed().isEmpty()) {
        device.modbus.ycScale = QStringLiteral("1.0");
    }
    if (device.modbus.ytScale.trimmed().isEmpty()) {
        device.modbus.ytScale = QStringLiteral("1.0");
    }

    QMap<QString, QList<int>> pollGroupIndexes;
    QMap<QString, QList<int>> setGroupIndexes;

    for (int index = 0; index < device.bindings.size(); ++index) {
        configtool::PointBinding &binding = device.bindings[index];
        if (!binding.enabled || !binding.extensions.contains(QStringLiteral("modbusRegisterAddress"))) {
            continue;
        }

        const QString kind = modbusBindingKind(binding);
        QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"));
        if (dataType.trimmed().isEmpty()) {
            dataType = kind == QStringLiteral("yx")
                ? QStringLiteral("BIT")
                : QStringLiteral("WORD");
            binding.extensions.insert(QStringLiteral("modbusDataType"), dataType);
        }

        const int defaultFunCode = kind == QStringLiteral("yx")
            ? 2
            : (kind == QStringLiteral("yc") ? 3 : 6);
        const int funCode = modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), defaultFunCode);
        binding.extensions.insert(QStringLiteral("modbusFunctionCode"), funCode);

        QString scale = modbusBindingString(binding, QStringLiteral("modbusScale"));
        if (scale.trimmed().isEmpty()) {
            scale = QStringLiteral("1.0");
            binding.extensions.insert(QStringLiteral("modbusScale"), scale);
        }

        const QString groupKey = QStringLiteral("%1|%2|%3|%4")
            .arg(kind)
            .arg(funCode)
            .arg(dataType.trimmed().toUpper())
            .arg(scale.trimmed());
        if (isModbusSetKind(kind)) {
            setGroupIndexes[groupKey].append(index);
        } else {
            pollGroupIndexes[groupKey].append(index);
        }
    }

    int nextGroupNo = 1;
    int yxOrder = 1;
    int ycOrder = 1;
    int ykOrder = 1;
    int ytOrder = 1;

    for (const QString &groupKey : pollGroupIndexes.keys()) {
        QList<int> indexes = pollGroupIndexes.value(groupKey);
        std::sort(indexes.begin(), indexes.end(), [&](int left, int right) {
            return modbusBindingInt(device.bindings.at(left), QStringLiteral("modbusRegisterAddress"))
                < modbusBindingInt(device.bindings.at(right), QStringLiteral("modbusRegisterAddress"));
        });

        if (indexes.isEmpty()) {
            continue;
        }

        QList<int> runIndexes;
        int runStartAddr = 0;
        int runRegisterEnd = 0;

        auto flushRun = [&]() {
            if (runIndexes.isEmpty()) {
                return;
            }

            configtool::PointBinding &firstBinding = device.bindings[runIndexes.first()];
            const QString kind = modbusBindingKind(firstBinding);
            const QString dataType = modbusBindingString(firstBinding, QStringLiteral("modbusDataType"), QStringLiteral("WORD"));
            const QString scale = modbusBindingString(firstBinding, QStringLiteral("modbusScale"), QStringLiteral("1.0"));
            const int funCode = modbusBindingInt(firstBinding, QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : 3);
            const int groupNo = nextGroupNo++;

            int entryNo = 1;
            for (int bindingIndex : runIndexes) {
                configtool::PointBinding &binding = device.bindings[bindingIndex];
                binding.extensions.insert(QStringLiteral("modbusGroupNo"), groupNo);
                binding.extensions.insert(QStringLiteral("modbusEntryNo"), entryNo);
                binding.address = QString::number(buildModbusDataIndex(groupNo, entryNo));
                ++entryNo;
            }

            configtool::ModbusPollGroup group;
            group.kind = modbusKindFromString(kind);
            group.order = kind == QStringLiteral("yx") ? yxOrder++ : ycOrder++;
            group.groupNo = groupNo;
            group.funCode = funCode;
            group.startAddr = runStartAddr;
            group.regNum = qMax(modbusTypeRegisterCount(dataType), runRegisterEnd - runStartAddr);
            group.dataType = dataType;
            group.scale = kind == QStringLiteral("yc") ? scale : QStringLiteral("1.0");
            device.modbus.pollGroups.append(group);

            runIndexes.clear();
            runStartAddr = 0;
            runRegisterEnd = 0;
        };

        for (int bindingIndex : indexes) {
            const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
            const QString kind = modbusBindingKind(binding);
            const QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"), QStringLiteral("WORD"));
            const int funCode = modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : 3);
            const int regStep = qMax(1, modbusTypeRegisterCount(dataType));
            const int regAddr = modbusBindingInt(binding, QStringLiteral("modbusRegisterAddress"));
            const int nextRegisterEnd = regAddr + regStep;
            const bool continuous = runIndexes.isEmpty() || regAddr == runRegisterEnd;
            const bool withinFrameLimit = runIndexes.isEmpty()
                || (nextRegisterEnd - runStartAddr) <= modbusMaxReadQuantity(funCode);

            if (!runIndexes.isEmpty() && (!continuous || !withinFrameLimit)) {
                flushRun();
            }

            if (runIndexes.isEmpty()) {
                runStartAddr = regAddr;
                runRegisterEnd = nextRegisterEnd;
            } else {
                runRegisterEnd = qMax(runRegisterEnd, nextRegisterEnd);
            }
            runIndexes.append(bindingIndex);
        }
        flushRun();
    }

    for (const QString &groupKey : setGroupIndexes.keys()) {
        QList<int> indexes = setGroupIndexes.value(groupKey);
        std::sort(indexes.begin(), indexes.end(), [&](int left, int right) {
            return modbusBindingInt(device.bindings.at(left), QStringLiteral("modbusRegisterAddress"))
                < modbusBindingInt(device.bindings.at(right), QStringLiteral("modbusRegisterAddress"));
        });

        if (indexes.isEmpty()) {
            continue;
        }

        const int groupNo = nextGroupNo++;
        int entryNo = 1;
        for (int bindingIndex : indexes) {
            configtool::PointBinding &binding = device.bindings[bindingIndex];
            const QString kind = modbusBindingKind(binding);
            const QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"), QStringLiteral("WORD"));
            const QString scale = modbusBindingString(binding, QStringLiteral("modbusScale"), QStringLiteral("1.0"));

            configtool::ModbusSetPoint setPoint;
            setPoint.kind = modbusKindFromString(kind);
            setPoint.order = kind == QStringLiteral("yk") ? ykOrder++ : ytOrder++;
            setPoint.groupNo = groupNo;
            setPoint.entryNo = entryNo;
            setPoint.funCode = modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), 6);
            setPoint.regAddr = modbusBindingInt(binding, QStringLiteral("modbusRegisterAddress"));
            setPoint.dataType = kind == QStringLiteral("yt") ? dataType : QStringLiteral("WORD");
            setPoint.scale = kind == QStringLiteral("yt") ? scale : QStringLiteral("1.0");
            device.modbus.setPoints.append(setPoint);

            binding.extensions.insert(QStringLiteral("modbusGroupNo"), groupNo);
            binding.extensions.insert(QStringLiteral("modbusEntryNo"), entryNo);
            binding.address = QString::number(buildModbusDataIndex(groupNo, entryNo));
            ++entryNo;
        }
    }
}

void MainWindow::pasteClipboardIntoModelPointsTable()
{
    const QList<QStringList> clipboardRows = parseClipboardTable(QApplication::clipboard()->text());
    if (clipboardRows.isEmpty()) {
        return;
    }

    QModelIndexList targets = sortedEditableTargetIndexes(m_modelPointsTable);
    const bool useSelectedCells = targets.size() > 1;
    const int startRow = useSelectedCells ? targets.first().row() : m_modelPointsTable->currentRow();
    const int startColumn = useSelectedCells ? targets.first().column() : m_modelPointsTable->currentColumn();
    if (startRow < 0 || startColumn < 0) {
        return;
    }

    pushConfigUndoSnapshot();
    m_updatingModelPointsTable = true;
    if (useSelectedCells && clipboardRows.size() == 1 && clipboardRows.first().size() == 1) {
        const QString value = clipboardRows.first().first();
        for (const QModelIndex &target : targets) {
            if (QTableWidgetItem *item = m_modelPointsTable->item(target.row(), target.column());
                item && (item->flags() & Qt::ItemIsEditable)) {
                item->setText(value);
                applyModelPointCellText(target.row(), target.column(), value);
            }
        }
    } else if (useSelectedCells && clipboardRows.size() * clipboardRows.first().size() == targets.size()) {
        int valueIndex = 0;
        for (const QStringList &clipboardRow : clipboardRows) {
            for (const QString &value : clipboardRow) {
                const QModelIndex target = targets.at(valueIndex++);
                if (QTableWidgetItem *item = m_modelPointsTable->item(target.row(), target.column());
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(value);
                    applyModelPointCellText(target.row(), target.column(), value);
                }
            }
        }
    } else {
        for (int rowOffset = 0; rowOffset < clipboardRows.size(); ++rowOffset) {
            const int row = startRow + rowOffset;
            if (row >= m_modelPointsTable->rowCount()) {
                break;
            }
            for (int columnOffset = 0; columnOffset < clipboardRows.at(rowOffset).size(); ++columnOffset) {
                const int column = startColumn + columnOffset;
                if (column >= m_modelPointsTable->columnCount()) {
                    break;
                }
                if (QTableWidgetItem *item = m_modelPointsTable->item(row, column);
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    const QString value = clipboardRows.at(rowOffset).at(columnOffset);
                    item->setText(value);
                    applyModelPointCellText(row, column, value);
                }
            }
        }
    }
    m_updatingModelPointsTable = false;

    const int modelIndex = currentConfigModelIndex();
    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    refreshModelDetail(modelIndex);
    m_modelPointsTable->setCurrentCell(startRow, startColumn);
}

void MainWindow::pasteClipboardIntoDeviceBindingsTable()
{
    const QList<QStringList> clipboardRows = parseClipboardTable(QApplication::clipboard()->text());
    if (clipboardRows.isEmpty()) {
        return;
    }

    QModelIndexList targets = sortedEditableTargetIndexes(m_deviceBindingsTable);
    const bool useSelectedCells = targets.size() > 1;
    const int startRow = useSelectedCells ? targets.first().row() : m_deviceBindingsTable->currentRow();
    const int startColumn = useSelectedCells ? targets.first().column() : m_deviceBindingsTable->currentColumn();
    if (startRow < 0 || startColumn < 0) {
        return;
    }

    pushConfigUndoSnapshot();
    m_updatingDeviceBindingsTable = true;
    if (useSelectedCells && clipboardRows.size() == 1 && clipboardRows.first().size() == 1) {
        const QString value = clipboardRows.first().first();
        for (const QModelIndex &target : targets) {
            if (QTableWidgetItem *item = m_deviceBindingsTable->item(target.row(), target.column());
                item && (item->flags() & Qt::ItemIsEditable)) {
                item->setText(value);
                applyDeviceBindingCellText(target.row(), target.column(), value);
            }
        }
    } else if (useSelectedCells && clipboardRows.size() * clipboardRows.first().size() == targets.size()) {
        int valueIndex = 0;
        for (const QStringList &clipboardRow : clipboardRows) {
            for (const QString &value : clipboardRow) {
                const QModelIndex target = targets.at(valueIndex++);
                if (QTableWidgetItem *item = m_deviceBindingsTable->item(target.row(), target.column());
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    item->setText(value);
                    applyDeviceBindingCellText(target.row(), target.column(), value);
                }
            }
        }
    } else {
        for (int rowOffset = 0; rowOffset < clipboardRows.size(); ++rowOffset) {
            const int row = startRow + rowOffset;
            if (row >= m_deviceBindingsTable->rowCount()) {
                break;
            }
            for (int columnOffset = 0; columnOffset < clipboardRows.at(rowOffset).size(); ++columnOffset) {
                const int column = startColumn + columnOffset;
                if (column >= m_deviceBindingsTable->columnCount()) {
                    break;
                }
                if (QTableWidgetItem *item = m_deviceBindingsTable->item(row, column);
                    item && (item->flags() & Qt::ItemIsEditable)) {
                    const QString value = clipboardRows.at(rowOffset).at(columnOffset);
                    item->setText(value);
                    applyDeviceBindingCellText(row, column, value);
                }
            }
        }
    }
    m_updatingDeviceBindingsTable = false;

    const int deviceIndex = currentConfigDeviceIndex();
    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    m_deviceBindingsTable->setCurrentCell(startRow, startColumn);
}

void MainWindow::pushConfigUndoSnapshot()
{
    if (m_restoringConfigUndo) {
        return;
    }

    m_configUndoStack.append(m_configProjectManager.project());
    constexpr int maxUndoSnapshots = 50;
    while (m_configUndoStack.size() > maxUndoSnapshots) {
        m_configUndoStack.removeFirst();
    }
}

void MainWindow::undoLastConfigEdit()
{
    if (m_configUndoStack.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有可撤回的编辑"), 2000);
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    const int deviceIndex = currentConfigDeviceIndex();
    const QWidget *currentPage = m_mainTabWidget ? m_mainTabWidget->currentWidget() : nullptr;

    m_restoringConfigUndo = true;
    m_configProjectManager.project() = m_configUndoStack.takeLast();
    m_restoringConfigUndo = false;

    refreshConfigObjectViews();
    if (modelIndex >= 0 && modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
    if (deviceIndex >= 0 && deviceIndex < m_configDeviceTable->rowCount()) {
        m_configDeviceTable->selectRow(deviceIndex);
    }

    if (currentPage == m_modelEditorPage) {
        refreshModelDetail(currentConfigModelIndex());
        m_mainTabWidget->setCurrentWidget(m_modelEditorPage);
    } else if (currentPage == m_deviceEditorPage) {
        refreshDeviceEditor(currentConfigDeviceIndex());
        m_mainTabWidget->setCurrentWidget(m_deviceEditorPage);
    }
    statusBar()->showMessage(QStringLiteral("已撤回上一步编辑"), 2000);
}

void MainWindow::refreshConfigImportSummary(const configtool::ImportReport &report)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    m_configProjectNameValueLabel->setText(project.projectName.isEmpty() ? QStringLiteral("-") : project.projectName);
    m_configSourceRootValueLabel->setText(project.sourceRoot.isEmpty() ? QStringLiteral("-") : project.sourceRoot);
    m_configModelCountValueLabel->setText(QString::number(project.models.size()));
    m_configDeviceCountValueLabel->setText(QString::number(project.devices.size()));
    m_configIssueCountValueLabel->setText(QString::number(importIssueProblemCount(report.issues)));

    QString statusMessage = QStringLiteral("导入结果: 模型 %1，设备 %2")
        .arg(report.importedModelCount)
        .arg(report.importedDeviceCount);
    if (report.issues.isEmpty()) {
        statusMessage += QStringLiteral("，未发现错误或警告。");
    } else {
        statusMessage += QStringLiteral("，问题数 %1。")
            .arg(importIssueProblemCount(report.issues));
    }
    statusBar()->showMessage(statusMessage, 8000);
    refreshConfigIssueTable(report.issues, QStringLiteral("导入"));
    refreshConfigObjectViews();
}

void MainWindow::refreshConfigObjectViews()
{
    const configtool::ConfigProject &project = m_configProjectManager.project();

    const int previousModelIndex = currentConfigModelIndex();
    const int previousDeviceIndex = currentConfigDeviceIndex();

    m_configModelTable->setRowCount(project.models.size());
    for (int row = 0; row < project.models.size(); ++row) {
        const configtool::ModelTemplate &model = project.models.at(row);
        int pointCount = 0;
        for (const configtool::ServiceTemplate &service : model.services) {
            pointCount += service.points.size();
        }

        m_configModelTable->setItem(row, 0, new QTableWidgetItem(model.modelId));
        m_configModelTable->setItem(row, 1, new QTableWidgetItem(model.displayName));
        m_configModelTable->setItem(row, 2, new QTableWidgetItem(model.deviceType));
        m_configModelTable->setItem(row, 3, new QTableWidgetItem(QString::number(pointCount)));
    }

    m_configDeviceTable->setRowCount(project.devices.size());
    for (int row = 0; row < project.devices.size(); ++row) {
        const configtool::ProtocolDeviceInstance &device = project.devices.at(row);
        m_configDeviceTable->setItem(row, 0, new QTableWidgetItem(device.deviceId));
        m_configDeviceTable->setItem(row, 1, new QTableWidgetItem(device.deviceDesc));
        m_configDeviceTable->setItem(row, 2, new QTableWidgetItem(device.modelId));
        m_configDeviceTable->setItem(row, 3, new QTableWidgetItem(device.transport.stationAddress));
        m_configDeviceTable->setItem(row, 4, new QTableWidgetItem(QString::number(device.bindings.size())));
    }

    if (previousModelIndex >= 0 && previousModelIndex < project.models.size()) {
        m_configModelTable->selectRow(previousModelIndex);
    } else if (!project.models.isEmpty()) {
        m_configModelTable->selectRow(0);
    } else {
        refreshModelOverview(-1);
        refreshModelDetail(-1);
    }

    if (previousDeviceIndex >= 0 && previousDeviceIndex < project.devices.size()) {
        m_configDeviceTable->selectRow(previousDeviceIndex);
    } else if (!project.devices.isEmpty()) {
        m_configDeviceTable->selectRow(0);
    } else {
        refreshDeviceDetail(-1);
        refreshDeviceEditor(-1);
    }

    refreshSelectionOverview();
    refreshEditorNavigationCombos();
    refreshLogicCenterOverview();
    refreshLogicAgcAvcPage();
    refreshLogicComputationPointPage();
    refreshLogicControlRulePage();
    refreshLogicOnlineLinkPage();
}


bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::BackButton
            || mouseEvent->button() == Qt::ForwardButton) {
            if (event->type() == QEvent::MouseButtonRelease) {
                if (mouseEvent->button() == Qt::BackButton) {
                    navigateBack();
                } else {
                    navigateForward();
                }
            }
            return true;
        }
    }

    auto dropPosition = [](QDropEvent *dropEvent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return dropEvent->position().toPoint();
#else
        return dropEvent->pos();
#endif
    };
    auto insertRowAt = [](QTableWidget *table, const QPoint &dropPos) {
        const int rowCount = table->rowCount();
        int insertRow = table->rowAt(dropPos.y());
        if (insertRow < 0) {
            return rowCount;
        }

        const int rowTop = table->rowViewportPosition(insertRow);
        const int rowHeight = table->rowHeight(insertRow);
        if (dropPos.y() > rowTop + rowHeight / 2) {
            ++insertRow;
        }
        return insertRow;
    };
    auto showDropLine = [](QTableWidget *table, QFrame *line, int insertRow) {
        if (!table || !line) {
            return;
        }

        const int rowCount = table->rowCount();
        int y = 0;
        if (insertRow <= 0) {
            y = 0;
        } else if (insertRow >= rowCount) {
            const int lastRow = rowCount - 1;
            y = table->rowViewportPosition(lastRow) + table->rowHeight(lastRow);
        } else {
            y = table->rowViewportPosition(insertRow);
        }

        y = qMax(0, y - line->height() / 2);
        line->setGeometry(0, y, table->viewport()->width(), line->height());
        line->raise();
        line->show();
    };
    auto hideDropLine = [](QFrame *line) {
        if (line) {
            line->hide();
        }
    };

    if (m_logicComputationPointTable
        && watched == m_logicComputationPointTable->viewport()) {

        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const int pressedColumn = m_logicComputationPointTable->columnAt(mouseEvent->pos().x());
                const bool canDragRow = pressedColumn == LogicComputationColumnDragHandle;
                m_logicComputationPointTable->setDragEnabled(canDragRow);
                m_logicComputationDragRow = canDragRow
                    ? m_logicComputationPointTable->rowAt(mouseEvent->pos().y())
                    : -1;
            }
        } else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDropEvent *>(event);
            if (m_logicComputationDragRow >= 0) {
                showDropLine(m_logicComputationPointTable,
                             m_logicComputationDropLine,
                             insertRowAt(m_logicComputationPointTable, dropPosition(dragEvent)));
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            hideDropLine(m_logicComputationDropLine);
            m_logicComputationPointTable->setDragEnabled(false);
            m_logicComputationDragRow = -1;
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
            const int rowCount = logic.computationPoints.size();
            const int sourceRow = m_logicComputationDragRow;
            m_logicComputationDragRow = -1;
            hideDropLine(m_logicComputationDropLine);
            m_logicComputationPointTable->setDragEnabled(false);

            if (sourceRow < 0 || sourceRow >= rowCount || rowCount < 2) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int insertRow = insertRowAt(m_logicComputationPointTable, dropPosition(dropEvent));
            const int destinationRow = insertRow > sourceRow ? insertRow - 1 : insertRow;
            if (destinationRow < 0 || destinationRow >= rowCount || destinationRow == sourceRow) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            pushConfigUndoSnapshot();
            logic.computationPoints.move(sourceRow, destinationRow);
            refreshLogicCenterOverview();
            refreshLogicComputationPointPage();
            m_logicComputationPointTable->selectRow(destinationRow);
            statusBar()->showMessage(QStringLiteral("已调整计算点顺序"), 5000);

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            return true;
        }
    }

    if (m_iec101PointsTable
        && watched == m_iec101PointsTable->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const int pressedColumn = m_iec101PointsTable->columnAt(mouseEvent->pos().x());
                const bool canDragRow = pressedColumn == Iec101PointColumnDragHandle;
                m_iec101PointsTable->setDragEnabled(canDragRow);
                m_iec101PointDragRow = canDragRow
                    ? m_iec101PointsTable->rowAt(mouseEvent->pos().y())
                    : -1;
            }
        } else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDropEvent *>(event);
            if (m_iec101PointDragRow >= 0) {
                showDropLine(m_iec101PointsTable,
                             m_iec101PointDropLine,
                             insertRowAt(m_iec101PointsTable, dropPosition(dragEvent)));
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            hideDropLine(m_iec101PointDropLine);
            m_iec101PointsTable->setDragEnabled(false);
            m_iec101PointDragRow = -1;
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            hideDropLine(m_iec101PointDropLine);
            m_iec101PointsTable->setDragEnabled(false);

            const int sourceRow = m_iec101PointDragRow;
            m_iec101PointDragRow = -1;
            if (sourceRow < 0 || sourceRow >= m_iec101PointsTable->rowCount()) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int insertRow = qBound(0,
                                         insertRowAt(m_iec101PointsTable, dropPosition(dropEvent)),
                                         m_iec101PointsTable->rowCount());
            const int destinationRow = insertRow > sourceRow ? insertRow - 1 : insertRow;
            if (destinationRow < 0
                || destinationRow >= m_iec101PointsTable->rowCount()
                || destinationRow == sourceRow
                || m_iec101PointsTable->rowCount() < 2) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            pushIec101PointsUndoSnapshot();

            // 收集所有行的数据（从旧表格中提取，之后清空重建）
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
            allRows.reserve(m_iec101PointsTable->rowCount());
            for (int row = 0; row < m_iec101PointsTable->rowCount(); ++row) {
                RowSnapshot rs;
                rs.deviceId = m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)
                    ? m_iec101PointsTable->item(row, Iec101PointColumnDeviceId)->text().trimmed() : QString();
                rs.dataRef = m_iec101PointsTable->item(row, Iec101PointColumnDataRef)
                    ? m_iec101PointsTable->item(row, Iec101PointColumnDataRef)->text().trimmed() : QString();
                rs.description = m_iec101PointsTable->item(row, Iec101PointColumnDescription)
                    ? m_iec101PointsTable->item(row, Iec101PointColumnDescription)->text().trimmed() : QString();
                QTableWidgetItem *checkItem = m_iec101PointsTable->item(row, Iec101PointColumnEnabled);
                rs.category = checkItem ? checkItem->data(Qt::UserRole).toInt() : 0;
                rs.enabled = checkItem ? (checkItem->checkState() == Qt::Checked) : true;
                rs.deviceaddr = m_iec101PointsTable->item(row, Iec101PointColumnAddress)
                    ? m_iec101PointsTable->item(row, Iec101PointColumnAddress)->text().trimmed() : QString();
                QWidget *w = m_iec101PointsTable->cellWidget(row, Iec101PointColumnDeadzoneType);
                if (auto *combo = qobject_cast<QComboBox *>(w)) {
                    rs.deathzoneType = combo->currentData().toString();
                }
                rs.deathzone = m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)
                    ? m_iec101PointsTable->item(row, Iec101PointColumnDeadzone)->text().trimmed() : QStringLiteral("0.2");
                allRows.append(rs);
            }

            // 移动数据
            allRows.move(sourceRow, destinationRow);

            // 完全重建表格（安全：旧 item/widget 被 Qt 正确删除，无残留状态）
            m_iec101PointsTable->blockSignals(true);
            m_iec101PointsTable->setRowCount(0);

            for (const RowSnapshot &rs : allRows) {
                const int row = m_iec101PointsTable->rowCount();
                m_iec101PointsTable->insertRow(row);

                // Col 0: 拖动手柄
                auto *handleItem = new QTableWidgetItem(QStringLiteral("⋮"));
                handleItem->setTextAlignment(Qt::AlignCenter);
                handleItem->setToolTip(QStringLiteral("拖动调整顺序"));
                handleItem->setForeground(QColor(QStringLiteral("#9a9a9a")));
                handleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
                m_iec101PointsTable->setItem(row, Iec101PointColumnDragHandle, handleItem);

                // Col 1: 启用
                auto *checkItem = new QTableWidgetItem();
                checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                checkItem->setCheckState(rs.enabled ? Qt::Checked : Qt::Unchecked);
                checkItem->setData(Qt::UserRole, rs.category);
                m_iec101PointsTable->setItem(row, Iec101PointColumnEnabled, checkItem);

                // Col 2: DeviceId
                auto *devIdItem = new QTableWidgetItem(rs.deviceId);
                devIdItem->setFlags(devIdItem->flags() & ~Qt::ItemIsEditable);
                m_iec101PointsTable->setItem(row, Iec101PointColumnDeviceId, devIdItem);

                // Col 3: DataRef
                auto *dataRefItem = new QTableWidgetItem(rs.dataRef);
                dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
                m_iec101PointsTable->setItem(row, Iec101PointColumnDataRef, dataRefItem);

                // Col 4: Description
                auto *descItem = new QTableWidgetItem(rs.description);
                descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
                m_iec101PointsTable->setItem(row, Iec101PointColumnDescription, descItem);

                // Col 5: 北向101地址
                m_iec101PointsTable->setItem(row, Iec101PointColumnAddress, new QTableWidgetItem(rs.deviceaddr));

                // Col 6: 死区类型
                auto *dzTypeCombo = new QComboBox();
                dzTypeCombo->addItem(QStringLiteral("0 — 百分比"), QStringLiteral("0"));
                dzTypeCombo->addItem(QStringLiteral("1 — 固定值"), QStringLiteral("1"));
                const int dzTypeIdx = dzTypeCombo->findData(rs.deathzoneType);
                dzTypeCombo->setCurrentIndex(dzTypeIdx >= 0 ? dzTypeIdx : 0);
                m_iec101PointsTable->setCellWidget(row, Iec101PointColumnDeadzoneType, dzTypeCombo);

                // Col 7: 死区值
                m_iec101PointsTable->setItem(row, Iec101PointColumnDeadzone, new QTableWidgetItem(rs.deathzone));
            }

            m_iec101PointsTable->blockSignals(false);
            applyIec101PointsFilter();
            m_iec101PointsTable->selectRow(destinationRow);
            highlightIec101DuplicateAddresses();
            statusBar()->showMessage(QStringLiteral("已调整 IEC101 点表顺序"), 5000);

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            return true;
        }
    }

    if (m_modelPointsTable
        && watched == m_modelPointsTable->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const int pressedColumn = m_modelPointsTable->columnAt(mouseEvent->pos().x());
                const bool singleCategoryView = m_modelPointFilterTabBar
                    && m_modelPointFilterTabBar->currentIndex() > 0;
                const bool canDragRow = singleCategoryView
                    && pressedColumn == ModelPointColumnDragHandle;
                m_modelPointsTable->setDragEnabled(canDragRow);
                m_modelPointDragRow = canDragRow
                    ? m_modelPointsTable->rowAt(mouseEvent->pos().y())
                    : -1;
            }
        } else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDropEvent *>(event);
            if (m_modelPointDragRow >= 0) {
                if (m_modelPointFilterTabBar && m_modelPointFilterTabBar->currentIndex() > 0) {
                    showDropLine(m_modelPointsTable,
                                 m_modelPointDropLine,
                                 insertRowAt(m_modelPointsTable, dropPosition(dragEvent)));
                } else {
                    hideDropLine(m_modelPointDropLine);
                }
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            hideDropLine(m_modelPointDropLine);
            m_modelPointsTable->setDragEnabled(false);
            m_modelPointDragRow = -1;
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            hideDropLine(m_modelPointDropLine);
            m_modelPointsTable->setDragEnabled(false);

            const int sourceRow = m_modelPointDragRow;
            m_modelPointDragRow = -1;
            const int modelIndex = currentConfigModelIndex();
            configtool::ConfigProject &project = m_configProjectManager.project();
            const bool singleCategoryView = m_modelPointFilterTabBar
                && m_modelPointFilterTabBar->currentIndex() > 0;
            if (!singleCategoryView
                || modelIndex < 0
                || modelIndex >= project.models.size()
                || sourceRow < 0
                || sourceRow >= m_modelPointsTable->rowCount()) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            QTableWidgetItem *sourceItem = m_modelPointsTable->item(sourceRow, ModelPointColumnDragHandle);
            if (!sourceItem) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int serviceIndex = sourceItem->data(Qt::UserRole).toInt();
            const int sourcePointIndex = sourceItem->data(Qt::UserRole + 1).toInt();
            configtool::ModelTemplate &model = project.models[modelIndex];
            if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            configtool::ServiceTemplate &service = model.services[serviceIndex];
            if (sourcePointIndex < 0 || sourcePointIndex >= service.points.size() || service.points.size() < 2) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            const int insertRow = qBound(0,
                                         insertRowAt(m_modelPointsTable, dropPosition(dropEvent)),
                                         service.points.size());
            const int destinationPointIndex = insertRow > sourcePointIndex ? insertRow - 1 : insertRow;
            if (destinationPointIndex < 0
                || destinationPointIndex >= service.points.size()
                || destinationPointIndex == sourcePointIndex) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                return true;
            }

            pushConfigUndoSnapshot();
            const QString movedPointId = service.points.at(sourcePointIndex).pointId;
            service.points.move(sourcePointIndex, destinationPointIndex);
            refreshConfigObjectViews();
            refreshModelDetail(modelIndex);
            selectModelPointById(movedPointId);
            statusBar()->showMessage(QStringLiteral("已调整模型点位顺序"), 5000);

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::refreshSelectionOverview()
{
    const int modelIndex = currentConfigModelIndex();
    const int deviceIndex = currentConfigDeviceIndex();
    const configtool::ConfigProject &project = m_configProjectManager.project();

    QString modelGroupTitle = QStringLiteral("模型列表");
    QString deviceGroupTitle = QStringLiteral("设备列表");

    if (modelIndex >= 0 && modelIndex < project.models.size()) {
        const configtool::ModelTemplate &model = project.models.at(modelIndex);
        const QString displayName = model.displayName.isEmpty() ? model.modelId : model.displayName;
        modelGroupTitle = QStringLiteral("模型列表  [当前: %1]").arg(displayName);
    }

    if (deviceIndex >= 0 && deviceIndex < project.devices.size()) {
        const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
        const QString deviceName = device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc;
        deviceGroupTitle = QStringLiteral("设备列表  [当前: %1]").arg(deviceName);
    }

    m_modelGroupBox->setTitle(modelGroupTitle);
    m_deviceGroupBox->setTitle(deviceGroupTitle);
}

void MainWindow::refreshEditorNavigationCombos()
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    const int modelIndex = currentConfigModelIndex();
    const int deviceIndex = currentConfigDeviceIndex();

    m_updatingEditorNavigationCombos = true;
    if (m_modelEditorCombo) {
        QSignalBlocker blocker(m_modelEditorCombo);
        m_modelEditorCombo->clear();
        for (int row = 0; row < project.models.size(); ++row) {
            m_modelEditorCombo->addItem(modelChoiceText(project.models.at(row)), row);
        }
        m_modelEditorCombo->setEnabled(!project.models.isEmpty());
        const int comboIndex = m_modelEditorCombo->findData(modelIndex);
        m_modelEditorCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : -1);
    }

    if (m_deviceEditorCombo) {
        QSignalBlocker blocker(m_deviceEditorCombo);
        m_deviceEditorCombo->clear();
        for (int row = 0; row < project.devices.size(); ++row) {
            m_deviceEditorCombo->addItem(deviceChoiceText(project.devices.at(row)), row);
        }
        m_deviceEditorCombo->setEnabled(!project.devices.isEmpty());
        const int comboIndex = m_deviceEditorCombo->findData(deviceIndex);
        m_deviceEditorCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : -1);
    }
    m_updatingEditorNavigationCombos = false;
}

void MainWindow::refreshModelDetail(int modelIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (m_modelEditorCombo && !m_updatingEditorNavigationCombos) {
        QSignalBlocker blocker(m_modelEditorCombo);
        const int comboIndex = m_modelEditorCombo->findData(modelIndex);
        m_modelEditorCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : -1);
    }

    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        if (m_modelIdEdit && !m_modelIdEdit->hasFocus()) {
            m_modelIdEdit->setProperty(ModelEditorOriginalModelIdProperty, QString());
        }
        for (QLineEdit *edit : {m_modelIdEdit, m_modelDisplayNameEdit, m_modelDeviceTypeEdit,
                                m_modelVersionEdit, m_modelManufacturerIdEdit,
                                m_modelManufacturerDescEdit, m_modelSchemaEdit}) {
            edit->clear();
        }
        if (m_modelNorthVisibleCheck) {
            QSignalBlocker blocker(m_modelNorthVisibleCheck);
            m_modelNorthVisibleCheck->setChecked(true);
        }
        m_modelValidationLabel->setText(QStringLiteral("请选择一个模型。"));
        m_modelPointsTable->setRowCount(0);
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
    if (m_modelIdEdit && !m_modelIdEdit->hasFocus()) {
        m_modelIdEdit->setProperty(ModelEditorOriginalModelIdProperty, model.modelId);
    }
    for (auto pair : {qMakePair(m_modelIdEdit, model.modelId),
                      qMakePair(m_modelDisplayNameEdit, model.displayName),
                      qMakePair(m_modelDeviceTypeEdit, model.deviceType),
                      qMakePair(m_modelVersionEdit, model.version),
                      qMakePair(m_modelManufacturerIdEdit, model.manufacturerId),
                      qMakePair(m_modelManufacturerDescEdit, model.manufacturerDesc),
                      qMakePair(m_modelSchemaEdit, model.schema)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }
    if (m_modelNorthVisibleCheck) {
        QSignalBlocker blocker(m_modelNorthVisibleCheck);
        m_modelNorthVisibleCheck->setChecked(model.northVisible);
    }

    const int filterTabIndex = m_modelPointFilterTabBar ? m_modelPointFilterTabBar->currentIndex() : 0;
    const QString dataRefKeyword = m_modelPointDataRefFilterEdit
        ? m_modelPointDataRefFilterEdit->text().trimmed()
        : QString();
    const QString descriptionKeyword = m_modelPointDescriptionFilterEdit
        ? m_modelPointDescriptionFilterEdit->text().trimmed()
        : QString();
    int totalPointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        if (filterTabIndex != 0 && modelPointFilterTabIndex(service.type) != filterTabIndex) {
            continue;
        }

        for (const configtool::PointTemplate &point : service.points) {
            const bool matchesDataRef = dataRefKeyword.isEmpty()
                || point.dataRef().contains(dataRefKeyword, Qt::CaseInsensitive);
            const bool matchesDescription = descriptionKeyword.isEmpty()
                || point.description.contains(descriptionKeyword, Qt::CaseInsensitive);
            if (matchesDataRef && matchesDescription) {
                ++totalPointCount;
            }
        }
    }

    m_modelPointsTable->setRowCount(totalPointCount);
    m_updatingModelPointsTable = true;
    m_updatingModelPointCategory = true;
    int row = 0;
    for (int serviceIndex = 0; serviceIndex < model.services.size(); ++serviceIndex) {
        const configtool::ServiceTemplate &service = model.services.at(serviceIndex);
        if (filterTabIndex != 0 && modelPointFilterTabIndex(service.type) != filterTabIndex) {
            continue;
        }
        for (int pointIndex = 0; pointIndex < service.points.size(); ++pointIndex) {
            const configtool::PointTemplate &point = service.points.at(pointIndex);
            const bool matchesDataRef = dataRefKeyword.isEmpty()
                || point.dataRef().contains(dataRefKeyword, Qt::CaseInsensitive);
            const bool matchesDescription = descriptionKeyword.isEmpty()
                || point.description.contains(descriptionKeyword, Qt::CaseInsensitive);
            if (!matchesDataRef || !matchesDescription) {
                continue;
            }

            auto *handleItem = new QTableWidgetItem(QStringLiteral("\u22EE"));
            auto *northVisibleItem = new QTableWidgetItem();
            auto *categoryItem = new QTableWidgetItem(configtool::modelServiceTypeDisplayName(point.category));
            auto *nameItem = new QTableWidgetItem(point.doName);
            auto *descriptionItem = new QTableWidgetItem(point.description);
            auto *ldNameItem = new QTableWidgetItem(point.ldName);
            auto *lnTypeItem = new QTableWidgetItem(point.lnType);
            auto *lnInstItem = new QTableWidgetItem(point.lnInst);
            auto *dataRefItem = new QTableWidgetItem(point.dataRef());
            const QString dataType = normalizedModelDataType(point.category, point.dataType);
            auto *dataTypeItem = new QTableWidgetItem(dataType);
            auto *unitItem = new QTableWidgetItem(point.unit);

            const bool allowRowDrag = filterTabIndex > 0;
            handleItem->setTextAlignment(Qt::AlignCenter);
            handleItem->setToolTip(QStringLiteral("拖动调整顺序"));
            handleItem->setForeground(QColor(QStringLiteral("#9a9a9a")));
            Qt::ItemFlags handleFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            if (allowRowDrag) {
                handleFlags |= Qt::ItemIsDragEnabled;
            }
            handleItem->setFlags(handleFlags);
            handleItem->setData(Qt::UserRole, serviceIndex);
            handleItem->setData(Qt::UserRole + 1, pointIndex);
            handleItem->setData(Qt::UserRole + 2, point.pointId);
            northVisibleItem->setFlags((northVisibleItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            northVisibleItem->setFlags(northVisibleItem->flags() & ~Qt::ItemIsDragEnabled);
            northVisibleItem->setCheckState(point.northVisible ? Qt::Checked : Qt::Unchecked);
            northVisibleItem->setData(Qt::UserRole, serviceIndex);
            northVisibleItem->setData(Qt::UserRole + 1, pointIndex);
            northVisibleItem->setData(Qt::UserRole + 2, point.pointId);
            categoryItem->setData(Qt::UserRole, serviceIndex);
            categoryItem->setData(Qt::UserRole + 1, pointIndex);
            categoryItem->setData(Qt::UserRole + 2, point.pointId);
            categoryItem->setFlags((categoryItem->flags() & ~Qt::ItemIsEditable) & ~Qt::ItemIsDragEnabled);
            dataRefItem->setFlags((dataRefItem->flags() & ~Qt::ItemIsEditable) & ~Qt::ItemIsDragEnabled);
            for (QTableWidgetItem *editableItem : {nameItem,
                                                   descriptionItem,
                                                   ldNameItem,
                                                   lnTypeItem,
                                                   lnInstItem,
                                                   dataTypeItem,
                                                   unitItem}) {
                editableItem->setFlags(editableItem->flags() & ~Qt::ItemIsDragEnabled);
            }

            if (duplicateRefs.contains(point.dataRef())) {
                const QColor duplicateColor(QStringLiteral("#c0392b"));
                handleItem->setForeground(duplicateColor);
                northVisibleItem->setForeground(duplicateColor);
                categoryItem->setForeground(duplicateColor);
                nameItem->setForeground(duplicateColor);
                descriptionItem->setForeground(duplicateColor);
                ldNameItem->setForeground(duplicateColor);
                lnTypeItem->setForeground(duplicateColor);
                lnInstItem->setForeground(duplicateColor);
                dataRefItem->setForeground(duplicateColor);
                dataTypeItem->setForeground(duplicateColor);
                unitItem->setForeground(duplicateColor);
            }

            m_modelPointsTable->setItem(row, ModelPointColumnDragHandle, handleItem);
            m_modelPointsTable->setItem(row, ModelPointColumnNorthVisible, northVisibleItem);
            m_modelPointsTable->setItem(row, ModelPointColumnCategory, categoryItem);
            m_modelPointsTable->setItem(row, ModelPointColumnDoName, nameItem);
            m_modelPointsTable->setItem(row, ModelPointColumnDescription, descriptionItem);
            m_modelPointsTable->setItem(row, ModelPointColumnLdName, ldNameItem);
            m_modelPointsTable->setItem(row, ModelPointColumnLnType, lnTypeItem);
            m_modelPointsTable->setItem(row, ModelPointColumnLnInst, lnInstItem);
            m_modelPointsTable->setItem(row, ModelPointColumnDataRef, dataRefItem);
            m_modelPointsTable->setItem(row, ModelPointColumnDataType, dataTypeItem);
            m_modelPointsTable->setItem(row, ModelPointColumnUnit, unitItem);

            auto *dataTypeCombo = new QComboBox(m_modelPointsTable);
            dataTypeCombo->setObjectName(QStringLiteral("modelPointDataTypeCombo"));
            dataTypeCombo->addItems(modelDataTypeOptions());
            dataTypeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            dataTypeCombo->setMinimumContentsLength(8);
            dataTypeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            dataTypeCombo->setProperty("serviceIndex", serviceIndex);
            dataTypeCombo->setProperty("pointIndex", pointIndex);
            const int dataTypeIndex = dataTypeCombo->findText(dataType);
            dataTypeCombo->setCurrentIndex(dataTypeIndex >= 0 ? dataTypeIndex : dataTypeCombo->findText(defaultModelDataTypeForService(point.category)));
            connect(dataTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, dataTypeCombo](int index) {
                if (m_updatingModelPointsTable || m_restoringConfigUndo || index < 0) {
                    return;
                }

                const int modelIndex = currentConfigModelIndex();
                if (modelIndex < 0) {
                    return;
                }

                const int serviceIndex = dataTypeCombo->property("serviceIndex").toInt();
                const int pointIndex = dataTypeCombo->property("pointIndex").toInt();

                configtool::ConfigProject &project = m_configProjectManager.project();
                if (modelIndex >= project.models.size()) {
                    return;
                }

                configtool::ModelTemplate &model = project.models[modelIndex];
                if (serviceIndex < 0 || serviceIndex >= model.services.size()) {
                    return;
                }

                configtool::ServiceTemplate &service = model.services[serviceIndex];
                if (pointIndex < 0 || pointIndex >= service.points.size()) {
                    return;
                }

                configtool::PointTemplate &point = service.points[pointIndex];
                const QString dataType = normalizedModelDataType(point.category, dataTypeCombo->currentText());
                if (point.dataType == dataType) {
                    return;
                }

                pushConfigUndoSnapshot();
                point.dataType = dataType;
                updateModelPointControlKind(point);
                const QString pointId = point.pointId;
                refreshConfigObjectViews();
                refreshModelDetail(modelIndex);
                selectModelPointById(pointId);
                m_modelPointsTable->setCurrentCell(m_modelPointsTable->currentRow(), ModelPointColumnDataType);
            });
            m_modelPointsTable->setCellWidget(row, ModelPointColumnDataType, dataTypeCombo);

            auto *categoryCombo = new QComboBox(m_modelPointsTable);
            categoryCombo->setObjectName(QStringLiteral("modelPointCategoryCombo"));
            categoryCombo->addItem(QStringLiteral("遥测"), static_cast<int>(configtool::ModelServiceType::Measurement));
            categoryCombo->addItem(QStringLiteral("遥信"), static_cast<int>(configtool::ModelServiceType::Status));
            categoryCombo->addItem(QStringLiteral("控制"), static_cast<int>(configtool::ModelServiceType::Control));
            categoryCombo->setProperty("serviceIndex", serviceIndex);
            categoryCombo->setProperty("pointIndex", pointIndex);
            const int categoryComboIndex = categoryCombo->findData(static_cast<int>(point.category));
            categoryCombo->setCurrentIndex(categoryComboIndex >= 0 ? categoryComboIndex : 0);
            connect(categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                    this, &MainWindow::onModelPointCategoryChanged);
            m_modelPointsTable->setCellWidget(row, ModelPointColumnCategory, categoryCombo);
            ++row;
        }
    }
    m_updatingModelPointsTable = false;
    m_updatingModelPointCategory = false;

    int hiddenNorthPointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            if (!point.northVisible) {
                ++hiddenNorthPointCount;
            }
        }
    }

    if (duplicateRefs.isEmpty()) {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        QString message = QStringLiteral("当前模型点位的 DataRef 唯一。");
        if (!model.northVisible) {
            message += QStringLiteral(" 当前模型北向不可见，导出时不会生成 northmodel。");
        } else if (hiddenNorthPointCount > 0) {
            message += QStringLiteral(" 已隐藏 %1 个北向点位。").arg(hiddenNorthPointCount);
        }
        m_modelValidationLabel->setText(message);
    } else {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
        m_modelValidationLabel->setText(
            QStringLiteral("检测到重复 DataRef：%1。请调整 LDname/LNtype/LNinst/DOname 组合。")
                .arg(QStringList(duplicateRefs.begin(), duplicateRefs.end()).join(QStringLiteral("，"))));
    }
}

void MainWindow::selectModelPointById(const QString &pointId)
{
    if (pointId.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_modelPointsTable->rowCount(); ++row) {
        QTableWidgetItem *handleItem = m_modelPointsTable->item(row, ModelPointColumnDragHandle);
        if (handleItem && handleItem->data(Qt::UserRole + 2).toString() == pointId) {
            m_modelPointsTable->selectRow(row);
            return;
        }
    }
}

void MainWindow::refreshModelOverview(int modelIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        m_modelOverviewIdLabel->setText(QStringLiteral("-"));
        m_modelOverviewDisplayNameLabel->setText(QStringLiteral("-"));
        m_modelOverviewDeviceTypeLabel->setText(QStringLiteral("-"));
        m_modelOverviewVersionLabel->setText(QStringLiteral("-"));
        m_modelOverviewPointCountLabel->setText(QStringLiteral("0"));
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    int pointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        pointCount += service.points.size();
    }

    m_modelOverviewIdLabel->setText(model.modelId.isEmpty() ? QStringLiteral("-") : model.modelId);
    m_modelOverviewDisplayNameLabel->setText(model.displayName.isEmpty() ? QStringLiteral("-") : model.displayName);
    m_modelOverviewDeviceTypeLabel->setText(model.deviceType.isEmpty() ? QStringLiteral("-") : model.deviceType);
    m_modelOverviewVersionLabel->setText(model.version.isEmpty() ? QStringLiteral("-") : model.version);
    m_modelOverviewPointCountLabel->setText(QString::number(pointCount));
}

void MainWindow::refreshDeviceDetail(int deviceIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        m_deviceDetailTitleLabel->setText(QStringLiteral("-"));
        m_deviceDetailModelLabel->setText(QStringLiteral("-"));
        m_deviceDetailAddressLabel->setText(QStringLiteral("-"));
        m_deviceDetailIpLabel->setText(QStringLiteral("-"));
        m_deviceDetailPortLabel->setText(QStringLiteral("-"));
        m_deviceDetailBindingCountLabel->setText(QStringLiteral("0"));
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    m_deviceDetailTitleLabel->setText(device.deviceDesc.isEmpty() ? device.deviceId : device.deviceDesc);
    m_deviceDetailModelLabel->setText(device.modelId);
    m_deviceDetailAddressLabel->setText(device.transport.stationAddress);
    m_deviceDetailIpLabel->setText(device.transport.ip);
    m_deviceDetailPortLabel->setText(device.transport.port);
    m_deviceDetailBindingCountLabel->setText(QString::number(device.bindings.size()));
}

void MainWindow::refreshDeviceEditor(int deviceIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (m_deviceEditorCombo && !m_updatingEditorNavigationCombos) {
        QSignalBlocker blocker(m_deviceEditorCombo);
        const int comboIndex = m_deviceEditorCombo->findData(deviceIndex);
        m_deviceEditorCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : -1);
    }

    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        if (m_deviceIdEdit && !m_deviceIdEdit->hasFocus()) {
            m_deviceIdEdit->setProperty(DeviceEditorOriginalDeviceIdProperty, QString());
        }
        for (QLineEdit *edit : {m_deviceIdEdit, m_deviceDescEdit, m_deviceModelEdit,
                                m_deviceStationAddressEdit, m_deviceIpEdit,
                                m_devicePortEdit}) {
            if (edit) {
                edit->clear();
            }
        }
        if (m_modbusParamsGroupBox) {
            m_modbusParamsGroupBox->setVisible(false);
        }
        if (m_modbusSerialPortCombo) {
            QSignalBlocker blocker(m_modbusSerialPortCombo);
            m_modbusSerialPortCombo->setCurrentIndex(0);
        }
        if (m_modbusHwVariantCombo) {
            QSignalBlocker blocker(m_modbusHwVariantCombo);
            m_modbusHwVariantCombo->setCurrentIndex(0);
        }
        for (auto pair : {qMakePair(m_modbusBaudCombo, QStringLiteral("9600")),
                          qMakePair(m_modbusDataBitsCombo, QStringLiteral("8")),
                          qMakePair(m_modbusStopBitsCombo, QStringLiteral("1")),
                          qMakePair(m_modbusParityCombo, QStringLiteral("N"))}) {
            QSignalBlocker blocker(pair.first);
            const int index = pair.first->findText(pair.second);
            pair.first->setCurrentIndex(index >= 0 ? index : 0);
        }
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #666666; }");
        m_deviceValidationLabel->setText(QStringLiteral("请选择一个 104 设备。"));
        m_deviceBindingsTable->setRowCount(0);
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    if (m_deviceIdEdit && !m_deviceIdEdit->hasFocus()) {
        m_deviceIdEdit->setProperty(DeviceEditorOriginalDeviceIdProperty, device.deviceId);
    }
    for (auto pair : {qMakePair(m_deviceIdEdit, device.deviceId),
                      qMakePair(m_deviceDescEdit, device.deviceDesc),
                      qMakePair(m_deviceModelEdit, device.modelId),
                      qMakePair(m_deviceStationAddressEdit, device.transport.stationAddress),
                      qMakePair(m_deviceIpEdit, device.transport.ip),
                      qMakePair(m_devicePortEdit, device.transport.port)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }

    const bool modbusDevice = isModbusDevice(device);
    if (m_modbusParamsGroupBox) {
        m_modbusParamsGroupBox->setVisible(modbusDevice);
    }
    if (modbusDevice) {
        const QString type = device.transport.protocolOptions.value(QStringLiteral("type")).toString(QStringLiteral("TCP")).toUpper();
        {
            QSignalBlocker blocker(m_modbusTypeCombo);
            const int typeIndex = m_modbusTypeCombo->findText(type);
            m_modbusTypeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
        }
        const QJsonObject rtu = device.transport.serial;
        {
            QSignalBlocker blocker(m_modbusSerialPortCombo);
            const QString serialPort = uiJsonValueToString(rtu.value(QStringLiteral("serialPort")));
            const int serialIndex = m_modbusSerialPortCombo->findText(serialPort);
            m_modbusSerialPortCombo->setCurrentIndex(serialIndex >= 0 ? serialIndex : 0);
        }
        {
            QSignalBlocker blocker(m_modbusHwVariantCombo);
            const QString hwVariant = project.modbus.hwVariant.trimmed();
            const int hwIndex = m_modbusHwVariantCombo->findData(hwVariant);
            m_modbusHwVariantCombo->setCurrentIndex(hwIndex >= 0 ? hwIndex : 0);
        }
        for (auto pair : {qMakePair(m_modbusBaudCombo, uiJsonValueToString(rtu.value(QStringLiteral("baud")))),
                          qMakePair(m_modbusDataBitsCombo, uiJsonValueToString(rtu.value(QStringLiteral("dataBits")))),
                          qMakePair(m_modbusStopBitsCombo, uiJsonValueToString(rtu.value(QStringLiteral("stopBits")))),
                          qMakePair(m_modbusParityCombo, uiJsonValueToString(rtu.value(QStringLiteral("parity"))))}) {
            QSignalBlocker blocker(pair.first);
            const int index = pair.first->findText(pair.second);
            pair.first->setCurrentIndex(index >= 0 ? index : 0);
        }
        QSignalBlocker debugBlocker(m_modbusDebugCheck);
        const QString debug = device.transport.protocolOptions.value(QStringLiteral("debug")).toString().toLower();
        m_modbusDebugCheck->setChecked(debug == QStringLiteral("on")
            || debug == QStringLiteral("1")
            || debug == QStringLiteral("true")
            || debug == QStringLiteral("yes"));
    }

    const QSet<QString> duplicateAddresses = device.protocol == configtool::ProtocolType::Iec104
        ? duplicateIec104ChannelBindingAddresses(project, device)
        : duplicateBindingAddresses(device);
    int emptyAddressCount = 0;
    for (const configtool::PointBinding &binding : device.bindings) {
        if (binding.enabled && binding.address.trimmed().isEmpty()) {
            ++emptyAddressCount;
        }
    }

    const QString dataRefKeyword = m_deviceBindingDataRefFilterEdit
        ? m_deviceBindingDataRefFilterEdit->text().trimmed()
        : QString();
    const QString descriptionKeyword = m_deviceBindingDescriptionFilterEdit
        ? m_deviceBindingDescriptionFilterEdit->text().trimmed()
        : QString();
    QList<int> visibleBindingIndexes;
    for (int bindingIndex = 0; bindingIndex < device.bindings.size(); ++bindingIndex) {
        const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
        const bool matchesDataRef = dataRefKeyword.isEmpty()
            || binding.dataRef.contains(dataRefKeyword, Qt::CaseInsensitive);
        const bool matchesDescription = descriptionKeyword.isEmpty()
            || binding.descriptionOverride.contains(descriptionKeyword, Qt::CaseInsensitive);
        if (matchesDataRef && matchesDescription) {
            visibleBindingIndexes.append(bindingIndex);
        }
    }

    if (modbusDevice) {
        int missingRegisterCount = 0;
        m_updatingDeviceBindingsTable = true;
        m_deviceBindingsTable->clear();
        m_deviceBindingsTable->setColumnCount(ModbusBindingColumnCount);
        m_deviceBindingsTable->setHorizontalHeaderLabels({
            QStringLiteral("启用"),
            QStringLiteral("点位类型"),
            QStringLiteral("DataRef"),
            QStringLiteral("描述"),
            QStringLiteral("功能码"),
            QStringLiteral("寄存器"),
            QStringLiteral("数据类型"),
            QStringLiteral("比例"),
            QStringLiteral("组号"),
            QStringLiteral("序号"),
            QStringLiteral("dataIndex"),
            QStringLiteral("虚拟点标志")
        });
        m_deviceBindingsTable->setRowCount(visibleBindingIndexes.size());
        m_deviceBindingsTable->setColumnWidth(ModbusColumnEnabled, 56);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnKind, 90);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataRef, 260);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDescription, 180);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnFunCode, 64);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnRegister, 80);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataType, 130);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnScale, 64);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnGroupNo, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnEntryNo, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataIndex, 90);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnSelfSignal, 80);

        for (int row = 0; row < visibleBindingIndexes.size(); ++row) {
            const int bindingIndex = visibleBindingIndexes.at(row);
            const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
            auto *enabledItem = new QTableWidgetItem();
            enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
            enabledItem->setData(Qt::UserRole, bindingIndex);

            const QString kind = modbusBindingKind(binding);
            const QString dataType = normalizedModbusDataTypeForKind(
                kind,
                modbusBindingString(binding, QStringLiteral("modbusDataType"), defaultModbusDataTypeForKind(kind)));
            const QString scale = modbusBindingString(binding, QStringLiteral("modbusScale"), QStringLiteral("1.0"));
            const QString funCode = QString::number(modbusBindingInt(binding, QStringLiteral("modbusFunctionCode"), kind == QStringLiteral("yx") ? 2 : (kind == QStringLiteral("yc") ? 3 : 6)));
            const bool hasRegister = binding.extensions.contains(QStringLiteral("modbusRegisterAddress"));
            const QString registerAddress = hasRegister
                ? QString::number(modbusBindingInt(binding, QStringLiteral("modbusRegisterAddress")))
                : QString();
            const QString groupNo = binding.extensions.contains(QStringLiteral("modbusGroupNo"))
                ? QString::number(modbusBindingInt(binding, QStringLiteral("modbusGroupNo")))
                : QString();
            const QString entryNo = binding.extensions.contains(QStringLiteral("modbusEntryNo"))
                ? QString::number(modbusBindingInt(binding, QStringLiteral("modbusEntryNo")))
                : QString();

            auto *kindItem = new QTableWidgetItem(modbusKindDisplayName(kind));
            kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
            kindItem->setData(Qt::UserRole, bindingIndex);
            auto *dataRefItem = new QTableWidgetItem(binding.dataRef);
            auto *descriptionItem = new QTableWidgetItem(binding.descriptionOverride);
            auto *funCodeItem = new QTableWidgetItem(funCode);
            auto *registerItem = new QTableWidgetItem(registerAddress);
            auto *dataTypeItem = new QTableWidgetItem(dataType);
            auto *scaleItem = new QTableWidgetItem(scale);
            auto *groupItem = new QTableWidgetItem(groupNo);
            auto *entryItem = new QTableWidgetItem(entryNo);
            auto *dataIndexItem = new QTableWidgetItem(binding.address);
            auto *selfSignalItem = new QTableWidgetItem();
            selfSignalItem->setFlags((selfSignalItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            selfSignalItem->setCheckState(selfSignalFlagChecked(binding.selfSignalFlag) ? Qt::Checked : Qt::Unchecked);

            dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsEditable);
            entryItem->setFlags(entryItem->flags() & ~Qt::ItemIsEditable);
            dataIndexItem->setFlags(dataIndexItem->flags() & ~Qt::ItemIsEditable);

            if (binding.enabled && !hasRegister) {
                ++missingRegisterCount;
                const QColor warningColor(QStringLiteral("#b9770e"));
                dataRefItem->setForeground(warningColor);
                registerItem->setForeground(warningColor);
            }

            m_deviceBindingsTable->setItem(row, ModbusColumnEnabled, enabledItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnKind, kindItem);
            auto *kindCombo = new QComboBox(m_deviceBindingsTable);
            kindCombo->setObjectName(QStringLiteral("modbusPointKindCombo"));
            kindCombo->addItem(QStringLiteral("遥信"), QStringLiteral("yx"));
            kindCombo->addItem(QStringLiteral("遥测"), QStringLiteral("yc"));
            kindCombo->addItem(QStringLiteral("遥控"), QStringLiteral("yk"));
            kindCombo->addItem(QStringLiteral("遥调"), QStringLiteral("yt"));
            kindCombo->setMinimumWidth(76);
            kindCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            const int kindIndex = kindCombo->findData(kind);
            kindCombo->setCurrentIndex(kindIndex >= 0 ? kindIndex : kindCombo->findData(QStringLiteral("yc")));
            kindCombo->setProperty("bindingIndex", bindingIndex);
            connect(kindCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, kindCombo](int) {
                if (m_updatingDeviceBindingsTable || m_restoringConfigUndo) {
                    return;
                }

                const int deviceIndex = currentConfigDeviceIndex();
                if (deviceIndex < 0) {
                    return;
                }

                configtool::ConfigProject &project = m_configProjectManager.project();
                if (deviceIndex >= project.devices.size()) {
                    return;
                }

                configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
                const int bindingIndex = kindCombo->property("bindingIndex").toInt();
                if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
                    return;
                }

                pushConfigUndoSnapshot();
                const QString kind = kindCombo->currentData().toString();
                device.bindings[bindingIndex].extensions.insert(QStringLiteral("modbusKind"), kind);
                device.bindings[bindingIndex].extensions.insert(QStringLiteral("modbusDataType"), defaultModbusDataTypeForKind(kind));
                rebuildModbusDeviceConfig(device);
                refreshDeviceDetail(deviceIndex);
                refreshDeviceEditor(deviceIndex);
                for (int row = 0; row < m_deviceBindingsTable->rowCount(); ++row) {
                    const QTableWidgetItem *item = m_deviceBindingsTable->item(row, ModbusColumnEnabled);
                    if (item && item->data(Qt::UserRole).toInt() == bindingIndex) {
                        m_deviceBindingsTable->setCurrentCell(row, ModbusColumnKind);
                        break;
                    }
                }
            });
            m_deviceBindingsTable->setCellWidget(row, ModbusColumnKind, kindCombo);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataRef, dataRefItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDescription, descriptionItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnFunCode, funCodeItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnRegister, registerItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataType, dataTypeItem);
            auto *dataTypeCombo = new QComboBox(m_deviceBindingsTable);
            dataTypeCombo->setObjectName(QStringLiteral("modbusDataTypeCombo"));
            dataTypeCombo->addItems(modbusDataTypeOptionsForKind(kind));
            dataTypeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            dataTypeCombo->setMinimumContentsLength(4);
            dataTypeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            const int dataTypeIndex = dataTypeCombo->findText(dataType);
            dataTypeCombo->setCurrentIndex(dataTypeIndex >= 0 ? dataTypeIndex : 0);
            dataTypeCombo->setProperty("bindingIndex", bindingIndex);
            connect(dataTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, dataTypeCombo](int) {
                if (m_updatingDeviceBindingsTable || m_restoringConfigUndo) {
                    return;
                }

                const int deviceIndex = currentConfigDeviceIndex();
                if (deviceIndex < 0) {
                    return;
                }

                configtool::ConfigProject &project = m_configProjectManager.project();
                if (deviceIndex >= project.devices.size()) {
                    return;
                }

                configtool::ProtocolDeviceInstance &device = project.devices[deviceIndex];
                const int bindingIndex = dataTypeCombo->property("bindingIndex").toInt();
                if (bindingIndex < 0 || bindingIndex >= device.bindings.size()) {
                    return;
                }

                pushConfigUndoSnapshot();
                configtool::PointBinding &binding = device.bindings[bindingIndex];
                binding.extensions.insert(QStringLiteral("modbusDataType"),
                                          normalizedModbusDataTypeForKind(modbusBindingKind(binding),
                                                                         dataTypeCombo->currentText()));
                rebuildModbusDeviceConfig(device);
                refreshDeviceDetail(deviceIndex);
                refreshDeviceEditor(deviceIndex);
                for (int row = 0; row < m_deviceBindingsTable->rowCount(); ++row) {
                    const QTableWidgetItem *item = m_deviceBindingsTable->item(row, ModbusColumnEnabled);
                    if (item && item->data(Qt::UserRole).toInt() == bindingIndex) {
                        m_deviceBindingsTable->setCurrentCell(row, ModbusColumnDataType);
                        break;
                    }
                }
            });
            m_deviceBindingsTable->setCellWidget(row, ModbusColumnDataType, dataTypeCombo);
            m_deviceBindingsTable->setItem(row, ModbusColumnScale, scaleItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnGroupNo, groupItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnEntryNo, entryItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataIndex, dataIndexItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnSelfSignal, selfSignalItem);
        }
        m_updatingDeviceBindingsTable = false;

        auto fitDataTypeCombosToCells = [this]() {
            if (!m_deviceBindingsTable
                || m_deviceBindingsTable->columnCount() != ModbusBindingColumnCount) {
                return;
            }

            for (int row = 0; row < m_deviceBindingsTable->rowCount(); ++row) {
                QWidget *widget = m_deviceBindingsTable->cellWidget(row, ModbusColumnDataType);
                if (!widget) {
                    continue;
                }

                const QModelIndex index = m_deviceBindingsTable->model()->index(row, ModbusColumnDataType);
                const QRect rect = m_deviceBindingsTable->visualRect(index);
                if (rect.isValid()) {
                    widget->setGeometry(rect.adjusted(5, 3, -5, -3));
                }
            }
            m_deviceBindingsTable->viewport()->update();
        };
        fitDataTypeCombosToCells();
        QTimer::singleShot(0, this, fitDataTypeCombosToCells);

        if (missingRegisterCount > 0) {
            m_deviceValidationLabel->setStyleSheet("QLabel { color: #b9770e; }");
            m_deviceValidationLabel->setText(QStringLiteral("当前有 %1 个启用点位未填写 Modbus 寄存器地址。填写后会自动生成分组、组内序号和 dataIndex。")
                .arg(missingRegisterCount));
        } else {
            m_deviceValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
            m_deviceValidationLabel->setText(QStringLiteral("Modbus 映射已生成：轮询组 %1 个，写入项 %2 个。")
                .arg(device.modbus.pollGroups.size())
                .arg(device.modbus.setPoints.size()));
        }
        return;
    }

    m_updatingDeviceBindingsTable = true;
    m_deviceBindingsTable->clear();
    m_deviceBindingsTable->setColumnCount(Iec104BindingColumnCount);
    m_deviceBindingsTable->setHorizontalHeaderLabels({
        QStringLiteral("启用"),
        QStringLiteral("DataRef"),
        QStringLiteral("描述"),
        QStringLiteral("地址"),
        QStringLiteral("初值"),
        QStringLiteral("虚拟点标志")
    });
    m_deviceBindingsTable->setRowCount(visibleBindingIndexes.size());
    for (int row = 0; row < visibleBindingIndexes.size(); ++row) {
        const int bindingIndex = visibleBindingIndexes.at(row);
        const configtool::PointBinding &binding = device.bindings.at(bindingIndex);
        auto *enabledItem = new QTableWidgetItem();
        enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, bindingIndex);
        auto *dataRefItem = new QTableWidgetItem(binding.dataRef);
        auto *descriptionItem = new QTableWidgetItem(binding.descriptionOverride);
        auto *addressItem = new QTableWidgetItem(binding.address);
        auto *initValueItem = new QTableWidgetItem(binding.initValue);
        auto *selfSignalItem = new QTableWidgetItem();
        selfSignalItem->setFlags((selfSignalItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        selfSignalItem->setCheckState(selfSignalFlagChecked(binding.selfSignalFlag) ? Qt::Checked : Qt::Unchecked);
        dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);

        if (binding.enabled && duplicateAddresses.contains(binding.address.trimmed())) {
            const QColor duplicateColor(QStringLiteral("#c0392b"));
            enabledItem->setForeground(duplicateColor);
            dataRefItem->setForeground(duplicateColor);
            descriptionItem->setForeground(duplicateColor);
            addressItem->setForeground(duplicateColor);
            initValueItem->setForeground(duplicateColor);
            selfSignalItem->setForeground(duplicateColor);
        } else if (binding.enabled && binding.address.trimmed().isEmpty()) {
            const QColor warningColor(QStringLiteral("#b9770e"));
            dataRefItem->setForeground(warningColor);
            addressItem->setForeground(warningColor);
        }

        m_deviceBindingsTable->setItem(row, 0, enabledItem);
        m_deviceBindingsTable->setItem(row, 1, dataRefItem);
        m_deviceBindingsTable->setItem(row, 2, descriptionItem);
        m_deviceBindingsTable->setItem(row, 3, addressItem);
        m_deviceBindingsTable->setItem(row, 4, initValueItem);
        m_deviceBindingsTable->setItem(row, 5, selfSignalItem);
    }
    m_updatingDeviceBindingsTable = false;

    if (!duplicateAddresses.isEmpty()) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #c0392b; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("检测到同通道重复 104 地址：%1。请检查 %2 下所有设备的启用点位地址。")
                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，")),
                     iec104ChannelDisplayName(device)));
    } else if (emptyAddressCount > 0) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #b9770e; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("当前仍有 %1 个启用点位未填写 104 地址，导出前需要补齐。")
                .arg(emptyAddressCount));
    } else {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        m_deviceValidationLabel->setText(QStringLiteral("当前同通道设备地址分配未发现重复。"));
    }
}

