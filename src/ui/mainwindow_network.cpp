#include "mainwindow.h"

#include "network/ssh_client.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
enum InterfaceColumn { IfName, IfLink, IfMac, IfCurrent, IfTarget, IfPrefix, IfColumnCount };
enum RouteColumn { RouteDestination, RouteGateway, RouteDevice, RouteMetric, RouteColumnCount };

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

bool isIpv4(const QString &text)
{
    QHostAddress address;
    return address.setAddress(text.trimmed()) && address.protocol() == QAbstractSocket::IPv4Protocol;
}
}

void MainWindow::setupNetworkPage()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("设备网络配置"));
    dialog->setModal(false);
    dialog->resize(980, 720);
    m_networkConfigPage = dialog;
    auto *layout = new QVBoxLayout(m_networkConfigPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel(QStringLiteral("SSH: root@设备IP:10022"), m_networkConfigPage));
    m_readNetworkConfigBtn = new QPushButton(QStringLiteral("读取设备状态"), m_networkConfigPage);
    m_loadNetworkConfigBtn = new QPushButton(QStringLiteral("从项目加载"), m_networkConfigPage);
    m_saveNetworkConfigBtn = new QPushButton(QStringLiteral("保存到项目"), m_networkConfigPage);
    m_applyNetworkConfigBtn = new QPushButton(QStringLiteral("校验并应用到设备"), m_networkConfigPage);
    toolbar->addWidget(m_readNetworkConfigBtn);
    toolbar->addWidget(m_loadNetworkConfigBtn);
    toolbar->addWidget(m_saveNetworkConfigBtn);
    toolbar->addWidget(m_applyNetworkConfigBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    auto *hint = new QLabel(QStringLiteral(
        "应用会持久化到 /etc/cepb/network.conf。修改当前 SSH 所在网口后连接会切换到新的设备IP。"),
        m_networkConfigPage);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *pingRow = new QHBoxLayout();
    pingRow->addWidget(new QLabel(QStringLiteral("设备侧 Ping:"), m_networkConfigPage));
    m_networkPingTargetEdit = new QLineEdit(m_networkConfigPage);
    m_networkPingTargetEdit->setPlaceholderText(QStringLiteral("输入目标 IPv4，例如 192.168.1.1"));
    m_networkPingTargetEdit->setMaximumWidth(280);
    m_networkPingBtn = new QPushButton(QStringLiteral("测试连通性"), m_networkConfigPage);
    pingRow->addWidget(m_networkPingTargetEdit);
    pingRow->addWidget(m_networkPingBtn);
    pingRow->addStretch();
    layout->addLayout(pingRow);
    m_networkPingOutput = new QTextEdit(m_networkConfigPage);
    m_networkPingOutput->setReadOnly(true);
    m_networkPingOutput->setMaximumHeight(105);
    m_networkPingOutput->setPlaceholderText(QStringLiteral("Ping 结果将在这里显示。命令从设备端执行。"));
    layout->addWidget(m_networkPingOutput);

    layout->addWidget(new QLabel(QStringLiteral("网口配置"), m_networkConfigPage));
    m_networkInterfaceTable = new QTableWidget(8, IfColumnCount, m_networkConfigPage);
    m_networkInterfaceTable->setHorizontalHeaderLabels({QStringLiteral("网口"), QStringLiteral("链路"),
                                                         QStringLiteral("MAC"), QStringLiteral("当前 IPv4"),
                                                         QStringLiteral("目标 IPv4"), QStringLiteral("前缀")});
    m_networkInterfaceTable->verticalHeader()->setVisible(false);
    m_networkInterfaceTable->setAlternatingRowColors(true);
    m_networkInterfaceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_networkInterfaceTable->horizontalHeader()->setStretchLastSection(true);
    m_networkInterfaceTable->setColumnWidth(0, 70);
    m_networkInterfaceTable->setColumnWidth(1, 80);
    m_networkInterfaceTable->setColumnWidth(2, 160);
    m_networkInterfaceTable->setColumnWidth(3, 150);
    m_networkInterfaceTable->setColumnWidth(4, 150);
    for (int row = 0; row < 8; ++row) {
        m_networkInterfaceTable->setItem(row, IfName, readOnlyItem(QStringLiteral("eth%1").arg(row)));
        m_networkInterfaceTable->setItem(row, IfLink, readOnlyItem(QStringLiteral("-")));
        m_networkInterfaceTable->setItem(row, IfMac, readOnlyItem(QStringLiteral("-")));
        m_networkInterfaceTable->setItem(row, IfCurrent, readOnlyItem(QStringLiteral("-")));
        m_networkInterfaceTable->setItem(row, IfTarget,
                                         new QTableWidgetItem(QStringLiteral("192.168.%1.10").arg(row)));
        m_networkInterfaceTable->setItem(row, IfPrefix, new QTableWidgetItem(QStringLiteral("24")));
    }
    layout->addWidget(m_networkInterfaceTable, 2);

    auto *routeToolbar = new QHBoxLayout();
    routeToolbar->addWidget(new QLabel(QStringLiteral("静态路由（只管理本页面创建的路由）"), m_networkConfigPage));
    m_addNetworkRouteBtn = new QPushButton(QStringLiteral("添加路由"), m_networkConfigPage);
    m_deleteNetworkRouteBtn = new QPushButton(QStringLiteral("删除路由"), m_networkConfigPage);
    routeToolbar->addWidget(m_addNetworkRouteBtn);
    routeToolbar->addWidget(m_deleteNetworkRouteBtn);
    routeToolbar->addStretch();
    layout->addLayout(routeToolbar);

    m_networkRouteTable = new QTableWidget(0, RouteColumnCount, m_networkConfigPage);
    m_networkRouteTable->setHorizontalHeaderLabels({QStringLiteral("目标网络/CIDR"), QStringLiteral("网关"),
                                                     QStringLiteral("出口网口"), QStringLiteral("Metric")});
    m_networkRouteTable->verticalHeader()->setVisible(false);
    m_networkRouteTable->setAlternatingRowColors(true);
    m_networkRouteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_networkRouteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_networkRouteTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_networkRouteTable, 1);

    connect(m_readNetworkConfigBtn, &QPushButton::clicked, this, &MainWindow::onReadNetworkConfigClicked);
    connect(m_loadNetworkConfigBtn, &QPushButton::clicked, this, &MainWindow::onLoadNetworkConfigClicked);
    connect(m_saveNetworkConfigBtn, &QPushButton::clicked, this, &MainWindow::onSaveNetworkConfigClicked);
    connect(m_applyNetworkConfigBtn, &QPushButton::clicked, this, &MainWindow::onApplyNetworkConfigClicked);
    connect(m_addNetworkRouteBtn, &QPushButton::clicked, this, &MainWindow::onAddNetworkRouteClicked);
    connect(m_deleteNetworkRouteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteNetworkRouteClicked);
    connect(m_networkPingBtn, &QPushButton::clicked, this, &MainWindow::onPingNetworkTargetClicked);
    connect(m_networkPingTargetEdit, &QLineEdit::returnPressed, this, &MainWindow::onPingNetworkTargetClicked);
}

void MainWindow::onPingNetworkTargetClicked()
{
    const QString target = m_networkPingTargetEdit->text().trimmed();
    if (!isIpv4(target)) {
        QMessageBox::warning(m_networkConfigPage, QStringLiteral("Ping 测试"), QStringLiteral("请输入有效的 IPv4 地址。"));
        return;
    }
    if (deviceHost().isEmpty()) {
        QMessageBox::warning(m_networkConfigPage, QStringLiteral("Ping 测试"), QStringLiteral("请先填写右上角设备IP。"));
        return;
    }

    m_networkPingBtn->setEnabled(false);
    m_networkPingOutput->setPlainText(QStringLiteral("正在通过设备 Ping %1 ...").arg(target));
    QApplication::processEvents();
    const SshClient::Connection connection{deviceHost(), fixedRemoteSshPort().toUShort(),
                                           fixedRemoteUser(), fixedRemotePassword()};
    const auto result = SshClient::execCommand(
        connection, QStringLiteral("ping -c 4 -W 2 %1").arg(target), 15000);
    m_networkPingBtn->setEnabled(true);

    const QString detail = result.output.trimmed().isEmpty() ? result.error.trimmed() : result.output.trimmed();
    m_networkPingOutput->setPlainText(
        QStringLiteral("%1：%2\n\n%3")
            .arg(result.ok ? QStringLiteral("连通") : QStringLiteral("不通"), target, detail));
    statusBar()->showMessage(result.ok ? QStringLiteral("设备可以连通 %1").arg(target)
                                       : QStringLiteral("设备无法连通 %1").arg(target),
                             5000);
}

void MainWindow::onAddNetworkRouteClicked()
{
    const int row = m_networkRouteTable->rowCount();
    m_networkRouteTable->insertRow(row);
    m_networkRouteTable->setItem(row, RouteDestination, new QTableWidgetItem(QStringLiteral("10.0.0.0/8")));
    m_networkRouteTable->setItem(row, RouteGateway, new QTableWidgetItem(QStringLiteral("192.168.0.1")));
    m_networkRouteTable->setItem(row, RouteDevice, new QTableWidgetItem(QStringLiteral("eth0")));
    m_networkRouteTable->setItem(row, RouteMetric, new QTableWidgetItem(QStringLiteral("100")));
}

void MainWindow::onDeleteNetworkRouteClicked()
{
    if (m_networkRouteTable->currentRow() >= 0)
        m_networkRouteTable->removeRow(m_networkRouteTable->currentRow());
}

bool MainWindow::validateNetworkConfig(QString *errorMessage) const
{
    QSet<QString> addresses;
    for (int row = 0; row < 8; ++row) {
        const QString name = m_networkInterfaceTable->item(row, IfName)->text();
        const QString address = m_networkInterfaceTable->item(row, IfTarget)->text().trimmed();
        bool prefixOk = false;
        const int prefix = m_networkInterfaceTable->item(row, IfPrefix)->text().trimmed().toInt(&prefixOk);
        if (!isIpv4(address)) {
            if (errorMessage) *errorMessage = QStringLiteral("%1 的目标 IPv4 无效：%2").arg(name, address);
            return false;
        }
        if (!prefixOk || prefix < 1 || prefix > 32) {
            if (errorMessage) *errorMessage = QStringLiteral("%1 的前缀必须为 1～32").arg(name);
            return false;
        }
        if (addresses.contains(address)) {
            if (errorMessage) *errorMessage = QStringLiteral("存在重复 IPv4 地址：%1").arg(address);
            return false;
        }
        addresses.insert(address);
    }

    const QRegularExpression devicePattern(QStringLiteral("^eth[0-7]$"));
    for (int row = 0; row < m_networkRouteTable->rowCount(); ++row) {
        auto value = [this, row](int column) {
            const auto *item = m_networkRouteTable->item(row, column);
            return item ? item->text().trimmed() : QString();
        };
        const QString destination = value(RouteDestination);
        const int slash = destination.indexOf(QLatin1Char('/'));
        bool prefixOk = false;
        const int prefix = slash > 0 ? destination.mid(slash + 1).toInt(&prefixOk) : -1;
        if (slash <= 0 || !isIpv4(destination.left(slash)) || !prefixOk || prefix < 0 || prefix > 32) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由目标无效：%2").arg(row + 1).arg(destination);
            return false;
        }
        if (!isIpv4(value(RouteGateway))) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由网关无效").arg(row + 1);
            return false;
        }
        if (!devicePattern.match(value(RouteDevice)).hasMatch()) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由出口必须为 eth0～eth7").arg(row + 1);
            return false;
        }
        bool metricOk = false;
        const int metric = value(RouteMetric).toInt(&metricOk);
        if (!metricOk || metric < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由 Metric 无效").arg(row + 1);
            return false;
        }
    }
    return true;
}

QByteArray MainWindow::serializeNetworkConfig() const
{
    QByteArray data("# CEPB persistent network configuration\nVERSION|1\n");
    for (int row = 0; row < 8; ++row) {
        data += QStringLiteral("INTERFACE|%1|%2|%3\n")
                    .arg(m_networkInterfaceTable->item(row, IfName)->text(),
                         m_networkInterfaceTable->item(row, IfTarget)->text().trimmed(),
                         m_networkInterfaceTable->item(row, IfPrefix)->text().trimmed()).toUtf8();
    }
    for (int row = 0; row < m_networkRouteTable->rowCount(); ++row) {
        QStringList fields;
        for (int column = 0; column < RouteColumnCount; ++column) {
            const auto *item = m_networkRouteTable->item(row, column);
            fields << (item ? item->text().trimmed() : QString());
        }
        data += QStringLiteral("ROUTE|%1\n").arg(fields.join(QLatin1Char('|'))).toUtf8();
    }
    return data;
}

bool MainWindow::loadNetworkConfigData(const QByteArray &data, QString *errorMessage)
{
    QHash<QString, QPair<QString, QString>> interfaces;
    QList<QStringList> routes;
    for (const QString &rawLine : QString::fromUtf8(data).split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.value(0) == QStringLiteral("VERSION") && fields.value(1) != QStringLiteral("1")) {
            if (errorMessage) *errorMessage = QStringLiteral("不支持的网络配置版本");
            return false;
        }
        if (fields.value(0) == QStringLiteral("INTERFACE") && fields.size() == 4)
            interfaces.insert(fields.at(1), qMakePair(fields.at(2), fields.at(3)));
        else if (fields.value(0) == QStringLiteral("ROUTE") && fields.size() == 5)
            routes.append(fields.mid(1));
    }
    for (int row = 0; row < 8; ++row) {
        const QString name = QStringLiteral("eth%1").arg(row);
        if (!interfaces.contains(name)) {
            if (errorMessage) *errorMessage = QStringLiteral("配置缺少 %1").arg(name);
            return false;
        }
        m_networkInterfaceTable->item(row, IfTarget)->setText(interfaces.value(name).first);
        m_networkInterfaceTable->item(row, IfPrefix)->setText(interfaces.value(name).second);
    }
    m_networkRouteTable->setRowCount(0);
    for (const QStringList &route : routes) {
        const int row = m_networkRouteTable->rowCount();
        m_networkRouteTable->insertRow(row);
        for (int column = 0; column < RouteColumnCount; ++column)
            m_networkRouteTable->setItem(row, column, new QTableWidgetItem(route.value(column)));
    }
    return true;
}

void MainWindow::onReadNetworkConfigClicked()
{
    if (deviceHost().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("读取网络配置"), QStringLiteral("请先填写设备IP。"));
        return;
    }
    QProgressDialog progress(QStringLiteral("正在读取设备网络状态..."), QString(), 0, 0, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.show();
    QApplication::processEvents();
    const SshClient::Connection connection{deviceHost(), fixedRemoteSshPort().toUShort(), fixedRemoteUser(), fixedRemotePassword()};
    const QString command = QStringLiteral(
        "for d in eth0 eth1 eth2 eth3 eth4 eth5 eth6 eth7; do [ -e /sys/class/net/$d ] || continue; "
        "s=$(cat /sys/class/net/$d/operstate 2>/dev/null); m=$(cat /sys/class/net/$d/address 2>/dev/null); "
        "a=$(ip -o -4 addr show dev $d scope global 2>/dev/null | awk 'NR==1 {print $4}'); "
        "printf 'IF|%s|%s|%s|%s\\n' \"$d\" \"$s\" \"$m\" \"$a\"; done");
    const auto result = SshClient::execCommand(connection, command, 30000);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("读取网络配置"), result.error);
        return;
    }
    for (const QString &line : result.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.size() < 5 || fields.value(0) != QStringLiteral("IF")) continue;
        bool rowOk = false;
        const int row = fields.at(1).mid(3).toInt(&rowOk);
        if (!rowOk || row < 0 || row >= 8) continue;
        const QString cidr = fields.at(4);
        const int slash = cidr.indexOf(QLatin1Char('/'));
        m_networkInterfaceTable->item(row, IfLink)->setText(fields.at(2));
        m_networkInterfaceTable->item(row, IfMac)->setText(fields.at(3));
        m_networkInterfaceTable->item(row, IfCurrent)->setText(slash > 0 ? cidr.left(slash) : cidr);
        if (slash > 0) {
            m_networkInterfaceTable->item(row, IfTarget)->setText(cidr.left(slash));
            m_networkInterfaceTable->item(row, IfPrefix)->setText(cidr.mid(slash + 1));
        }
    }
    statusBar()->showMessage(QStringLiteral("设备网络状态读取完成"), 5000);
}

void MainWindow::onSaveNetworkConfigClicked()
{
    QString error;
    if (!validateNetworkConfig(&error)) {
        QMessageBox::warning(this, QStringLiteral("保存网络配置"), error);
        return;
    }
    const QString root = m_configImportDirEdit ? m_configImportDirEdit->text().trimmed() : QString();
    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        QMessageBox::warning(this, QStringLiteral("保存网络配置"), QStringLiteral("请先在配置概览选择有效的工程目录。"));
        return;
    }
    QDir dir(root);
    if (!dir.mkpath(QStringLiteral("Network"))) {
        QMessageBox::warning(this, QStringLiteral("保存网络配置"), QStringLiteral("无法创建 Network 目录。"));
        return;
    }
    QFile file(dir.filePath(QStringLiteral("Network/network.conf")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(serializeNetworkConfig()) < 0) {
        QMessageBox::warning(this, QStringLiteral("保存网络配置"), file.errorString());
        return;
    }
    statusBar()->showMessage(QStringLiteral("网络配置已保存到项目 Network/network.conf"), 5000);
}

void MainWindow::onLoadNetworkConfigClicked()
{
    const QString root = m_configImportDirEdit ? m_configImportDirEdit->text().trimmed() : QString();
    QFile file(QDir(root).filePath(QStringLiteral("Network/network.conf")));
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("加载网络配置"), QStringLiteral("无法读取 %1").arg(file.fileName()));
        return;
    }
    QString error;
    if (!loadNetworkConfigData(file.readAll(), &error) || !validateNetworkConfig(&error)) {
        QMessageBox::warning(this, QStringLiteral("加载网络配置"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("项目网络配置已加载"), 5000);
}

QString MainWindow::localNetworkSupportFile(const QString &relativePath) const
{
    const QString appDir = QApplication::applicationDirPath();
    const QStringList candidates = {QDir(appDir).filePath(relativePath), QDir(appDir).filePath(QStringLiteral("../%1").arg(relativePath)),
                                    QDir::current().filePath(relativePath), QDir::current().filePath(QStringLiteral("../%1").arg(relativePath))};
    for (const QString &candidate : candidates)
        if (QFileInfo::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
    return QString();
}

void MainWindow::onApplyNetworkConfigClicked()
{
    QString error;
    if (!validateNetworkConfig(&error)) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), error);
        return;
    }
    QString helper = localNetworkSupportFile(QStringLiteral("scripts/device/cepb-network-apply"));
    QString service = localNetworkSupportFile(QStringLiteral("scripts/systemd/cepb-network.service"));
    if (helper.isEmpty()) helper = localNetworkSupportFile(QStringLiteral("device/cepb-network-apply"));
    if (service.isEmpty()) service = localNetworkSupportFile(QStringLiteral("systemd/cepb-network.service"));
    if (helper.isEmpty() || service.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), QStringLiteral("找不到设备端网络脚本或 service 文件，请检查发布包。"));
        return;
    }
    const QString oldHost = deviceHost();
    QString newHost = oldHost;
    for (int row = 0; row < 8; ++row)
        if (m_networkInterfaceTable->item(row, IfCurrent)->text().trimmed() == oldHost)
            newHost = m_networkInterfaceTable->item(row, IfTarget)->text().trimmed();

    QString prompt = QStringLiteral("将上传并持久化当前网络配置，然后由设备异步应用。是否继续？");
    if (newHost != oldHost)
        prompt += QStringLiteral("\n\n当前管理地址将从 %1 改为 %2，SSH 会断开；提交后右上角设备IP将切换到新地址。").arg(oldHost, newHost);
    if (QMessageBox::warning(this, QStringLiteral("应用网络配置"), prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    QTemporaryFile configFile(QDir::temp().filePath(QStringLiteral("cepb_network_XXXXXX.conf")));
    if (!configFile.open() || configFile.write(serializeNetworkConfig()) < 0 || !configFile.flush()) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), QStringLiteral("无法生成临时配置文件。"));
        return;
    }
    configFile.close();
    QProgressDialog progress(QStringLiteral("正在上传并校验网络配置..."), QString(), 0, 4, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    const SshClient::Connection connection{oldHost, fixedRemoteSshPort().toUShort(), fixedRemoteUser(), fixedRemotePassword()};
    QString output;
    bool ok = SshClient::uploadFileScp(connection, helper, QStringLiteral("/tmp/cepb-network-apply"), &output);
    progress.setValue(1);
    if (ok) ok = SshClient::uploadFileScp(connection, service, QStringLiteral("/tmp/cepb-network.service"), &output);
    progress.setValue(2);
    if (ok) ok = SshClient::uploadFileScp(connection, configFile.fileName(), QStringLiteral("/tmp/cepb-network.conf"), &output);
    progress.setValue(3);
    if (ok) {
        const QString command = QStringLiteral(
            "set -e; install -d -m 0755 /etc/cepb /usr/local/sbin /etc/systemd/system; "
            "install -m 0755 /tmp/cepb-network-apply /usr/local/sbin/cepb-network-apply; "
            "install -m 0644 /tmp/cepb-network.service /etc/systemd/system/cepb-network.service; "
            "/usr/local/sbin/cepb-network-apply check /tmp/cepb-network.conf; "
            "install -m 0644 /tmp/cepb-network.conf /etc/cepb/network.conf; systemctl daemon-reload; "
            "systemctl enable cepb-network.service; systemd-run --quiet --no-block --collect "
            "--unit=cepb-network-apply-now-$(date +%s) /bin/systemctl restart cepb-network.service");
        const auto result = SshClient::execCommand(connection, command, 30000);
        ok = result.ok;
        output = result.ok ? result.output : result.error;
    }
    progress.setValue(4);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), QStringLiteral("网络配置下发失败：\n%1").arg(output));
        return;
    }
    if (newHost != oldHost && m_ipEdit) m_ipEdit->setText(newHost);
    statusBar()->showMessage(QStringLiteral("网络配置已通过校验并提交设备应用"), 8000);
    QMessageBox::information(this, QStringLiteral("应用网络配置"),
                             newHost == oldHost ? QStringLiteral("网络配置已提交设备应用，请稍后重新读取状态确认。")
                                                : QStringLiteral("网络配置已提交，请等待设备应用后使用 %1 重新读取状态。").arg(newHost));
}
