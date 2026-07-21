#include "mainwindow_config_p.h"

using namespace cepb_config_helpers;

namespace {

constexpr int LogicPointDataRefRole = Qt::UserRole + 20;

class LogicPointDisplayResolver
{
public:
    explicit LogicPointDisplayResolver(const configtool::ConfigProject &project)
    {
        QHash<QString, const configtool::ModelTemplate *> models;
        for (const configtool::ModelTemplate &model : project.models) {
            models.insert(model.modelId.trimmed(), &model);
        }

        for (const configtool::ProtocolDeviceInstance &device : project.devices) {
            const QString deviceId = device.deviceId.trimmed();
            QHash<QString, QString> &descriptions = m_descriptionsByDevice[deviceId];
            const configtool::ModelTemplate *model = models.value(device.modelId.trimmed(), nullptr);
            if (model) {
                for (const configtool::ServiceTemplate &service : model->services) {
                    for (const configtool::PointTemplate &point : service.points) {
                        const QString dataRef = point.dataRef().trimmed();
                        if (!dataRef.isEmpty()) {
                            descriptions.insert(dataRef, point.description.trimmed());
                        }
                    }
                }
            }

            for (const configtool::PointBinding &binding : device.bindings) {
                const QString dataRef = binding.dataRef.trimmed();
                if (dataRef.isEmpty()) {
                    continue;
                }
                const QString overrideDescription = binding.descriptionOverride.trimmed();
                if (!overrideDescription.isEmpty() || !descriptions.contains(dataRef)) {
                    descriptions.insert(dataRef, overrideDescription);
                }
            }
        }

        for (const configtool::LogicComputationPoint &point : project.logicCenter.computationPoints) {
            const QString deviceId = point.deviceId.trimmed();
            const QString dataRef = point.dataRef.trimmed();
            if (dataRef.isEmpty()) {
                continue;
            }
            QHash<QString, QString> &descriptions = m_descriptionsByDevice[deviceId];
            if (descriptions.value(dataRef).trimmed().isEmpty()) {
                descriptions.insert(dataRef, point.description.trimmed());
            }
        }
    }

    QString displayText(const QString &deviceId, const QString &dataRef) const
    {
        const QString normalizedDeviceId = deviceId.trimmed();
        const QString normalizedDataRef = dataRef.trimmed();
        const auto deviceIt = m_descriptionsByDevice.constFind(normalizedDeviceId);
        if (deviceIt == m_descriptionsByDevice.constEnd()) {
            return normalizedDataRef;
        }
        const QHash<QString, QString> &descriptions = deviceIt.value();
        const QString description = descriptions.value(normalizedDataRef).trimmed();
        if (description.isEmpty()) {
            return normalizedDataRef;
        }

        QStringList duplicateRefs;
        for (auto it = descriptions.constBegin(); it != descriptions.constEnd(); ++it) {
            if (it.value().trimmed() == description) {
                duplicateRefs.append(it.key());
            }
        }
        if (duplicateRefs.size() <= 1) {
            return description;
        }

        const QString shortRef = normalizedDataRef.section(QLatin1Char('.'), -1);
        int sameShortRefCount = 0;
        for (const QString &duplicateRef : duplicateRefs) {
            if (duplicateRef.section(QLatin1Char('.'), -1) == shortRef) {
                ++sameShortRefCount;
            }
        }
        return sameShortRefCount <= 1
            ? QStringLiteral("%1（%2）").arg(description, shortRef)
            : QStringLiteral("%1（%2）").arg(description, normalizedDataRef);
    }

private:
    QHash<QString, QHash<QString, QString>> m_descriptionsByDevice;
};

QString logicPointToolTip(const QString &deviceId, const QString &dataRef)
{
    return QStringLiteral("点位标识：%1#%2").arg(deviceId.trimmed(), dataRef.trimmed());
}

void updateLogicPointButton(QPushButton *button,
                            const QString &title,
                            const configtool::LogicOperand &point,
                            const LogicPointDisplayResolver &displayResolver)
{
    if (!button) {
        return;
    }
    if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
        button->setText(QStringLiteral("%1\n点击选择").arg(title));
        button->setToolTip({});
        return;
    }
    button->setText(QStringLiteral("%1\n%2\n%3")
                        .arg(title,
                             point.deviceId,
                             displayResolver.displayText(point.deviceId, point.dataRef)));
    button->setToolTip(logicPointToolTip(point.deviceId, point.dataRef));
}

} // namespace

void MainWindow::refreshLogicCenterOverview()
{
}

configtool::AgcAvcGroup *MainWindow::ensureLogicAgcAvcGroup()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (logic.agcAvcGroups.isEmpty()) {
        configtool::AgcAvcGroup group;
        group.groupId = QStringLiteral("default");
        group.virtualDeviceId = QStringLiteral("999");
        logic.agcAvcGroups.append(group);
    }
    return &logic.agcAvcGroups[0];
}

configtool::AgcAvcGroup *MainWindow::currentLogicAgcAvcGroup()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (logic.agcAvcGroups.isEmpty()) {
        return nullptr;
    }
    return &logic.agcAvcGroups[0];
}

void MainWindow::refreshLogicAgcAvcPage()
{
    if (!m_logicAgcAvcPage) {
        return;
    }

    const configtool::AgcAvcGroup defaultGroup;
    const configtool::AgcAvcGroup *group = currentLogicAgcAvcGroup();
    if (!group) {
        group = &defaultGroup;
    }
    m_updatingLogicAgcAvcPage = true;

    m_logicAgcAvcGroupIdEdit->setText(group->groupId);
    m_logicAgcAvcVirtualDeviceIdEdit->setText(group->virtualDeviceId);
    m_logicMeasurementTotalPEdit->setValue(group->measurementScale.totalP);
    m_logicMeasurementTotalQEdit->setValue(group->measurementScale.totalQ);

    m_logicGateEnableReverseCheck->setChecked(group->gateReverse.enable);
    m_logicGateDistantReverseCheck->setChecked(group->gateReverse.distant);
    m_logicGateLockReverseCheck->setChecked(group->gateReverse.lock);
    m_logicGateUplockReverseCheck->setChecked(group->gateReverse.uplock);
    m_logicGateDownlockReverseCheck->setChecked(group->gateReverse.downlock);
    m_logicGateOpenloopReverseCheck->setChecked(group->gateReverse.openloop);

    m_logicAgcFollowEnableCheck->setChecked(group->agcFollow.enable);
    m_logicAgcFollowPeriodEdit->setValue(group->agcFollow.periodMs);
    m_logicAgcFollowStepEdit->setValue(group->agcFollow.step);
    m_logicAgcFollowToleranceEdit->setValue(group->agcFollow.tolerance);
    m_logicAvcFollowEnableCheck->setChecked(group->avcFollow.enable);
    m_logicAvcFollowPeriodEdit->setValue(group->avcFollow.periodMs);
    m_logicAvcFollowStepEdit->setValue(group->avcFollow.step);
    m_logicAvcFollowToleranceEdit->setValue(group->avcFollow.tolerance);

    m_logicAgcAvcDeviceTable->setRowCount(group->devices.size());
    for (int row = 0; row < group->devices.size(); ++row) {
        const configtool::AgcAvcDevice &device = group->devices.at(row);
        const QStringList values = {
            device.deviceId,
            device.ctrlDataRefP,
            device.ctrlDataRefQ,
            device.onlineDeviceId,
            device.onlineDataRef,
            QString::number(device.onlineOkValue),
            doubleToUiText(device.pMin),
            doubleToUiText(device.pMax),
            doubleToUiText(device.qMin),
            doubleToUiText(device.qMax),
            doubleToUiText(device.scaleP),
            doubleToUiText(device.scaleQ)
        };
        for (int column = 0; column < values.size(); ++column) {
            m_logicAgcAvcDeviceTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }

    m_updatingLogicAgcAvcPage = false;
}

void MainWindow::refreshLogicComputationPointPage()
{
    if (!m_logicComputationPointTable) {
        return;
    }

    m_updatingLogicComputationPointPage = true;
    const QList<configtool::LogicComputationPoint> &points =
        m_configProjectManager.project().logicCenter.computationPoints;
    m_logicComputationPointTable->setRowCount(points.size());
    const configtool::ConfigProject &project = m_configProjectManager.project();
    const LogicPointDisplayResolver displayResolver(project);
    for (int row = 0; row < points.size(); ++row) {
        const configtool::LogicComputationPoint &point = points.at(row);
        QStringList operands;
        QStringList operandDataRefs;
        QStringList operandToolTips;
        for (int index = 0; index < point.operands.size(); ++index) {
            const configtool::LogicOperand &operand = point.operands.at(index);
            operands.append(QStringLiteral("{%1} %2")
                                .arg(index + 1)
                                .arg(displayResolver.displayText(operand.deviceId, operand.dataRef)));
            operandDataRefs.append(operand.dataRef);
            operandToolTips.append(QStringLiteral("{%1} %2")
                                       .arg(index + 1)
                                       .arg(logicPointToolTip(operand.deviceId, operand.dataRef)));
        }

        auto *handleItem = new QTableWidgetItem(QStringLiteral("\u22EE"));
        auto *deviceItem = new QTableWidgetItem(point.deviceId);
        auto *dataRefItem = new QTableWidgetItem(
            displayResolver.displayText(point.deviceId, point.dataRef));
        auto *formulaItem = new QTableWidgetItem(point.formula);
        auto *dropItem = new QTableWidgetItem();
        auto *operandsItem = new QTableWidgetItem(operands.join(QStringLiteral("; ")));
        auto *descriptionItem = new QTableWidgetItem(point.description);

        dataRefItem->setData(LogicPointDataRefRole, point.dataRef);
        dataRefItem->setToolTip(logicPointToolTip(point.deviceId, point.dataRef));
        operandsItem->setData(LogicPointDataRefRole, operandDataRefs);
        operandsItem->setToolTip(operandToolTips.join(QLatin1Char('\n')));

        handleItem->setTextAlignment(Qt::AlignCenter);
        handleItem->setToolTip(QStringLiteral("拖动调整顺序"));
        handleItem->setForeground(QColor(QStringLiteral("#9a9a9a")));
        handleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);

        const Qt::ItemFlags readOnlyFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        const Qt::ItemFlags editableFlags = readOnlyFlags | Qt::ItemIsEditable;
        deviceItem->setFlags(readOnlyFlags);
        dataRefItem->setFlags(readOnlyFlags);
        formulaItem->setFlags(editableFlags);
        dropItem->setFlags(readOnlyFlags | Qt::ItemIsUserCheckable);
        dropItem->setCheckState(point.dropOperands ? Qt::Checked : Qt::Unchecked);
        operandsItem->setFlags(readOnlyFlags);
        descriptionItem->setFlags(editableFlags);

        m_logicComputationPointTable->setItem(row, LogicComputationColumnDragHandle, handleItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnOutputDevice, deviceItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnOutputPoint, dataRefItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnFormula, formulaItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnDropOperands, dropItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnOperands, operandsItem);
        m_logicComputationPointTable->setItem(row, LogicComputationColumnDescription, descriptionItem);
    }
    m_updatingLogicComputationPointPage = false;
}

QList<configtool::LogicOperand> MainWindow::collectLogicTemplateOperands(configtool::ModelServiceType preferredType,
                                                                         const QString &title,
                                                                         int minimumCount)
{
    bool ok = false;
    const int count = QInputDialog::getInt(this,
                                           title,
                                           QStringLiteral("源点数量:"),
                                           minimumCount,
                                           minimumCount,
                                           128,
                                           1,
                                           &ok);
    if (!ok) {
        return {};
    }

    QList<configtool::LogicOperand> operands;
    for (int index = 0; index < count; ++index) {
        PointSelectorDialog dialog(this);
        dialog.setProject(&m_configProjectManager.project());
        dialog.setServiceTypeFilter(preferredType);
        dialog.setWindowTitle(QStringLiteral("%1 - 选择第 %2 个源点").arg(title).arg(index + 1));
        if (dialog.exec() != QDialog::Accepted) {
            return {};
        }

        const PointSelectorDialog::SelectedPoint selected = dialog.selectedPoint();
        if (!selected.valid) {
            return {};
        }

        configtool::LogicOperand operand;
        operand.deviceId = selected.deviceId;
        operand.dataRef = selected.dataRef;
        operands.append(operand);
    }

    return operands;
}

void MainWindow::upsertLogicTemplatePoint(const configtool::LogicComputationTemplateRequest &request)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    configtool::LogicComputationPoint point = configtool::buildLogicComputationPointFromTemplate(request);
    if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成计算点"), QStringLiteral("输出设备和输出点不能为空。"));
        return;
    }
    if (point.operands.isEmpty() || point.formula.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("生成计算点"), QStringLiteral("源点不足，无法生成有效公式。"));
        return;
    }

    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicComputationTemplate(int templateIndex)
{
    const QStringList templateNames = {
        QStringLiteral("单点映射/改名"),
        QStringLiteral("原点缩放"),
        QStringLiteral("遥信 OR"),
        QStringLiteral("遥信 AND"),
        QStringLiteral("多点求和"),
        QStringLiteral("总功率因数计算")
    };

    if (templateIndex < 0 || templateIndex >= templateNames.size()) {
        return;
    }
    if (templateIndex == 0) {
        generateLogicSinglePointTemplateVisual();
        return;
    }
    if (templateIndex == 1) {
        generateLogicSourcePointScaleTemplateVisual();
        return;
    }
    if (templateIndex == 2) {
        generateLogicStatusOrTemplateVisual();
        return;
    }
    if (templateIndex == 3) {
        generateLogicStatusAndTemplateVisual();
        return;
    }
    if (templateIndex == 4) {
        generateLogicMultiPointSumTemplateVisual();
        return;
    }
    if (templateIndex == 5) {
        generateLogicPowerFactorTemplateVisual();
        return;
    }
}

void MainWindow::generateLogicSinglePointTemplateVisual()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("单点映射/改名 模板"));
    dialog.resize(900, 360);

    configtool::LogicOperand inputPoint;
    configtool::LogicOperand outputPoint;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    auto selectMeasurementPoint = [this](const QString &title, configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Measurement);
        selector.setWindowTitle(title);
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("输入点乘以系数，映射为输出点"), &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(14);

    auto *inputBtn = new QPushButton(&dialog);
    inputBtn->setMinimumSize(220, 110);
    inputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(inputBtn, 1);

    auto *middlePanel = new QWidget(&dialog);
    auto *middleLayout = new QVBoxLayout(middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(8);
    auto *multiplyLabel = new QLabel(QStringLiteral("x"), &dialog);
    multiplyLabel->setAlignment(Qt::AlignCenter);
    QFont opFont = multiplyLabel->font();
    opFont.setBold(true);
    opFont.setPointSize(18);
    multiplyLabel->setFont(opFont);
    auto *coefficientEdit = new QDoubleSpinBox(&dialog);
    coefficientEdit->setDecimals(6);
    coefficientEdit->setRange(-1000000000.0, 1000000000.0);
    coefficientEdit->setValue(1.0);
    coefficientEdit->setSingleStep(0.1);
    coefficientEdit->setPrefix(QStringLiteral("系数 "));
    coefficientEdit->setMinimumWidth(160);
    auto *mapLabel = new QLabel(QStringLiteral("映射到"), &dialog);
    mapLabel->setAlignment(Qt::AlignCenter);
    middleLayout->addWidget(multiplyLabel);
    middleLayout->addWidget(coefficientEdit);
    middleLayout->addWidget(mapLabel);
    sceneLayout->addWidget(middlePanel);

    auto *outputBtn = new QPushButton(&dialog);
    outputBtn->setMinimumSize(220, 110);
    outputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(outputBtn, 1);
    mainLayout->addLayout(sceneLayout, 1);

    auto updateInput = [&]() {
        updateLogicPointButton(inputBtn, QStringLiteral("输入点"), inputPoint, displayResolver);
    };
    auto updateOutput = [&]() {
        updateLogicPointButton(outputBtn, QStringLiteral("输出点"), outputPoint, displayResolver);
    };
    connect(inputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(QStringLiteral("选择输入点"), inputPoint)) {
            updateInput();
        }
    });
    connect(outputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(QStringLiteral("选择输出点"), outputPoint)) {
            updateOutput();
        }
    });
    updateInput();
    updateOutput();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (inputPoint.deviceId.trimmed().isEmpty() || inputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("单点映射/改名"), QStringLiteral("请先选择输入点。"));
            return;
        }
        if (outputPoint.deviceId.trimmed().isEmpty() || outputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("单点映射/改名"), QStringLiteral("请先选择输出点。"));
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationPoint point;
    point.deviceId = outputPoint.deviceId;
    point.dataRef = outputPoint.dataRef;
    point.dropOperands = true;
    point.description = QStringLiteral("可视化模板生成: 单点映射/改名");
    point.operands.append(inputPoint);
    const double coefficient = coefficientEdit->value();
    point.formula = qFuzzyCompare(coefficient + 1.0, 2.0)
        ? QStringLiteral("{1}")
        : QStringLiteral("{1} * %1").arg(doubleToUiText(coefficient));

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicSourcePointScaleTemplateVisual()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("原点缩放 模板"));
    dialog.resize(620, 300);

    configtool::LogicOperand pointRef;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    auto selectMeasurementPoint = [this](configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Measurement);
        selector.setWindowTitle(QStringLiteral("选择原点"));
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("原点值乘以系数后，仍写回同一个点"), &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(14);

    auto *pointBtn = new QPushButton(&dialog);
    pointBtn->setMinimumSize(220, 110);
    pointBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(pointBtn, 1);

    auto *middlePanel = new QWidget(&dialog);
    auto *middleLayout = new QVBoxLayout(middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(8);
    auto *multiplyLabel = new QLabel(QStringLiteral("x"), &dialog);
    multiplyLabel->setAlignment(Qt::AlignCenter);
    QFont opFont = multiplyLabel->font();
    opFont.setBold(true);
    opFont.setPointSize(18);
    multiplyLabel->setFont(opFont);
    auto *coefficientEdit = new QDoubleSpinBox(&dialog);
    coefficientEdit->setDecimals(6);
    coefficientEdit->setRange(-1000000000.0, 1000000000.0);
    coefficientEdit->setValue(1.0);
    coefficientEdit->setSingleStep(0.1);
    coefficientEdit->setPrefix(QStringLiteral("系数 "));
    coefficientEdit->setMinimumWidth(160);
    auto *equalsLabel = new QLabel(QStringLiteral("=> 同一点"), &dialog);
    equalsLabel->setAlignment(Qt::AlignCenter);
    middleLayout->addWidget(multiplyLabel);
    middleLayout->addWidget(coefficientEdit);
    middleLayout->addWidget(equalsLabel);
    sceneLayout->addWidget(middlePanel);
    mainLayout->addLayout(sceneLayout, 1);

    auto updatePoint = [&]() {
        updateLogicPointButton(pointBtn, QStringLiteral("原点"), pointRef, displayResolver);
    };
    connect(pointBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(pointRef)) {
            updatePoint();
        }
    });
    updatePoint();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (pointRef.deviceId.trimmed().isEmpty() || pointRef.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("原点缩放"), QStringLiteral("请先选择原点。"));
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationPoint point;
    point.deviceId = pointRef.deviceId;
    point.dataRef = pointRef.dataRef;
    point.dropOperands = true;
    point.description = QStringLiteral("可视化模板生成: 原点缩放");
    point.operands.append(pointRef);
    point.formula = QStringLiteral("{1} * %1").arg(doubleToUiText(coefficientEdit->value()));

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicStatusOrTemplateVisual()
{
    generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType::StatusOr,
                                      QStringLiteral("遥信 OR"),
                                      QStringLiteral("OR"),
                                      QStringLiteral("任一输入点为 1 时，输出点为 1"),
                                      QStringLiteral("StatusOr"));
}

void MainWindow::generateLogicStatusAndTemplateVisual()
{
    generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType::StatusAnd,
                                      QStringLiteral("遥信 AND"),
                                      QStringLiteral("AND"),
                                      QStringLiteral("所有输入点都为 1 时，输出点为 1"),
                                      QStringLiteral("StatusAnd"));
}

void MainWindow::generateLogicMultiPointSumTemplateVisual()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("多点求和 模板"));
    dialog.resize(1080, 420);

    QList<configtool::LogicOperand> inputPoints;
    for (int i = 0; i < 3; ++i) {
        inputPoints.append(configtool::LogicOperand());
    }
    configtool::LogicOperand outputPoint;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    auto selectMeasurementPoint = [this](const QString &title, configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Measurement);
        selector.setWindowTitle(title);
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("多个遥测输入点相加后，写入输出点"), &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *topLayout = new QHBoxLayout();
    auto *addInputBtn = new QPushButton(QStringLiteral("+"), &dialog);
    auto *removeInputBtn = new QPushButton(QStringLiteral("-"), &dialog);
    addInputBtn->setFixedSize(52, 40);
    removeInputBtn->setFixedSize(52, 40);
    topLayout->addWidget(addInputBtn);
    topLayout->addWidget(removeInputBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(10);

    auto *inputHost = new QWidget(&dialog);
    auto *inputLayout = new QGridLayout(inputHost);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);

    auto *inputScroll = new QScrollArea(&dialog);
    inputScroll->setWidgetResizable(true);
    inputScroll->setMinimumHeight(120);
    inputScroll->setWidget(inputHost);
    sceneLayout->addWidget(inputScroll, 1);

    auto *arrowLabel = new QLabel(QStringLiteral("=>"), &dialog);
    arrowLabel->setAlignment(Qt::AlignCenter);
    QFont arrowFont = arrowLabel->font();
    arrowFont.setBold(true);
    arrowFont.setPointSize(14);
    arrowLabel->setFont(arrowFont);
    sceneLayout->addWidget(arrowLabel);

    auto *outputBtn = new QPushButton(&dialog);
    outputBtn->setMinimumSize(190, 96);
    outputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(outputBtn);
    mainLayout->addLayout(sceneLayout, 1);

    std::function<void()> rebuildInputs;
    auto updateOutput = [&]() {
        updateLogicPointButton(outputBtn, QStringLiteral("输出点"), outputPoint, displayResolver);
    };

    rebuildInputs = [&]() {
        while (QLayoutItem *child = inputLayout->takeAt(0)) {
            if (QWidget *widget = child->widget()) {
                widget->deleteLater();
            }
            delete child;
        }

        constexpr int maxColumnsPerRow = 4;
        for (int index = 0; index < inputPoints.size(); ++index) {
            const int row = index / maxColumnsPerRow;
            const int columnInRow = index % maxColumnsPerRow;
            if (columnInRow > 0) {
                auto *plusLabel = new QLabel(QStringLiteral("+"), &dialog);
                plusLabel->setAlignment(Qt::AlignCenter);
                QFont plusFont = plusLabel->font();
                plusFont.setBold(true);
                plusFont.setPointSize(16);
                plusLabel->setFont(plusFont);
                plusLabel->setMinimumWidth(28);
                inputLayout->addWidget(plusLabel, row, columnInRow * 2 - 1);
            }
            auto *button = new QPushButton(&dialog);
            updateLogicPointButton(button,
                                   QStringLiteral("输入点 %1").arg(index + 1),
                                   inputPoints.at(index),
                                   displayResolver);
            button->setMinimumSize(150, 90);
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            inputLayout->addWidget(button, row, columnInRow * 2);
            connect(button, &QPushButton::clicked, &dialog, [&, index]() {
                if (selectMeasurementPoint(QStringLiteral("选择输入点 %1").arg(index + 1), inputPoints[index])) {
                    rebuildInputs();
                }
            });
        }
        inputLayout->setColumnStretch(maxColumnsPerRow * 2, 1);
        removeInputBtn->setEnabled(inputPoints.size() > 1);
    };

    connect(addInputBtn, &QPushButton::clicked, &dialog, [&]() {
        inputPoints.append(configtool::LogicOperand());
        rebuildInputs();
    });
    connect(removeInputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (inputPoints.size() <= 1) {
            return;
        }
        inputPoints.removeLast();
        rebuildInputs();
    });
    connect(outputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(QStringLiteral("选择输出点"), outputPoint)) {
            updateOutput();
        }
    });

    updateOutput();
    rebuildInputs();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (outputPoint.deviceId.trimmed().isEmpty() || outputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("多点求和"), QStringLiteral("请先选择输出点。"));
            return;
        }
        for (int index = 0; index < inputPoints.size(); ++index) {
            const configtool::LogicOperand &point = inputPoints.at(index);
            if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
                QMessageBox::information(&dialog,
                                         QStringLiteral("多点求和"),
                                         QStringLiteral("请先选择输入点 %1。").arg(index + 1));
                return;
            }
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationTemplateRequest request;
    request.type = configtool::LogicComputationTemplateType::Sum;
    request.outputDeviceId = outputPoint.deviceId;
    request.outputDataRef = outputPoint.dataRef;
    request.operands = inputPoints;
    request.dropOperands = true;
    request.description = QStringLiteral("可视化模板生成: 多点求和");
    upsertLogicTemplatePoint(request);
}

void MainWindow::generateLogicPowerFactorTemplateVisual()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("总功率因数计算 模板"));
    dialog.resize(1160, 620);

    QList<configtool::LogicOperand> pPoints;
    QList<configtool::LogicOperand> qPoints;
    for (int i = 0; i < 3; ++i) {
        pPoints.append(configtool::LogicOperand());
        qPoints.append(configtool::LogicOperand());
    }
    configtool::LogicOperand outputPoint;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    auto selectMeasurementPoint = [this](const QString &title, configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Measurement);
        selector.setWindowTitle(title);
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *formulaLabel = new QLabel(
        QStringLiteral("<div style='font-size:18pt; font-weight:600;'>"
                       "P = P<sub>1</sub> + P<sub>2</sub> + ...，"
                       "Q = Q<sub>1</sub> + Q<sub>2</sub> + ...</div>"
                       "<table align='center' cellspacing='0' cellpadding='2' style='font-size:22pt; margin-top:8px;'>"
                       "<tr>"
                       "<td rowspan='2'>cos &phi; = </td>"
                       "<td align='center' style='border-bottom:1px solid;'>P</td>"
                       "</tr>"
                       "<tr>"
                       "<td align='center'>&radic;(P<sup>2</sup> + Q<sup>2</sup>)</td>"
                       "</tr>"
                       "</table>"),
        &dialog);
    formulaLabel->setTextFormat(Qt::RichText);
    formulaLabel->setAlignment(Qt::AlignCenter);
    formulaLabel->setMinimumHeight(94);
    mainLayout->addWidget(formulaLabel);

    auto *selectionLayout = new QHBoxLayout();
    selectionLayout->setSpacing(10);

    auto createPointGroup = [&](const QString &title,
                                const QString &pointPrefix,
                                QList<configtool::LogicOperand> &points) {
        auto *pointsRef = &points;
        auto *group = new QGroupBox(title, &dialog);
        auto *groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(10, 8, 10, 10);
        groupLayout->setSpacing(8);

        auto *toolbar = new QHBoxLayout();
        auto *addBtn = new QPushButton(QStringLiteral("+"), group);
        auto *removeBtn = new QPushButton(QStringLiteral("-"), group);
        addBtn->setFixedSize(46, 34);
        removeBtn->setFixedSize(46, 34);
        toolbar->addWidget(addBtn);
        toolbar->addWidget(removeBtn);
        toolbar->addStretch();
        groupLayout->addLayout(toolbar);

        auto *host = new QWidget(group);
        auto *rowLayout = new QGridLayout(host);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        auto *scroll = new QScrollArea(group);
        scroll->setWidgetResizable(true);
        scroll->setMinimumHeight(130);
        scroll->setWidget(host);
        groupLayout->addWidget(scroll);

        auto rebuild = std::make_shared<std::function<void()>>();
        *rebuild = [&, rowLayout, removeBtn, pointPrefix, title, pointsRef, rebuild]() {
            while (QLayoutItem *child = rowLayout->takeAt(0)) {
                if (QWidget *widget = child->widget()) {
                    widget->deleteLater();
                }
                delete child;
            }

            constexpr int maxColumnsPerRow = 2;
            for (int index = 0; index < pointsRef->size(); ++index) {
                const int row = index / maxColumnsPerRow;
                const int columnInRow = index % maxColumnsPerRow;
                if (columnInRow > 0) {
                    auto *plusLabel = new QLabel(QStringLiteral("+"), &dialog);
                    plusLabel->setAlignment(Qt::AlignCenter);
                    QFont plusFont = plusLabel->font();
                    plusFont.setBold(true);
                    plusFont.setPointSize(16);
                    plusLabel->setFont(plusFont);
                    plusLabel->setMinimumWidth(28);
                    rowLayout->addWidget(plusLabel, row, columnInRow * 2 - 1);
                }
                auto *button = new QPushButton(&dialog);
                updateLogicPointButton(button,
                                       QStringLiteral("%1%2").arg(pointPrefix).arg(index + 1),
                                       pointsRef->at(index),
                                       displayResolver);
                button->setMinimumSize(150, 90);
                button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                rowLayout->addWidget(button, row, columnInRow * 2);
                connect(button, &QPushButton::clicked, &dialog, [&, index, title, pointPrefix, rebuild, pointsRef]() {
                    if (selectMeasurementPoint(QStringLiteral("%1 - 选择 %2%3")
                                                   .arg(title, pointPrefix)
                                                   .arg(index + 1),
                                               (*pointsRef)[index])) {
                        (*rebuild)();
                    }
                });
            }
            rowLayout->setColumnStretch(maxColumnsPerRow * 2, 1);
            removeBtn->setEnabled(pointsRef->size() > 1);
        };

        connect(addBtn, &QPushButton::clicked, &dialog, [&, rebuild, pointsRef]() {
            pointsRef->append(configtool::LogicOperand());
            (*rebuild)();
        });
        connect(removeBtn, &QPushButton::clicked, &dialog, [&, rebuild, pointsRef]() {
            if (pointsRef->size() <= 1) {
                return;
            }
            pointsRef->removeLast();
            (*rebuild)();
        });
        (*rebuild)();
        return group;
    };

    selectionLayout->addWidget(createPointGroup(QStringLiteral("P 点位求和"), QStringLiteral("P"), pPoints), 1);
    selectionLayout->addWidget(createPointGroup(QStringLiteral("Q 点位求和"), QStringLiteral("Q"), qPoints), 1);

    auto *outputPanel = new QWidget(&dialog);
    auto *outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(8);
    auto *outputTitle = new QLabel(QStringLiteral("输出"), &dialog);
    outputTitle->setAlignment(Qt::AlignCenter);
    QFont outputTitleFont = outputTitle->font();
    outputTitleFont.setBold(true);
    outputTitle->setFont(outputTitleFont);
    auto *outputBtn = new QPushButton(&dialog);
    outputBtn->setMinimumSize(190, 96);
    outputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outputLayout->addWidget(outputTitle);
    outputLayout->addWidget(outputBtn);
    outputLayout->addStretch();
    selectionLayout->addWidget(outputPanel);
    mainLayout->addLayout(selectionLayout, 1);

    auto updateOutput = [&]() {
        updateLogicPointButton(outputBtn,
                               QStringLiteral("功率因数输出点"),
                               outputPoint,
                               displayResolver);
    };
    connect(outputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectMeasurementPoint(QStringLiteral("选择功率因数输出点"), outputPoint)) {
            updateOutput();
        }
    });
    updateOutput();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (outputPoint.deviceId.trimmed().isEmpty() || outputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("总功率因数计算"), QStringLiteral("请先选择输出点。"));
            return;
        }

        auto validatePoints = [&](const QList<configtool::LogicOperand> &points, const QString &name) {
            for (int index = 0; index < points.size(); ++index) {
                const configtool::LogicOperand &point = points.at(index);
                if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
                    QMessageBox::information(&dialog,
                                             QStringLiteral("总功率因数计算"),
                                             QStringLiteral("请先选择 %1%2。").arg(name).arg(index + 1));
                    return false;
                }
            }
            return true;
        };

        if (!validatePoints(pPoints, QStringLiteral("P")) || !validatePoints(qPoints, QStringLiteral("Q"))) {
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto sumFormula = [](int startIndex, int count) {
        QStringList parts;
        for (int i = 0; i < count; ++i) {
            parts.append(QStringLiteral("{%1}").arg(startIndex + i + 1));
        }
        return parts.size() == 1
            ? parts.first()
            : QStringLiteral("(%1)").arg(parts.join(QStringLiteral("+")));
    };

    configtool::LogicComputationPoint point;
    point.deviceId = outputPoint.deviceId;
    point.dataRef = outputPoint.dataRef;
    point.dropOperands = true;
    point.description = QStringLiteral("可视化模板生成: 总功率因数计算");
    point.operands = pPoints;
    point.operands.append(qPoints);
    const QString pFormula = sumFormula(0, pPoints.size());
    const QString qFormula = sumFormula(pPoints.size(), qPoints.size());
    point.formula = QStringLiteral("%1/sqrt(sqr(%1)+sqr(%2))").arg(pFormula, qFormula);

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();
    const bool inserted = configtool::upsertLogicComputationPoint(logic, point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    statusBar()->showMessage(inserted
        ? QStringLiteral("已生成计算点 %1#%2").arg(point.deviceId, point.dataRef)
        : QStringLiteral("已更新计算点 %1#%2").arg(point.deviceId, point.dataRef),
        5000);
}

void MainWindow::generateLogicStatusTemplateVisual(configtool::LogicComputationTemplateType type,
                                                   const QString &templateName,
                                                   const QString &operatorText,
                                                   const QString &titleText,
                                                   const QString &defaultOutputDataRef)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("%1 模板").arg(templateName));
    dialog.resize(1080, 420);

    QList<configtool::LogicOperand> inputPoints;
    for (int i = 0; i < 3; ++i) {
        inputPoints.append(configtool::LogicOperand());
    }
    configtool::LogicOperand outputPoint;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    auto selectStatusPoint = [this](const QString &title, configtool::LogicOperand &point) {
        PointSelectorDialog selector(this);
        selector.setProject(&m_configProjectManager.project());
        selector.setServiceTypeFilter(configtool::ModelServiceType::Status);
        selector.setWindowTitle(title);
        if (selector.exec() != QDialog::Accepted) {
            return false;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return false;
        }

        point.deviceId = selected.deviceId;
        point.dataRef = selected.dataRef;
        return true;
    };

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(titleText, &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *topLayout = new QHBoxLayout();
    auto *addInputBtn = new QPushButton(QStringLiteral("+"), &dialog);
    auto *removeInputBtn = new QPushButton(QStringLiteral("-"), &dialog);
    addInputBtn->setFixedSize(52, 40);
    removeInputBtn->setFixedSize(52, 40);
    topLayout->addWidget(addInputBtn);
    topLayout->addWidget(removeInputBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    auto *sceneLayout = new QHBoxLayout();
    sceneLayout->setSpacing(10);

    auto *inputHost = new QWidget(&dialog);
    auto *inputLayout = new QGridLayout(inputHost);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);

    auto *inputScroll = new QScrollArea(&dialog);
    inputScroll->setWidgetResizable(true);
    inputScroll->setMinimumHeight(120);
    inputScroll->setWidget(inputHost);
    sceneLayout->addWidget(inputScroll, 1);

    auto *arrowLabel = new QLabel(QStringLiteral("=>"), &dialog);
    arrowLabel->setAlignment(Qt::AlignCenter);
    QFont arrowFont = arrowLabel->font();
    arrowFont.setBold(true);
    arrowFont.setPointSize(14);
    arrowLabel->setFont(arrowFont);
    sceneLayout->addWidget(arrowLabel);

    auto *outputBtn = new QPushButton(&dialog);
    outputBtn->setMinimumSize(190, 96);
    outputBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sceneLayout->addWidget(outputBtn);
    mainLayout->addLayout(sceneLayout, 1);

    std::function<void()> rebuildInputs;
    auto updateOutput = [&]() {
        updateLogicPointButton(outputBtn, QStringLiteral("输出点"), outputPoint, displayResolver);
    };

    rebuildInputs = [&]() {
        while (QLayoutItem *child = inputLayout->takeAt(0)) {
            if (QWidget *widget = child->widget()) {
                widget->deleteLater();
            }
            delete child;
        }

        constexpr int maxColumnsPerRow = 4;
        for (int index = 0; index < inputPoints.size(); ++index) {
            const int row = index / maxColumnsPerRow;
            const int columnInRow = index % maxColumnsPerRow;
            if (columnInRow > 0) {
                auto *orLabel = new QLabel(operatorText, &dialog);
                orLabel->setAlignment(Qt::AlignCenter);
                QFont orFont = orLabel->font();
                orFont.setBold(true);
                orFont.setPointSize(12);
                orLabel->setFont(orFont);
                orLabel->setMinimumWidth(28);
                inputLayout->addWidget(orLabel, row, columnInRow * 2 - 1);
            }
            auto *button = new QPushButton(&dialog);
            updateLogicPointButton(button,
                                   QStringLiteral("输入点 %1").arg(index + 1),
                                   inputPoints.at(index),
                                   displayResolver);
            button->setMinimumSize(150, 90);
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            inputLayout->addWidget(button, row, columnInRow * 2);
            connect(button, &QPushButton::clicked, &dialog, [&, index]() {
                if (selectStatusPoint(QStringLiteral("选择输入点 %1").arg(index + 1), inputPoints[index])) {
                    rebuildInputs();
                }
            });
        }
        inputLayout->setColumnStretch(maxColumnsPerRow * 2, 1);
        removeInputBtn->setEnabled(inputPoints.size() > 1);
    };

    connect(addInputBtn, &QPushButton::clicked, &dialog, [&]() {
        inputPoints.append(configtool::LogicOperand());
        rebuildInputs();
    });
    connect(removeInputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (inputPoints.size() <= 1) {
            return;
        }
        inputPoints.removeLast();
        rebuildInputs();
    });
    connect(outputBtn, &QPushButton::clicked, &dialog, [&]() {
        if (selectStatusPoint(QStringLiteral("选择输出点"), outputPoint)) {
            updateOutput();
        }
    });

    updateOutput();
    rebuildInputs();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (outputPoint.deviceId.trimmed().isEmpty() || outputPoint.dataRef.trimmed().isEmpty()) {
            QMessageBox::information(&dialog, templateName, QStringLiteral("请先选择输出点。"));
            return;
        }
        for (int index = 0; index < inputPoints.size(); ++index) {
            const configtool::LogicOperand &point = inputPoints.at(index);
            if (point.deviceId.trimmed().isEmpty() || point.dataRef.trimmed().isEmpty()) {
                QMessageBox::information(&dialog,
                                         templateName,
                                         QStringLiteral("请先选择输入点 %1。").arg(index + 1));
                return;
            }
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicComputationTemplateRequest request;
    request.type = type;
    request.outputDeviceId = outputPoint.deviceId;
    request.outputDataRef = outputPoint.dataRef;
    request.operands = inputPoints;
    request.dropOperands = true;
    request.description = QStringLiteral("可视化模板生成: %1").arg(templateName);
    upsertLogicTemplatePoint(request);
}

bool MainWindow::editLogicComputationOperands(const QString &title,
                                              const QString &formula,
                                              QList<configtool::LogicOperand> &operands)
{
    const int operandCount = formulaOperandCount(formula);
    if (operandCount <= 0) {
        QMessageBox::information(this,
                                 title,
                                 QStringLiteral("公式中没有 {1} 这类源点占位符，不需要配置源点。"));
        operands.clear();
        return true;
    }

    while (operands.size() < operandCount) {
        operands.append(configtool::LogicOperand());
    }
    while (operands.size() > operandCount) {
        operands.removeLast();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(820, 420);

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *formulaLabel = new QLabel(QStringLiteral("公式: %1").arg(formula.trimmed()), &dialog);
    formulaLabel->setWordWrap(true);
    mainLayout->addWidget(formulaLabel);

    auto *hintLabel = new QLabel(QStringLiteral("按公式中的 {1}、{2} 顺序配置源点。"), &dialog);
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    auto *table = new QTableWidget(operandCount, 3, &dialog);
    table->setHorizontalHeaderLabels({
        QStringLiteral("占位符"),
        QStringLiteral("源设备"),
        QStringLiteral("源点")
    });
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 90);
    table->setColumnWidth(1, 160);
    mainLayout->addWidget(table, 1);
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    auto refreshRow = [&](int row) {
        const configtool::LogicOperand &operand = operands.at(row);
        auto *placeholderItem = new QTableWidgetItem(QStringLiteral("{%1}").arg(row + 1));
        auto *deviceItem = new QTableWidgetItem(operand.deviceId);
        auto *dataRefItem = new QTableWidgetItem(
            displayResolver.displayText(operand.deviceId, operand.dataRef));
        dataRefItem->setData(LogicPointDataRefRole, operand.dataRef);
        if (!operand.deviceId.trimmed().isEmpty() && !operand.dataRef.trimmed().isEmpty()) {
            dataRefItem->setToolTip(logicPointToolTip(operand.deviceId, operand.dataRef));
        }
        placeholderItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        deviceItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        dataRefItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        table->setItem(row, 0, placeholderItem);
        table->setItem(row, 1, deviceItem);
        table->setItem(row, 2, dataRefItem);
    };

    for (int row = 0; row < operandCount; ++row) {
        refreshRow(row);
    }

    auto selectOperand = [&](int row) {
        if (row < 0 || row >= operands.size()) {
            return;
        }

        PointSelectorDialog selector(&dialog);
        selector.setProject(&m_configProjectManager.project());
        selector.setWindowTitle(QStringLiteral("选择 %1").arg(table->item(row, 0)->text()));
        if (selector.exec() != QDialog::Accepted) {
            return;
        }

        const PointSelectorDialog::SelectedPoint selected = selector.selectedPoint();
        if (!selected.valid) {
            return;
        }

        operands[row].deviceId = selected.deviceId;
        operands[row].dataRef = selected.dataRef;
        refreshRow(row);
        table->selectRow(row);
    };

    auto *toolbar = new QHBoxLayout();
    auto *selectBtn = new QPushButton(QStringLiteral("选择源点"), &dialog);
    auto *clearBtn = new QPushButton(QStringLiteral("清空本行"), &dialog);
    toolbar->addWidget(selectBtn);
    toolbar->addWidget(clearBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    connect(selectBtn, &QPushButton::clicked, &dialog, [&]() {
        selectOperand(table->currentRow());
    });
    connect(clearBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = table->currentRow();
        if (row < 0 || row >= operands.size()) {
            return;
        }

        operands[row] = configtool::LogicOperand();
        refreshRow(row);
        table->selectRow(row);
    });
    connect(table, &QTableWidget::cellDoubleClicked, &dialog, [&](int row, int) {
        selectOperand(row);
    });
    if (operandCount > 0) {
        table->selectRow(0);
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        for (int row = 0; row < operands.size(); ++row) {
            const configtool::LogicOperand &operand = operands.at(row);
            if (operand.deviceId.trimmed().isEmpty() || operand.dataRef.trimmed().isEmpty()) {
                QMessageBox::information(&dialog,
                                         title,
                                         QStringLiteral("请先配置 %1 的源点。").arg(table->item(row, 0)->text()));
                table->selectRow(row);
                return;
            }
        }
        dialog.accept();
    });

    return dialog.exec() == QDialog::Accepted;
}

void MainWindow::onAddLogicComputationPointClicked()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    PointSelectorDialog outputSelector(this);
    outputSelector.setProject(&m_configProjectManager.project());
    outputSelector.setWindowTitle(QStringLiteral("选择计算点输出点"));
    if (outputSelector.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint output = outputSelector.selectedPoint();
    if (!output.valid) {
        return;
    }
    for (const configtool::LogicComputationPoint &existing : logic.computationPoints) {
        if (existing.deviceId.trimmed() == output.deviceId.trimmed()
            && existing.dataRef.trimmed() == output.dataRef.trimmed()) {
            QMessageBox::warning(this,
                                 QStringLiteral("新增计算点"),
                                 QStringLiteral("计算点 %1#%2 已存在，请选择其他输出点或直接编辑现有行。")
                                     .arg(output.deviceId, output.dataRef));
            return;
        }
    }

    bool ok = false;
    const QString formula = QInputDialog::getText(this,
                                                  QStringLiteral("新增计算点"),
                                                  QStringLiteral("公式:"),
                                                  QLineEdit::Normal,
                                                  QStringLiteral("{1}"),
                                                  &ok).trimmed();
    if (!ok) {
        return;
    }
    if (formula.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("新增计算点"), QStringLiteral("公式不能为空。"));
        return;
    }
    if (formulaOperandCount(formula) <= 0) {
        QMessageBox::warning(this,
                             QStringLiteral("新增计算点"),
                             QStringLiteral("公式至少需要包含一个 {1} 这类源点占位符。"));
        return;
    }

    configtool::LogicComputationPoint point;
    point.deviceId = output.deviceId;
    point.dataRef = output.dataRef;
    point.formula = formula;
    point.dropOperands = true;
    point.description = QStringLiteral("手工新增计算点");
    if (!editLogicComputationOperands(QStringLiteral("配置计算点源点"), point.formula, point.operands)) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.computationPoints.append(point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    const int row = logic.computationPoints.size() - 1;
    if (row >= 0) {
        m_logicComputationPointTable->selectRow(row);
    }
    statusBar()->showMessage(QStringLiteral("已新增计算点 %1#%2").arg(point.deviceId, point.dataRef), 5000);
}

void MainWindow::onCopyLogicComputationPointClicked()
{
    if (!m_logicComputationPointTable) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    const int row = m_logicComputationPointTable->currentRow();
    if (row < 0 || row >= logic.computationPoints.size()) {
        QMessageBox::information(this, QStringLiteral("复制计算点"), QStringLiteral("请先选择要复制的计算点。"));
        return;
    }

    configtool::LogicComputationPoint point = logic.computationPoints.at(row);
    PointSelectorDialog outputSelector(this);
    outputSelector.setProject(&m_configProjectManager.project());
    outputSelector.setWindowTitle(QStringLiteral("选择复制后的输出点"));
    if (outputSelector.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint output = outputSelector.selectedPoint();
    if (!output.valid) {
        return;
    }
    for (int index = 0; index < logic.computationPoints.size(); ++index) {
        const configtool::LogicComputationPoint &existing = logic.computationPoints.at(index);
        if (existing.deviceId.trimmed() == output.deviceId.trimmed()
            && existing.dataRef.trimmed() == output.dataRef.trimmed()) {
            QMessageBox::warning(this,
                                 QStringLiteral("复制计算点"),
                                 QStringLiteral("计算点 %1#%2 已存在，请选择其他输出点。")
                                     .arg(output.deviceId, output.dataRef));
            return;
        }
    }

    point.deviceId = output.deviceId;
    point.dataRef = output.dataRef;
    point.description = point.description.trimmed().isEmpty()
        ? QStringLiteral("复制计算点")
        : QStringLiteral("%1 (复制)").arg(point.description);

    pushConfigUndoSnapshot();
    logic.computationPoints.append(point);
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    const int newRow = logic.computationPoints.size() - 1;
    if (newRow >= 0) {
        m_logicComputationPointTable->selectRow(newRow);
    }
    statusBar()->showMessage(QStringLiteral("已复制计算点 %1#%2").arg(point.deviceId, point.dataRef), 5000);
}

void MainWindow::onDeleteLogicComputationPointClicked()
{
    if (!m_logicComputationPointTable) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    QSet<int> selectedRows;
    const QModelIndexList indexes = m_logicComputationPointTable->selectionModel()
        ? m_logicComputationPointTable->selectionModel()->selectedIndexes()
        : QModelIndexList();
    for (const QModelIndex &index : indexes) {
        if (index.row() >= 0 && index.row() < logic.computationPoints.size()) {
            selectedRows.insert(index.row());
        }
    }
    const int currentRow = m_logicComputationPointTable->currentRow();
    if (selectedRows.isEmpty() && currentRow >= 0 && currentRow < logic.computationPoints.size()) {
        selectedRows.insert(currentRow);
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("删除计算点"), QStringLiteral("请先选择要删除的计算点。"));
        return;
    }

    QList<int> rows = selectedRows.values();
    std::sort(rows.begin(), rows.end());
    const int firstRow = rows.first();
    QString objectId;
    if (rows.size() == 1) {
        const configtool::LogicComputationPoint point = logic.computationPoints.at(firstRow);
        objectId = QStringLiteral("%1#%2").arg(point.deviceId, point.dataRef);
    }

    if (QMessageBox::question(this,
                              QStringLiteral("删除计算点"),
                              rows.size() == 1
                                  ? QStringLiteral("确定删除计算点 %1 吗？").arg(objectId)
                                  : QStringLiteral("确定删除选中的 %1 个计算点吗？").arg(rows.size()),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    pushConfigUndoSnapshot();
    std::sort(rows.begin(), rows.end(), [](int left, int right) {
        return left > right;
    });
    for (int row : rows) {
        if (row >= 0 && row < logic.computationPoints.size()) {
            logic.computationPoints.removeAt(row);
        }
    }
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();

    if (!logic.computationPoints.isEmpty()) {
        m_logicComputationPointTable->selectRow(qMin(firstRow, logic.computationPoints.size() - 1));
    }

    statusBar()->showMessage(rows.size() == 1
                                 ? QStringLiteral("已删除计算点 %1").arg(objectId)
                                 : QStringLiteral("已删除 %1 个计算点").arg(rows.size()),
                             5000);
}

void MainWindow::onLogicComputationPointItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicComputationPointPage || m_restoringConfigUndo || !item) {
        return;
    }

    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    const int row = item->row();
    if (row < 0 || row >= logic.computationPoints.size()) {
        return;
    }

    configtool::LogicComputationPoint &point = logic.computationPoints[row];
    const int column = item->column();
    bool refreshTable = false;

    pushConfigUndoSnapshot();
    if (column == LogicComputationColumnFormula) {
        point.formula = item->text().trimmed();
        const int operandCount = formulaOperandCount(point.formula);
        while (point.operands.size() < operandCount) {
            point.operands.append(configtool::LogicOperand());
        }
        while (point.operands.size() > operandCount) {
            point.operands.removeLast();
        }
        refreshTable = true;
    } else if (column == LogicComputationColumnDropOperands) {
        point.dropOperands = item->checkState() == Qt::Checked;
    } else if (column == LogicComputationColumnDescription) {
        point.description = item->text();
    } else {
        return;
    }

    refreshLogicCenterOverview();
    if (refreshTable) {
        refreshLogicComputationPointPage();
        if (row < m_logicComputationPointTable->rowCount()) {
            m_logicComputationPointTable->setCurrentCell(row, column);
        }
    }
}

void MainWindow::onLogicComputationPointCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }

    if (column == LogicComputationColumnOutputDevice || column == LogicComputationColumnOutputPoint) {
        selectLogicComputationOutputPoint(row);
    } else if (column == LogicComputationColumnOperands) {
        selectLogicComputationOperands(row);
    }
}

void MainWindow::selectLogicComputationOutputPoint(int row)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.computationPoints.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setWindowTitle(QStringLiteral("选择计算点输出点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint selected = dialog.selectedPoint();
    if (!selected.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicComputationPoint &point = logic.computationPoints[row];
    point.deviceId = selected.deviceId;
    point.dataRef = selected.dataRef;
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    m_logicComputationPointTable->setCurrentCell(row, LogicComputationColumnOutputPoint);
}

void MainWindow::selectLogicComputationOperands(int row)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.computationPoints.size()) {
        return;
    }

    QList<configtool::LogicOperand> operands = logic.computationPoints.at(row).operands;
    if (!editLogicComputationOperands(QStringLiteral("编辑计算点源点"),
                                      logic.computationPoints.at(row).formula,
                                      operands)) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.computationPoints[row].operands = operands;
    refreshLogicCenterOverview();
    refreshLogicComputationPointPage();
    m_logicComputationPointTable->setCurrentCell(row, LogicComputationColumnOperands);
}

int MainWindow::currentLogicControlRuleIndex() const
{
    if (!m_logicControlRuleTable || !m_logicControlRuleTable->selectionModel()) {
        return -1;
    }

    const QModelIndexList rows = m_logicControlRuleTable->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void MainWindow::refreshLogicControlRulePage()
{
    if (!m_logicControlRuleTable) {
        return;
    }

    const int previousRow = currentLogicControlRuleIndex();
    const QList<configtool::LogicControlRule> &rules =
        m_configProjectManager.project().logicCenter.controlRules;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    m_updatingLogicControlRulePage = true;
    m_logicControlRuleTable->setRowCount(rules.size());
    for (int row = 0; row < rules.size(); ++row) {
        const configtool::LogicControlRule &rule = rules.at(row);
        const QStringList values = {
            rule.matchDeviceId,
            displayResolver.displayText(rule.matchDeviceId, rule.matchDataRef),
            QString::number(rule.targets.size()),
            rule.description
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == LogicControlRuleColumnTargetCount) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            if (column == LogicControlRuleColumnPoint) {
                item->setData(LogicPointDataRefRole, rule.matchDataRef);
                item->setToolTip(logicPointToolTip(rule.matchDeviceId, rule.matchDataRef));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            m_logicControlRuleTable->setItem(row, column, item);
        }
    }
    m_updatingLogicControlRulePage = false;

    if (previousRow >= 0 && previousRow < rules.size()) {
        m_logicControlRuleTable->selectRow(previousRow);
    } else if (!rules.isEmpty()) {
        m_logicControlRuleTable->selectRow(0);
    } else {
        refreshLogicControlTargetTable();
    }
    refreshLogicControlTargetTable();
}

void MainWindow::refreshLogicControlTargetTable()
{
    if (!m_logicControlTargetTable) {
        return;
    }

    const int ruleIndex = currentLogicControlRuleIndex();
    const QList<configtool::LogicControlRule> &rules =
        m_configProjectManager.project().logicCenter.controlRules;
    const LogicPointDisplayResolver displayResolver(m_configProjectManager.project());

    m_updatingLogicControlRulePage = true;
    if (ruleIndex < 0 || ruleIndex >= rules.size()) {
        m_logicControlTargetTable->setRowCount(0);
        m_updatingLogicControlRulePage = false;
        refreshLogicControlPreview();
        return;
    }

    const QList<configtool::LogicControlTarget> &targets = rules.at(ruleIndex).targets;
    m_logicControlTargetTable->setRowCount(targets.size());
    for (int row = 0; row < targets.size(); ++row) {
        const configtool::LogicControlTarget &target = targets.at(row);
        const QString targetType = target.targetType.trimmed().isEmpty()
            ? QStringLiteral("ctrlcmd")
            : target.targetType.trimmed().toLower();
        const QString preview = expandedLogicControlExpression(
            target.expr,
            m_logicControlPreviewValueEdit ? m_logicControlPreviewValueEdit->text() : QStringLiteral("1"));
        const QStringList values = {
            targetType,
            target.deviceId,
            displayResolver.displayText(target.deviceId, target.dataRef),
            target.expr,
            preview
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == LogicControlTargetColumnType) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            if (column == LogicControlTargetColumnPreview) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            if (column == LogicControlTargetColumnPoint) {
                item->setData(LogicPointDataRefRole, target.dataRef);
                item->setToolTip(logicPointToolTip(target.deviceId, target.dataRef));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            if (isLogicControlTotalTarget(target)) {
                const QString totalTargetTip = QStringLiteral("该目标会进入 AGC/AVC 总控分配逻辑。");
                item->setToolTip(item->toolTip().isEmpty()
                    ? totalTargetTip
                    : item->toolTip() + QLatin1Char('\n') + totalTargetTip);
            }
            m_logicControlTargetTable->setItem(row, column, item);
        }

        auto *typeCombo = new QComboBox(m_logicControlTargetTable);
        typeCombo->setObjectName(QStringLiteral("logicControlTargetTypeCombo"));
        configureTableCellCombo(typeCombo, this);
        typeCombo->addItem(QStringLiteral("ctrlcmd"), QStringLiteral("ctrlcmd"));
        typeCombo->addItem(QStringLiteral("datawrite"), QStringLiteral("data_write"));
        typeCombo->setPlaceholderText(QStringLiteral("请选择"));
        QString typeToolTip = QStringLiteral("ctrlcmd：生成控制命令\ndatawrite：生成内部 DataWrite（配置值 data_write）");
        if (isLogicControlTotalTarget(target)) {
            typeToolTip += QStringLiteral("\n该目标会进入 AGC/AVC 总控分配逻辑。");
        }
        typeCombo->setToolTip(typeToolTip);
        typeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        const QString configuredType = targetType;
        int typeIndex = typeCombo->findData(configuredType);
        if (typeIndex < 0 && configuredType == QStringLiteral("datawrite")) {
            typeIndex = typeCombo->findData(QStringLiteral("data_write"));
        }
        typeCombo->setCurrentIndex(typeIndex);

        connect(typeCombo,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                [this, typeCombo, ruleIndex, row](int index) {
                    if (m_updatingLogicControlRulePage || m_restoringConfigUndo || index < 0) {
                        return;
                    }

                    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
                    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
                        || row < 0 || row >= logic.controlRules.at(ruleIndex).targets.size()) {
                        return;
                    }

                    const QString selectedType = typeCombo->itemData(index).toString();
                    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[row];
                    if (target.targetType == selectedType) {
                        return;
                    }

                    pushConfigUndoSnapshot();
                    target.targetType = selectedType;
                    refreshLogicCenterOverview();
                    refreshLogicControlRulePage();
                    m_logicControlRuleTable->selectRow(ruleIndex);
                    if (row < m_logicControlTargetTable->rowCount()) {
                        m_logicControlTargetTable->selectRow(row);
                        m_logicControlTargetTable->setCurrentCell(row, LogicControlTargetColumnType);
                    }
                });

        hideComboBackedItemText(m_logicControlTargetTable->item(row, LogicControlTargetColumnType));
        m_logicControlTargetTable->setCellWidget(row, LogicControlTargetColumnType, typeCombo);
    }
    m_updatingLogicControlRulePage = false;
    refreshLogicControlPreview();
}

void MainWindow::onLogicControlRuleSelectionChanged()
{
    refreshLogicControlTargetTable();
}

void MainWindow::onAddLogicControlRuleClicked()
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    pushConfigUndoSnapshot();

    configtool::LogicControlRule rule;
    rule.matchDeviceId = QStringLiteral("device_%1").arg(logic.controlRules.size() + 1);
    rule.matchDataRef = QStringLiteral("PROT.SlrInvCtlGGIO.1.TotalP_Ctrl");
    rule.description = QStringLiteral("手工新增控制转换");

    configtool::LogicControlTarget target;
    target.deviceId = rule.matchDeviceId;
    target.dataRef = rule.matchDataRef;
    target.expr = QStringLiteral("{x}");
    target.targetType = QStringLiteral("ctrlcmd");
    rule.targets.append(target);

    logic.controlRules.append(rule);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(logic.controlRules.size() - 1);
}

void MainWindow::onCopyLogicControlRuleClicked()
{
    const int row = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要复制的控制转换规则。"));
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlRule rule = logic.controlRules.at(row);
    rule.matchDataRef += QStringLiteral("_copy");
    rule.description = rule.description.trimmed().isEmpty()
        ? QStringLiteral("复制的控制转换")
        : QStringLiteral("%1 (复制)").arg(rule.description);
    logic.controlRules.insert(row + 1, rule);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(row + 1);
}

void MainWindow::onDeleteLogicControlRuleClicked()
{
    const int row = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的控制转换规则。"));
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules.removeAt(row);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    if (!logic.controlRules.isEmpty()) {
        m_logicControlRuleTable->selectRow(qMin(row, logic.controlRules.size() - 1));
    }
}

void MainWindow::onAddLogicControlTargetClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()) {
        onAddLogicControlRuleClicked();
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget target;
    target.deviceId = logic.controlRules.at(ruleIndex).matchDeviceId;
    target.dataRef = logic.controlRules.at(ruleIndex).matchDataRef;
    target.expr = QStringLiteral("{x}");
    target.targetType = QStringLiteral("ctrlcmd");
    logic.controlRules[ruleIndex].targets.append(target);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(logic.controlRules.at(ruleIndex).targets.size() - 1);
}

void MainWindow::onDeleteLogicControlTargetClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable ? m_logicControlTargetTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || targetRow < 0 || targetRow >= logic.controlRules.at(ruleIndex).targets.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的目标动作。"));
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules[ruleIndex].targets.removeAt(targetRow);
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
}

void MainWindow::onLogicControlRuleItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicControlRulePage || m_restoringConfigUndo || !item) {
        return;
    }

    const int row = item->row();
    const int column = item->column();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlRule &rule = logic.controlRules[row];
    const QString text = item->text().trimmed();
    if (column == LogicControlRuleColumnDevice) {
        rule.matchDeviceId = text;
    } else if (column == LogicControlRuleColumnPoint) {
        rule.matchDataRef = text;
    } else if (column == LogicControlRuleColumnDescription) {
        rule.description = text;
    } else {
        return;
    }

    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    if (row < m_logicControlRuleTable->rowCount()) {
        m_logicControlRuleTable->setCurrentCell(row, column);
    }
}

void MainWindow::onLogicControlTargetItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicControlRulePage || m_restoringConfigUndo || !item) {
        return;
    }

    const int ruleIndex = currentLogicControlRuleIndex();
    const int row = item->row();
    const int column = item->column();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || row < 0 || row >= logic.controlRules.at(ruleIndex).targets.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[row];
    const QString text = item->text().trimmed();
    if (column == LogicControlTargetColumnDevice) {
        target.deviceId = text;
    } else if (column == LogicControlTargetColumnPoint) {
        target.dataRef = text;
    } else if (column == LogicControlTargetColumnExpr) {
        target.expr = text;
    } else {
        return;
    }

    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    if (row < m_logicControlTargetTable->rowCount()) {
        m_logicControlTargetTable->setCurrentCell(row, column);
    }
}

void MainWindow::onLogicControlRuleCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }
    if (column == LogicControlRuleColumnDevice || column == LogicControlRuleColumnPoint) {
        selectLogicControlMatchPoint(row);
    }
}

void MainWindow::onLogicControlTargetCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }
    if (column == LogicControlTargetColumnDevice || column == LogicControlTargetColumnPoint) {
        selectLogicControlTargetPoint(row);
    }
}

void MainWindow::selectLogicControlMatchPoint(int row)
{
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (row < 0 || row >= logic.controlRules.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setServiceTypeFilter(configtool::ModelServiceType::Control);
    dialog.setWindowTitle(QStringLiteral("选择源遥控/遥调点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules[row].matchDeviceId = point.deviceId;
    logic.controlRules[row].matchDataRef = point.dataRef;
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(row);
}

void MainWindow::selectLogicControlTargetPoint(int row)
{
    const int ruleIndex = currentLogicControlRuleIndex();
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || row < 0 || row >= logic.controlRules.at(ruleIndex).targets.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setServiceTypeFilter(configtool::ModelServiceType::Control);
    dialog.setWindowTitle(QStringLiteral("选择目标遥控/遥调点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[row];
    target.deviceId = point.deviceId;
    target.dataRef = point.dataRef;
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(row);
}

void MainWindow::onLogicControlPreviewEdited()
{
    refreshLogicControlTargetTable();
}

void MainWindow::refreshLogicControlPreview()
{
    if (!m_logicControlPreviewLabel || !m_logicControlTargetTable) {
        return;
    }

    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable->currentRow();
    const QList<configtool::LogicControlRule> &rules =
        m_configProjectManager.project().logicCenter.controlRules;
    if (ruleIndex < 0 || ruleIndex >= rules.size()
        || targetRow < 0 || targetRow >= rules.at(ruleIndex).targets.size()) {
        m_logicControlPreviewLabel->setText(QStringLiteral("选择目标动作后显示表达式展开结果。"));
        return;
    }

    const configtool::LogicControlTarget &target = rules.at(ruleIndex).targets.at(targetRow);
    QString text = QStringLiteral("%1/%2: %3 => %4")
                       .arg(target.deviceId,
                            target.dataRef,
                            target.expr,
                            expandedLogicControlExpression(target.expr, m_logicControlPreviewValueEdit->text()));
    if (isLogicControlTotalTarget(target)) {
        text += QStringLiteral("；该目标会进入 AGC/AVC 分配逻辑");
    }
    m_logicControlPreviewLabel->setText(text);
}

void MainWindow::onLogicControlTemplateClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable ? m_logicControlTargetTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || targetRow < 0 || targetRow >= logic.controlRules.at(ruleIndex).targets.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个目标动作。"));
        return;
    }

    QString expr;
    QObject *senderObject = sender();
    if (senderObject == m_logicControlTemplateOriginalBtn) {
        expr = QStringLiteral("{x}");
    } else if (senderObject == m_logicControlTemplateInvertBtn) {
        expr = QStringLiteral("1 - {x}");
    } else if (senderObject == m_logicControlTemplateScaleBtn) {
        expr = QStringLiteral("{x} * 1.0");
    } else if (senderObject == m_logicControlTemplateFixedBtn) {
        expr = QStringLiteral("1");
    } else {
        return;
    }

    pushConfigUndoSnapshot();
    logic.controlRules[ruleIndex].targets[targetRow].expr = expr;
    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(targetRow);
}

void MainWindow::onInsertLogicControlRealtimeRefClicked()
{
    const int ruleIndex = currentLogicControlRuleIndex();
    const int targetRow = m_logicControlTargetTable ? m_logicControlTargetTable->currentRow() : -1;
    configtool::LogicCenterConfig &logic = m_configProjectManager.project().logicCenter;
    if (ruleIndex < 0 || ruleIndex >= logic.controlRules.size()
        || targetRow < 0 || targetRow >= logic.controlRules.at(ruleIndex).targets.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个目标动作。"));
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setWindowTitle(QStringLiteral("选择实时引用点"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::LogicControlTarget &target = logic.controlRules[ruleIndex].targets[targetRow];
    const QString ref = QStringLiteral("{rt:%1#%2}").arg(point.deviceId, point.dataRef);
    if (target.expr.trimmed().isEmpty()) {
        target.expr = QStringLiteral("{x} + %1").arg(ref);
    } else {
        target.expr += QStringLiteral(" + %1").arg(ref);
    }

    refreshLogicCenterOverview();
    refreshLogicControlRulePage();
    m_logicControlRuleTable->selectRow(ruleIndex);
    m_logicControlTargetTable->selectRow(targetRow);
}

void MainWindow::onLogicAgcAvcBasicEdited()
{
    if (m_updatingLogicAgcAvcPage || m_restoringConfigUndo) {
        return;
    }

    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    pushConfigUndoSnapshot();
    group->groupId = m_logicAgcAvcGroupIdEdit->text().trimmed();
    group->virtualDeviceId = m_logicAgcAvcVirtualDeviceIdEdit->text().trimmed();
    group->measurementScale.totalP = m_logicMeasurementTotalPEdit->value();
    group->measurementScale.totalQ = m_logicMeasurementTotalQEdit->value();

    group->gateReverse.enable = m_logicGateEnableReverseCheck->isChecked();
    group->gateReverse.distant = m_logicGateDistantReverseCheck->isChecked();
    group->gateReverse.lock = m_logicGateLockReverseCheck->isChecked();
    group->gateReverse.uplock = m_logicGateUplockReverseCheck->isChecked();
    group->gateReverse.downlock = m_logicGateDownlockReverseCheck->isChecked();
    group->gateReverse.openloop = m_logicGateOpenloopReverseCheck->isChecked();

    group->agcFollow.enable = m_logicAgcFollowEnableCheck->isChecked();
    group->agcFollow.periodMs = m_logicAgcFollowPeriodEdit->value();
    group->agcFollow.step = m_logicAgcFollowStepEdit->value();
    group->agcFollow.tolerance = m_logicAgcFollowToleranceEdit->value();
    group->avcFollow.enable = m_logicAvcFollowEnableCheck->isChecked();
    group->avcFollow.periodMs = m_logicAvcFollowPeriodEdit->value();
    group->avcFollow.step = m_logicAvcFollowStepEdit->value();
    group->avcFollow.tolerance = m_logicAvcFollowToleranceEdit->value();

    refreshLogicCenterOverview();
}

void MainWindow::onLogicAgcAvcDeviceItemChanged(QTableWidgetItem *item)
{
    if (m_updatingLogicAgcAvcPage || m_restoringConfigUndo || !item) {
        return;
    }

    configtool::AgcAvcGroup *group = currentLogicAgcAvcGroup();
    if (!group || item->row() < 0 || item->row() >= group->devices.size()) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::AgcAvcDevice &device = group->devices[item->row()];
    const QString text = item->text().trimmed();
    switch (item->column()) {
    case LogicAgcAvcDeviceColumnDeviceId:
        device.deviceId = text;
        if (device.onlineDeviceId.trimmed().isEmpty()) {
            device.onlineDeviceId = text;
        }
        break;
    case LogicAgcAvcDeviceColumnCtrlP:
        device.ctrlDataRefP = text;
        break;
    case LogicAgcAvcDeviceColumnCtrlQ:
        device.ctrlDataRefQ = text;
        break;
    case LogicAgcAvcDeviceColumnOnlineDevice:
        device.onlineDeviceId = text;
        break;
    case LogicAgcAvcDeviceColumnOnlinePoint:
        device.onlineDataRef = text;
        break;
    case LogicAgcAvcDeviceColumnOnlineOk:
        device.onlineOkValue = uiTextToInt(text, device.onlineOkValue);
        break;
    case LogicAgcAvcDeviceColumnPMin:
        device.pMin = uiTextToDouble(text, device.pMin);
        break;
    case LogicAgcAvcDeviceColumnPMax:
        device.pMax = uiTextToDouble(text, device.pMax);
        break;
    case LogicAgcAvcDeviceColumnQMin:
        device.qMin = uiTextToDouble(text, device.qMin);
        break;
    case LogicAgcAvcDeviceColumnQMax:
        device.qMax = uiTextToDouble(text, device.qMax);
        break;
    case LogicAgcAvcDeviceColumnScaleP:
        device.scaleP = uiTextToDouble(text, device.scaleP);
        break;
    case LogicAgcAvcDeviceColumnScaleQ:
        device.scaleQ = uiTextToDouble(text, device.scaleQ);
        break;
    default:
        break;
    }

    refreshLogicCenterOverview();
}

void MainWindow::onAddLogicAgcAvcDeviceClicked()
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    pushConfigUndoSnapshot();
    configtool::AgcAvcDevice device;
    device.deviceId = QStringLiteral("device_%1").arg(group->devices.size() + 1);
    device.onlineDeviceId = device.deviceId;
    device.ctrlDataRefP = QStringLiteral("PROT.SlrInvCtlGGIO.1.TotalP_Ctrl");
    device.ctrlDataRefQ = QStringLiteral("PROT.SlrInvCtlGGIO.1.TotalQ_Ctrl");
    device.pMax = 1000.0;
    device.pMin = 0.0;
    device.qMax = 1000.0;
    device.qMin = -1000.0;
    group->devices.append(device);
    refreshLogicAgcAvcPage();
    refreshLogicCenterOverview();
    m_logicAgcAvcDeviceTable->selectRow(group->devices.size() - 1);
}

void MainWindow::onLogicAgcAvcDeviceCellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }

    m_logicAgcAvcDeviceTable->setCurrentCell(row, column);
    if (column == LogicAgcAvcDeviceColumnCtrlP) {
        selectLogicAgcAvcPointForColumn(LogicAgcAvcDeviceColumnCtrlP,
                                        configtool::ModelServiceType::Control,
                                        false);
    } else if (column == LogicAgcAvcDeviceColumnCtrlQ) {
        selectLogicAgcAvcPointForColumn(LogicAgcAvcDeviceColumnCtrlQ,
                                        configtool::ModelServiceType::Control,
                                        false);
    } else if (column == LogicAgcAvcDeviceColumnOnlinePoint) {
        selectLogicAgcAvcPointForColumn(LogicAgcAvcDeviceColumnOnlinePoint,
                                        configtool::ModelServiceType::Status,
                                        true);
    }
}

void MainWindow::onDeleteLogicAgcAvcDeviceClicked()
{
    configtool::AgcAvcGroup *group = currentLogicAgcAvcGroup();
    if (!group) {
        return;
    }
    const int row = m_logicAgcAvcDeviceTable->currentRow();
    if (row < 0 || row >= group->devices.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的南向设备。"));
        return;
    }

    pushConfigUndoSnapshot();
    group->devices.removeAt(row);
    refreshLogicAgcAvcPage();
    refreshLogicCenterOverview();
}

void MainWindow::selectLogicAgcAvcPointForColumn(int dataRefColumn,
                                                 configtool::ModelServiceType preferredType,
                                                 bool updateOnlineDevice)
{
    configtool::AgcAvcGroup *group = ensureLogicAgcAvcGroup();
    int row = m_logicAgcAvcDeviceTable->currentRow();
    if (row < 0) {
        onAddLogicAgcAvcDeviceClicked();
        row = m_logicAgcAvcDeviceTable->currentRow();
    }
    if (row < 0 || row >= group->devices.size()) {
        return;
    }

    PointSelectorDialog dialog(this);
    dialog.setProject(&m_configProjectManager.project());
    dialog.setServiceTypeFilter(preferredType);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PointSelectorDialog::SelectedPoint point = dialog.selectedPoint();
    if (!point.valid) {
        return;
    }

    pushConfigUndoSnapshot();
    configtool::AgcAvcDevice &device = group->devices[row];
    device.deviceId = point.deviceId;
    if (dataRefColumn == LogicAgcAvcDeviceColumnCtrlP) {
        device.ctrlDataRefP = point.dataRef;
    } else if (dataRefColumn == LogicAgcAvcDeviceColumnCtrlQ) {
        device.ctrlDataRefQ = point.dataRef;
    } else if (dataRefColumn == LogicAgcAvcDeviceColumnOnlinePoint) {
        device.onlineDataRef = point.dataRef;
        if (updateOnlineDevice) {
            device.onlineDeviceId = point.deviceId;
        }
    }
    if (device.onlineDeviceId.trimmed().isEmpty()) {
        device.onlineDeviceId = device.deviceId;
    }

    refreshLogicAgcAvcPage();
    refreshLogicCenterOverview();
    m_logicAgcAvcDeviceTable->setCurrentCell(row, dataRefColumn);
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
    if (rows.isEmpty()) {
        return -1;
    }

    const int row = rows.first().row();
    QTableWidgetItem *anchorItem = m_configDeviceTable->item(row, 0);
    if (!anchorItem) {
        return -1;
    }

    const QVariant deviceIndexData = anchorItem->data(Qt::UserRole);
    return deviceIndexData.isValid() ? deviceIndexData.toInt() : row;
}

bool MainWindow::selectConfigDeviceByIndex(int deviceIndex)
{
    if (!m_configDeviceTable || deviceIndex < 0) {
        return false;
    }

    for (int row = 0; row < m_configDeviceTable->rowCount(); ++row) {
        QTableWidgetItem *anchorItem = m_configDeviceTable->item(row, 0);
        if (anchorItem && anchorItem->data(Qt::UserRole).toInt() == deviceIndex) {
            m_configDeviceTable->selectRow(row);
            return true;
        }
    }

    return false;
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

    QTableWidgetItem *handleItem = m_modelPointsTable->item(selectedRow, ModelPointColumnDragHandle);
    if (!handleItem) {
        return qMakePair(-1, -1);
    }

    return qMakePair(handleItem->data(Qt::UserRole).toInt(),
                     handleItem->data(Qt::UserRole + 1).toInt());
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

QSet<QString> MainWindow::duplicateIec104ChannelBindingAddresses(
    const configtool::ConfigProject &project,
    const configtool::ProtocolDeviceInstance &device) const
{
    return duplicateIec104BindingAddressesByChannel(project.devices).value(iec104ChannelKey(device));
}
