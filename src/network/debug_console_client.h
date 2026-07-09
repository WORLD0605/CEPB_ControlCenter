#ifndef DEBUG_CONSOLE_CLIENT_H
#define DEBUG_CONSOLE_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class DebugConsoleClient : public QObject
{
    Q_OBJECT
public:
    explicit DebugConsoleClient(QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    void setPromptPattern(const QString &prompt);
    QString promptPattern() const;

    // 发送命令，自动追加 \n
    void sendCommand(const QString &command);

    // 当前是否有命令正在等待回复
    bool isExecutingCommand() const { return m_executingCommand; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &errorString);

    // 主动推送的日志行（设备通过 broadcastLog 发送的内容）
    void logLineReceived(const QString &line);

    // 发送命令后的完整回复（不含 prompt，首尾已 trim）
    void commandReplyReceived(const QString &reply);

private slots:
    void onReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    void processLine(const QString &line);
    bool isPrompt(const QString &line) const;
    void flushPendingPrompt();

    QTcpSocket *m_socket = nullptr;
    QByteArray m_readBuffer;

    QString m_promptPattern; // 如 "South_104>"
    bool m_executingCommand = false;
    QStringList m_commandReplyBuffer;
};

#endif // DEBUG_CONSOLE_CLIENT_H
