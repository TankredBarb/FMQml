#pragma once

#include "FileProvider.h"
#include "SftpPath.h"

#include <atomic>
#include <functional>
#include <thread>
#include <libssh/libssh.h>
#include <libssh/sftp.h>

struct SftpCredentials {
    SftpPath::Address address;
    QString password;
    std::shared_ptr<std::atomic_bool> revoked;
};

namespace SftpSessions {
void remember(const SftpCredentials &credentials);
std::optional<SftpCredentials> credentials(const SftpPath::Address &address);
void forget(const QString &identity);
QList<SftpPath::Address> addresses();
}

// Each instance belongs to one worker; only native socket shutdown is performed
// by the watchdog. No libssh session is shared between workers.
class SftpClient final {
public:
    explicit SftpClient(std::function<bool()> cancelled = {}, const QString &knownHostsFile = {});
    ~SftpClient();
    bool connect(const SftpCredentials &credentials, const QString &acceptedFingerprint = {});
    bool list(const SftpPath::Address &address, bool includeHidden,
              const std::function<void(const QList<FileEntry> &)> &publish);
    std::optional<FileEntry> stat(const SftpPath::Address &address);
    bool downloadToFile(const SftpPath::Address &address, const QString &destination,
                        const std::function<bool(qint64, qint64)> &progress);
    bool uploadFromFile(const QString &source, const SftpPath::Address &address,
                        const std::function<bool(qint64, qint64)> &progress);
    bool makePath(const SftpPath::Address &address);
    std::optional<SftpPath::Address> availableChildPath(const SftpPath::Address &parent, const QString &name, bool directory);
    bool createFolder(const SftpPath::Address &address);
    bool createFile(const SftpPath::Address &address);
    bool remove(const SftpPath::Address &address);
    bool rename(const SftpPath::Address &source, const SftpPath::Address &destination,
                bool replaceFile = false);
    QString error() const { return m_error; }
    QString fingerprint() const { return m_fingerprint; }
    bool needsHostConfirmation() const { return m_needsHostConfirmation; }

private:
    bool cancelled() const;
    void touch();
    bool fail(const QString &message);
    void resolveDirectoryLink(FileEntry &entry);
    QString serverError(const QString &operation) const;
    ssh_session m_session = nullptr;
    sftp_session m_sftp = nullptr;
    std::function<bool()> m_cancelled;
    QString m_error;
    QString m_fingerprint;
    QString m_knownHostsFile;
    std::shared_ptr<std::atomic_bool> m_revoked;
    bool m_hasPermit = false;
    bool m_needsHostConfirmation = false;
    std::atomic<qint64> m_activity{0};
    std::atomic_bool m_cleaningUp{false};
    std::jthread m_watchdog;
};
