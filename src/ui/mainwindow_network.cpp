#include "mainwindow.h"

#include "network/ssh_client.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
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
#include <QSaveFile>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>

namespace {
enum InterfaceColumn { IfName, IfLink, IfMac, IfAddress, IfPrefix, IfColumnCount };
enum RouteColumn { RouteDestination, RouteGateway, RouteDevice, RouteMetric, RouteSource, RouteColumnCount };
constexpr int LastReadAddressRole = Qt::UserRole;

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
    m_readNetworkConfigBtn = new QPushButton(QStringLiteral("读取网络配置"), m_networkConfigPage);
    m_applyNetworkConfigBtn = new QPushButton(QStringLiteral("应用网络配置"), m_networkConfigPage);
    toolbar->addWidget(m_readNetworkConfigBtn);
    toolbar->addWidget(m_applyNetworkConfigBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    auto *hint = new QLabel(QStringLiteral(
        "读取后可直接在 IP 列修改网口地址；应用会持久化到 /etc/cepb/network.conf。"
        "修改当前 SSH 所在网口后连接会切换到新的设备IP。"),
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
                                                         QStringLiteral("MAC"), QStringLiteral("IP"),
                                                         QStringLiteral("前缀")});
    m_networkInterfaceTable->verticalHeader()->setVisible(false);
    m_networkInterfaceTable->setAlternatingRowColors(true);
    m_networkInterfaceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_networkInterfaceTable->horizontalHeader()->setStretchLastSection(true);
    m_networkInterfaceTable->setColumnWidth(0, 70);
    m_networkInterfaceTable->setColumnWidth(1, 80);
    m_networkInterfaceTable->setColumnWidth(2, 160);
    m_networkInterfaceTable->setColumnWidth(3, 150);
    for (int row = 0; row < 8; ++row) {
        m_networkInterfaceTable->setItem(row, IfName, readOnlyItem(QStringLiteral("eth%1").arg(row)));
        m_networkInterfaceTable->setItem(row, IfLink, readOnlyItem(QStringLiteral("-")));
        m_networkInterfaceTable->setItem(row, IfMac, readOnlyItem(QStringLiteral("-")));
        m_networkInterfaceTable->setItem(row, IfAddress,
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
    m_networkRouteTable->setHorizontalHeaderLabels(
        {QStringLiteral("目标网络/CIDR"), QStringLiteral("网关（直连留空）"),
         QStringLiteral("出口网口"), QStringLiteral("Metric（可选）"), QStringLiteral("源 IPv4（可选）")});
    m_networkRouteTable->verticalHeader()->setVisible(false);
    m_networkRouteTable->setAlternatingRowColors(true);
    m_networkRouteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_networkRouteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_networkRouteTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_networkRouteTable, 1);

    connect(m_readNetworkConfigBtn, &QPushButton::clicked, this, &MainWindow::onReadNetworkConfigClicked);
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
    m_networkRouteTable->setItem(row, RouteGateway, new QTableWidgetItem());
    m_networkRouteTable->setItem(row, RouteDevice, new QTableWidgetItem(QStringLiteral("eth0")));
    m_networkRouteTable->setItem(row, RouteMetric, new QTableWidgetItem(QStringLiteral("100")));
    m_networkRouteTable->setItem(row, RouteSource, new QTableWidgetItem());
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
        const QString address = m_networkInterfaceTable->item(row, IfAddress)->text().trimmed();
        bool prefixOk = false;
        const int prefix = m_networkInterfaceTable->item(row, IfPrefix)->text().trimmed().toInt(&prefixOk);
        if (!isIpv4(address)) {
            if (errorMessage) *errorMessage = QStringLiteral("%1 的 IP 无效：%2").arg(name, address);
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
        const QString gateway = value(RouteGateway);
        if (!gateway.isEmpty() && !isIpv4(gateway)) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由网关无效").arg(row + 1);
            return false;
        }
        const QString device = value(RouteDevice);
        if (!devicePattern.match(device).hasMatch()) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由出口必须为 eth0～eth7").arg(row + 1);
            return false;
        }
        const QString metricText = value(RouteMetric);
        bool metricOk = true;
        const int metric = metricText.isEmpty() ? 0 : metricText.toInt(&metricOk);
        if (!metricOk || metric < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由 Metric 无效").arg(row + 1);
            return false;
        }
        const QString source = value(RouteSource);
        if (!source.isEmpty() && !isIpv4(source)) {
            if (errorMessage) *errorMessage = QStringLiteral("第 %1 条路由源 IPv4 无效").arg(row + 1);
            return false;
        }
        if (!source.isEmpty()) {
            const int interfaceRow = device.mid(3).toInt();
            const QString interfaceAddress =
                m_networkInterfaceTable->item(interfaceRow, IfAddress)->text().trimmed();
            if (source != interfaceAddress) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("第 %1 条路由源 IPv4 必须是 %2 的 IP：%3")
                                        .arg(row + 1)
                                        .arg(device, interfaceAddress);
                }
                return false;
            }
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
                         m_networkInterfaceTable->item(row, IfAddress)->text().trimmed(),
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
        else if (fields.value(0) == QStringLiteral("ROUTE")
                 && (fields.size() == 5 || fields.size() == 6)) {
            QStringList route = fields.mid(1);
            while (route.size() < RouteColumnCount) route.append(QString());
            routes.append(route);
        }
    }
    for (int row = 0; row < 8; ++row) {
        const QString name = QStringLiteral("eth%1").arg(row);
        if (!interfaces.contains(name)) {
            if (errorMessage) *errorMessage = QStringLiteral("配置缺少 %1").arg(name);
            return false;
        }
        m_networkInterfaceTable->item(row, IfAddress)->setText(interfaces.value(name).first);
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
        "printf 'IF|%s|%s|%s|%s\\n' \"$d\" \"$s\" \"$m\" \"$a\"; done; "
        "if [ -r /etc/cepb/network.conf ]; then while IFS= read -r line || [ -n \"$line\" ]; do "
        "printf 'CF|%s\\n' \"$line\"; done < /etc/cepb/network.conf; fi");
    const auto result = SshClient::execCommand(connection, command, 30000);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("读取网络配置"), result.error);
        return;
    }
    QByteArray persistentConfig;
    QHash<int, QStringList> liveInterfaces;
    for (const QString &line : result.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.startsWith(QStringLiteral("CF|"))) {
            persistentConfig += line.mid(3).toUtf8();
            persistentConfig += '\n';
            continue;
        }
        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.size() < 5 || fields.value(0) != QStringLiteral("IF")) continue;
        bool rowOk = false;
        const int row = fields.at(1).mid(3).toInt(&rowOk);
        if (!rowOk || row < 0 || row >= 8) continue;
        liveInterfaces.insert(row, fields);
    }
    const auto showLiveInterfaces = [this, &liveInterfaces]() {
        for (int row = 0; row < 8; ++row) {
            auto *addressItem = m_networkInterfaceTable->item(row, IfAddress);
            addressItem->setData(LastReadAddressRole, QString());
            if (!liveInterfaces.contains(row)) {
                m_networkInterfaceTable->item(row, IfLink)->setText(QStringLiteral("-"));
                m_networkInterfaceTable->item(row, IfMac)->setText(QStringLiteral("-"));
                continue;
            }
            const QStringList fields = liveInterfaces.value(row);
            const QString cidr = fields.value(4);
            const int slash = cidr.indexOf(QLatin1Char('/'));
            m_networkInterfaceTable->item(row, IfLink)->setText(fields.value(2));
            m_networkInterfaceTable->item(row, IfMac)->setText(fields.value(3));
            if (slash > 0) {
                const QString currentAddress = cidr.left(slash);
                addressItem->setText(currentAddress);
                addressItem->setData(LastReadAddressRole, currentAddress);
                m_networkInterfaceTable->item(row, IfPrefix)->setText(cidr.mid(slash + 1));
            }
        }
    };
    if (!persistentConfig.isEmpty()) {
        QString configError;
        if (!loadNetworkConfigData(persistentConfig, &configError) || !validateNetworkConfig(&configError)) {
            showLiveInterfaces();
            QMessageBox::warning(m_networkConfigPage,
                                 QStringLiteral("读取网络配置"),
                                 QStringLiteral("实时网络状态已读取，但设备持久化配置无效：\n%1").arg(configError));
            return;
        }
    } else {
        m_networkRouteTable->setRowCount(0);
    }
    showLiveInterfaces();
    statusBar()->showMessage(QStringLiteral("网络配置读取完成，IP 列显示设备当前地址，可直接修改后应用"), 6000);
}

bool MainWindow::saveNetworkConfigToProject(QString *errorMessage) const
{
    const QString root = m_configImportDirEdit ? m_configImportDirEdit->text().trimmed() : QString();
    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        if (errorMessage) *errorMessage = QStringLiteral("请先在配置概览选择有效的工程目录。");
        return false;
    }
    QDir dir(root);
    if (!dir.mkpath(QStringLiteral("Network"))) {
        if (errorMessage) *errorMessage = QStringLiteral("无法创建项目 Network 目录。");
        return false;
    }
    QSaveFile file(dir.filePath(QStringLiteral("Network/network.conf")));
    if (!file.open(QIODevice::WriteOnly) || file.write(serializeNetworkConfig()) < 0 || !file.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("保存项目网络配置失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

void MainWindow::loadNetworkProjectIfAvailable()
{
    const QString root = m_configImportDirEdit ? m_configImportDirEdit->text().trimmed() : QString();
    if (root.isEmpty() || root == m_loadedNetworkProjectRoot) {
        return;
    }
    m_loadedNetworkProjectRoot = root;
    QFile file(QDir(root).filePath(QStringLiteral("Network/network.conf")));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QString error;
    if (!loadNetworkConfigData(file.readAll(), &error) || !validateNetworkConfig(&error)) {
        QMessageBox::warning(m_networkConfigPage,
                             QStringLiteral("加载项目网络配置"),
                             QStringLiteral("当前项目的 Network/network.conf 无效：\n%1").arg(error));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已加载当前项目的网络配置"), 4000);
}

QString MainWindow::localNetworkSupportFile(const QString &relativePath) const
{
    const QString appDir = QApplication::applicationDirPath();
    const QStringList candidates = {QDir(appDir).filePath(relativePath), QDir(appDir).filePath(QStringLiteral("../%1").arg(relativePath)),
                                    QDir::current().filePath(relativePath), QDir::current().filePath(QStringLiteral("../%1").arg(relativePath))};
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isFile() && info.size() > 0) return info.absoluteFilePath();
    }
    return QString();
}

void MainWindow::onApplyNetworkConfigClicked()
{
    QString error;
    if (!validateNetworkConfig(&error)) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), error);
        return;
    }
    const QString projectRoot = m_configImportDirEdit ? m_configImportDirEdit->text().trimmed() : QString();
    if (projectRoot.isEmpty() || !QFileInfo(projectRoot).isDir()
        || !QDir(projectRoot).mkpath(QStringLiteral("Network"))) {
        QMessageBox::warning(this,
                             QStringLiteral("应用网络配置"),
                             QStringLiteral("请先在配置概览选择可写的工程目录。应用成功后会自动保存项目网络配置。"));
        return;
    }
    QString helper = localNetworkSupportFile(QStringLiteral("scripts/device/cepb-network-apply"));
    QString service = localNetworkSupportFile(QStringLiteral("scripts/systemd/cepb-network.service"));
    QString cfgApplyDropIn = localNetworkSupportFile(QStringLiteral("scripts/systemd/cfg-apply-oneshot.conf"));
    if (helper.isEmpty()) helper = localNetworkSupportFile(QStringLiteral("device/cepb-network-apply"));
    if (service.isEmpty()) service = localNetworkSupportFile(QStringLiteral("systemd/cepb-network.service"));
    if (cfgApplyDropIn.isEmpty()) cfgApplyDropIn = localNetworkSupportFile(QStringLiteral("systemd/cfg-apply-oneshot.conf"));
    if (helper.isEmpty() || service.isEmpty() || cfgApplyDropIn.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("应用网络配置"),
            QStringLiteral("找不到设备端网络脚本、service 或 cfg-apply 时序配置，请检查发布包。"));
        return;
    }
    const QString oldHost = deviceHost();
    QString newHost = oldHost;
    for (int row = 0; row < 8; ++row) {
        const auto *addressItem = m_networkInterfaceTable->item(row, IfAddress);
        if (addressItem->data(LastReadAddressRole).toString().trimmed() == oldHost) {
            newHost = addressItem->text().trimmed();
            break;
        }
    }

    QString prompt = QStringLiteral("将上传并持久化当前网络配置，然后由设备异步应用。是否继续？");
    if (newHost != oldHost)
        prompt += QStringLiteral("\n\n当前管理地址将从 %1 改为 %2，SSH 会断开；提交后右上角设备IP将切换到新地址。").arg(oldHost, newHost);
    if (QMessageBox::warning(this, QStringLiteral("应用网络配置"), prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    QTemporaryFile configFile(QDir::temp().filePath(QStringLiteral("cepb_network_XXXXXX.conf")));
    const QByteArray serializedConfig = serializeNetworkConfig();
    if (!configFile.open() || configFile.write(serializedConfig) != serializedConfig.size()
        || !configFile.flush()) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), QStringLiteral("无法生成临时配置文件。"));
        return;
    }
    configFile.close();
    const auto fileSha256 = [](const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray();
        return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
    };
    const QFileInfo helperInfo(helper);
    const QFileInfo serviceInfo(service);
    const QFileInfo cfgApplyDropInInfo(cfgApplyDropIn);
    const QFileInfo configInfo(configFile.fileName());
    const QByteArray helperSha256 = fileSha256(helper);
    const QByteArray serviceSha256 = fileSha256(service);
    const QByteArray cfgApplyDropInSha256 = fileSha256(cfgApplyDropIn);
    const QByteArray configSha256 = fileSha256(configFile.fileName());
    if (helperInfo.size() <= 0 || serviceInfo.size() <= 0 || cfgApplyDropInInfo.size() <= 0
        || configInfo.size() <= 0 || helperSha256.isEmpty() || serviceSha256.isEmpty()
        || cfgApplyDropInSha256.isEmpty() || configSha256.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("应用网络配置"),
            QStringLiteral("本地网络支持文件为空或无法读取，已停止下发，设备配置未被修改。"));
        return;
    }

    const QString uploadToken = QUuid::createUuid().toString(QUuid::Id128);
    const QString remotePrefix = QStringLiteral("/tmp/cepb-network-%1").arg(uploadToken);
    const QString remoteHelper = remotePrefix + QStringLiteral(".helper");
    const QString remoteService = remotePrefix + QStringLiteral(".service");
    const QString remoteCfgApplyDropIn = remotePrefix + QStringLiteral(".dropin");
    const QString remoteConfig = remotePrefix + QStringLiteral(".conf");
    QProgressDialog progress(QStringLiteral("正在上传并校验网络配置..."), QString(), 0, 6, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    const SshClient::Connection connection{oldHost, fixedRemoteSshPort().toUShort(), fixedRemoteUser(), fixedRemotePassword()};
    QString output;
    bool ok = SshClient::uploadFileScp(connection, helper, remoteHelper, &output);
    progress.setValue(1);
    if (ok) ok = SshClient::uploadFileScp(connection, service, remoteService, &output);
    progress.setValue(2);
    if (ok) {
        ok = SshClient::uploadFileScp(connection, cfgApplyDropIn, remoteCfgApplyDropIn, &output);
    }
    progress.setValue(3);
    if (ok) ok = SshClient::uploadFileScp(connection, configFile.fileName(), remoteConfig, &output);
    progress.setValue(4);
    if (ok) {
        QString command = QStringLiteral("set -e; ");
        auto appendRemoteFileCheck = [&command](const QString &path, qint64 size, const QByteArray &sha256) {
            command += QStringLiteral(
                           "test \"$(wc -c < %1)\" -eq %2; "
                           "test \"$(sha256sum %1 | cut -d' ' -f1)\" = %3; ")
                           .arg(path)
                           .arg(size)
                           .arg(QString::fromLatin1(sha256));
        };
        appendRemoteFileCheck(remoteHelper, helperInfo.size(), helperSha256);
        appendRemoteFileCheck(remoteService, serviceInfo.size(), serviceSha256);
        appendRemoteFileCheck(remoteCfgApplyDropIn, cfgApplyDropInInfo.size(), cfgApplyDropInSha256);
        appendRemoteFileCheck(remoteConfig, configInfo.size(), configSha256);

        const QString installedHelperTmp =
            QStringLiteral("/usr/local/sbin/.cepb-network-apply.%1").arg(uploadToken);
        const QString installedServiceTmp =
            QStringLiteral("/etc/systemd/system/.cepb-network.service.%1").arg(uploadToken);
        const QString installedDropInTmp =
            QStringLiteral("/etc/systemd/system/cfg-apply.service.d/.10-cepb-network-ordering.%1")
                .arg(uploadToken);
        const QString installedConfigTmp =
            QStringLiteral("/etc/cepb/.network.conf.%1").arg(uploadToken);
        command += QStringLiteral(
                       "/bin/sh %1 check %2; "
                       "install -d -m 0755 /etc/cepb /usr/local/sbin /etc/systemd/system "
                       "/etc/systemd/system/cfg-apply.service.d; "
                       "install -m 0755 %1 %3; "
                       "install -m 0644 %4 %5; "
                       "install -m 0644 %6 %7; "
                       "install -m 0644 %2 %8; "
                       "mv -f %3 /usr/local/sbin/cepb-network-apply; "
                       "mv -f %5 /etc/systemd/system/cepb-network.service; "
                       "mv -f %7 /etc/systemd/system/cfg-apply.service.d/10-cepb-network-ordering.conf; "
                       "mv -f %8 /etc/cepb/network.conf; "
                       "sync; rm -f /run/cepb-network-apply.status; systemctl daemon-reload; "
                       "test \"$(systemctl show cfg-apply.service -p Type --value)\" = oneshot; "
                       "systemctl enable cepb-network.service; sync; "
                       "systemctl reset-failed cepb-network.service || true; "
                       "systemd-run --quiet --no-block --collect "
                       "--unit=cepb-network-apply-now-$(date +%s)-$$ "
                       "/bin/systemctl restart cepb-network.service; "
                       "rm -f %1 %2 %4 %6")
                       .arg(remoteHelper,
                            remoteConfig,
                            installedHelperTmp,
                            remoteService,
                            installedServiceTmp,
                            remoteCfgApplyDropIn,
                            installedDropInTmp,
                            installedConfigTmp);
        const auto result = SshClient::execCommand(connection, command, 30000);
        ok = result.ok;
        output = result.ok ? result.output : result.error;
    }
    progress.setValue(5);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("应用网络配置"), QStringLiteral("网络配置下发失败：\n%1").arg(output));
        return;
    }

    progress.setLabelText(QStringLiteral("设备正在应用网络配置，正在重新连接并核验结果..."));
    QStringList verificationHosts;
    verificationHosts.append(newHost);
    if (oldHost != newHost) verificationHosts.append(oldHost);
    bool applyVerified = false;
    bool applyReportedFailure = false;
    QString verificationDetail;
    QElapsedTimer verificationTimer;
    verificationTimer.start();
    while (verificationTimer.elapsed() < 45000 && !applyVerified && !applyReportedFailure) {
        for (const QString &host : verificationHosts) {
            SshClient::Connection verificationConnection{
                host, fixedRemoteSshPort().toUShort(), fixedRemoteUser(), fixedRemotePassword()};
            verificationConnection.timeoutMs = 2000;
            const auto result = SshClient::execCommand(
                verificationConnection,
                QStringLiteral(
                    "cat /run/cepb-network-apply.status 2>/dev/null || true; "
                    "printf 'SERVICE_ACTIVE|'; systemctl is-active cepb-network.service || true; "
                    "printf 'SERVICE_RESULT|'; "
                    "systemctl show cepb-network.service -p Result --value --no-page"),
                5000);
            if (!result.ok) {
                verificationDetail = result.error;
                continue;
            }

            verificationDetail = result.output.trimmed();
            if (verificationDetail.contains(QStringLiteral("STATE|success"))) {
                applyVerified = true;
                break;
            }
            if (verificationDetail.contains(QStringLiteral("STATE|failed"))) {
                applyReportedFailure = true;
                break;
            }
        }
        if (!applyVerified && !applyReportedFailure) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            QThread::msleep(1000);
        }
    }
    progress.setValue(6);
    if (!applyVerified) {
        const QString summary = applyReportedFailure
            ? QStringLiteral("设备已明确报告网络配置应用失败。")
            : QStringLiteral("45 秒内无法通过新旧管理地址确认设备应用结果。");
        QMessageBox::warning(
            this,
            QStringLiteral("应用网络配置"),
            QStringLiteral("%1\n\n配置文件已经写入设备，但本次不保存到项目，也不将状态标记为成功。\n\n%2")
                .arg(summary, verificationDetail));
        return;
    }

    QString saveError;
    const bool projectSaved = saveNetworkConfigToProject(&saveError);
    if (projectSaved) {
        m_loadedNetworkProjectRoot = projectRoot;
    }
    if (newHost != oldHost && m_ipEdit) m_ipEdit->setText(newHost);
    for (int row = 0; row < 8; ++row) {
        auto *addressItem = m_networkInterfaceTable->item(row, IfAddress);
        addressItem->setData(LastReadAddressRole, addressItem->text().trimmed());
    }
    statusBar()->showMessage(projectSaved
                                 ? QStringLiteral("网络配置已在设备应用成功，并保存到当前项目")
                                 : QStringLiteral("网络配置已在设备应用成功，但项目保存失败"),
                             8000);
    if (!projectSaved) {
        QMessageBox::warning(this,
                             QStringLiteral("应用网络配置"),
                             QStringLiteral("设备网络配置已应用成功，但自动保存到当前项目失败：\n%1").arg(saveError));
        return;
    }
    QMessageBox::information(this, QStringLiteral("应用网络配置"),
                              newHost == oldHost
                                  ? QStringLiteral("网络配置已在设备应用成功并保存到当前项目。")
                                  : QStringLiteral("网络配置已在设备应用成功并保存到当前项目，管理地址已切换为 %1。").arg(newHost));
}
