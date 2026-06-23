#include "MegaFileProviderPlugin.h"
#include "MegaPath.h"
#include "MegaCache.h"
#include "MegaClient.h"
#include "MegaClientInterface.h"
#include "MegaAuth.h"
#include "MegaPresentation.h"
#include "CleanupSubsystem.h"

#include <megaapi.h>

using namespace mega;

#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QTemporaryFile>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QTimer>
#include <QVariantList>
#include <QWaitCondition>

#include <functional>

namespace {

constexpr QLatin1StringView MegaSignOutAction{"signOut"};
constexpr QLatin1StringView MegaSignInAction{"signIn"};
constexpr QLatin1StringView MegaAuthStatusAction{"authStatus"};
constexpr qint64 MegaOpenReadFallbackLimitBytes = 512ll * 1024ll * 1024ll;
#ifdef FM_MEGA_PROVIDER_TESTING
MegaClientInterface *s_clientForTesting = nullptr;
#endif

MegaClientInterface &megaClient()
{
#ifdef FM_MEGA_PROVIDER_TESTING
    if (s_clientForTesting) {
        return *s_clientForTesting;
    }
#endif
    return defaultMegaClient();
}

QString megaByteSizeText(qint64 size)
{
    return size >= 0 ? QLocale().formattedDataSize(size) : QStringLiteral("unknown");
}

QVariantList megaAccountStatusProperties()
{
    const qint64 used = megaClient().accountStorageUsedBytes() >= 0
        ? megaClient().accountStorageUsedBytes()
        : MegaCache::accountStorageUsedBytes();
    const qint64 total = megaClient().accountStorageMaxBytes();

    QString storageValue;
    if (total >= 0) {
        storageValue = QStringLiteral("%1 / %2").arg(megaByteSizeText(used), megaByteSizeText(total));
    } else {
        storageValue = megaByteSizeText(used);
    }

    return QVariantList{
        QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Signed in")},
            {QStringLiteral("value"), megaClient().isAccountAuthenticated() ? QStringLiteral("Yes") : QStringLiteral("No")},
        },
        QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Account")},
            {QStringLiteral("value"), megaClient().accountEmail().isEmpty() ? MegaAuth::savedEmail() : megaClient().accountEmail()},
        },
        QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Saved session")},
            {QStringLiteral("value"), MegaAuth::savedSession().isEmpty() ? QStringLiteral("No") : QStringLiteral("Yes")},
        },
        QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Access mode")},
            {QStringLiteral("value"), QStringLiteral("Read-write account access")},
        },
        QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Storage usage")},
            {QStringLiteral("value"), storageValue},
        },
    };
}

QVariantMap megaStorageInfoMap()
{
    const qint64 used = megaClient().accountStorageUsedBytes() >= 0
        ? megaClient().accountStorageUsedBytes()
        : MegaCache::accountStorageUsedBytes();
    const qint64 total = megaClient().accountStorageMaxBytes();
    const qint64 free = (total >= 0 && used >= 0) ? (total - used) : -1;
    const double percent = (total > 0 && used >= 0) ? (static_cast<double>(used) / total) : 0.0;
    const bool valid = (total >= 0 && used >= 0);
    const bool isCritical = valid && total > 0 && free >= 0 && (static_cast<double>(free) / static_cast<double>(total)) < 0.10;

    return {
        {QStringLiteral("valid"), valid},
        {QStringLiteral("total"), total},
        {QStringLiteral("free"), free},
        {QStringLiteral("used"), used},
        {QStringLiteral("percent"), percent},
        {QStringLiteral("totalStr"), megaByteSizeText(total)},
        {QStringLiteral("freeStr"), megaByteSizeText(free)},
        {QStringLiteral("usedStr"), megaByteSizeText(used)},
        {QStringLiteral("fs"), QStringLiteral("MEGA")},
        {QStringLiteral("isCritical"), isCritical},
    };
}

QVariantMap runBlockingMegaAuthorization(const std::function<int()> &startAuthorization,
                                         const QString &successMessage,
                                         const QString &startFailureMessage)
{
    MegaClientInterface &client = megaClient();
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    bool finished = false;
    bool signedIn = false;
    bool timedOut = false;
    QString accountEmail;

    const QMetaObject::Connection authConnection = QObject::connect(
        &client, &MegaClientInterface::accountAuthorizationChanged,
        &loop, [&](bool changedSignedIn, const QString &changedEmail, const QString &) {
            finished = true;
            signedIn = changedSignedIn;
            accountEmail = changedEmail;
            loop.quit();
        });
    const QMetaObject::Connection timeoutConnection = QObject::connect(
        &timeout, &QTimer::timeout,
        &loop, [&]() {
            finished = true;
            timedOut = true;
            signedIn = client.isAccountAuthenticated();
            accountEmail = client.accountEmail();
            loop.quit();
        });

    const int startResult = startAuthorization();
    if (startResult != 0) {
        QObject::disconnect(authConnection);
        QObject::disconnect(timeoutConnection);
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("title"), QStringLiteral("MEGA")},
            {QStringLiteral("message"), startFailureMessage},
        };
    }

    if (!finished) {
        timeout.start(60000);
        loop.exec();
    }

    QObject::disconnect(authConnection);
    QObject::disconnect(timeoutConnection);

    const bool ok = signedIn || client.isAccountAuthenticated();
    const QString email = accountEmail.isEmpty() ? client.accountEmail() : accountEmail;
    if (ok) {
        const QString session = client.accountSessionToken();
        if (session.trimmed().isEmpty()) {
            qWarning() << "[MegaFileProvider] MEGA authorization succeeded but SDK session token is empty";
        } else if (!MegaAuth::rememberAuthorization(session, email)) {
            qWarning() << "[MegaFileProvider] Could not persist MEGA authorization in the platform credential store";
        }
    }
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("title"), QStringLiteral("MEGA")},
        {QStringLiteral("message"), ok
            ? successMessage
            : (timedOut
                ? QStringLiteral("MEGA sign in did not complete before the timeout.")
                : QStringLiteral("MEGA sign in failed."))},
        {QStringLiteral("signedIn"), ok},
        {QStringLiteral("accountEmail"), email},
        {QStringLiteral("accountLabel"), email.isEmpty() ? QStringLiteral("Signed in") : email},
        {QStringLiteral("refreshCurrentPath"), ok},
    };
}

class CleanupManagedTemporaryFile final : public QTemporaryFile
{
public:
    explicit CleanupManagedTemporaryFile(const QString &fileTemplate)
        : QTemporaryFile(fileTemplate)
    {
        setAutoRemove(false);
    }

    ~CleanupManagedTemporaryFile() override
    {
        const QString path = fileName();
        close();
        if (!m_cleanupLeaseId.isEmpty()) {
            CleanupSubsystem::instance().scheduleDelete(m_cleanupLeaseId);
        }
    }

    void setCleanupLeaseId(const QString &leaseId)
    {
        m_cleanupLeaseId = leaseId;
    }

    QString cleanupLeaseId() const
    {
        return m_cleanupLeaseId;
    }

    Q_DISABLE_COPY_MOVE(CleanupManagedTemporaryFile)

private:
    QString m_cleanupLeaseId;
};

QString megaOpenReadStagingRoot(const QString &stagingParentPath, const QString &sourcePath)
{
    const QString resolved = StagingLocationPolicy::resolveStagingParentDirectory(
        stagingParentPath,
        sourcePath,
        stagingParentPath,
        true);
    if (resolved.isEmpty()) {
        return {};
    }

    const QString root = QDir(resolved).filePath(QStringLiteral("mega-openread"));
    return QDir().mkpath(root) ? root : QString{};
}

bool waitForMegaMutation(const std::function<qint64()> &startMutation,
                         const QString &operation,
                         const QString &path,
                         QString *resultPath,
                         QString *errorStr)
{
    QMutex waitMutex;
    QWaitCondition waitCondition;
    bool finished = false;
    bool success = false;
    QString operationError;
    QString operationResultPath;
    qint64 requestId = 0;

    MegaClientInterface &client = megaClient();
    const QMetaObject::Connection finishedConn = QObject::connect(
        &client, &MegaClientInterface::mutationFinished,
        &client,
        [&](qint64 emittedRequestId,
            const QString &emittedOperation,
            const QString &emittedPath,
            bool emittedSuccess,
            const QString &emittedError,
            const QString &emittedResultPath) {
            if ((requestId > 0 && emittedRequestId != requestId)
                || emittedOperation != operation
                || MegaPath::normalizedPath(emittedPath) != MegaPath::normalizedPath(path)) {
                return;
            }
            {
                QMutexLocker waitLocker(&waitMutex);
                finished = true;
                success = emittedSuccess;
                operationError = emittedError;
                operationResultPath = emittedResultPath;
            }
            waitCondition.wakeAll();
        },
        Qt::DirectConnection);

    requestId = startMutation ? startMutation() : 0;
    if (requestId <= 0) {
        QObject::disconnect(finishedConn);
        if (errorStr) {
            *errorStr = QStringLiteral("Could not start MEGA %1 operation").arg(operation);
        }
        return false;
    }

    bool timedOut = false;
    {
        QMutexLocker waitLocker(&waitMutex);
        if (!finished) {
            timedOut = !waitCondition.wait(&waitMutex, 30 * 60 * 1000);
        }
    }

    QObject::disconnect(finishedConn);
    if (timedOut) {
        megaClient().cancelAll();
        operationError = QStringLiteral("MEGA %1 operation timed out").arg(operation);
    }
    if (!finished || !success) {
        if (errorStr) {
            *errorStr = operationError.isEmpty()
                ? QStringLiteral("Unknown MEGA %1 error").arg(operation)
                : operationError;
        }
        return false;
    }
    if (resultPath) {
        *resultPath = operationResultPath;
    }
    if (errorStr) {
        errorStr->clear();
    }
    return true;
}

} // namespace

class MegaFileProvider final : public FileProvider
{
    Q_OBJECT

public:
    explicit MegaFileProvider(QObject *parent = nullptr)
        : FileProvider(parent)
        , m_currentGeneration(0)
        , m_pendingScanGeneration(0)
    {
        connect(&megaClient(), &MegaClientInterface::publicLinkLoaded, this, &MegaFileProvider::onPublicLinkLoaded);
        connect(&megaClient(), &MegaClientInterface::accountNodesLoaded, this, &MegaFileProvider::onAccountNodesLoaded);
    }

    ~MegaFileProvider() override = default;

    QString scheme() const override
    {
        return QStringLiteral("mega");
    }

    bool canHandle(const QString &path) const override
    {
        if (MegaPath::isSchemePath(path)) {
            return true;
        }
        QString linkId, linkKey;
        bool isFolder = false;
        return !MegaPath::fromUserInput(path, linkId, linkKey, isFolder).isEmpty();
    }

    Capabilities capabilities() const override
    {
        return Browse | ReadMetadata | Create | Rename | Remove | Transfer;
    }

    bool canCreateChildren(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        return !MegaPath::isLinkPath(normalized) && megaClient().isAccountAuthenticated();
    }

    bool canRemovePath(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        return normalized != MegaPath::Root
            && !MegaPath::isLinkPath(normalized)
            && megaClient().isAccountAuthenticated();
    }

    bool isReadOnlyContainer(const QString &path) const override
    {
        return MegaPath::isLinkPath(MegaPath::normalizedPath(path)) || !megaClient().isAccountAuthenticated();
    }

    void scan(const QString &path) override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        m_currentPath = normalized;
        m_currentGeneration++;

        emit started();

        if (!MegaPath::isLinkPath(normalized)) {
            if (MegaCache::getChildren(normalized).has_value()) {
                emitChildEntries(normalized, m_currentGeneration);
                emit finished(normalized, true, m_currentGeneration);
                return;
            }

            if (!megaClient().isAccountAuthenticated()) {
                emit finished(normalized, false, m_currentGeneration, QStringLiteral("MEGA account is not signed in"));
                return;
            }

            m_pendingScanPath = normalized;
            m_pendingScanGeneration = m_currentGeneration;
            if (megaClient().loadAccountRoot() != 0) {
                m_pendingScanPath.clear();
                emit finished(normalized, false, m_currentGeneration, QStringLiteral("Could not load MEGA account root"));
            }
            return;
        }

        const QString linkId = MegaPath::linkIdForPath(normalized);

        // If the path is already cached
        if (MegaCache::getChildren(normalized).has_value()) {
            emitChildEntries(normalized, m_currentGeneration);
            emit finished(normalized, true, m_currentGeneration);
            return;
        }

        // If the root of this link is loaded but this sub-path is not cached -> not found
        const QString rootPath = QStringLiteral("mega://link/") + linkId;
        if (MegaCache::getEntry(rootPath).has_value()) {
            emit finished(normalized, false, m_currentGeneration, QStringLiteral("Path not found"));
            return;
        }

        // Otherwise, fetch from SDK
        m_pendingScanPath = normalized;
        m_pendingScanGeneration = m_currentGeneration;

        megaClient().getPublicNode(linkId);
    }

    void cancel() override
    {
        megaClient().cancelAll();
    }

    void setShowHidden(bool show) override
    {
        Q_UNUSED(show)
    }

    bool isRunning() const override
    {
        return !m_pendingScanPath.isEmpty();
    }

    QString currentPath() const override
    {
        return m_currentPath;
    }

    int currentGeneration() const override
    {
        return m_currentGeneration;
    }

    bool pathExists(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        return normalized == MegaPath::Root || MegaCache::getEntry(normalized).has_value();
    }

    bool isDirectory(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        const auto entry = MegaCache::getEntry(normalized);
        return normalized == MegaPath::Root || (entry.has_value() && entry->isDirectory);
    }

    bool isSymLink(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }

    QString normalizedPath(const QString &path) const override
    {
        return MegaPath::normalizedPath(path);
    }

    QString fileName(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        if (MegaPath::isLinkPath(normalized) && MegaPath::relativePathForPath(normalized).isEmpty()) {
            const auto entry = MegaCache::getEntry(normalized);
            const QString linkId = MegaPath::linkIdForPath(normalized);
            if (entry && !entry->name.trimmed().isEmpty() && entry->name != linkId) {
                return entry->name;
            }
            return QStringLiteral("MEGA Public Folder");
        }
        return MegaPath::fallbackFileNameForPath(normalized);
    }

    QString absolutePath(const QString &path) const override
    {
        return MegaPath::normalizedPath(path);
    }

    QString parentPath(const QString &path) const override
    {
        return MegaPath::parentPath(path);
    }

    QString childPath(const QString &parentPath, const QString &name) const override
    {
        return MegaPath::childPath(parentPath, name);
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        return MegaCache::getEntry(MegaPath::normalizedPath(path));
    }

    bool ensureParentDirectory(const QString &path) const override
    {
        const QString parent = MegaPath::parentPath(path);
        return !parent.isEmpty() && isDirectory(parent) && canCreateChildren(parent);
    }

    bool makePath(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        const QString parent = MegaPath::parentPath(normalized);
        const QString name = MegaPath::fallbackFileNameForPath(normalized);
        if (parent.isEmpty() || name.isEmpty()) {
            return false;
        }
        QString createdPath;
        return const_cast<MegaFileProvider *>(this)->createFolder(parent, name, &createdPath);
    }

    bool removePath(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        if (!canRemovePath(normalized)) {
            return false;
        }
        QString error;
        if (!waitForMegaMutation([normalized]() { return megaClient().startRemove(normalized); },
                                 QStringLiteral("remove"),
                                 normalized,
                                 nullptr,
                                 &error)) {
            qWarning() << "[MegaFileProvider] removePath failed" << normalized << error;
            return false;
        }
        MegaCache::removeChild(MegaPath::parentPath(normalized), normalized);
        MegaCache::removeSubtree(normalized);
        return true;
    }

    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        Q_UNUSED(includeHidden)
        return MegaCache::getChildren(MegaPath::normalizedPath(path)).value_or(QStringList{});
    }

    bool movePath(const QString &sourcePath, const QString &destinationPath) const override
    {
        const QString source = MegaPath::normalizedPath(sourcePath);
        const QString destination = MegaPath::normalizedPath(destinationPath);
        if (!canRemovePath(source)
            || MegaPath::isLinkPath(destination)
            || destination == MegaPath::Root
            || !megaClient().isAccountAuthenticated()) {
            return false;
        }
        QString resultPath;
        QString error;
        if (!waitForMegaMutation([source, destination]() { return megaClient().startMove(source, destination); },
                                 QStringLiteral("move"),
                                 source,
                                 &resultPath,
                                 &error)) {
            qWarning() << "[MegaFileProvider] movePath failed" << source << destination << error;
            return false;
        }
        const QString resolvedDestination = resultPath.isEmpty() ? destination : MegaPath::normalizedPath(resultPath);
        MegaCache::removeChild(MegaPath::parentPath(source), source);
        MegaCache::appendChild(MegaPath::parentPath(resolvedDestination), resolvedDestination);
        MegaCache::renameSubtree(source, resolvedDestination, MegaPath::fallbackFileNameForPath(resolvedDestination));
        return true;
    }

    std::unique_ptr<QIODevice> openRead(const QString &path) const override
    {
        return openRead(path, {});
    }

    std::unique_ptr<QIODevice> openRead(const QString &path, const QString &stagingParentPath) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);

        const std::optional<FileEntry> entry = MegaCache::getEntry(normalized);
        if (entry && !entry->isDirectory && entry->size > MegaOpenReadFallbackLimitBytes) {
            qWarning() << "[MegaFileProvider] openRead refused large fallback materialization"
                       << "path:" << normalized
                       << "size:" << entry->size
                       << "limit:" << MegaOpenReadFallbackLimitBytes;
            return nullptr;
        }

        const QString stagingRoot = megaOpenReadStagingRoot(stagingParentPath, normalized);
        if (stagingRoot.isEmpty()) {
            qWarning() << "[MegaFileProvider] openRead cannot resolve cleanup staging root"
                       << "path:" << normalized
                       << "stagingParent:" << stagingParentPath;
            return nullptr;
        }

        QString templatePath = QDir(stagingRoot).filePath(QStringLiteral("mega-preview-XXXXXX"));
        const QString suffix = QFileInfo(MegaPath::fallbackFileNameForPath(normalized)).suffix();
        if (!suffix.isEmpty()) {
            templatePath += QLatin1Char('.') + suffix;
        }

        auto tempFile = std::make_unique<CleanupManagedTemporaryFile>(templatePath);
        if (!tempFile->open()) {
            return nullptr;
        }

        const QString tempPath = tempFile->fileName();
        tempFile->close();

        QString leaseId;
        CleanupSubsystem::instance().registerArtifact(
            CleanupArtifactKind::RemotePreview,
            tempPath,
            stagingRoot,
            false,
            &leaseId);
        tempFile->setCleanupLeaseId(leaseId);

        if (!copyToLocalFile(normalized, tempPath, nullptr, nullptr)) {
            if (!leaseId.isEmpty()) {
                CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
                tempFile->setCleanupLeaseId({});
            }
            return nullptr;
        }

        if (!tempFile->QFile::open(QIODevice::ReadOnly)) {
            if (!leaseId.isEmpty()) {
                CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
                tempFile->setCleanupLeaseId({});
            }
            return nullptr;
        }

        return tempFile;
    }

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progressCallback,
                         QString *errorStr) const override
    {
        const QString normalized = MegaPath::normalizedPath(sourcePath);

        const QString partialPath = destinationFilePath + QStringLiteral(".part");
        QFile::remove(partialPath);

        QElapsedTimer elapsed;
        elapsed.start();

        QMutex waitMutex;
        QWaitCondition waitCondition;
        bool transferSuccess = false;
        bool transferFinished = false;
        QString transferError;
        qint64 downloadRequestId = 0;

        MegaClientInterface &client = megaClient();

        QMetaObject::Connection progressConn = connect(&client, &MegaClientInterface::downloadProgress,
            &client,
            [&](qint64 requestId, const QString &path, qint64 processed, qint64 total) {
                if ((downloadRequestId > 0 && requestId != downloadRequestId) || MegaPath::normalizedPath(path) != normalized) {
                    return;
                }


                if (progressCallback && !progressCallback(processed, total)) {
                    qWarning() << "[MegaFileProvider] copyToLocalFile progress callback cancelled"
                               << "request:" << requestId
                               << "source:" << normalized;
                    megaClient().cancelAll();
                }
            }, Qt::DirectConnection);

        QMetaObject::Connection finishedConn = connect(&client, &MegaClientInterface::downloadFinished,
            &client,
            [&](qint64 requestId, const QString &path, bool success, const QString &errorString) {
                if ((downloadRequestId > 0 && requestId != downloadRequestId) || MegaPath::normalizedPath(path) != normalized) {
                    return;
                }

                {
                    QMutexLocker waitLocker(&waitMutex);
                    transferSuccess = success;
                    transferFinished = true;
                    transferError = errorString;
                }

                waitCondition.wakeAll();
            }, Qt::DirectConnection);

        downloadRequestId = client.startDownload(normalized, partialPath);

        bool timedOut = false;
        {
            QMutexLocker waitLocker(&waitMutex);
            if (!transferFinished) {
                timedOut = !waitCondition.wait(&waitMutex, 30 * 60 * 1000);
            }
        }

        disconnect(progressConn);
        disconnect(finishedConn);

        if (timedOut) {
            transferError = QStringLiteral("MEGA download timed out");
            qWarning() << "[MegaFileProvider] copyToLocalFile timeout"
                       << "request:" << downloadRequestId
                       << "source:" << normalized
                       << "elapsedMs:" << elapsed.elapsed();
            megaClient().cancelAll();
        }

        if (!transferFinished || !transferSuccess) {
            qWarning() << "[MegaFileProvider] copyToLocalFile failed"
                       << "request:" << downloadRequestId
                       << "finished:" << transferFinished
                       << "success:" << transferSuccess
                       << "error:" << transferError
                       << "partialExists:" << QFile::exists(partialPath)
                       << "elapsedMs:" << elapsed.elapsed()
                       << "source:" << normalized;
            QFile::remove(partialPath);
            if (errorStr) {
                *errorStr = transferError.isEmpty() ? QStringLiteral("Unknown download error") : transferError;
            }
            return false;
        }

        QFile::remove(destinationFilePath);
        if (!QFile::rename(partialPath, destinationFilePath)) {
            QFile::remove(partialPath);
            if (errorStr) {
                *errorStr = QStringLiteral("Could not move MEGA download into place");
            }
            return false;
        }


        return true;
    }

    bool copyFromLocalFile(const QString &sourceFilePath,
                           const QString &destinationPath,
                           const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progressCallback,
                           QString *errorStr) const override
    {
        const QFileInfo sourceInfo(sourceFilePath);
        if (!sourceInfo.isFile()) {
            if (errorStr) {
                *errorStr = QStringLiteral("MEGA upload source is not a regular file");
            }
            return false;
        }

        const QString normalized = MegaPath::normalizedPath(destinationPath);
        const QString parent = MegaPath::parentPath(normalized);
        if (!canCreateChildren(parent)) {
            if (errorStr) {
                *errorStr = QStringLiteral("MEGA upload destination is not writable");
            }
            return false;
        }

        QMutex waitMutex;
        QWaitCondition waitCondition;
        bool transferSuccess = false;
        bool transferFinished = false;
        QString transferError;
        qint64 uploadRequestId = 0;

        MegaClientInterface &client = megaClient();
        QMetaObject::Connection progressConn = connect(&client, &MegaClientInterface::uploadProgress,
            &client,
            [&](qint64 requestId, const QString &path, qint64 processed, qint64 total) {
                if ((uploadRequestId > 0 && requestId != uploadRequestId) || MegaPath::normalizedPath(path) != normalized) {
                    return;
                }
                if (progressCallback && !progressCallback(processed, total)) {
                    qWarning() << "[MegaFileProvider] copyFromLocalFile progress callback cancelled"
                               << "request:" << requestId
                               << "destination:" << normalized;
                    megaClient().cancelAll();
                }
            }, Qt::DirectConnection);

        QMetaObject::Connection finishedConn = connect(&client, &MegaClientInterface::mutationFinished,
            &client,
            [&](qint64 requestId, const QString &operation, const QString &path, bool success, const QString &errorString, const QString &) {
                if ((uploadRequestId > 0 && requestId != uploadRequestId)
                    || operation != QStringLiteral("upload")
                    || MegaPath::normalizedPath(path) != normalized) {
                    return;
                }
                {
                    QMutexLocker waitLocker(&waitMutex);
                    transferSuccess = success;
                    transferFinished = true;
                    transferError = errorString;
                }
                waitCondition.wakeAll();
            }, Qt::DirectConnection);

        uploadRequestId = client.startUpload(sourceFilePath, normalized);

        bool timedOut = false;
        {
            QMutexLocker waitLocker(&waitMutex);
            if (!transferFinished) {
                timedOut = !waitCondition.wait(&waitMutex, 30 * 60 * 1000);
            }
        }

        disconnect(progressConn);
        disconnect(finishedConn);

        if (timedOut) {
            transferError = QStringLiteral("MEGA upload timed out");
            megaClient().cancelAll();
        }
        if (!transferFinished || !transferSuccess) {
            if (errorStr) {
                *errorStr = transferError.isEmpty() ? QStringLiteral("Unknown upload error") : transferError;
            }
            return false;
        }
        FileEntry entry;
        entry.name = MegaPath::fallbackFileNameForPath(normalized);
        entry.path = normalized;
        entry.isDirectory = false;
        entry.isReadOnly = false;
        entry.size = sourceInfo.size();
        const int suffixIndex = entry.name.lastIndexOf(QLatin1Char('.'));
        entry.suffix = suffixIndex >= 0 ? entry.name.mid(suffixIndex + 1).toLower() : QString{};
        entry.modified = sourceInfo.lastModified();
        entry.created = sourceInfo.birthTime().isValid() ? sourceInfo.birthTime() : sourceInfo.lastModified();
        entry.iconName = {};
        MegaPresentation::enrichEntryPresentation(entry);
        MegaCache::cacheEntry(normalized, entry, {});
        MegaCache::appendChild(parent, normalized);
        if (errorStr) {
            errorStr->clear();
        }
        return true;
    }

    std::unique_ptr<QIODevice> openWrite(const QString &path, bool truncate = true) const override
    {
        Q_UNUSED(path)
        Q_UNUSED(truncate)
        return nullptr;
    }

    bool renamePath(const QString &oldPath, const QString &newName) override
    {
        const QString normalized = MegaPath::normalizedPath(oldPath);
        const QString trimmedName = newName.trimmed();
        if (!canRemovePath(normalized) || trimmedName.isEmpty() || trimmedName.contains(QLatin1Char('/'))) {
            return false;
        }
        QString resultPath;
        QString error;
        if (!waitForMegaMutation([normalized, trimmedName]() { return megaClient().startRename(normalized, trimmedName); },
                                 QStringLiteral("rename"),
                                 normalized,
                                 &resultPath,
                                 &error)) {
            qWarning() << "[MegaFileProvider] renamePath failed" << normalized << trimmedName << error;
            return false;
        }
        const QString renamedPath = resultPath.isEmpty()
            ? MegaPath::childPath(MegaPath::parentPath(normalized), trimmedName)
            : MegaPath::normalizedPath(resultPath);
        MegaCache::removeChild(MegaPath::parentPath(normalized), normalized);
        MegaCache::appendChild(MegaPath::parentPath(renamedPath), renamedPath);
        MegaCache::renameSubtree(normalized, renamedPath, trimmedName);
        return true;
    }

    bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        const QString parent = MegaPath::normalizedPath(parentPath);
        const QString trimmedName = name.trimmed();
        if (!canCreateChildren(parent) || trimmedName.isEmpty() || trimmedName.contains(QLatin1Char('/'))) {
            return false;
        }
        QString resultPath;
        QString error;
        if (!waitForMegaMutation([parent, trimmedName]() { return megaClient().startCreateFolder(parent, trimmedName); },
                                 QStringLiteral("createFolder"),
                                 MegaPath::childPath(parent, trimmedName),
                                 &resultPath,
                                 &error)) {
            qWarning() << "[MegaFileProvider] createFolder failed" << parent << trimmedName << error;
            return false;
        }
        if (createdPath) {
            *createdPath = resultPath.isEmpty() ? MegaPath::childPath(parent, trimmedName) : resultPath;
        }
        const QString path = resultPath.isEmpty() ? MegaPath::childPath(parent, trimmedName) : resultPath;
        FileEntry entry;
        entry.name = trimmedName;
        entry.path = path;
        entry.isDirectory = true;
        entry.isReadOnly = false;
        entry.iconName = QStringLiteral("folder");
        MegaPresentation::enrichEntryPresentation(entry);
        MegaCache::cacheEntry(path, entry, {});
        MegaCache::cacheChildren(path, {});
        MegaCache::appendChild(parent, path);
        return true;
    }

    bool createFile(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        const QString parent = MegaPath::normalizedPath(parentPath);
        const QString trimmedName = name.trimmed();
        if (!canCreateChildren(parent) || trimmedName.isEmpty() || trimmedName.contains(QLatin1Char('/'))) {
            return false;
        }

        const QString stagingRoot = megaOpenReadStagingRoot({}, MegaPath::childPath(parent, trimmedName));
        if (stagingRoot.isEmpty()) {
            return false;
        }
        auto tempFile = std::make_unique<CleanupManagedTemporaryFile>(
            QDir(stagingRoot).filePath(QStringLiteral("mega-empty-upload-XXXXXX")));
        if (!tempFile->open()) {
            return false;
        }
        const QString tempPath = tempFile->fileName();
        QString leaseId;
        CleanupSubsystem::instance().registerArtifact(
            CleanupArtifactKind::ProviderTransfer,
            tempPath,
            stagingRoot,
            false,
            &leaseId);
        tempFile->setCleanupLeaseId(leaseId);
        tempFile->close();

        QString uploadError;
        const QString destination = MegaPath::childPath(parent, trimmedName);
        const bool uploaded = copyFromLocalFile(tempPath, destination, nullptr, &uploadError);
        if (!leaseId.isEmpty()) {
            CleanupSubsystem::instance().scheduleDelete(leaseId);
            tempFile->setCleanupLeaseId({});
        }
        if (!uploaded) {
            qWarning() << "[MegaFileProvider] createFile upload failed" << destination << uploadError;
            return false;
        }
        if (createdPath) {
            *createdPath = destination;
        }
        return true;
    }

    QVariantMap storageInfo(const QString &path) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        if (MegaPath::isLinkPath(normalized) || !megaClient().isAccountAuthenticated()) {
            return {};
        }
        return megaStorageInfoMap();
    }

private slots:
    void onAccountNodesLoaded(bool success, const QString &errorString)
    {
        if (m_pendingScanPath.isEmpty()) {
            return;
        }

        const QString scanPath = m_pendingScanPath;
        const int gen = m_pendingScanGeneration;
        m_pendingScanPath.clear();

        if (success) {
            if (MegaCache::getChildren(scanPath).has_value()) {
                emitChildEntries(scanPath, gen);
                emit finished(scanPath, true, gen);
            } else {
                emit finished(scanPath, false, gen, QStringLiteral("Path not found after loading MEGA account"));
            }
        } else {
            emit finished(scanPath, false, gen, errorString);
        }
    }

    void onPublicLinkLoaded(const QString &linkId, bool success, const QString &errorString)
    {
        if (m_pendingScanPath.isEmpty()) {
            return;
        }

        const QString pendingLinkId = MegaPath::linkIdForPath(m_pendingScanPath);
        if (pendingLinkId != linkId) {
            return;
        }

        const QString scanPath = m_pendingScanPath;
        const int gen = m_pendingScanGeneration;
        m_pendingScanPath.clear();

        if (success) {
            if (MegaCache::getEntry(scanPath).has_value()) {
                emitChildEntries(scanPath, gen);
                emit finished(scanPath, true, gen);
            } else {
                emit finished(scanPath, false, gen, QStringLiteral("Path not found after loading link"));
            }
        } else {
            emit finished(scanPath, false, gen, errorString);
        }
    }

private:
    void emitChildEntries(const QString &parentPath, int generation)
    {
        const QList<FileEntry> entries = MegaCache::childEntries(parentPath);
        if (!entries.isEmpty()) {
            emit batchReady(entries, generation);
        }
    }

    QString m_currentPath;
    int m_currentGeneration;
    QString m_pendingScanPath;
    int m_pendingScanGeneration;
};

// MegaFileProviderPlugin implementation

MegaFileProviderPlugin::MegaFileProviderPlugin()
{
    // Force initialization of the active MEGA client in the main thread.
    MegaClientInterface &client = megaClient();
    connect(&client, &MegaClientInterface::accountAuthorizationChanged,
            this, [](bool signedIn, const QString &accountEmail, const QString &session) {
                if (signedIn) {
                    if (!MegaAuth::rememberAuthorization(session, accountEmail)) {
                        qWarning() << "[MegaFileProvider] Could not persist MEGA authorization change";
                    }
                } else {
                    MegaAuth::clearSavedAuthorization();
                }
            });

#ifndef FM_MEGA_PROVIDER_TESTING
    const QString session = MegaAuth::savedSession();
    if (!session.isEmpty() && !client.isAccountAuthenticated()) {
        client.resumeAccountSession(session);
    }
#endif
}

#ifdef FM_MEGA_PROVIDER_TESTING
void MegaFileProviderPlugin::setClientForTesting(MegaClientInterface *client)
{
    s_clientForTesting = client;
}
#endif

int MegaFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString MegaFileProviderPlugin::pluginId() const
{
    return QStringLiteral("mega");
}

QString MegaFileProviderPlugin::displayName() const
{
    return QStringLiteral("MEGA");
}

QStringList MegaFileProviderPlugin::schemes() const
{
    return { QStringLiteral("mega") };
}

bool MegaFileProviderPlugin::canHandle(const QString &path) const
{
    if (MegaPath::isSchemePath(path)) {
        return true;
    }
    QString linkId, linkKey; bool isFolder;
    return !MegaPath::fromUserInput(path, linkId, linkKey, isFolder).isEmpty();
}

std::unique_ptr<FileProvider> MegaFileProviderPlugin::createProvider()
{
    return std::make_unique<MegaFileProvider>();
}

QString MegaFileProviderPlugin::preprocessPath(const QString &path) const
{
    QString linkId, linkKey;
    bool isFolder = false;
    QString result = MegaPath::fromUserInput(path, linkId, linkKey, isFolder);
    if (!result.isEmpty()) {
        MegaCache::storeKey(linkId, linkKey, isFolder);
        return result;
    }
    return path;
}

int MegaFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString MegaFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString MegaFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> MegaFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    const QString targetPath = MegaPath::normalizedPath(context.targetPath);
    if (!MegaPath::isSchemePath(targetPath)) {
        return {};
    }

    QList<FileActionDescriptor> actions;
    FileActionDescriptor status;
    status.id = QString(MegaAuthStatusAction);
    status.text = QStringLiteral("MEGA account status");
    status.iconSource = QStringLiteral("../assets/icons/info.svg");
    status.order = 900;
    actions.append(status);

    if (megaClient().isAccountAuthenticated()) {
        FileActionDescriptor signOut;
        signOut.id = QString(MegaSignOutAction);
        signOut.text = QStringLiteral("Sign out from MEGA");
        signOut.iconSource = QStringLiteral("../assets/icons/close.svg");
        signOut.order = 910;
        actions.append(signOut);
    } else {
        FileActionDescriptor signIn;
        signIn.id = QString(MegaSignInAction);
        signIn.text = QStringLiteral("Sign in to MEGA");
        signIn.iconSource = QStringLiteral("../assets/icons/plugin.svg");
        signIn.order = 905;
        actions.append(signIn);
    }
    return actions;
}

QVariantMap MegaFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId == MegaAuthStatusAction) {
        const bool signedIn = megaClient().isAccountAuthenticated();
        if (signedIn) {
            megaClient().requestAccountDetails();
        }
        const QString accountEmail = megaClient().accountEmail().isEmpty()
            ? MegaAuth::savedEmail()
            : megaClient().accountEmail();
        const QString label = signedIn
            ? (accountEmail.isEmpty() ? QStringLiteral("Signed in") : accountEmail)
            : (MegaAuth::savedSession().isEmpty()
                ? QStringLiteral("Not signed in")
                : (accountEmail.isEmpty() ? QStringLiteral("Saved session available") : accountEmail));
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("MEGA")},
            {QStringLiteral("subtitle"), QStringLiteral("Account authorization")},
            {QStringLiteral("message"), signedIn
                ? QStringLiteral("MEGA account access is active.")
                : QStringLiteral("MEGA account access is not active.")},
            {QStringLiteral("signedIn"), signedIn},
            {QStringLiteral("accountEmail"), accountEmail},
            {QStringLiteral("accountLabel"), label},
            {QStringLiteral("properties"), megaAccountStatusProperties()},
        };
    }

    if (actionId == MegaSignInAction) {
        const QString session = context.parameters.value(QStringLiteral("session")).toString().trimmed();
        const QString email = context.parameters.value(QStringLiteral("email")).toString().trimmed();
        const QString password = context.parameters.value(QStringLiteral("password")).toString();

        if (!session.isEmpty()) {
            return runBlockingMegaAuthorization(
                [session]() { return megaClient().resumeAccountSession(session); },
                QStringLiteral("MEGA session was resumed."),
                QStringLiteral("Could not start MEGA session resume."));
        }

        if (email.isEmpty() || password.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("MEGA")},
                {QStringLiteral("message"), QStringLiteral("MEGA sign in requires email and password parameters.")},
                {QStringLiteral("requiresInput"), true},
                {QStringLiteral("inputKind"), QStringLiteral("megaCredentials")},
            };
        }

        return runBlockingMegaAuthorization(
            [email, password]() { return megaClient().loginToAccount(email, password); },
            QStringLiteral("MEGA sign in completed."),
            QStringLiteral("Could not start MEGA sign in."));
    }

    if (actionId == MegaSignOutAction) {
        QString error;
        const bool ok = megaClient().logoutAccount(&error);
        if (ok) {
            MegaAuth::clearSavedAuthorization();
        }
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("MEGA")},
            {QStringLiteral("message"), ok ? QStringLiteral("MEGA authorization was removed.") : error},
            {QStringLiteral("refreshCurrentPath"), ok},
        };
    }

    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("title"), QStringLiteral("MEGA")},
        {QStringLiteral("message"), QStringLiteral("Unknown MEGA action.")},
    };
}

#include "MegaFileProviderPlugin.moc"
