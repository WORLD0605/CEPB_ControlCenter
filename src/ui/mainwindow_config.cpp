#include "mainwindow.h"

#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>

namespace {

int modelPointFilterTabIndex(configtool::ModelServiceType type)
{
    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return 1;
    case configtool::ModelServiceType::Status:
        return 2;
    case configtool::ModelServiceType::Control:
        return 3;
    }

    return 0;
}

configtool::ModelServiceType modelServiceTypeFromTabIndex(int index)
{
    switch (index) {
    case 2:
        return configtool::ModelServiceType::Status;
    case 3:
        return configtool::ModelServiceType::Control;
    case 1:
    default:
        return configtool::ModelServiceType::Measurement;
    }
}

configtool::PointSignalType signalTypeForModelService(configtool::ModelServiceType type)
{
    switch (type) {
    case configtool::ModelServiceType::Measurement:
        return configtool::PointSignalType::Yc;
    case configtool::ModelServiceType::Status:
        return configtool::PointSignalType::Yx;
    case configtool::ModelServiceType::Control:
        return configtool::PointSignalType::Ctrl;
    }

    return configtool::PointSignalType::Yc;
}

QStringList splitClipboardLine(const QString &line)
{
    return line.split('\t');
}

QList<QStringList> parseClipboardTable(const QString &text)
{
    QList<QStringList> table;
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QStringList lines = normalized.split('\n');
    for (int index = 0; index < lines.size(); ++index) {
        if (index == lines.size() - 1 && lines.at(index).isEmpty()) {
            continue;
        }
        table.append(splitClipboardLine(lines.at(index)));
    }

    return table;
}

QModelIndexList sortedEditableTargetIndexes(QTableWidget *table)
{
    QModelIndexList indexes;
    if (table && table->selectionModel()) {
        indexes = table->selectionModel()->selectedIndexes();
    }

    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &left, const QModelIndex &right) {
        if (left.row() != right.row()) {
            return left.row() < right.row();
        }
        return left.column() < right.column();
    });

    return indexes;
}

} // namespace

void MainWindow::onBrowseConfigImportDirClicked()
{
    const QString startDir = configBrowseStartDir();
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择配置工程目录"),
        startDir);
    if (!dir.isEmpty()) {
        const QString projectRoot = normalizedConfigProjectRoot(dir);
        m_configImportDirEdit->setText(projectRoot);
        QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
        settings.setValue(QStringLiteral("config/lastBrowseDir"), projectRoot);
        onImportIec104ConfigClicked();
    }
}

void MainWindow::onImportIec104ConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QString appDir = resolveIec104AppDir(projectRoot);
    if (appDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到 cepiec104 子目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ImportReport report;
    const QString projectName = QFileInfo(projectRoot).fileName().trimmed().isEmpty()
        ? QStringLiteral("配置工程")
        : QFileInfo(projectRoot).fileName();
    m_configProjectManager.createEmptyProject(projectName, projectRoot);
    const bool ok = m_configProjectManager.importIec104AppDirectory(appDir, report);
    refreshConfigImportSummary(report);

    if (!ok && report.hasErrors()) {
        statusBar()->showMessage(QStringLiteral("配置导入失败"), 5000);
        return;
    }

    statusBar()->showMessage(QStringLiteral("配置导入完成"), 5000);
}

void MainWindow::onExportIec104ConfigClicked()
{
    const QString projectRoot = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (projectRoot.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择配置工程目录"));
        return;
    }

    const QString appDir = resolveIec104AppDir(projectRoot);
    if (appDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到 cepiec104 子目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ExportReport report;
    const bool ok = m_configProjectManager.exportIec104AppDirectory(appDir, report);

    QStringList issueLines;
    for (const configtool::ImportIssue &issue : report.issues) {
        const QString severity = issue.severity == configtool::ImportIssueSeverity::Error
            ? QStringLiteral("错误")
            : QStringLiteral("警告");
        issueLines << QStringLiteral("[%1] %2").arg(severity, issue.message);
    }

    if (!ok) {
        const QString detail = issueLines.isEmpty()
            ? QStringLiteral("导出失败，但未返回详细错误。")
            : issueLines.join('\n');
        QMessageBox::warning(this, QStringLiteral("导出失败"), detail);
        statusBar()->showMessage(QStringLiteral("配置导出失败"), 5000);
        return;
    }

    QString statusMessage = QStringLiteral("配置导出完成: 模型 %1，设备 %2")
        .arg(report.exportedModelCount)
        .arg(report.exportedDeviceCount);
    if (!issueLines.isEmpty()) {
        statusMessage += QStringLiteral("，警告 %1 条").arg(issueLines.size());
    }
    statusBar()->showMessage(statusMessage, 8000);

    if (!issueLines.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出完成"), issueLines.join('\n'));
    }
}

QString MainWindow::normalizedConfigProjectRoot(const QString &selectedPath) const
{
    if (selectedPath.isEmpty()) {
        return QString();
    }

    const QFileInfo selectedInfo(selectedPath);
    const QString absolutePath = selectedInfo.absoluteFilePath();
    const QString folderName = selectedInfo.fileName().trimmed();
    if (folderName.compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }

    return absolutePath;
}

QString MainWindow::resolveIec104AppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepiec104"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepiec104"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::configBrowseStartDir() const
{
    const QString currentValue = normalizedConfigProjectRoot(m_configImportDirEdit->text());
    if (!currentValue.isEmpty() && QDir(currentValue).exists()) {
        return currentValue;
    }

    QSettings settings(QStringLiteral("CEPB"), QStringLiteral("ControlCenter"));
    const QString lastDir = settings.value(QStringLiteral("config/lastBrowseDir")).toString().trimmed();
    if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
        return lastDir;
    }

    return QDir::homePath();
}

void MainWindow::onConfigModelSelectionChanged()
{
    const int modelIndex = currentConfigModelIndex();
    refreshModelOverview(modelIndex);
    refreshModelDetail(modelIndex);
    refreshSelectionOverview();
}

void MainWindow::onConfigDeviceSelectionChanged()
{
    const int deviceIndex = currentConfigDeviceIndex();
    refreshDeviceDetail(deviceIndex);
    if (deviceIndex >= 0) {
        refreshDeviceEditor(deviceIndex);
    } else {
        refreshDeviceEditor(-1);
    }
    refreshSelectionOverview();
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
    model.modelId = m_modelIdEdit->text().trimmed();
    model.name = model.modelId;
    model.displayName = m_modelDisplayNameEdit->text().trimmed();
    model.deviceType = m_modelDeviceTypeEdit->text().trimmed();
    model.version = m_modelVersionEdit->text().trimmed();
    model.manufacturerId = m_modelManufacturerIdEdit->text().trimmed();
    model.manufacturerDesc = m_modelManufacturerDescEdit->text().trimmed();
    model.schema = m_modelSchemaEdit->text().trimmed();

    refreshConfigObjectViews();
    if (modelIndex < m_configModelTable->rowCount()) {
        m_configModelTable->selectRow(modelIndex);
    }
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
    point.lnType = QStringLiteral("CustomGGIO");
    point.lnInst = QStringLiteral("1");
    point.doName = point.name;
    point.doType = newPointType == configtool::ModelServiceType::Status
        ? QStringLiteral("SPS")
        : (newPointType == configtool::ModelServiceType::Control
            ? QStringLiteral("SPC")
            : QStringLiteral("MV"));
    point.dataType = newPointType == configtool::ModelServiceType::Measurement
        ? QStringLiteral("Float")
        : QStringLiteral("Bool");
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
    const QPair<int, int> pointLocation = currentModelPointLocation();
    if (modelIndex < 0 || pointLocation.first < 0 || pointLocation.second < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个点位。"));
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除点位"),
                              QStringLiteral("确定删除当前选中的模型点位吗？")) != QMessageBox::Yes) {
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

    service.points.removeAt(pointLocation.second);
    refreshConfigObjectViews();
    refreshModelDetail(modelIndex);
    statusBar()->showMessage(QStringLiteral("已删除模型点位"), 3000);
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
    const QString suggestedDeviceId = QStringLiteral("DEV_%1_%2")
        .arg(model.modelId.isEmpty() ? QStringLiteral("new") : model.modelId)
        .arg(project.devices.size() + 1);
    bool accepted = false;
    const QString deviceId = QInputDialog::getText(
        this,
        QStringLiteral("创建设备骨架"),
        QStringLiteral("请输入 DeviceId:"),
        QLineEdit::Normal,
        suggestedDeviceId,
        &accepted).trimmed();
    if (!accepted || deviceId.isEmpty()) {
        return;
    }

    configtool::ProtocolDeviceInstance device;
    device.deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    device.appType = QStringLiteral("cepiec104");
    device.protocol = configtool::ProtocolType::Iec104;
    device.deviceId = deviceId;
    device.deviceDesc = model.displayName.isEmpty() ? model.modelId : model.displayName;
    device.modelId = model.modelId;

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            configtool::PointBinding binding;
            binding.bindingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            binding.pointRef = point.pointRef(model.modelId);
            binding.dataRef = point.dataRef();
            binding.descriptionOverride = point.description;
            binding.enabled = true;
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
    device.deviceId = m_deviceIdEdit->text().trimmed();
    device.deviceDesc = m_deviceDescEdit->text().trimmed();
    device.transport.stationAddress = m_deviceStationAddressEdit->text().trimmed();
    device.transport.ip = m_deviceIpEdit->text().trimmed();
    device.transport.port = m_devicePortEdit->text().trimmed();
    device.transport.channel = m_deviceChannelEdit->text().trimmed();

    refreshConfigObjectViews();
    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    if (deviceIndex < m_configDeviceTable->rowCount()) {
        m_configDeviceTable->selectRow(deviceIndex);
    }
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
    if (item->column() == 0) {
        binding.enabled = item->checkState() == Qt::Checked;
    } else {
        applyDeviceBindingCellText(item->row(), item->column(), item->text());
    }

    refreshDeviceDetail(deviceIndex);
    refreshDeviceEditor(deviceIndex);
    m_deviceBindingsTable->setCurrentCell(item->row(), item->column());
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

    QTableWidgetItem *categoryItem = m_modelPointsTable->item(item->row(), 0);
    if (!categoryItem) {
        return;
    }

    const int serviceIndex = categoryItem->data(Qt::UserRole).toInt();
    const int pointIndex = categoryItem->data(Qt::UserRole + 1).toInt();

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
    applyModelPointCellText(item->row(), item->column(), item->text());

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
    QTableWidgetItem *categoryItem = m_modelPointsTable->item(row, 0);
    if (!categoryItem) {
        return;
    }

    const int modelIndex = currentConfigModelIndex();
    const int serviceIndex = categoryItem->data(Qt::UserRole).toInt();
    const int pointIndex = categoryItem->data(Qt::UserRole + 1).toInt();

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
    const QString value = text.trimmed();
    switch (column) {
    case 1:
        point.name = value;
        point.doName = point.name;
        break;
    case 2:
        point.description = value;
        break;
    case 3:
        point.ldName = value;
        break;
    case 4:
        point.lnType = value;
        break;
    case 5:
        point.lnInst = value;
        break;
    case 7:
        point.dataType = value;
        break;
    case 8:
        point.unit = value;
        break;
    default:
        break;
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
        binding.selfSignalFlag = value;
        break;
    default:
        break;
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
    m_configIssueCountValueLabel->setText(QString::number(report.issues.size()));

    QString statusMessage = QStringLiteral("导入结果: 模型 %1，设备 %2")
        .arg(report.importedModelCount)
        .arg(report.importedDeviceCount);
    if (report.issues.isEmpty()) {
        statusMessage += QStringLiteral("，未发现错误或警告。");
    } else {
        statusMessage += QStringLiteral("，问题数 %1。")
            .arg(report.issues.size());
    }
    statusBar()->showMessage(statusMessage, 8000);
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

void MainWindow::refreshModelDetail(int modelIndex)
{
    const configtool::ConfigProject &project = m_configProjectManager.project();
    if (modelIndex < 0 || modelIndex >= project.models.size()) {
        for (QLineEdit *edit : {m_modelIdEdit, m_modelDisplayNameEdit, m_modelDeviceTypeEdit,
                                m_modelVersionEdit, m_modelManufacturerIdEdit,
                                m_modelManufacturerDescEdit, m_modelSchemaEdit}) {
            edit->clear();
        }
        m_modelValidationLabel->setText(QStringLiteral("请选择一个模型。"));
        m_modelPointsTable->setRowCount(0);
        return;
    }

    const configtool::ModelTemplate &model = project.models.at(modelIndex);
    const QSet<QString> duplicateRefs = duplicateDataRefsForModel(model);
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

    const int filterTabIndex = m_modelPointFilterTabBar ? m_modelPointFilterTabBar->currentIndex() : 0;
    int totalPointCount = 0;
    for (const configtool::ServiceTemplate &service : model.services) {
        if (filterTabIndex == 0 || modelPointFilterTabIndex(service.type) == filterTabIndex) {
            totalPointCount += service.points.size();
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
        for (int pointIndex = 0; pointIndex < service.points.size(); ++pointIndex, ++row) {
            const configtool::PointTemplate &point = service.points.at(pointIndex);
            auto *categoryItem = new QTableWidgetItem(configtool::modelServiceTypeDisplayName(point.category));
            auto *nameItem = new QTableWidgetItem(point.doName);
            auto *descriptionItem = new QTableWidgetItem(point.description);
            auto *ldNameItem = new QTableWidgetItem(point.ldName);
            auto *lnTypeItem = new QTableWidgetItem(point.lnType);
            auto *lnInstItem = new QTableWidgetItem(point.lnInst);
            auto *dataRefItem = new QTableWidgetItem(point.dataRef());
            auto *dataTypeItem = new QTableWidgetItem(point.dataType);
            auto *unitItem = new QTableWidgetItem(point.unit);

            categoryItem->setData(Qt::UserRole, serviceIndex);
            categoryItem->setData(Qt::UserRole + 1, pointIndex);
            categoryItem->setData(Qt::UserRole + 2, point.pointId);
            categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsEditable);
            dataRefItem->setFlags(dataRefItem->flags() & ~Qt::ItemIsEditable);

            if (duplicateRefs.contains(point.dataRef())) {
                const QColor duplicateColor(QStringLiteral("#c0392b"));
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

            m_modelPointsTable->setItem(row, 0, categoryItem);
            m_modelPointsTable->setItem(row, 1, nameItem);
            m_modelPointsTable->setItem(row, 2, descriptionItem);
            m_modelPointsTable->setItem(row, 3, ldNameItem);
            m_modelPointsTable->setItem(row, 4, lnTypeItem);
            m_modelPointsTable->setItem(row, 5, lnInstItem);
            m_modelPointsTable->setItem(row, 6, dataRefItem);
            m_modelPointsTable->setItem(row, 7, dataTypeItem);
            m_modelPointsTable->setItem(row, 8, unitItem);

            auto *categoryCombo = new QComboBox(m_modelPointsTable);
            categoryCombo->addItem(QStringLiteral("遥测"), static_cast<int>(configtool::ModelServiceType::Measurement));
            categoryCombo->addItem(QStringLiteral("遥信"), static_cast<int>(configtool::ModelServiceType::Status));
            categoryCombo->addItem(QStringLiteral("控制"), static_cast<int>(configtool::ModelServiceType::Control));
            categoryCombo->setProperty("serviceIndex", serviceIndex);
            categoryCombo->setProperty("pointIndex", pointIndex);
            const int categoryComboIndex = categoryCombo->findData(static_cast<int>(point.category));
            categoryCombo->setCurrentIndex(categoryComboIndex >= 0 ? categoryComboIndex : 0);
            connect(categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                    this, &MainWindow::onModelPointCategoryChanged);
            m_modelPointsTable->setCellWidget(row, 0, categoryCombo);
        }
    }
    m_updatingModelPointsTable = false;
    m_updatingModelPointCategory = false;

    if (duplicateRefs.isEmpty()) {
        m_modelValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        m_modelValidationLabel->setText(QStringLiteral("当前模型点位的 DataRef 唯一。"));
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
        QTableWidgetItem *categoryItem = m_modelPointsTable->item(row, 0);
        if (categoryItem && categoryItem->data(Qt::UserRole + 2).toString() == pointId) {
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
    if (deviceIndex < 0 || deviceIndex >= project.devices.size()) {
        for (QLineEdit *edit : {m_deviceIdEdit, m_deviceDescEdit, m_deviceModelEdit,
                                m_deviceStationAddressEdit, m_deviceIpEdit,
                                m_devicePortEdit, m_deviceChannelEdit}) {
            if (edit) {
                edit->clear();
            }
        }
        m_deviceCompatIpbLabel->setText(QStringLiteral("-"));
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #666666; }");
        m_deviceValidationLabel->setText(QStringLiteral("请选择一个 104 设备。"));
        m_deviceBindingsTable->setRowCount(0);
        return;
    }

    const configtool::ProtocolDeviceInstance &device = project.devices.at(deviceIndex);
    for (auto pair : {qMakePair(m_deviceIdEdit, device.deviceId),
                      qMakePair(m_deviceDescEdit, device.deviceDesc),
                      qMakePair(m_deviceModelEdit, device.modelId),
                      qMakePair(m_deviceStationAddressEdit, device.transport.stationAddress),
                      qMakePair(m_deviceIpEdit, device.transport.ip),
                      qMakePair(m_devicePortEdit, device.transport.port),
                      qMakePair(m_deviceChannelEdit, device.transport.channel)}) {
        QSignalBlocker blocker(pair.first);
        pair.first->setText(pair.second);
    }

    const QJsonValue ipbValue = device.transport.source.rawExtra.value(QStringLiteral("ipb"));
    if (ipbValue.isUndefined() || ipbValue.isNull()) {
        m_deviceCompatIpbLabel->setText(QStringLiteral("-"));
    } else if (ipbValue.isString()) {
        m_deviceCompatIpbLabel->setText(ipbValue.toString());
    } else if (ipbValue.isDouble()) {
        m_deviceCompatIpbLabel->setText(QString::number(ipbValue.toInt()));
    } else {
        m_deviceCompatIpbLabel->setText(QString::fromUtf8(QJsonDocument(ipbValue.toObject()).toJson(QJsonDocument::Compact)));
    }

    const QSet<QString> duplicateAddresses = duplicateBindingAddresses(device);
    int emptyAddressCount = 0;
    for (const configtool::PointBinding &binding : device.bindings) {
        if (binding.enabled && binding.address.trimmed().isEmpty()) {
            ++emptyAddressCount;
        }
    }

    m_updatingDeviceBindingsTable = true;
    m_deviceBindingsTable->setRowCount(device.bindings.size());
    for (int row = 0; row < device.bindings.size(); ++row) {
        const configtool::PointBinding &binding = device.bindings.at(row);
        auto *enabledItem = new QTableWidgetItem();
        enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, row);
        auto *dataRefItem = new QTableWidgetItem(binding.dataRef);
        auto *descriptionItem = new QTableWidgetItem(binding.descriptionOverride);
        auto *addressItem = new QTableWidgetItem(binding.address);
        auto *initValueItem = new QTableWidgetItem(binding.initValue);
        auto *selfSignalItem = new QTableWidgetItem(binding.selfSignalFlag);
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
            QStringLiteral("检测到重复 104 地址：%1。请调整地址列，避免启用点位地址冲突。")
                .arg(QStringList(duplicateAddresses.begin(), duplicateAddresses.end()).join(QStringLiteral("，"))));
    } else if (emptyAddressCount > 0) {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #b9770e; }");
        m_deviceValidationLabel->setText(
            QStringLiteral("当前仍有 %1 个启用点位未填写 104 地址，导出前需要补齐。")
                .arg(emptyAddressCount));
    } else {
        m_deviceValidationLabel->setStyleSheet("QLabel { color: #2e7d32; }");
        m_deviceValidationLabel->setText(QStringLiteral("当前设备地址分配未发现重复。"));
    }
}

int MainWindow::currentConfigModelIndex() const
{
    if (!m_configModelTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_configModelTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

int MainWindow::currentConfigDeviceIndex() const
{
    if (!m_configDeviceTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_configDeviceTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QPair<int, int> MainWindow::currentModelPointLocation() const
{
    if (!m_modelPointsTable->selectionModel()) {
        return qMakePair(-1, -1);
    }

    int selectedRow = m_modelPointsTable->currentRow();
    const QModelIndexList indexes = m_modelPointsTable->selectionModel()->selectedIndexes();
    if (!indexes.isEmpty()) {
        selectedRow = indexes.first().row();
    }

    if (selectedRow < 0) {
        return qMakePair(-1, -1);
    }

    QTableWidgetItem *categoryItem = m_modelPointsTable->item(selectedRow, 0);
    if (!categoryItem) {
        return qMakePair(-1, -1);
    }

    return qMakePair(categoryItem->data(Qt::UserRole).toInt(),
                     categoryItem->data(Qt::UserRole + 1).toInt());
}

QSet<QString> MainWindow::duplicateDataRefsForModel(const configtool::ModelTemplate &model) const
{
    QSet<QString> seenRefs;
    QSet<QString> duplicateRefs;

    for (const configtool::ServiceTemplate &service : model.services) {
        for (const configtool::PointTemplate &point : service.points) {
            const QString ref = point.dataRef();
            if (ref.isEmpty()) {
                continue;
            }

            if (seenRefs.contains(ref)) {
                duplicateRefs.insert(ref);
            } else {
                seenRefs.insert(ref);
            }
        }
    }

    return duplicateRefs;
}

QSet<QString> MainWindow::duplicateBindingAddresses(const configtool::ProtocolDeviceInstance &device) const
{
    QSet<QString> seenAddresses;
    QSet<QString> duplicateAddresses;

    for (const configtool::PointBinding &binding : device.bindings) {
        if (!binding.enabled) {
            continue;
        }

        const QString address = binding.address.trimmed();
        if (address.isEmpty()) {
            continue;
        }

        if (seenAddresses.contains(address)) {
            duplicateAddresses.insert(address);
        } else {
            seenAddresses.insert(address);
        }
    }

    return duplicateAddresses;
}
