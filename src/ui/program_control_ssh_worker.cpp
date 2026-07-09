#include "program_control_ssh_worker.h"

ProgramControlSshWorker::ProgramControlSshWorker(QObject *parent)
    : QObject(parent)
{
}

ProgramControlSshWorker::~ProgramControlSshWorker() = default;

void ProgramControlSshWorker::connectSession(quint64 serial,
                                             const QString &key,
                                             const QString &host,
                                             quint16 port,
                                             const QString &user,
                                             const QString &password,
                                             int timeoutMs)
{
    const SshClient::Connection connection{host, port, user, password, timeoutMs};
    m_session = std::make_unique<SshClient::Session>(connection);

    QString error;
    if (!m_session->connect(&error)) {
        m_session.reset();
        emit connectFailed(serial, error);
        return;
    }

    emit connected(serial, key);
}

void ProgramControlSshWorker::disconnectSession()
{
    m_session.reset();
    emit disconnected();
}

void ProgramControlSshWorker::runCommand(quint64 serial,
                                         int kind,
                                         const QString &title,
                                         const QString &appName,
                                         const QString &command)
{
    SshClient::CommandResult result;
    if (!m_session || !m_session->isConnected()) {
        result.error = QStringLiteral("程序控制 SSH 已断开");
        result.output = result.error;
    } else {
        result = m_session->execCommand(command);
    }

    emit commandFinished(serial,
                         kind,
                         title,
                         appName,
                         result.ok ? 0 : result.exitCode,
                         result.output.isEmpty() ? result.error : result.output);
}

void ProgramControlSshWorker::runUpgrade(quint64 serial,
                                         int kind,
                                         const QString &title,
                                         const QString &appName,
                                         const QString &localPath,
                                         const QString &remoteTempPath,
                                         const QString &installCommand)
{
    SshClient::CommandResult result;
    if (!m_session || !m_session->isConnected()) {
        result.error = QStringLiteral("程序控制 SSH 已断开");
        result.output = result.error;
        emit commandFinished(serial, kind, title, appName, result.exitCode, result.output);
        return;
    }

    QString uploadError;
    const bool uploaded = m_session->uploadFileScp(
        localPath,
        remoteTempPath,
        &uploadError,
        [this, serial, appName](qint64 sent, qint64 total) {
            emit upgradeProgress(serial, appName, sent, total);
            return true;
        });
    if (!uploaded) {
        result.error = uploadError;
        result.output = result.error;
        emit commandFinished(serial, kind, title, appName, result.exitCode, result.output);
        return;
    }

    result = m_session->execCommand(installCommand);
    emit commandFinished(serial,
                         kind,
                         title,
                         appName,
                         result.ok ? 0 : result.exitCode,
                         result.output.isEmpty() ? result.error : result.output);
}

void ProgramControlSshWorker::runInstall(quint64 serial,
                                         int kind,
                                         const QString &title,
                                         const QString &appName,
                                         const QString &localBinaryPath,
                                         const QString &remoteBinaryTempPath,
                                         const QString &localServicePath,
                                         const QString &remoteServiceTempPath,
                                         const QString &installCommand)
{
    SshClient::CommandResult result;
    if (!m_session || !m_session->isConnected()) {
        result.error = QStringLiteral("程序控制 SSH 已断开");
        result.output = result.error;
        emit commandFinished(serial, kind, title, appName, result.exitCode, result.output);
        return;
    }

    QString uploadError;
    const bool binaryUploaded = m_session->uploadFileScp(
        localBinaryPath,
        remoteBinaryTempPath,
        &uploadError,
        [this, serial, appName](qint64 sent, qint64 total) {
            emit upgradeProgress(serial, appName, sent, total);
            return true;
        });
    if (!binaryUploaded) {
        result.error = uploadError;
        result.output = result.error;
        emit commandFinished(serial, kind, title, appName, result.exitCode, result.output);
        return;
    }

    const bool serviceUploaded = m_session->uploadFileScp(
        localServicePath,
        remoteServiceTempPath,
        &uploadError,
        [this, serial, appName](qint64 sent, qint64 total) {
            emit upgradeProgress(serial, appName, sent, total);
            return true;
        });
    if (!serviceUploaded) {
        result.error = uploadError;
        result.output = result.error;
        emit commandFinished(serial, kind, title, appName, result.exitCode, result.output);
        return;
    }

    result = m_session->execCommand(installCommand);
    emit commandFinished(serial,
                         kind,
                         title,
                         appName,
                         result.ok ? 0 : result.exitCode,
                         result.output.isEmpty() ? result.error : result.output);
}
