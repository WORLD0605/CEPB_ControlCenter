#include "mainwindow.h"
#include "config/modbus_mapping_utils.h"

#include <algorithm>
#include <QApplication>
#include <QCheckBox>
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
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>

namespace {

constexpr int Iec104BindingColumnCount = 6;
constexpr int ModbusBindingColumnCount = 12;
constexpr int ModbusColumnEnabled = 0;
constexpr int ModbusColumnKind = 1;
constexpr int ModbusColumnDataRef = 2;
constexpr int ModbusColumnDescription = 3;
constexpr int ModbusColumnFunCode = 4;
constexpr int ModbusColumnRegister = 5;
constexpr int ModbusColumnDataType = 6;
constexpr int ModbusColumnScale = 7;
constexpr int ModbusColumnGroupNo = 8;
constexpr int ModbusColumnEntryNo = 9;
constexpr int ModbusColumnDataIndex = 10;
constexpr int ModbusColumnSelfSignal = 11;

using configtool::buildModbusDataIndex;
using configtool::isModbusDevice;
using configtool::isModbusSetKind;
using configtool::modbusBindingInt;
using configtool::modbusBindingKind;
using configtool::modbusBindingString;
using configtool::modbusKindDisplayName;
using configtool::modbusKindFromString;
using configtool::modbusMaxReadQuantity;
using configtool::modbusTypeRegisterCount;
using configtool::normalizedModbusKind;

bool selfSignalFlagChecked(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("1")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("on");
}

QString uiJsonValueToString(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        const int integer = value.toInt();
        return qFuzzyCompare(number + 1.0, static_cast<double>(integer) + 1.0)
            ? QString::number(integer)
            : QString::number(number);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return QString();
}

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

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString modbusAppDir = resolveModbusAppDir(projectRoot);
    const QString logicCenterAppDir = resolveLogicCenterAppDir(projectRoot);
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到 cepiec104、cepmodbus 或 cepLogicCenter 子目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ImportReport report;
    const QString projectName = QFileInfo(projectRoot).fileName().trimmed().isEmpty()
        ? QStringLiteral("配置工程")
        : QFileInfo(projectRoot).fileName();
    m_configProjectManager.createEmptyProject(projectName, projectRoot);
    bool ok = true;
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.importIec104AppDirectory(iec104AppDir, report) && ok;
    }
    if (!modbusAppDir.isEmpty()) {
        ok = m_configProjectManager.importModbusAppDirectory(modbusAppDir, report) && ok;
    }
    if (!logicCenterAppDir.isEmpty()) {
        const QString logicConfigPath = QDir(logicCenterAppDir)
            .filePath(QStringLiteral("etc/LogicCenter_Config.json"));
        if (QFileInfo::exists(logicConfigPath)) {
            ok = m_configProjectManager.importLogicCenterConfigFile(logicConfigPath, report) && ok;
        } else {
            report.addIssue(configtool::ImportIssueSeverity::Warning,
                            logicConfigPath,
                            QStringLiteral("未找到 LogicCenter_Config.json，已跳过 LogicCenter 配置导入"));
        }
    }
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

    const QString iec104AppDir = resolveIec104AppDir(projectRoot);
    const QString resolvedModbusDir = resolveModbusAppDir(projectRoot);
    const QString resolvedLogicCenterDir = resolveLogicCenterAppDir(projectRoot);
    const QString modbusAppDir = resolvedModbusDir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("cepmodbus"))
        : resolvedModbusDir;
    const QString logicCenterAppDir = resolvedLogicCenterDir.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"))
        : resolvedLogicCenterDir;
    if (iec104AppDir.isEmpty() && modbusAppDir.isEmpty() && logicCenterAppDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("当前工程目录下未找到可导出的配置目录"));
        return;
    }

    m_configImportDirEdit->setText(projectRoot);

    configtool::ExportReport report;
    bool ok = true;
    if (!iec104AppDir.isEmpty()) {
        ok = m_configProjectManager.exportIec104AppDirectory(iec104AppDir, report) && ok;
    }
    ok = m_configProjectManager.exportModbusAppDirectory(modbusAppDir, report) && ok;
    ok = m_configProjectManager.exportLogicCenterConfigFile(
        QDir(logicCenterAppDir).filePath(QStringLiteral("etc/LogicCenter_Config.json")),
        report) && ok;

    QStringList issueLines;
    bool hasErrors = false;
    for (const configtool::ImportIssue &issue : report.issues) {
        const QString severity = issue.severity == configtool::ImportIssueSeverity::Error
            ? QStringLiteral("错误")
            : QStringLiteral("警告");
        hasErrors = hasErrors || issue.severity == configtool::ImportIssueSeverity::Error;
        issueLines << QStringLiteral("[%1] %2").arg(severity, issue.message);
    }

    if (!ok && hasErrors) {
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
    if (folderName.compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0) {
        return QDir(absolutePath).absoluteFilePath(QStringLiteral(".."));
    }
    if (folderName.compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0) {
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

QString MainWindow::resolveModbusAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepmodbus"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepmodbus"));
    return QDir(appDir).exists() ? appDir : QString();
}

QString MainWindow::resolveLogicCenterAppDir(const QString &projectRoot) const
{
    if (projectRoot.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo rootInfo(projectRoot);
    if (rootInfo.fileName().compare(QStringLiteral("cepLogicCenter"), Qt::CaseInsensitive) == 0
        && rootInfo.isDir()) {
        return rootInfo.absoluteFilePath();
    }

    const QString appDir = QDir(projectRoot).filePath(QStringLiteral("cepLogicCenter"));
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
    if (m_deleteModelBtn) {
        m_deleteModelBtn->setEnabled(modelIndex >= 0);
    }
    refreshModelOverview(modelIndex);
    refreshModelDetail(modelIndex);
    refreshSelectionOverview();
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
    device.deviceId = m_deviceIdEdit->text().trimmed();
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
    if (isModbusDevice(device)) {
        switch (column) {
        case ModbusColumnKind:
            binding.extensions.insert(QStringLiteral("modbusKind"), normalizedModbusKind(value));
            break;
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
            binding.extensions.insert(QStringLiteral("modbusDataType"), value);
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

    const QSet<QString> duplicateAddresses = duplicateBindingAddresses(device);
    int emptyAddressCount = 0;
    for (const configtool::PointBinding &binding : device.bindings) {
        if (binding.enabled && binding.address.trimmed().isEmpty()) {
            ++emptyAddressCount;
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
            QStringLiteral("自发标志")
        });
        m_deviceBindingsTable->setRowCount(device.bindings.size());
        m_deviceBindingsTable->setColumnWidth(ModbusColumnEnabled, 56);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnKind, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataRef, 260);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDescription, 180);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnFunCode, 64);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnRegister, 80);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataType, 90);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnScale, 64);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnGroupNo, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnEntryNo, 58);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnDataIndex, 90);
        m_deviceBindingsTable->setColumnWidth(ModbusColumnSelfSignal, 80);

        for (int row = 0; row < device.bindings.size(); ++row) {
            const configtool::PointBinding &binding = device.bindings.at(row);
            auto *enabledItem = new QTableWidgetItem();
            enabledItem->setFlags((enabledItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            enabledItem->setCheckState(binding.enabled ? Qt::Checked : Qt::Unchecked);
            enabledItem->setData(Qt::UserRole, row);

            const QString kind = modbusBindingKind(binding);
            const QString dataType = modbusBindingString(binding, QStringLiteral("modbusDataType"), kind == QStringLiteral("yx") ? QStringLiteral("BIT") : QStringLiteral("WORD"));
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
            kindItem->setData(Qt::UserRole, row);
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
            kindCombo->addItem(QStringLiteral("遥信"), QStringLiteral("yx"));
            kindCombo->addItem(QStringLiteral("遥测"), QStringLiteral("yc"));
            kindCombo->addItem(QStringLiteral("遥控"), QStringLiteral("yk"));
            kindCombo->addItem(QStringLiteral("遥调"), QStringLiteral("yt"));
            const int kindIndex = kindCombo->findData(kind);
            kindCombo->setCurrentIndex(kindIndex >= 0 ? kindIndex : kindCombo->findData(QStringLiteral("yc")));
            kindCombo->setProperty("bindingIndex", row);
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
                device.bindings[bindingIndex].extensions.insert(QStringLiteral("modbusKind"), kindCombo->currentData().toString());
                rebuildModbusDeviceConfig(device);
                refreshDeviceDetail(deviceIndex);
                refreshDeviceEditor(deviceIndex);
                if (bindingIndex < m_deviceBindingsTable->rowCount()) {
                    m_deviceBindingsTable->setCurrentCell(bindingIndex, ModbusColumnKind);
                }
            });
            m_deviceBindingsTable->setCellWidget(row, ModbusColumnKind, kindCombo);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataRef, dataRefItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDescription, descriptionItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnFunCode, funCodeItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnRegister, registerItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataType, dataTypeItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnScale, scaleItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnGroupNo, groupItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnEntryNo, entryItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnDataIndex, dataIndexItem);
            m_deviceBindingsTable->setItem(row, ModbusColumnSelfSignal, selfSignalItem);
        }
        m_updatingDeviceBindingsTable = false;

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
        QStringLiteral("自发标志")
    });
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
