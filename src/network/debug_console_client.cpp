#include "debug_console_client.h"
#include <QNetworkProxy>
#include <QDebug>

DebugConsoleClient::DebugConsoleClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    m_socket->setProxy(QNetworkProxy::NoProxy);

    connect(m_socket, &QTcpSocket::readyRead,
            this, &DebugConsoleClient::onReadyRead);
    connect(m_socket, &QTcpSocket::connected,
            this, &DebugConsoleClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &DebugConsoleClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &DebugConsoleClient::onErrorOccurred);
}

void DebugConsoleClient::connectToHost(const QString &host, quint16 port)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qWarning() << "Socket not in UnconnectedState, aborting first.";
        m_socket->abort();
    }
    m_readBuffer.clear();
    m_executingCommand = false;
    m_commandReplyBuffer.clear();
    m_socket->connectToHost(host, port);
}

void DebugConsoleClient::disconnectFromHost()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->disconnectFromHost();
}

bool DebugConsoleClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void DebugConsoleClient::setPromptPattern(const QString &prompt)
{
    m_promptPattern = prompt.trimmed();
}

QString DebugConsoleClient::promptPattern() const
{
    return m_promptPattern;
}

void DebugConsoleClient::sendCommand(const QString &command)
{
    if (!isConnected()) {
        qWarning() << "Cannot send command: not connected";
        return;
    }
    m_executingCommand = true;
    m_commandReplyBuffer.clear();
    m_socket->write(command.toUtf8() + "\n");
    m_socket->flush();
}

void DebugConsoleClient::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());

    // 先处理以 \n 结尾的完整行
    while (true) {
        int nlIndex = m_readBuffer.indexOf('\n');
        if (nlIndex == -1)
            break;

        QByteArray lineBytes = m_readBuffer.left(nlIndex);
        m_readBuffer.remove(0, nlIndex + 1);

        if (!lineBytes.isEmpty() && lineBytes.endsWith('\r'))
            lineBytes.chop(1);

        processLine(QString::fromUtf8(lineBytes));
    }

    // 处理无换行结尾的 prompt（debug console 的 prompt 通常不跟 \n）
    flushPendingPrompt();
}

void DebugConsoleClient::processLine(const QString &line)
{
    if (isPrompt(line)) {
        if (m_executingCommand) {
            // 如果 reply buffer 为空，说明这是前置/冗余 prompt，继续等待
            if (m_commandReplyBuffer.isEmpty()) {
                return;
            }
            QString reply = m_commandReplyBuffer.join('\n');
            m_executingCommand = false;
            m_commandReplyBuffer.clear();
            emit commandReplyReceived(reply.trimmed());
        }
        return;
    }

    if (m_executingCommand) {
        m_commandReplyBuffer.append(line);
    } else {
        emit logLineReceived(line);
    }
}

bool DebugConsoleClient::isPrompt(const QString &line) const
{
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return false;

    if (!m_promptPattern.isEmpty()) {
        return trimmed == m_promptPattern;
    }

    // 启发式检测：常见 shell prompt 结尾字符
    return trimmed.endsWith('>') ||
           trimmed.endsWith('$') ||
           trimmed.endsWith('#') ||
           trimmed.endsWith(':');
}

void DebugConsoleClient::flushPendingPrompt()
{
    if (m_readBuffer.isEmpty())
        return;

    QString pending = QString::fromUtf8(m_readBuffer).trimmed();
    if (isPrompt(pending)) {
        processLine(pending);
        m_readBuffer.clear();
    }
}

void DebugConsoleClient::onSocketConnected()
{
    emit connected();
}

void DebugConsoleClient::onSocketDisconnected()
{
    m_executingCommand = false;
    m_commandReplyBuffer.clear();
    m_readBuffer.clear();
    emit disconnected();
}

void DebugConsoleClient::onErrorOccurred(QAbstractSocket::SocketError /*error*/)
{
    emit errorOccurred(m_socket->errorString());
}
