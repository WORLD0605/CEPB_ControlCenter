#include "mainwindow.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

namespace {

constexpr int kMinimumTimeoutMs = 1000;
constexpr int kMaximumTimeoutMs = 24 * 60 * 60 * 1000;
constexpr int kMinimumPriority = -1000000;
constexpr int kMaximumPriority = 1000000;

QSpinBox *createPrioritySpinBox(QWidget *parent, int value)
{
    auto *spinBox = new QSpinBox(parent);
    spinBox->setRange(kMinimumPriority, kMaximumPriority);
    spinBox->setValue(value);
    spinBox->setToolTip(QStringLiteral("数值越大，控制优先级越高；相同优先级按先到先得处理"));
    return spinBox;
}

QSpinBox *createTimeoutSpinBox(QWidget *parent, int value)
{
    auto *spinBox = new QSpinBox(parent);
    spinBox->setRange(kMinimumTimeoutMs, kMaximumTimeoutMs);
    spinBox->setSingleStep(1000);
    spinBox->setSuffix(QStringLiteral(" ms"));
    spinBox->setValue(qBound(kMinimumTimeoutMs, value, kMaximumTimeoutMs));
    return spinBox;
}

} // namespace

void MainWindow::openControlPriorityDialog()
{
    const configtool::LogicControlPriorityConfig original =
        m_configProjectManager.project().logicCenter.controlPriority;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("北向控制优先级"));
    dialog.setModal(true);
    dialog.resize(520, 430);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *description = new QLabel(
        QStringLiteral("此配置写入 LogicCenter/etc/LogicCenter_Config.json。"
                       "多个北向 APP 控制同一物理点时，数值越大优先级越高；"
                       "相同优先级按先到先得处理。保存工程并上传后，重启 LogicCenter 生效。"),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *enableCheck = new QCheckBox(QStringLiteral("启用北向控制优先级仲裁"), &dialog);
    enableCheck->setChecked(original.enable);
    layout->addWidget(enableCheck);

    auto *priorityGroup = new QGroupBox(QStringLiteral("各北向 APP 优先级"), &dialog);
    auto *priorityForm = new QFormLayout(priorityGroup);
    priorityForm->setHorizontalSpacing(24);
    priorityForm->setVerticalSpacing(10);

    QMap<QString, QSpinBox *> priorityEditors;
    const QStringList appNames{
        QStringLiteral("North_CEP"),
        QStringLiteral("North_101"),
        QStringLiteral("North_104"),
        QStringLiteral("North_Mqtt")
    };
    for (const QString &appName : appNames) {
        QSpinBox *editor = createPrioritySpinBox(
            priorityGroup,
            original.sourcePriorities.value(appName, 100));
        priorityEditors.insert(appName, editor);
        priorityForm->addRow(appName + QLatin1Char(':'), editor);
    }
    layout->addWidget(priorityGroup);

    auto *policyGroup = new QGroupBox(QStringLiteral("仲裁策略"), &dialog);
    auto *policyForm = new QFormLayout(policyGroup);
    policyForm->setHorizontalSpacing(24);
    policyForm->setVerticalSpacing(10);

    auto *selectTimeoutSpin = createTimeoutSpinBox(policyGroup, original.selectTimeoutMs);
    selectTimeoutSpin->setToolTip(QStringLiteral("遥控选择或遥调预置成功后，资源锁的最长保留时间"));
    policyForm->addRow(QStringLiteral("选择/预置超时:"), selectTimeoutSpin);

    auto *executeTimeoutSpin = createTimeoutSpinBox(policyGroup, original.executeTimeoutMs);
    executeTimeoutSpin->setToolTip(QStringLiteral("控制进入执行状态后，资源锁的最长保留时间"));
    policyForm->addRow(QStringLiteral("执行超时:"), executeTimeoutSpin);

    auto *preemptCheck = new QCheckBox(
        QStringLiteral("允许高优先级抢占尚未执行的选择/预置"), policyGroup);
    preemptCheck->setChecked(original.preemptSelected);
    policyForm->addRow(QString(), preemptCheck);
    layout->addWidget(policyGroup);

    auto *hint = new QLabel(
        QStringLiteral("提示：正在执行的控制不会被抢占；关闭总开关后 LogicCenter 将跳过优先级拦截。"),
        &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    configtool::LogicControlPriorityConfig updated = original;
    updated.enable = enableCheck->isChecked();
    for (auto it = priorityEditors.constBegin(); it != priorityEditors.constEnd(); ++it) {
        updated.sourcePriorities.insert(it.key(), it.value()->value());
    }
    updated.selectTimeoutMs = selectTimeoutSpin->value();
    updated.executeTimeoutMs = executeTimeoutSpin->value();
    updated.preemptSelected = preemptCheck->isChecked();

    pushConfigUndoSnapshot();
    configtool::LogicCenterConfig &logicCenter =
        m_configProjectManager.project().logicCenter;
    logicCenter.controlPriority = updated;
    logicCenter.hasControlPriority = true;

    statusBar()->showMessage(
        QStringLiteral("北向控制优先级已更新，请点击“保存配置”写入工程"), 5000);
}
