#include "FilePanelController.h"

#include "../core/FileProviderFactory.h"
#include "../core/OperationQueue.h"
#include "../core/CleanupSubsystem.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QUuid>
#include <QScopeGuard>
#include <QtConcurrent>

FilePanelController::~FilePanelController()
{
    cancelRemoteMutation();
}

void FilePanelController::cancelRemoteMutation()
{
    if (m_remoteMutationCancelled) m_remoteMutationCancelled->store(true);
}

bool FilePanelController::startRemoteMutation(const QString &operation, const QString &path, const QString &name)
{
    if (m_remoteMutationPending) return false;
    if (operation == QLatin1String("open")) CleanupSubsystem::instance();
    m_remoteMutationCancelled = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = m_remoteMutationCancelled;
    const QString startPath = currentPath();
    const int navigation = m_navigationRequestId;
    m_remoteMutationPending = true;
    setLastError({});
    setStatusMessage(operation == QLatin1String("open") ? QStringLiteral("Downloading file…")
                     : operation == QLatin1String("rename") ? QStringLiteral("Renaming…") : QStringLiteral("Creating…"));
    emit remoteMutationPendingChanged();
    emit capabilitiesChanged();
    disconnect(&m_remoteMutationWatcher, nullptr, this, nullptr);
    connect(&m_remoteMutationWatcher, &QFutureWatcher<RemoteMutationResult>::finished, this,
            [this, operation, path, startPath, navigation, cancelled]() {
        const auto result = m_remoteMutationWatcher.result();
        m_remoteMutationPending = false;
        emit remoteMutationPendingChanged();
        emit capabilitiesChanged();
        const bool sameLocation = currentPath() == startPath && m_navigationRequestId == navigation;
        if (operation == QLatin1String("open")) {
            if (result.success && sameLocation && !cancelled->load()) {
                setStatusMessage(QStringLiteral("Downloaded a read-only copy."));
                openFilePath(result.path);
            } else {
                if (!result.leaseId.isEmpty()) CleanupSubsystem::instance().scheduleDeleteOnFailure(result.leaseId);
                if (sameLocation) {
                    if (cancelled->load()) setStatusMessage(QStringLiteral("Operation cancelled."));
                    else setOperationError(result.error, path, operation);
                }
            }
            emit remoteMutationFinished(operation, path, result.success);
            return;
        }
        if (result.success) {
            FileAccessResolver::invalidate(path);
            FileAccessResolver::invalidate(result.path);
            if (sameLocation) {
                setLastError({});
                refresh();
                setStatusMessage(operation == QLatin1String("rename") ? QStringLiteral("Item renamed.")
                                                                      : QStringLiteral("Item created."));
            }
        } else if (sameLocation && !cancelled->load()) {
            setOperationError(result.error, path, operation);
        } else if (sameLocation) {
            refresh();
            setStatusMessage(QStringLiteral("Operation cancelled."));
        }
        emit remoteMutationFinished(operation, path, result.success);
        if (result.success && sameLocation && !cancelled->load()) {
            if (operation == QLatin1String("rename")) emit entryRenamed(path, result.path);
            else emit entryCreated(result.path);
        }
        emit contentsChanged(startPath);
    });
    m_remoteMutationWatcher.setFuture(QtConcurrent::run([operation, path, name, cancelled]() {
        const auto previous = OperationQueue::currentThreadAbortChecker();
        OperationQueue::setCurrentThreadAbortChecker([cancelled] { return cancelled->load(); });
        const auto restore = qScopeGuard([&] { OperationQueue::setCurrentThreadAbortChecker(previous); });
        RemoteMutationResult result;
        auto provider = FileProviderFactory::createProvider(path);
        if (cancelled->load()) { result.error = QStringLiteral("Operation cancelled."); return result; }
        if (!provider) { result.error = QStringLiteral("The remote provider is unavailable."); return result; }
        if (operation == QLatin1String("rename")) {
            result.success = provider->renamePath(path, name);
            result.path = provider->childPath(provider->parentPath(path), name.trimmed());
        } else if (operation == QLatin1String("createFolder")) {
            result.success = provider->createFolder(path, name, &result.path);
        } else if (operation == QLatin1String("createFile")) {
            result.success = provider->createFile(path, name, &result.path);
        } else if (operation == QLatin1String("open")) {
            const auto entry = provider->entryInfo(path);
            if (!entry || entry->isDirectory || entry->isSymLink || entry->isSystem) {
                result.error = provider->lastErrorString().isEmpty()
                    ? QStringLiteral("Cannot open this remote item.") : provider->lastErrorString();
                return result;
            }
            const QString directory = CleanupSubsystem::instance().allocateStagingDirectory(
                CleanupArtifactKind::RemotePreview, StagingLocationPolicy::defaultCleanupRoot(),
                QUuid::createUuid().toString(QUuid::WithoutBraces), &result.leaseId);
            if (directory.isEmpty() || result.leaseId.isEmpty()) {
                result.error = QStringLiteral("Cannot create a remote file staging folder.");
                return result;
            }
            QString fileName = provider->fileName(path);
            fileName.replace(QRegularExpression(QStringLiteral(R"([/\\\x00-\x1F])")), QStringLiteral("_"));
            if (fileName.isEmpty()) fileName = QStringLiteral("remote-file");
            result.path = QDir(directory).filePath(fileName.left(180));
            result.success = provider->copyToLocalFile(path, result.path,
                [cancelled](qint64, qint64) { return !cancelled->load(); }, &result.error);
            if (result.success && !QFile::setPermissions(result.path, QFileDevice::ReadOwner)) {
                result.success = false;
                result.error = QStringLiteral("Cannot make the downloaded copy read-only.");
            }
            if (!result.success) CleanupSubsystem::instance().scheduleDeleteOnFailure(result.leaseId);
        }
        if (!result.success && result.error.isEmpty()) result.error = provider->lastErrorString();
        return result;
    }));
    return true;
}
