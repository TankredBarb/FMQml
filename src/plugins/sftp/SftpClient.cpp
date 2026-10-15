#include "SftpClient.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <QScopeGuard>
#include <fcntl.h>
#include <chrono>
#include <deque>
#include <algorithm>
#include <utility>
#include <sys/socket.h>
#include <sys/stat.h>
#include <poll.h>
#include <cerrno>

namespace {
QMutex credentialsMutex;
QHash<QString, SftpCredentials> activeCredentials;
QMutex knownHostsMutex;
QSemaphore connectionPermits(4);

qint64 nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

FileEntry entryFor(const SftpPath::Address &address, const QString &name, sftp_attributes attrs)
{
    FileEntry entry;
    entry.name = name;
    entry.path = address.encoded();
    entry.size = static_cast<qint64>(attrs->size);
    entry.isDirectory = attrs->type == SSH_FILEXFER_TYPE_DIRECTORY;
    entry.isSymLink = attrs->type == SSH_FILEXFER_TYPE_SYMLINK;
    entry.isSystem = !entry.isDirectory && !entry.isSymLink && attrs->type != SSH_FILEXFER_TYPE_REGULAR;
    entry.isHidden = name.startsWith(QLatin1Char('.'));
    entry.suffix = name.contains(QLatin1Char('.')) ? name.section(QLatin1Char('.'), -1).toLower() : QString{};
    entry.modified = QDateTime::fromSecsSinceEpoch(attrs->mtime);
    entry.modifiedText = entry.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    entry.attributesText = QString::number(attrs->permissions & 07777, 8);
    entry.iconName = entry.isDirectory ? QStringLiteral("folder") : QString{};
    return entry;
}
}

namespace SftpSessions {
void remember(const SftpCredentials &credentials)
{
    QMutexLocker lock(&credentialsMutex);
    auto &saved = activeCredentials[credentials.address.identity()];
    if (saved.revoked) saved.revoked->store(true);
    saved = credentials;
    saved.revoked = std::make_shared<std::atomic_bool>(false);
}

std::optional<SftpCredentials> credentials(const SftpPath::Address &address)
{
    QMutexLocker lock(&credentialsMutex);
    const auto found = activeCredentials.constFind(address.identity());
    if (found == activeCredentials.cend()) return std::nullopt;
    return *found;
}

void forget(const QString &identity)
{
    QMutexLocker lock(&credentialsMutex);
    const auto found = activeCredentials.constFind(identity);
    if (found != activeCredentials.cend() && found->revoked) found->revoked->store(true);
    activeCredentials.remove(identity);
}

QList<SftpPath::Address> addresses()
{
    QMutexLocker lock(&credentialsMutex);
    QList<SftpPath::Address> result;
    for (const auto &credentials : activeCredentials) result.append(credentials.address);
    return result;
}
}

SftpClient::SftpClient(std::function<bool()> cancelled, const QString &knownHostsFile)
    : m_cancelled(std::move(cancelled)), m_knownHostsFile(knownHostsFile) {}

SftpClient::~SftpClient()
{
    if (m_session && ssh_get_fd(m_session) != SSH_INVALID_SOCKET)
        ::shutdown(ssh_get_fd(m_session), SHUT_RDWR);
    if (m_watchdog.joinable()) {
        m_watchdog.request_stop();
        m_watchdog.join();
    }
    if (m_sftp) sftp_free(m_sftp);
    if (m_session) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
    }
    if (m_hasPermit) connectionPermits.release();
}

bool SftpClient::cancelled() const
{
    return (m_cancelled && m_cancelled()) || (m_revoked && m_revoked->load());
}
void SftpClient::touch() { m_activity.store(nowMs()); }
bool SftpClient::fail(const QString &message) { m_error = message; return false; }

QString SftpClient::serverError(const QString &operation) const
{
    // Server-provided text may contain credentials or other untrusted strings.
    if (cancelled()) return QStringLiteral("SFTP request cancelled.");
    if (nowMs() - m_activity.load() >= 15000) return QStringLiteral("SFTP request timed out.");
    const int code = m_sftp ? sftp_get_error(m_sftp) : -1;
    if (code == SSH_FX_PERMISSION_DENIED) return QStringLiteral("SFTP permission denied: %1.").arg(operation);
    if (code == SSH_FX_NO_SUCH_FILE || code == SSH_FX_NO_SUCH_PATH)
        return QStringLiteral("SFTP path does not exist: %1.").arg(operation);
    return QStringLiteral("SFTP request failed: %1 (code %2).").arg(operation).arg(code);
}

bool SftpClient::connect(const SftpCredentials &credentials, const QString &acceptedFingerprint)
{
    if (!credentials.address.valid()) return fail(credentials.address.error);
    m_revoked = credentials.revoked;
    if (credentials.address.url.userName().isEmpty()) return fail(QStringLiteral("Enter your SSH login in Settings > Files > SFTP."));
    if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
    const qint64 permitStart = nowMs();
    while (!connectionPermits.tryAcquire(1, 50)) {
        if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
        if (nowMs() - permitStart >= 15000) return fail(QStringLiteral("SFTP connection limit reached. Try again after another request finishes."));
    }
    m_hasPermit = true;
    m_session = ssh_new();
    if (!m_session) return fail(QStringLiteral("Cannot create an SSH session."));
    const QByteArray host = credentials.address.url.host().toUtf8();
    const QByteArray user = credentials.address.url.userName(QUrl::FullyDecoded).toUtf8();
    const unsigned int port = credentials.address.url.port(22);
    const long timeout = 10;
    const bool processConfig = false;
    if (ssh_options_set(m_session, SSH_OPTIONS_PROCESS_CONFIG, &processConfig) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_HOST, host.constData()) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData()) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_PORT, &port) != SSH_OK
        || ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeout) != SSH_OK)
        return fail(QStringLiteral("Cannot configure the SSH connection."));
    if (!m_knownHostsFile.isEmpty()
        && ssh_options_set(m_session, SSH_OPTIONS_KNOWNHOSTS, m_knownHostsFile.toUtf8().constData()) != SSH_OK)
        return fail(QStringLiteral("Cannot configure the SSH known_hosts file."));
    touch();
    // Keep TCP connection and SSH handshake cancellable on this worker. The
    // socket watchdog below can only start once libssh has connected.
    ssh_set_blocking(m_session, 0);
    const qint64 connectStarted = nowMs();
    int connected;
    while ((connected = ssh_connect(m_session)) == SSH_AGAIN) {
        if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
        if (nowMs() - connectStarted >= timeout * 1000)
            return fail(QStringLiteral("Cannot connect to the SSH server. Check the host, port and network."));
        const int flags = ssh_get_poll_flags(m_session);
        const short events = ((flags & SSH_READ_PENDING) ? POLLIN : 0)
                           | ((flags & SSH_WRITE_PENDING) ? POLLOUT : 0);
        pollfd descriptor{ssh_get_fd(m_session), events, 0};
        if (::poll(&descriptor, 1, 25) < 0 && errno != EINTR)
            return fail(QStringLiteral("Cannot connect to the SSH server. Check the host, port and network."));
    }
    if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
    if (connected != SSH_OK) return fail(QStringLiteral("Cannot connect to the SSH server. Check the host, port and network."));
    ssh_set_blocking(m_session, 1);
    const socket_t socket = ssh_get_fd(m_session);
    touch();
    m_watchdog = std::jthread([this, socket](std::stop_token stop) {
        while (!stop.stop_requested()) {
            if ((!m_cleaningUp.load() && cancelled()) || nowMs() - m_activity.load() >= 15000) {
                ::shutdown(socket, SHUT_RDWR);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    });

    ssh_key key = nullptr;
    unsigned char *hash = nullptr;
    size_t hashLength = 0;
    if (ssh_get_server_publickey(m_session, &key) != SSH_OK)
        return fail(QStringLiteral("Cannot read the SSH server key."));
    const int hashResult = ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &hashLength);
    ssh_key_free(key);
    if (hashResult != SSH_OK) return fail(QStringLiteral("Cannot fingerprint the SSH server key."));
    char *fingerprint = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hashLength);
    ssh_clean_pubkey_hash(&hash);
    m_fingerprint = QString::fromLatin1(fingerprint);
    ssh_string_free_char(fingerprint);
    if (m_fingerprint.isEmpty()) return fail(QStringLiteral("Cannot fingerprint the SSH server key."));
    QMutexLocker knownHostsLock(&knownHostsMutex);
    const auto known = ssh_session_is_known_server(m_session);
    if (known == SSH_KNOWN_HOSTS_CHANGED || known == SSH_KNOWN_HOSTS_OTHER)
        return fail(QStringLiteral("The SSH server key has changed. Verify the server before updating known_hosts."));
    if (known == SSH_KNOWN_HOSTS_ERROR) return fail(QStringLiteral("Cannot verify the SSH server in known_hosts."));
    if (!acceptedFingerprint.isEmpty() && acceptedFingerprint != m_fingerprint)
        return fail(QStringLiteral("The SSH server key changed after confirmation. Connection refused."));
    if (known != SSH_KNOWN_HOSTS_OK) {
        if (acceptedFingerprint.isEmpty()) {
            m_needsHostConfirmation = true;
            return fail(QStringLiteral("Confirm the SSH server fingerprint before signing in."));
        }
        if (ssh_session_update_known_hosts(m_session) != SSH_OK)
            return fail(QStringLiteral("Cannot save the confirmed SSH server to known_hosts."));
    }
    knownHostsLock.unlock();
    if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
    touch();
    const QByteArray password = credentials.password.toUtf8();
    int auth = ssh_userauth_password(m_session, nullptr, password.constData());
    if (auth == SSH_AUTH_DENIED && (ssh_userauth_list(m_session, nullptr) & SSH_AUTH_METHOD_INTERACTIVE)) {
        // Accept one hidden password prompt only; arbitrary MFA is not supported.
        for (int round = 0; round < 3; ++round) {
            auth = ssh_userauth_kbdint(m_session, nullptr, nullptr);
            if (auth != SSH_AUTH_INFO) break;
            char echo = 1;
            const char *prompt = ssh_userauth_kbdint_getprompt(m_session, 0, &echo);
            if (ssh_userauth_kbdint_getnprompts(m_session) != 1 || !prompt || echo
                || !QString::fromUtf8(prompt).contains(QLatin1String("password"), Qt::CaseInsensitive))
                return fail(QStringLiteral("The server requested unsupported interactive authentication."));
            if (ssh_userauth_kbdint_setanswer(m_session, 0, password.constData()) < 0)
                return fail(QStringLiteral("Cannot answer the SSH password prompt."));
        }
    }
    if (auth != SSH_AUTH_SUCCESS) return fail(cancelled() ? QStringLiteral("SFTP request cancelled.")
        : QStringLiteral("SSH login failed. Check your login/password and whether password authentication is enabled."));
    touch();
    m_sftp = sftp_new(m_session);
    if (!m_sftp || sftp_init(m_sftp) != SSH_OK)
        return fail(QStringLiteral("SSH login succeeded, but the SFTP subsystem is unavailable."));
    return !cancelled() || fail(QStringLiteral("SFTP request cancelled."));
}

bool SftpClient::list(const SftpPath::Address &address, bool includeHidden,
                      const std::function<void(const QList<FileEntry> &)> &publish)
{
    touch();
    const QByteArray path = address.url.path(QUrl::FullyDecoded).toUtf8();
    sftp_dir directory = sftp_opendir(m_sftp, path.constData());
    if (!directory) return fail(serverError(QStringLiteral("open folder")));
    QList<FileEntry> batch;
    bool success = true;
    while (!cancelled()) {
        touch();
        sftp_attributes attrs = sftp_readdir(m_sftp, directory);
        if (!attrs) {
            success = sftp_dir_eof(directory) != 0;
            if (!success) fail(serverError(QStringLiteral("list folder")));
            break;
        }
        const QString name = QString::fromUtf8(attrs->name);
        if (name != QLatin1String(".") && name != QLatin1String("..")
            && (includeHidden || !name.startsWith(QLatin1Char('.')))) {
            const auto child = SftpPath::parse(SftpPath::child(address.encoded(), name));
            if (child.valid()) {
                auto entry = entryFor(child, name, attrs);
                resolveDirectoryLink(entry);
                batch.append(entry);
            }
        }
        sftp_attributes_free(attrs);
        if (batch.size() >= 128) {
            publish(batch);
            batch.clear();
        }
    }
    if (cancelled()) success = fail(QStringLiteral("SFTP request cancelled."));
    touch();
    if (sftp_closedir(directory) != SSH_OK && success) success = fail(serverError(QStringLiteral("close folder")));
    if (success && !batch.isEmpty()) publish(batch);
    return success;
}

std::optional<FileEntry> SftpClient::stat(const SftpPath::Address &address)
{
    touch();
    const QByteArray path = address.url.path(QUrl::FullyDecoded).toUtf8();
    sftp_attributes attrs = sftp_lstat(m_sftp, path.constData());
    if (!attrs) {
        fail(serverError(QStringLiteral("read metadata")));
        return std::nullopt;
    }
    FileEntry entry = entryFor(address, SftpPath::fileName(address.encoded()), attrs);
    sftp_attributes_free(attrs);
    resolveDirectoryLink(entry);
    return entry;
}

void SftpClient::resolveDirectoryLink(FileEntry &entry)
{
    if (!entry.isSymLink || cancelled()) return;
    touch();
    auto target = sftp_stat(m_sftp, SftpPath::parse(entry.path).url.path(QUrl::FullyDecoded).toUtf8().constData());
    if (!target) return; // Keep broken links visible without following them.
    entry.isDirectory = target->type == SSH_FILEXFER_TYPE_DIRECTORY;
    if (entry.isDirectory) entry.iconName = QStringLiteral("folder");
    sftp_attributes_free(target);
}

bool SftpClient::downloadToFile(const SftpPath::Address &address, const QString &destination,
                              const std::function<bool(qint64, qint64)> &progress)
{
    const auto entry = stat(address);
    if (!entry) return false;
    if (entry->isDirectory || entry->isSymLink || entry->isSystem)
        return fail(QStringLiteral("SFTP downloads require a regular file; symbolic links are not copied."));
    const QFileInfo target(destination);
    if (target.isSymLink() || target.isDir())
        return fail(QStringLiteral("Cannot replace a directory or symbolic link with a downloaded file."));
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) return fail(output.errorString());
    touch();
    sftp_file input = sftp_open(m_sftp, address.url.path(QUrl::FullyDecoded).toUtf8().constData(), O_RDONLY, 0);
    if (!input) return fail(serverError(QStringLiteral("open file for download")));
    touch();
    sftp_attributes opened = sftp_fstat(input);
    if (!opened || opened->type != SSH_FILEXFER_TYPE_REGULAR) {
        const QString error = opened ? QStringLiteral("SFTP downloads require a regular file.")
                                     : serverError(QStringLiteral("read opened file metadata"));
        if (opened) sftp_attributes_free(opened);
        sftp_close(input);
        return fail(error);
    }
    const qint64 total = static_cast<qint64>(opened->size);
    const unsigned int permissions = opened->permissions & 0777;
    sftp_attributes_free(opened);
    if (!target.exists()) {
        const std::pair<unsigned int, QFileDevice::Permission> modes[] = {
            {0400, QFileDevice::ReadOwner}, {0200, QFileDevice::WriteOwner}, {0100, QFileDevice::ExeOwner},
            {0040, QFileDevice::ReadGroup}, {0020, QFileDevice::WriteGroup}, {0010, QFileDevice::ExeGroup},
            {0004, QFileDevice::ReadOther}, {0002, QFileDevice::WriteOther}, {0001, QFileDevice::ExeOther}
        };
        QFileDevice::Permissions localPermissions;
        for (const auto &[mode, permission] : modes) {
            if (permissions & mode) localPermissions |= permission;
        }
        if (!output.setPermissions(localPermissions)) {
            sftp_close(input);
            return fail(QStringLiteral("Cannot set permissions on the downloaded file."));
        }
    }
    QByteArray buffer(32768, Qt::Uninitialized);
    qint64 processed = 0;
    bool success = true;
    {
        struct PendingRead { sftp_aio request = nullptr; qint64 bytes = 0; qint64 offset = 0; };
        std::deque<PendingRead> pending;
        const auto releasePending = qScopeGuard([&]() {
            for (const auto &read : pending) if (read.request) sftp_aio_free(read.request);
        });
        qint64 submitted = 0;
        while (success && processed < total && !cancelled()) {
            if (progress && !progress(processed, total)) { success = fail(QStringLiteral("SFTP download cancelled.")); break; }
            while (pending.size() < 16 && submitted < total) {
                if (cancelled()) { success = fail(QStringLiteral("SFTP download cancelled.")); break; }
                pending.emplace_back();
                auto &read = pending.back();
                read.offset = submitted;
                touch();
                read.bytes = sftp_aio_begin_read(input, std::min<qint64>(buffer.size(), total - submitted), &read.request);
                if (read.bytes <= 0) { success = fail(serverError(QStringLiteral("read file"))); break; }
                submitted += read.bytes;
            }
            if (!success) break;
            auto &read = pending.front();
            touch();
            const qint64 count = sftp_aio_wait_read(&read.request, buffer.data(), buffer.size());
            if (count < 0) { success = fail(serverError(QStringLiteral("read file"))); break; }
            if (count == 0) {
                success = fail(QStringLiteral("The SFTP source changed size during download. Destination was preserved.")); break;
            }
            if (output.write(buffer.constData(), count) != count) { success = fail(output.errorString()); break; }
            processed += count;
            if (count < read.bytes) {
                // A server may return a short read before EOF. Fetch the gap
                // before consuming later responses, preserving file order.
                const qint64 remaining = read.bytes - count;
                read.offset += count;
                sftp_seek64(input, read.offset);
                read.bytes = sftp_aio_begin_read(input, remaining, &read.request);
                sftp_seek64(input, submitted);
                if (read.bytes != remaining) { success = fail(serverError(QStringLiteral("read file"))); break; }
            } else pending.pop_front();
        }
        if (success && !cancelled()) {
            touch();
            const qint64 extra = sftp_read(input, buffer.data(), 1);
            if (extra < 0) success = fail(serverError(QStringLiteral("read file")));
            else if (extra > 0) success = fail(QStringLiteral("The SFTP source changed size during download. Destination was preserved."));
        }
    }
    m_cleaningUp.store(true);
    const auto finishCleanup = qScopeGuard([&]() { m_cleaningUp.store(false); });
    touch();
    const int closed = sftp_close(input);
    if (success && closed != SSH_OK) success = fail(serverError(QStringLiteral("close downloaded file")));
    if (success && (cancelled() || (progress && !progress(processed, total))))
        success = fail(QStringLiteral("SFTP download cancelled."));
    if (success && processed != total)
        success = fail(QStringLiteral("The SFTP source changed size during download. Destination was preserved."));
    if (!success) return false; // QSaveFile removes only its own temporary file.
    if (!output.commit()) return fail(output.errorString());
    return true;
}

bool SftpClient::uploadFromFile(const QString &source, const SftpPath::Address &address,
                              const std::function<bool(qint64, qint64)> &progress)
{
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile() || sourceInfo.isSymLink())
        return fail(QStringLiteral("SFTP uploads require a regular local file; symbolic links are not copied."));
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) return fail(input.errorString());
    struct stat sourceMetadata {};
    if (::fstat(input.handle(), &sourceMetadata) != 0)
        return fail(QStringLiteral("Cannot read local source file permissions."));
    const qint64 total = input.size();
    const QByteArray destination = address.url.path(QUrl::FullyDecoded).toUtf8();
    touch();
    sftp_attributes existing = sftp_lstat(m_sftp, destination.constData());
    unsigned int permissions = sourceMetadata.st_mode & 0777;
    if (existing) {
        const bool regular = existing->type == SSH_FILEXFER_TYPE_REGULAR;
        permissions = existing->permissions & 0777;
        sftp_attributes_free(existing);
        if (!regular) return fail(QStringLiteral("Cannot replace a directory or symbolic link with an uploaded file."));
        if (!sftp_extension_supported(m_sftp, "posix-rename@openssh.com", "1"))
            return fail(QStringLiteral("This SFTP server does not support atomic file replacement. The existing file was preserved."));
    } else if (sftp_get_error(m_sftp) != SSH_FX_NO_SUCH_FILE && sftp_get_error(m_sftp) != SSH_FX_NO_SUCH_PATH) {
        return fail(serverError(QStringLiteral("check upload destination")));
    }
    if (cancelled() || (progress && !progress(0, total))) return fail(QStringLiteral("SFTP upload cancelled."));
    const QString temporaryUrl = SftpPath::child(SftpPath::parent(address.encoded()),
        QStringLiteral(".fm-upload-%1.part").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const auto temporary = SftpPath::parse(temporaryUrl);
    const QByteArray temporaryPath = temporary.url.path(QUrl::FullyDecoded).toUtf8();
    touch();
    sftp_file output = sftp_open(m_sftp, temporaryPath.constData(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (!output) return fail(serverError(QStringLiteral("create upload temporary file")));
    bool committed = false;
    const auto cleanup = qScopeGuard([&]() {
        m_cleaningUp.store(true);
        touch();
        if (output) sftp_close(output);
        if (!committed && sftp_unlink(m_sftp, temporaryPath.constData()) != SSH_OK)
            m_error += QStringLiteral(" Temporary file may remain on the server: %1").arg(temporaryUrl);
        m_cleaningUp.store(false);
    });
    struct PendingWrite { sftp_aio request = nullptr; qint64 bytes = 0; };
    std::deque<PendingWrite> pending;
    const auto releasePending = qScopeGuard([&]() {
        for (const auto &write : pending) if (write.request) sftp_aio_free(write.request);
    });
    constexpr size_t maxInFlight = 16;
    QByteArray buffer(32768, Qt::Uninitialized);
    qint64 buffered = 0;
    qint64 bufferOffset = 0;
    qint64 submitted = 0;
    qint64 processed = 0;
    while (processed < total) {
        if (cancelled() || (progress && !progress(processed, total))) return fail(QStringLiteral("SFTP upload cancelled."));
        while (pending.size() < maxInFlight && submitted < total) {
            if (cancelled()) return fail(QStringLiteral("SFTP upload cancelled."));
            if (bufferOffset == buffered) {
                buffered = input.read(buffer.data(), std::min<qint64>(buffer.size(), total - submitted));
                bufferOffset = 0;
                if (buffered <= 0) return fail(input.error() == QFile::NoError
                    ? QStringLiteral("The local source changed during upload.") : input.errorString());
            }
            pending.emplace_back();
            auto &write = pending.back();
            touch();
            write.bytes = sftp_aio_begin_write(output, buffer.constData() + bufferOffset,
                                               buffered - bufferOffset, &write.request);
            if (write.bytes <= 0) return fail(serverError(QStringLiteral("write file; check remote free space")));
            bufferOffset += write.bytes;
            submitted += write.bytes;
        }
        auto &write = pending.front();
        touch();
        const qint64 acknowledged = sftp_aio_wait_write(&write.request);
        if (acknowledged < 0) return fail(serverError(QStringLiteral("write file; check remote free space")));
        if (acknowledged != write.bytes) return fail(QStringLiteral("The SFTP server did not acknowledge the complete write."));
        processed += acknowledged;
        pending.pop_front();
    }
    if (input.size() != total) return fail(QStringLiteral("The local source changed size during upload."));
    if (cancelled() || (progress && !progress(processed, total))) return fail(QStringLiteral("SFTP upload cancelled."));
    touch();
    if (sftp_extension_supported(m_sftp, "fsync@openssh.com", "1") && sftp_fsync(output) != SSH_OK)
        return fail(serverError(QStringLiteral("flush uploaded file")));
    touch();
    const int closed = sftp_close(output);
    output = nullptr;
    if (closed != SSH_OK) return fail(serverError(QStringLiteral("close uploaded file")));
    touch();
    if (sftp_chmod(m_sftp, temporaryPath.constData(), permissions) != SSH_OK)
        return fail(serverError(QStringLiteral("set uploaded file permissions")));
    if (cancelled()) return fail(QStringLiteral("SFTP upload cancelled."));
    if (!rename(temporary, address, true)) return false;
    committed = true;
    return true;
}

bool SftpClient::makePath(const SftpPath::Address &address)
{
    QString remote;
    for (const QString &part : address.url.path(QUrl::FullyDecoded).split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
        remote += QLatin1Char('/') + part;
        const QByteArray path = remote.toUtf8();
        touch();
        sftp_attributes existing = sftp_lstat(m_sftp, path.constData());
        if (existing) {
            bool directory = existing->type == SSH_FILEXFER_TYPE_DIRECTORY;
            if (existing->type == SSH_FILEXFER_TYPE_SYMLINK) {
                touch();
                auto target = sftp_stat(m_sftp, path.constData());
                directory = target && target->type == SSH_FILEXFER_TYPE_DIRECTORY;
                if (target) sftp_attributes_free(target);
            }
            sftp_attributes_free(existing);
            if (!directory) return fail(QStringLiteral("An SFTP parent is not a directory or is a symbolic link."));
            continue;
        }
        if (sftp_get_error(m_sftp) != SSH_FX_NO_SUCH_FILE && sftp_get_error(m_sftp) != SSH_FX_NO_SUCH_PATH)
            return fail(serverError(QStringLiteral("check parent folder")));
        touch();
        if (sftp_mkdir(m_sftp, path.constData(), 0755) != SSH_OK)
            return fail(serverError(QStringLiteral("create folder")));
    }
    return true;
}

std::optional<SftpPath::Address> SftpClient::availableChildPath(const SftpPath::Address &parent, const QString &name, bool directory)
{
    const int dot = directory ? -1 : name.lastIndexOf(QLatin1Char('.'));
    const QString base = dot > 0 ? name.left(dot) : name;
    const QString extension = dot > 0 ? name.mid(dot) : QString{};
    for (int i = 0; i < 1000; ++i) {
        if (cancelled()) { fail(QStringLiteral("SFTP request cancelled.")); return std::nullopt; }
        const QString candidate = i == 0 ? name : QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(extension);
        const QString path = SftpPath::child(parent.encoded(), candidate);
        if (path.isEmpty()) { fail(QStringLiteral("The SFTP name is invalid.")); return std::nullopt; }
        const auto address = SftpPath::parse(path);
        touch();
        sftp_attributes existing = sftp_lstat(m_sftp, address.url.path(QUrl::FullyDecoded).toUtf8().constData());
        if (existing) { sftp_attributes_free(existing); continue; }
        if (sftp_get_error(m_sftp) == SSH_FX_NO_SUCH_FILE || sftp_get_error(m_sftp) == SSH_FX_NO_SUCH_PATH) return address;
        fail(serverError(QStringLiteral("check new item name")));
        return std::nullopt;
    }
    fail(QStringLiteral("Cannot find an available SFTP name."));
    return std::nullopt;
}

bool SftpClient::createFolder(const SftpPath::Address &address)
{
    if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
    touch();
    if (sftp_mkdir(m_sftp, address.url.path(QUrl::FullyDecoded).toUtf8().constData(), 0755) != SSH_OK)
        return fail(serverError(QStringLiteral("create folder")));
    return true;
}

bool SftpClient::createFile(const SftpPath::Address &address)
{
    if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
    touch();
    sftp_file file = sftp_open(m_sftp, address.url.path(QUrl::FullyDecoded).toUtf8().constData(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (!file) return fail(serverError(QStringLiteral("create file")));
    touch();
    if (sftp_close(file) != SSH_OK) return fail(serverError(QStringLiteral("close created file")));
    return true;
}

bool SftpClient::remove(const SftpPath::Address &address)
{
    if (address.url.path() == QLatin1String("/")) return fail(QStringLiteral("Cannot remove the SFTP root."));
    struct Frame { SftpPath::Address address; bool visited; };
    QList<Frame> stack{{address, false}};
    while (!stack.isEmpty()) {
        if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
        const auto frame = stack.takeLast();
        const auto entry = stat(frame.address);
        if (!entry) return false;
        if (entry->isDirectory && !entry->isSymLink && !frame.visited) {
            stack.append({frame.address, true});
            if (!list(frame.address, true, [&](const QList<FileEntry> &entries) {
                for (const auto &child : entries) stack.append({SftpPath::parse(child.path), false});
            })) return false;
            continue;
        }
        const QByteArray path = frame.address.url.path(QUrl::FullyDecoded).toUtf8();
        touch();
        const int result = entry->isDirectory && !entry->isSymLink ? sftp_rmdir(m_sftp, path.constData()) : sftp_unlink(m_sftp, path.constData());
        if (result != SSH_OK) return fail(serverError(QStringLiteral("remove item")));
    }
    return true;
}

bool SftpClient::rename(const SftpPath::Address &source, const SftpPath::Address &destination, bool replaceFile)
{
    if (source.identity() != destination.identity()) return fail(QStringLiteral("SFTP rename requires the same server and login."));
    const QByteArray newPath = destination.url.path(QUrl::FullyDecoded).toUtf8();
    touch();
    sftp_attributes existing = sftp_lstat(m_sftp, newPath.constData());
    if (existing) {
        const bool regular = existing->type == SSH_FILEXFER_TYPE_REGULAR;
        sftp_attributes_free(existing);
        if (!replaceFile || !regular) return fail(QStringLiteral("The SFTP destination already exists."));
        if (!sftp_extension_supported(m_sftp, "posix-rename@openssh.com", "1"))
            return fail(QStringLiteral("This SFTP server does not support atomic file replacement. The existing file was preserved."));
    } else if (sftp_get_error(m_sftp) != SSH_FX_NO_SUCH_FILE && sftp_get_error(m_sftp) != SSH_FX_NO_SUCH_PATH) {
        return fail(serverError(QStringLiteral("check rename destination")));
    }
    if (cancelled()) return fail(QStringLiteral("SFTP request cancelled."));
    touch();
    const QByteArray oldPath = source.url.path(QUrl::FullyDecoded).toUtf8();
    if (sftp_rename(m_sftp, oldPath.constData(), newPath.constData()) != SSH_OK) {
        if (!ssh_is_connected(m_session))
            return fail(QStringLiteral("SFTP connection was lost during rename. It may have completed; verify the destination before retrying."));
        return fail(serverError(QStringLiteral("rename item")));
    }
    return true;
}
