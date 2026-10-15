#include "SftpFileProvider.h"
#include "SftpClient.h"
#include "OperationQueue.h"

#include <QMutexLocker>
#include <QtConcurrent>
#include <algorithm>

SftpFileProvider::SftpFileProvider(const QString &knownHostsFile) : m_knownHostsFile(knownHostsFile) {}

SftpFileProvider::~SftpFileProvider()
{
    cancel();
    for (auto &scan : m_scans) scan.waitForFinished();
}

bool SftpFileProvider::canHandle(const QString &path) const { return SftpPath::isSchemePath(path); }
bool SftpFileProvider::canCreateChildren(const QString &path) const
{
    const auto address = SftpPath::parse(path);
    return address.valid() && SftpSessions::credentials(address).has_value();
}
QString SftpFileProvider::normalizedPath(const QString &path) const
{
    const auto address = SftpPath::parse(path);
    return address.valid() ? address.encoded() : QString{};
}
QString SftpFileProvider::fileName(const QString &path) const { return SftpPath::fileName(path); }
QString SftpFileProvider::parentPath(const QString &path) const { return SftpPath::parent(path); }
QString SftpFileProvider::childPath(const QString &path, const QString &name) const { return SftpPath::child(path, name); }

void SftpFileProvider::scan(const QString &path)
{
    cancel();
    m_scans.removeIf([](const auto &scan) { return scan.isFinished(); });
    const int generation = m_generation.load();
    const auto address = SftpPath::parse(path);
    m_currentPath = address.encoded();
    const bool hidden = m_showHidden;
    clearLastError();
    {
        QMutexLocker lock(&m_mutex);
        m_entries.clear();
        m_cachedSession.reset();
    }
    emit started();
    m_scans.append(QtConcurrent::run([this, address, generation, hidden]() {
        const auto cancelled = [this, generation]() { return generation != m_generation.load(); };
        QString error = address.error;
        const auto credentials = address.valid() ? SftpSessions::credentials(address) : std::nullopt;
        bool success = false;
        if (address.valid() && !credentials) {
            error = QStringLiteral("Sign in with your login and password in Settings > Files > SFTP.");
        } else if (credentials) {
            {
                QMutexLocker lock(&m_mutex);
                if (cancelled()) return;
                m_cachedSession = credentials->revoked;
            }
            SftpClient client(cancelled, m_knownHostsFile);
            success = client.connect(*credentials) && client.list(address, hidden, [this, generation, &cancelled](const QList<FileEntry> &entries) {
                if (cancelled()) return;
                {
                    QMutexLocker lock(&m_mutex);
                    if (cancelled()) return;
                    for (const auto &entry : entries) m_entries.insert(entry.path, entry);
                }
                emit batchReady(entries, generation);
            });
            if (!success) error = client.error();
        }
        if (cancelled()) return;
        setError(error);
        emit finished(address.encoded(), success, generation, error);
    }));
}

void SftpFileProvider::cancel() { m_generation.fetch_add(1); }
bool SftpFileProvider::isRunning() const
{
    return std::any_of(m_scans.cbegin(), m_scans.cend(), [](const auto &scan) { return !scan.isFinished(); });
}

std::optional<FileEntry> SftpFileProvider::entryInfo(const QString &path) const
{
    clearLastError();
    const auto address = SftpPath::parse(path);
    if (!address.valid()) { setError(address.error); return std::nullopt; }
    const auto credentials = SftpSessions::credentials(address);
    if (!credentials) {
        setError(QStringLiteral("Sign in with your login and password in Settings > Files > SFTP."));
        return std::nullopt;
    }
    {
        QMutexLocker lock(&m_mutex);
        const auto found = m_entries.constFind(address.encoded());
        if (m_cachedSession == credentials->revoked && found != m_entries.cend()) return *found;
    }
    SftpClient client(OperationQueue::currentThreadAbortChecker(), m_knownHostsFile);
    if (!client.connect(*credentials)) { setError(client.error()); return std::nullopt; }
    const auto entry = client.stat(address);
    if (!entry) setError(client.error());
    return entry;
}

bool SftpFileProvider::pathExists(const QString &path) const { return entryInfo(path).has_value(); }
bool SftpFileProvider::isDirectory(const QString &path) const
{
    const auto entry = entryInfo(path);
    return entry && entry->isDirectory;
}
bool SftpFileProvider::isSymLink(const QString &path) const
{
    const auto entry = entryInfo(path);
    return entry && entry->isSymLink;
}

QStringList SftpFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    clearLastError();
    const auto address = SftpPath::parse(path);
    const auto credentials = address.valid() ? SftpSessions::credentials(address) : std::nullopt;
    if (!credentials) {
        setError(address.valid() ? QStringLiteral("Sign in through SFTP settings first.") : address.error);
        return {};
    }
    SftpClient client(OperationQueue::currentThreadAbortChecker(), m_knownHostsFile);
    QStringList paths;
    if (!client.connect(*credentials) || !client.list(address, includeHidden, [&paths](const QList<FileEntry> &entries) {
        for (const auto &entry : entries) paths.append(entry.path);
    })) {
        setError(client.error());
        return {};
    }
    return paths;
}

std::unique_ptr<SftpClient> SftpFileProvider::connectedClient(const QString &path) const
{
    clearLastError();
    const auto address = SftpPath::parse(path);
    if (!address.valid()) { setError(address.error); return nullptr; }
    const auto credentials = SftpSessions::credentials(address);
    if (!credentials) { setError(QStringLiteral("Sign in through SFTP settings first.")); return nullptr; }
    auto client = std::make_unique<SftpClient>(OperationQueue::currentThreadAbortChecker(), m_knownHostsFile);
    if (!client->connect(*credentials)) { setError(client->error()); return nullptr; }
    return client;
}

void SftpFileProvider::invalidateEntries() const
{
    QMutexLocker lock(&m_mutex);
    m_entries.clear();
    m_cachedSession.reset();
}

bool SftpFileProvider::copyToLocalFile(const QString &source, const QString &destination,
                                     const std::function<bool(qint64, qint64)> &progress, QString *error) const
{
    auto client = connectedClient(source);
    const bool success = client && client->downloadToFile(SftpPath::parse(source), destination, progress);
    if (client && !success) setError(client->error());
    if (error) *error = lastErrorString();
    return success;
}

bool SftpFileProvider::copyFromLocalFile(const QString &source, const QString &destination,
                                       const std::function<bool(qint64, qint64)> &progress, QString *error) const
{
    auto client = connectedClient(destination);
    const bool success = client && client->uploadFromFile(source, SftpPath::parse(destination), progress);
    if (client && !success) setError(client->error());
    if (success) invalidateEntries();
    if (error) *error = lastErrorString();
    return success;
}

bool SftpFileProvider::ensureParentDirectory(const QString &path) const { return makePath(parentPath(path)); }
bool SftpFileProvider::makePath(const QString &path) const
{
    auto client = connectedClient(path);
    if (!client) return false;
    if (!client->makePath(SftpPath::parse(path))) { setError(client->error()); return false; }
    invalidateEntries();
    return true;
}
bool SftpFileProvider::removePath(const QString &path) const
{
    auto client = connectedClient(path);
    if (!client) return false;
    if (!client->remove(SftpPath::parse(path))) { setError(client->error()); return false; }
    invalidateEntries();
    return true;
}
bool SftpFileProvider::movePath(const QString &source, const QString &destination) const
{
    if (SftpPath::parse(source).identity() != SftpPath::parse(destination).identity()) {
        setError(QStringLiteral("SFTP rename requires the same server and login."));
        return false;
    }
    auto client = connectedClient(source);
    if (!client) return false;
    const auto entry = client->stat(SftpPath::parse(source));
    if (!entry || !client->rename(SftpPath::parse(source), SftpPath::parse(destination), !entry->isDirectory && !entry->isSymLink)) {
        setError(client->error());
        return false;
    }
    invalidateEntries();
    return true;
}
bool SftpFileProvider::renamePath(const QString &path, const QString &newName)
{
    clearLastError();
    const QString destination = childPath(parentPath(path), newName.trimmed());
    if (destination.isEmpty()) { setError(QStringLiteral("The SFTP name is invalid.")); return false; }
    if (destination == normalizedPath(path)) return true;
    auto client = connectedClient(path);
    if (!client) return false;
    if (!client->rename(SftpPath::parse(path), SftpPath::parse(destination))) {
        setError(client->error());
        return false;
    }
    invalidateEntries();
    return true;
}
bool SftpFileProvider::createFolder(const QString &parent, const QString &name, QString *createdPath)
{
    if (createdPath) createdPath->clear();
    if (childPath(parent, name.trimmed()).isEmpty()) { setError(QStringLiteral("The SFTP folder name is invalid.")); return false; }
    auto client = connectedClient(parent);
    if (!client) return false;
    const auto path = client->availableChildPath(SftpPath::parse(parent), name.trimmed(), true);
    if (!path || !client->createFolder(*path)) { setError(client->error()); return false; }
    invalidateEntries();
    if (createdPath) *createdPath = path->encoded();
    return true;
}
bool SftpFileProvider::createFile(const QString &parent, const QString &name, QString *createdPath)
{
    if (createdPath) createdPath->clear();
    if (childPath(parent, name.trimmed()).isEmpty()) { setError(QStringLiteral("The SFTP file name is invalid.")); return false; }
    auto client = connectedClient(parent);
    if (!client) return false;
    const auto path = client->availableChildPath(SftpPath::parse(parent), name.trimmed(), false);
    if (!path || !client->createFile(*path)) { setError(client->error()); return false; }
    invalidateEntries();
    if (createdPath) *createdPath = path->encoded();
    return true;
}
bool SftpFileProvider::unsupported() const
{
    setError(QStringLiteral("SFTP file contents are accessed through queued transfers."));
    return false;
}
void SftpFileProvider::setError(const QString &error) const { QMutexLocker lock(&m_mutex); m_error = error; }
QString SftpFileProvider::lastErrorString() const { QMutexLocker lock(&m_mutex); return m_error; }
void SftpFileProvider::clearLastError() const { setError({}); }
