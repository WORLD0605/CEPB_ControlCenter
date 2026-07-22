#include "ssh_client.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QNetworkProxy>
#include <QSaveFile>
#include <QtGlobal>
#include <QTcpSocket>

#include <memory>

#include <libssh2.h>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

class Libssh2Runtime
{
public:
    Libssh2Runtime()
    {
        QMutexLocker locker(&mutex());
        if (refCount()++ == 0) {
            libssh2_init(0);
        }
    }

    ~Libssh2Runtime()
    {
        QMutexLocker locker(&mutex());
        if (--refCount() == 0) {
            libssh2_exit();
        }
    }

private:
    static QMutex &mutex()
    {
        static QMutex value;
        return value;
    }

    static int &refCount()
    {
        static int value = 0;
        return value;
    }
};

QString sessionLastError(LIBSSH2_SESSION *session, const QString &fallback)
{
    if (!session) {
        return fallback;
    }

    char *message = nullptr;
    const int length = libssh2_session_last_error(session, &message, nullptr, 0);
    if (message && length > 0) {
        return QString::fromUtf8(message, length);
    }
    return fallback;
}

bool waitSocket(QTcpSocket &socket, LIBSSH2_SESSION *session, int timeoutMs)
{
    const qintptr descriptor = socket.socketDescriptor();
    if (descriptor < 0) {
        return false;
    }

    int direction = libssh2_session_block_directions(session);
    if (direction == 0) {
        direction = LIBSSH2_SESSION_BLOCK_INBOUND | LIBSSH2_SESSION_BLOCK_OUTBOUND;
    }

    const bool waitRead = direction & LIBSSH2_SESSION_BLOCK_INBOUND;
    const bool waitWrite = direction & LIBSSH2_SESSION_BLOCK_OUTBOUND;

    fd_set readSet;
    fd_set writeSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    if (waitRead) {
#ifdef Q_OS_WIN
        FD_SET(SOCKET(descriptor), &readSet);
#else
        FD_SET(descriptor, &readSet);
#endif
    }
    if (waitWrite) {
#ifdef Q_OS_WIN
        FD_SET(SOCKET(descriptor), &writeSet);
#else
        FD_SET(descriptor, &writeSet);
#endif
    }

    timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    const int rc = select(
#ifdef Q_OS_WIN
        0,
#else
        int(descriptor + 1),
#endif
        waitRead ? &readSet : nullptr,
        waitWrite ? &writeSet : nullptr,
        nullptr,
        &timeout);
    return rc > 0 && socket.state() == QAbstractSocket::ConnectedState;
}

template <typename Func>
int runLibssh2(Func func, QTcpSocket &socket, LIBSSH2_SESSION *session, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    int rc = 0;
    while ((rc = func()) == LIBSSH2_ERROR_EAGAIN) {
        const int remaining = timeoutMs <= 0 ? 10000 : qMax(1, timeoutMs - int(timer.elapsed()));
        if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
            return LIBSSH2_ERROR_TIMEOUT;
        }
        if (!waitSocket(socket, session, qMin(remaining, 10000))) {
            return LIBSSH2_ERROR_SOCKET_TIMEOUT;
        }
    }
    return rc;
}

class SshSession
{
public:
    bool connect(const SshClient::Connection &connection, QString *error)
    {
        runtime = std::make_unique<Libssh2Runtime>();

        socket.setProxy(QNetworkProxy::NoProxy);
        socket.connectToHost(connection.host, connection.port);
        if (!socket.waitForConnected(connection.timeoutMs)) {
            setError(error, QStringLiteral("连接 %1:%2 失败：%3")
                           .arg(connection.host)
                           .arg(connection.port)
                           .arg(socket.errorString()));
            return false;
        }

        session = libssh2_session_init();
        if (!session) {
            setError(error, QStringLiteral("创建 SSH session 失败"));
            return false;
        }

        libssh2_session_method_pref(session,
                                    LIBSSH2_METHOD_KEX,
                                    "diffie-hellman-group14-sha256,"
                                    "diffie-hellman-group16-sha512,"
                                    "diffie-hellman-group18-sha512,"
                                    "ecdh-sha2-nistp256");
        libssh2_session_method_pref(session,
                                    LIBSSH2_METHOD_HOSTKEY,
                                    "rsa-sha2-512,rsa-sha2-256,ecdsa-sha2-nistp256");
        libssh2_session_method_pref(session,
                                    LIBSSH2_METHOD_CRYPT_CS,
                                    "aes128-ctr,aes192-ctr,aes256-ctr");
        libssh2_session_method_pref(session,
                                    LIBSSH2_METHOD_CRYPT_SC,
                                    "aes128-ctr,aes192-ctr,aes256-ctr");
        libssh2_session_method_pref(session,
                                    LIBSSH2_METHOD_MAC_CS,
                                    "hmac-sha2-256,hmac-sha2-512,hmac-sha1");
        libssh2_session_method_pref(session,
                                    LIBSSH2_METHOD_MAC_SC,
                                    "hmac-sha2-256,hmac-sha2-512,hmac-sha1");

        libssh2_session_set_blocking(session, 0);
        const int handshake = runLibssh2(
            [this]() { return libssh2_session_handshake(session, socket.socketDescriptor()); },
            socket,
            session,
            connection.timeoutMs);
        if (handshake != 0) {
            setError(error, sessionLastError(session, QStringLiteral("SSH 握手失败")));
            return false;
        }

        const QByteArray user = connection.user.toUtf8();
        const QByteArray password = connection.password.toUtf8();
        const int auth = runLibssh2(
            [&]() { return libssh2_userauth_password(session, user.constData(), password.constData()); },
            socket,
            session,
            connection.timeoutMs);
        if (auth != 0) {
            setError(error, sessionLastError(session, QStringLiteral("SSH 用户名或密码认证失败")));
            return false;
        }

        return true;
    }

    ~SshSession()
    {
        disconnect();
    }

    void disconnect()
    {
        if (session) {
            runLibssh2([this]() { return libssh2_session_disconnect(session, "shutdown"); },
                       socket,
                       session,
                       3000);
            libssh2_session_free(session);
            session = nullptr;
        }
        socket.disconnectFromHost();
    }

    LIBSSH2_SESSION *raw() const { return session; }
    QTcpSocket &tcpSocket() { return socket; }
    bool isConnected() const
    {
        return session && socket.state() == QAbstractSocket::ConnectedState;
    }

private:
    static void setError(QString *error, const QString &message)
    {
        if (error) {
            *error = message;
        }
    }

    std::unique_ptr<Libssh2Runtime> runtime;
    QTcpSocket socket;
    LIBSSH2_SESSION *session = nullptr;
};

LIBSSH2_CHANNEL *openSessionChannel(SshSession &ssh, QString *error, int timeoutMs)
{
    LIBSSH2_CHANNEL *channel = nullptr;
    QElapsedTimer timer;
    timer.start();
    while (!channel) {
        channel = libssh2_channel_open_session(ssh.raw());
        if (channel) {
            return channel;
        }
        const int rc = libssh2_session_last_errno(ssh.raw());
        if (rc != LIBSSH2_ERROR_EAGAIN) {
            if (error) {
                *error = sessionLastError(ssh.raw(), QStringLiteral("打开 SSH channel 失败"));
            }
            return nullptr;
        }
        if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
            if (error) {
                *error = QStringLiteral("打开 SSH channel 超时");
            }
            return nullptr;
        }
        waitSocket(ssh.tcpSocket(), ssh.raw(), 10000);
    }
    return nullptr;
}

bool writeAllToChannel(SshSession &ssh,
                       LIBSSH2_CHANNEL *channel,
                       const QByteArray &data,
                       QString *error,
                       int timeoutMs)
{
    qsizetype offset = 0;
    QElapsedTimer timer;
    timer.start();
    while (offset < data.size()) {
        const char *chunk = data.constData() + offset;
        const qsizetype remaining = data.size() - offset;
        const ssize_t written = libssh2_channel_write(channel, chunk, size_t(remaining));
        if (written > 0) {
            offset += written;
            continue;
        }
        if (written == LIBSSH2_ERROR_EAGAIN) {
            if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
                if (error) {
                    *error = QStringLiteral("写入 SSH channel 超时");
                }
                return false;
            }
            waitSocket(ssh.tcpSocket(), ssh.raw(), 10000);
            continue;
        }
        if (error) {
            *error = sessionLastError(ssh.raw(), QStringLiteral("写入 SSH channel 失败"));
        }
        return false;
    }
    return true;
}

SshClient::CommandResult execCommandOnSession(SshSession &ssh,
                                              const QString &command,
                                              int timeoutMs)
{
    SshClient::CommandResult result;
    QString error;

    if (!ssh.isConnected()) {
        result.error = QStringLiteral("SSH session 未连接");
        result.output = result.error;
        return result;
    }

    LIBSSH2_CHANNEL *channel = openSessionChannel(ssh, &error, timeoutMs);
    if (!channel) {
        result.error = error;
        result.output = error;
        return result;
    }

    libssh2_channel_handle_extended_data2(channel, LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE);
    const QByteArray commandBytes = command.toUtf8();
    const int exec = runLibssh2(
        [&]() { return libssh2_channel_exec(channel, commandBytes.constData()); },
        ssh.tcpSocket(),
        ssh.raw(),
        timeoutMs);
    if (exec != 0) {
        result.error = sessionLastError(ssh.raw(), QStringLiteral("执行远程命令失败"));
        result.output = result.error;
        libssh2_channel_free(channel);
        return result;
    }

    QByteArray output;
    char buffer[8192];
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        const ssize_t read = libssh2_channel_read(channel, buffer, sizeof(buffer));
        if (read > 0) {
            output.append(buffer, read);
            continue;
        }
        if (read == LIBSSH2_ERROR_EAGAIN) {
            if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
                result.error = QStringLiteral("远程命令执行超时");
                break;
            }
            waitSocket(ssh.tcpSocket(), ssh.raw(), 10000);
            continue;
        }
        if (libssh2_channel_eof(channel)) {
            break;
        }
        if (read < 0) {
            result.error = sessionLastError(ssh.raw(), QStringLiteral("读取远程命令输出失败"));
            break;
        }
        waitSocket(ssh.tcpSocket(), ssh.raw(), 100);
    }

    runLibssh2([&]() { return libssh2_channel_close(channel); },
               ssh.tcpSocket(),
               ssh.raw(),
               10000);
    result.exitCode = libssh2_channel_get_exit_status(channel);
    libssh2_channel_free(channel);
    result.output = QString::fromUtf8(output);
    result.ok = result.error.isEmpty() && result.exitCode == 0;
    if (!result.ok && result.error.isEmpty()) {
        result.error = result.output.trimmed().isEmpty()
            ? QStringLiteral("远程命令退出码：%1").arg(result.exitCode)
            : result.output.trimmed();
    }
    return result;
}

bool uploadFileScpOnSession(SshSession &ssh,
                            const QString &localPath,
                            const QString &remotePath,
                            QString *error,
                            const std::function<bool(qint64, qint64)> &progressCallback,
                            int openTimeoutMs)
{
    if (!ssh.isConnected()) {
        if (error) {
            *error = QStringLiteral("SSH session 未连接");
        }
        return false;
    }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法打开本地文件 %1：%2").arg(localPath, file.errorString());
        }
        return false;
    }

    const qint64 totalSize = QFileInfo(file).size();
    qint64 sentSize = 0;
    if (progressCallback && !progressCallback(sentSize, totalSize)) {
        if (error) {
            *error = QStringLiteral("SCP upload canceled");
        }
        return false;
    }

    const QByteArray remote = remotePath.toUtf8();
    LIBSSH2_CHANNEL *channel = nullptr;
    QElapsedTimer timer;
    timer.start();
    while (!channel) {
        channel = libssh2_scp_send64(ssh.raw(),
                                     remote.constData(),
                                     0644,
                                     libssh2_uint64_t(QFileInfo(file).size()),
                                     0,
                                     0);
        if (channel) {
            break;
        }
        const int rc = libssh2_session_last_errno(ssh.raw());
        if (rc != LIBSSH2_ERROR_EAGAIN) {
            if (error) {
                *error = sessionLastError(ssh.raw(), QStringLiteral("打开远程 SCP 写入通道失败"));
            }
            return false;
        }
        if (timer.elapsed() >= openTimeoutMs) {
            if (error) {
                *error = QStringLiteral("打开远程 SCP 写入通道超时");
            }
            return false;
        }
        waitSocket(ssh.tcpSocket(), ssh.raw(), 10000);
    }

    while (!file.atEnd()) {
        const QByteArray chunk = file.read(16 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            if (error) {
                *error = QStringLiteral("读取本地文件失败：%1").arg(file.errorString());
            }
            libssh2_channel_free(channel);
            return false;
        }
        if (!writeAllToChannel(ssh, channel, chunk, error, 180000)) {
            libssh2_channel_free(channel);
            return false;
        }
        sentSize += chunk.size();
        if (progressCallback && !progressCallback(sentSize, totalSize)) {
            if (error) {
                *error = QStringLiteral("SCP upload canceled");
            }
            libssh2_channel_free(channel);
            return false;
        }
    }

    runLibssh2([&]() { return libssh2_channel_send_eof(channel); }, ssh.tcpSocket(), ssh.raw(), 10000);
    runLibssh2([&]() { return libssh2_channel_wait_eof(channel); }, ssh.tcpSocket(), ssh.raw(), 10000);
    runLibssh2([&]() { return libssh2_channel_wait_closed(channel); }, ssh.tcpSocket(), ssh.raw(), 10000);
    libssh2_channel_free(channel);
    return true;
}

bool downloadFileScpOnSession(SshSession &ssh,
                              const QString &remotePath,
                              const QString &localPath,
                              QString *error,
                              const std::function<bool(qint64, qint64)> &progressCallback,
                              int openTimeoutMs)
{
    if (!ssh.isConnected()) {
        if (error) {
            *error = QStringLiteral("SSH session 未连接");
        }
        return false;
    }

    const QByteArray remote = remotePath.toUtf8();
    libssh2_struct_stat fileInfo;
    LIBSSH2_CHANNEL *channel = nullptr;
    QElapsedTimer timer;
    timer.start();
    while (!channel) {
        channel = libssh2_scp_recv2(ssh.raw(), remote.constData(), &fileInfo);
        if (channel) {
            break;
        }
        const int rc = libssh2_session_last_errno(ssh.raw());
        if (rc != LIBSSH2_ERROR_EAGAIN) {
            if (error) {
                *error = sessionLastError(ssh.raw(), QStringLiteral("打开远程 SCP 读取通道失败"));
            }
            return false;
        }
        if (timer.elapsed() >= openTimeoutMs) {
            if (error) {
                *error = QStringLiteral("打开远程 SCP 读取通道超时");
            }
            return false;
        }
        waitSocket(ssh.tcpSocket(), ssh.raw(), 10000);
    }

    const qint64 totalSize = qint64(fileInfo.st_size);
    if (progressCallback && !progressCallback(0, totalSize)) {
        if (error) {
            *error = QStringLiteral("SCP download canceled");
        }
        libssh2_channel_free(channel);
        return false;
    }

    QSaveFile file(localPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("无法创建本地文件 %1：%2").arg(localPath, file.errorString());
        }
        libssh2_channel_free(channel);
        return false;
    }

    qint64 received = 0;
    char buffer[64 * 1024];
    while (received < totalSize) {
        const ssize_t read = libssh2_channel_read(channel, buffer, sizeof(buffer));
        if (read > 0) {
            if (file.write(buffer, read) != read) {
                if (error) {
                    *error = QStringLiteral("写入本地文件失败：%1").arg(file.errorString());
                }
                file.cancelWriting();
                libssh2_channel_free(channel);
                return false;
            }
            received += qint64(read);
            if (progressCallback && !progressCallback(received, totalSize)) {
                if (error) {
                    *error = QStringLiteral("SCP download canceled");
                }
                file.cancelWriting();
                libssh2_channel_free(channel);
                return false;
            }
            continue;
        }
        if (read == LIBSSH2_ERROR_EAGAIN) {
            waitSocket(ssh.tcpSocket(), ssh.raw(), 10000);
            continue;
        }
        if (error) {
            *error = sessionLastError(ssh.raw(), QStringLiteral("读取远程文件失败"));
        }
        file.cancelWriting();
        libssh2_channel_free(channel);
        return false;
    }

    runLibssh2([&]() { return libssh2_channel_send_eof(channel); }, ssh.tcpSocket(), ssh.raw(), 10000);
    runLibssh2([&]() { return libssh2_channel_wait_eof(channel); }, ssh.tcpSocket(), ssh.raw(), 10000);
    runLibssh2([&]() { return libssh2_channel_wait_closed(channel); }, ssh.tcpSocket(), ssh.raw(), 10000);
    libssh2_channel_free(channel);

    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("提交本地文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace

class SshClient::Session::Impl
{
public:
    explicit Impl(const Connection &connection)
        : connection(connection)
    {
    }

    Connection connection;
    std::unique_ptr<SshSession> ssh;
};

SshClient::Session::Session(const Connection &connection)
    : d(std::make_unique<Impl>(connection))
{
}

SshClient::Session::~Session() = default;

bool SshClient::Session::connect(QString *error)
{
    disconnect();
    auto ssh = std::make_unique<SshSession>();
    if (!ssh->connect(d->connection, error)) {
        return false;
    }
    d->ssh = std::move(ssh);
    return true;
}

void SshClient::Session::disconnect()
{
    d->ssh.reset();
}

bool SshClient::Session::isConnected() const
{
    return d->ssh && d->ssh->isConnected();
}

SshClient::CommandResult SshClient::Session::execCommand(const QString &command, int timeoutMs)
{
    if (!d->ssh) {
        CommandResult result;
        result.error = QStringLiteral("SSH session 未连接");
        result.output = result.error;
        return result;
    }
    return execCommandOnSession(*d->ssh, command, timeoutMs);
}

bool SshClient::Session::uploadFileScp(const QString &localPath,
                                       const QString &remotePath,
                                       QString *error,
                                       const std::function<bool(qint64, qint64)> &progressCallback)
{
    if (!d->ssh) {
        if (error) {
            *error = QStringLiteral("SSH session 未连接");
        }
        return false;
    }
    return uploadFileScpOnSession(*d->ssh, localPath, remotePath, error, progressCallback, d->connection.timeoutMs);
}

bool SshClient::Session::downloadFileScp(
    const QString &remotePath,
    const QString &localPath,
    QString *error,
    const std::function<bool(qint64, qint64)> &progressCallback)
{
    if (!d->ssh) {
        if (error) {
            *error = QStringLiteral("SSH session 未连接");
        }
        return false;
    }
    return downloadFileScpOnSession(*d->ssh,
                                    remotePath,
                                    localPath,
                                    error,
                                    progressCallback,
                                    d->connection.timeoutMs);
}

SshClient::CommandResult SshClient::execCommand(const Connection &connection,
                                                const QString &command,
                                                int timeoutMs)
{
    CommandResult result;
    QString error;
    SshSession ssh;
    if (!ssh.connect(connection, &error)) {
        result.error = error;
        result.output = error;
        return result;
    }

    return execCommandOnSession(ssh, command, timeoutMs);
}

bool SshClient::uploadFileScp(const Connection &connection,
                              const QString &localPath,
                              const QString &remotePath,
                              QString *error,
                              const std::function<bool(qint64, qint64)> &progressCallback)
{
    SshSession ssh;
    if (!ssh.connect(connection, error)) {
        return false;
    }

    return uploadFileScpOnSession(ssh, localPath, remotePath, error, progressCallback, connection.timeoutMs);
}

bool SshClient::downloadFileScp(const Connection &connection,
                                const QString &remotePath,
                                const QString &localPath,
                                QString *error,
                                const std::function<bool(qint64, qint64)> &progressCallback)
{
    SshSession ssh;
    if (!ssh.connect(connection, error)) {
        return false;
    }
    return downloadFileScpOnSession(ssh,
                                    remotePath,
                                    localPath,
                                    error,
                                    progressCallback,
                                    connection.timeoutMs);
}
