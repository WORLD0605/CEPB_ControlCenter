#ifndef SSH_CLIENT_H
#define SSH_CLIENT_H

#include <functional>
#include <memory>

#include <QtGlobal>
#include <QString>

class SshClient
{
public:
    struct Connection {
        QString host;
        quint16 port = 10022;
        QString user = QStringLiteral("root");
        QString password = QStringLiteral("root");
        int timeoutMs = 10000;
    };

    struct CommandResult {
        bool ok = false;
        int exitCode = -1;
        QString output;
        QString error;
    };

    class Session
    {
    public:
        explicit Session(const Connection &connection);
        ~Session();

        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;

        bool connect(QString *error = nullptr);
        void disconnect();
        bool isConnected() const;
        CommandResult execCommand(const QString &command, int timeoutMs = 180000);
        bool uploadFileScp(const QString &localPath,
                           const QString &remotePath,
                           QString *error = nullptr,
                           const std::function<bool(qint64, qint64)> &progressCallback = {});

    private:
        class Impl;
        std::unique_ptr<Impl> d;
    };

    static CommandResult execCommand(const Connection &connection,
                                     const QString &command,
                                     int timeoutMs = 180000);
    static bool uploadFileScp(const Connection &connection,
                              const QString &localPath,
                              const QString &remotePath,
                              QString *error = nullptr,
                              const std::function<bool(qint64, qint64)> &progressCallback = {});
    static bool downloadFileScp(const Connection &connection,
                                const QString &remotePath,
                                const QString &localPath,
                                QString *error = nullptr);

private:
    SshClient() = default;
};

#endif
