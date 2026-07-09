#ifndef PROGRAM_CONTROL_SSH_WORKER_H
#define PROGRAM_CONTROL_SSH_WORKER_H

#include "network/ssh_client.h"

#include <QObject>
#include <QString>

#include <memory>

class ProgramControlSshWorker : public QObject
{
    Q_OBJECT
public:
    explicit ProgramControlSshWorker(QObject *parent = nullptr);
    ~ProgramControlSshWorker() override;

public slots:
    void connectSession(quint64 serial,
                        const QString &key,
                        const QString &host,
                        quint16 port,
                        const QString &user,
                        const QString &password,
                        int timeoutMs);
    void disconnectSession();
    void runCommand(quint64 serial,
                    int kind,
                    const QString &title,
                    const QString &appName,
                    const QString &command);
    void runUpgrade(quint64 serial,
                    int kind,
                    const QString &title,
                    const QString &appName,
                    const QString &localPath,
                    const QString &remoteTempPath,
                    const QString &installCommand);
    void runInstall(quint64 serial,
                    int kind,
                    const QString &title,
                    const QString &appName,
                    const QString &localBinaryPath,
                    const QString &remoteBinaryTempPath,
                    const QString &localServicePath,
                    const QString &remoteServiceTempPath,
                    const QString &installCommand);

signals:
    void connected(quint64 serial, const QString &key);
    void connectFailed(quint64 serial, const QString &message);
    void commandFinished(quint64 serial,
                         int kind,
                         const QString &title,
                         const QString &appName,
                         int exitCode,
                         const QString &output);
    void upgradeProgress(quint64 serial,
                         const QString &appName,
                         qint64 sentBytes,
                         qint64 totalBytes);
    void disconnected();

private:
    std::unique_ptr<SshClient::Session> m_session;
};

#endif
