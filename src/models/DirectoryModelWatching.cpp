#include "DirectoryModel.h"
#include "DirectoryModelAlgorithms.h"
#include "DirectoryWatchPolicy.h"

#include "../core/ArchiveSupport.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include "../core/FileError.h"
#include "../core/FileProviderFactory.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"
#include "../core/LocalFileBadgeResolver.h"
#include "../core/LocalMountPointIndex.h"
#include "../core/FavoritesStore.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QLocale>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QtGlobal>
#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "DirectoryModelInternal.h"

using namespace DirectoryModelInternal;

bool DirectoryModel::canWatchPath(const QString &path) const
{
    const bool providerCanWatch = !path.isEmpty()
        && !ArchiveSupport::isArchivePath(path)
        && m_provider
        && m_provider->capabilities().testFlag(FileProvider::Watch);
    if (!providerCanWatch) {
        return false;
    }

#ifdef Q_OS_LINUX
    if (m_provider->scheme() == QLatin1String("file")) {
        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        traceDirectoryWatch("watch-capabilities", path,
                            QStringLiteral("exists=%1 directory=%2 browse=%3 traverse=%4 exact=%5")
                                .arg(capabilities.exists)
                                .arg(capabilities.isDirectory)
                                .arg(capabilities.access.canBrowse)
                                .arg(capabilities.access.canTraverse)
                                .arg(capabilities.access.exact));
        // inotify runs in the desktop process, not in fm-admin-helper.  Trying
        // to watch an admin-only directory emits watchFailed; the recovery
        // path then mistakes QFileInfo's EACCES result for external removal
        // and navigates back to the parent.  Admin-backed scans deliberately
        // operate without a live watcher until the directory becomes locally
        // browsable again.
        if (!capabilities.isDirectory
                || !capabilities.access.canBrowse
                || !capabilities.access.canTraverse) {
            traceDirectoryWatch("watch-skip-admin-only", path);
            return false;
        }
    }
#endif

    return true;
}

void DirectoryModel::restartChangeWatcherForCurrentPath()
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceDirectoryNav("restartWatch-begin", m_currentPath,
                      QStringLiteral("watched=%1 suppress=%2 canWatch=%3")
                          .arg(QDir::toNativeSeparators(m_changeWatcher->watchedPath()))
                          .arg(m_suppressNextWatchRestart)
                          .arg(canWatchPath(m_currentPath)));
    m_changeWatcher->stop();
    m_parentChangeWatcher->stop();
    if (m_suppressNextWatchRestart) {
        m_suppressNextWatchRestart = false;
        m_deferredWatchRestartPending = true;
        m_deferredWatchRestartPath = m_currentPath;
        traceDirectoryWatch("restart-watch-suppressed", m_currentPath);
        traceDirectoryNav("restartWatch-end", m_currentPath,
                          QStringLiteral("result=suppressed elapsedMs=%1").arg(totalTimer.elapsed()));
        return;
    }
    if (!canWatchPath(m_currentPath)) {
        traceDirectoryNav("restartWatch-end", m_currentPath,
                          QStringLiteral("result=skip elapsedMs=%1").arg(totalTimer.elapsed()));
        return;
    }

    const bool watching = m_changeWatcher->watch(m_currentPath);
    if (!watching) {
        traceDirectoryWatch("restart-watch-failed", m_currentPath);
    }
    restartParentChangeWatcherForCurrentPath();
    traceDirectoryNav("restartWatch-end", m_currentPath,
                      QStringLiteral("result=%1 elapsedMs=%2 watched=%3")
                          .arg(watching)
                          .arg(totalTimer.elapsed())
                          .arg(QDir::toNativeSeparators(m_changeWatcher->watchedPath())));
}

void DirectoryModel::restartParentChangeWatcherForCurrentPath()
{
    m_parentChangeWatcher->stop();
    if (!canWatchPath(m_currentPath)) {
        return;
    }

    const QString currentPath = QDir::fromNativeSeparators(QFileInfo(m_currentPath).absoluteFilePath());
    const QString parentPath = QDir::fromNativeSeparators(QFileInfo(currentPath).absolutePath());
    if (parentPath.isEmpty() || sameFilesystemPath(parentPath, currentPath)) {
        return;
    }

    if (!m_parentChangeWatcher->watch(parentPath)) {
        traceDirectoryWatch("restart-parent-watch-failed", parentPath);
    }
}

void DirectoryModel::scheduleDeferredWatchRestart()
{
    if (!m_deferredWatchRestartPending) {
        return;
    }

    const QString expectedPath = m_deferredWatchRestartPath;
    traceDirectoryWatch("deferred-watch-schedule", expectedPath);
    QTimer::singleShot(600, this, [this, expectedPath]() {
        traceDirectoryWatch("deferred-watch-fire", expectedPath,
                            QStringLiteral("current=%1 loading=%2 watched=%3 pending=%4")
                                .arg(m_currentPath)
                                .arg(m_loading)
                                .arg(m_changeWatcher->watchedPath())
                                .arg(m_deferredWatchRestartPending));
        if (!m_deferredWatchRestartPending) {
            return;
        }
        if (m_loading
            || !sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                                   QDir::fromNativeSeparators(expectedPath))
            || !m_changeWatcher->watchedPath().isEmpty()) {
            return;
        }

        m_deferredWatchRestartPending = false;
        m_deferredWatchRestartPath.clear();
        restartChangeWatcherForCurrentPath();
    });
}

void DirectoryModel::onDirectoryEventsReady(const QList<DirectoryChangeEvent> &events)
{
    if (events.isEmpty() || m_loading) {
        return;
    }
    const QString watchedPath = m_changeWatcher->watchedPath();
    for (const DirectoryChangeEvent &event : events) {
        if (!DirectoryWatchPolicy::sourceMatches(event, watchedPath)) {
            traceDirectoryWatch("events-drop-source", m_currentPath,
                                QStringLiteral("source=%1 watched=%2")
                                    .arg(event.sourcePath)
                                    .arg(watchedPath));
            return;
        }
    }
    m_watchEventsReceived += events.size();

    if (m_bulkWatchSuppressed
        && sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                              QDir::fromNativeSeparators(m_bulkWatchSuppressedPath))) {
        for (const DirectoryChangeEvent &event : events) {
            if (!event.path.isEmpty()) {
                FileAccessResolver::invalidate(event.path);
            }
            if (!event.oldPath.isEmpty()) {
                FileAccessResolver::invalidate(event.oldPath);
            }
            if (!event.newPath.isEmpty()) {
                FileAccessResolver::invalidate(event.newPath);
            }
        }
        m_bulkWatchDirty = true;
        ++m_bulkWatchSuppressedBatches;
        m_bulkWatchSuppressedEvents += events.size();
        if (watchDebugEnabled()) {
            qDebug() << "[DirectoryWatch] bulk-suppressed"
                     << "path" << m_currentPath
                     << "incoming" << events.size()
                     << "batches" << m_bulkWatchSuppressedBatches
                     << "events" << m_bulkWatchSuppressedEvents;
        }
        return;
    }

    int transientPartEventsDropped = 0;
    int acceptedEvents = 0;
    for (const DirectoryChangeEvent &event : events) {
        if (DirectoryWatchPolicy::isTransientPartWrite(event)) {
            ++transientPartEventsDropped;
            continue;
        }
        DirectoryWatchPolicy::appendCoalesced(m_pendingDirectoryEvents, event);
        ++acceptedEvents;
    }

    if (m_pendingDirectoryEvents.size() > 256) {
        m_pendingDirectoryEvents.clear();
        DirectoryChangeEvent overflow;
        overflow.type = DirectoryChangeEvent::Type::Overflow;
        overflow.path = m_currentPath;
        m_pendingDirectoryEvents.append(overflow);
    }
    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] queued"
                 << "path" << m_currentPath
                 << "incoming" << events.size()
                 << "accepted" << acceptedEvents
                 << "pending" << m_pendingDirectoryEvents.size()
                 << "received" << m_watchEventsReceived
                 << "droppedPart" << transientPartEventsDropped;
    }
    if (acceptedEvents > 0 && !m_pendingDirectoryEvents.isEmpty()) {
        m_directoryEventTimer.start();
    }
}

void DirectoryModel::processPendingDirectoryEvents()
{
    if (m_pendingDirectoryEvents.isEmpty()) {
        return;
    }
    if (m_loading) {
        m_pendingDirectoryEvents.clear();
        return;
    }
    const QList<DirectoryChangeEvent> events = std::exchange(m_pendingDirectoryEvents, {});
    applyDirectoryChangeEvents(events);
}

void DirectoryModel::applyDirectoryChangeEvents(const QList<DirectoryChangeEvent> &events)
{
    ++m_watchBatchesApplied;
    bool needsRefresh = false;
    QHash<QString, DirectoryChangeEvent> pendingByPath;
    QList<DirectoryChangeEvent> orderedEvents;

    for (const DirectoryChangeEvent &event : events) {
        if (!event.path.isEmpty()) {
            FileAccessResolver::invalidate(event.path);
        }
        if (!event.oldPath.isEmpty()) {
            FileAccessResolver::invalidate(event.oldPath);
        }
        if (!event.newPath.isEmpty()) {
            FileAccessResolver::invalidate(event.newPath);
        }

        if (event.type == DirectoryChangeEvent::Type::Overflow) {
            if (!sameFilesystemPath(QDir::fromNativeSeparators(event.path), QDir::fromNativeSeparators(m_currentPath))) {
                continue;
            }
            if (!m_currentPath.isEmpty() && !QFileInfo::exists(m_currentPath)) {
                notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
                return;
            }
            needsRefresh = true;
            break;
        }

        if ((!event.path.isEmpty() && !pathIsInDirectory(event.path, m_currentPath))
            || (!event.oldPath.isEmpty() && !pathIsInDirectory(event.oldPath, m_currentPath))
            || (!event.newPath.isEmpty() && !pathIsInDirectory(event.newPath, m_currentPath))) {
            continue;
        }

        switch (event.type) {
        case DirectoryChangeEvent::Type::Added:
        case DirectoryChangeEvent::Type::Modified:
            if (!event.path.isEmpty()) {
                DirectoryChangeEvent coalesced = event;
                coalesced.type = DirectoryChangeEvent::Type::Modified;
                pendingByPath.insert(modelPathKey(event.path), coalesced);
            }
            break;
        case DirectoryChangeEvent::Type::Removed:
            if (!event.path.isEmpty()) {
                const QString normalizedPath = modelPathKey(event.path);
                pendingByPath.remove(normalizedPath);
                DirectoryChangeEvent coalesced = event;
                coalesced.path = QDir::fromNativeSeparators(event.path);
                pendingByPath.insert(normalizedPath, coalesced);
            }
            break;
        case DirectoryChangeEvent::Type::Renamed:
            if (!event.oldPath.isEmpty() && !event.newPath.isEmpty()) {
                pendingByPath.remove(modelPathKey(event.oldPath));
                pendingByPath.remove(modelPathKey(event.newPath));
                orderedEvents.append(event);
            }
            break;
        case DirectoryChangeEvent::Type::Overflow:
            break;
        }
    }

    if (!needsRefresh) {
        int renameCount = 0;
        int upsertCount = 0;
        int removeCount = 0;

        for (const DirectoryChangeEvent &event : std::as_const(orderedEvents)) {
            ++renameCount;
            if (!renamePath(event.oldPath, event.newPath)) {
                removePath(event.oldPath);
                upsertPath(event.newPath);
            }
        }

        for (const DirectoryChangeEvent &event : std::as_const(pendingByPath)) {
            switch (event.type) {
            case DirectoryChangeEvent::Type::Added:
            case DirectoryChangeEvent::Type::Modified:
                ++upsertCount;
                upsertPath(event.path);
                break;
            case DirectoryChangeEvent::Type::Removed:
                ++removeCount;
                removePath(event.path);
                break;
            case DirectoryChangeEvent::Type::Renamed:
            case DirectoryChangeEvent::Type::Overflow:
                break;
            }
        }
        if (watchDebugEnabled()) {
            qDebug() << "[DirectoryWatch] applied"
                     << "path" << m_currentPath
                     << "batch" << m_watchBatchesApplied
                     << "events" << events.size()
                     << "renames" << renameCount
                     << "upserts" << upsertCount
                     << "removes" << removeCount;
        }
        return;
    }

    ++m_watchOverflowRefreshes;
    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] overflow-refresh"
                 << "path" << m_currentPath
                 << "batch" << m_watchBatchesApplied
                 << "events" << events.size()
                 << "overflows" << m_watchOverflowRefreshes;
    }
    m_debounceTimer.start();
}

void DirectoryModel::onDirectoryWatchFailed(const QString &path, const QString &error)
{
    traceDirectoryWatch("watch-failed", path,
                        QStringLiteral("current=%1 recovering=%2 error=%3")
                            .arg(m_currentPath)
                            .arg(m_recoveringUnavailablePath)
                            .arg(error));

    const QString failedPath = QDir::fromNativeSeparators(path);
    const QString currentPath = QDir::fromNativeSeparators(m_currentPath);
    if (!currentPath.isEmpty()
        && sameFilesystemPath(failedPath, currentPath)
        && !m_loading) {
        if (!QFileInfo::exists(currentPath)) {
            notifyCurrentPathUnavailable(error);
            return;
        }
        refresh();
    }
}

void DirectoryModel::onParentDirectoryEventsReady(const QList<DirectoryChangeEvent> &events)
{
    if (events.isEmpty() || m_loading || m_currentPath.isEmpty()) {
        return;
    }

    const QString watchedPath = m_parentChangeWatcher->watchedPath();
    const QString currentPath = QDir::fromNativeSeparators(QFileInfo(m_currentPath).absoluteFilePath());
    for (const DirectoryChangeEvent &event : events) {
        if (!DirectoryWatchPolicy::sourceMatches(event, watchedPath)) {
            continue;
        }

        if (event.type == DirectoryChangeEvent::Type::Overflow) {
            if (!QFileInfo::exists(currentPath)) {
                notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
                return;
            }
            continue;
        }

        const QString removedPath = event.type == DirectoryChangeEvent::Type::Renamed
            ? event.oldPath
            : event.path;
        if (!removedPath.isEmpty()
            && sameFilesystemPath(QDir::fromNativeSeparators(removedPath), currentPath)
            && (event.type == DirectoryChangeEvent::Type::Removed
                || event.type == DirectoryChangeEvent::Type::Renamed)) {
            notifyCurrentPathUnavailable(QStringLiteral("Folder is no longer available"));
            return;
        }
    }
}

void DirectoryModel::onParentDirectoryWatchFailed(const QString &path, const QString &error)
{
    traceDirectoryWatch("parent-watch-failed", path,
                        QStringLiteral("current=%1 recovering=%2 error=%3")
                            .arg(m_currentPath)
                            .arg(m_recoveringUnavailablePath)
                            .arg(error));

    if (!m_currentPath.isEmpty()
        && !m_loading
        && !QFileInfo::exists(m_currentPath)) {
        notifyCurrentPathUnavailable(error);
    }
}

void DirectoryModel::onDebounceTimeout()
{
    if (!m_currentPath.isEmpty() && !m_loading) {
        refresh();
    }
}

void DirectoryModel::refresh()
{
    if (m_bulkWatchSuppressed) {
        m_pendingDirectoryEvents.clear();
    }
    if (!m_currentPath.isEmpty()) {
        m_provider->setShowHidden(m_showHidden);
        m_provider->refresh(m_currentPath);
    }
}

void DirectoryModel::refreshMountPointBadges()
{
    const QList<int> roles = {IsMountPointRole, PrimaryBadgeKindRole};
    for (int absoluteIndex = 0; absoluteIndex < m_entries.size(); ++absoluteIndex) {
        FileEntry &entry = m_entries[absoluteIndex];
        if (!entry.isDirectory || !QDir::isAbsolutePath(entry.path)) {
            continue;
        }

        const bool isMountPoint = LocalMountPointIndex::isMountPoint(entry.path);
        const bool isArchive = !entry.isDirectory
            && ArchiveSupport::isArchiveExtension(entry.suffix);
        const QString primaryBadgeKind = LocalFileBadgeResolver::primaryBadgeKind(
            entry.isBrokenSymLink, entry.isSymLink, isMountPoint, entry.isLocked, isArchive);
        if (entry.isMountPoint == isMountPoint
            && entry.primaryBadgeKind == primaryBadgeKind) {
            continue;
        }

        entry.isMountPoint = isMountPoint;
        entry.primaryBadgeKind = primaryBadgeKind;
        const int filteredIndex = m_filteredIndices.indexOf(absoluteIndex);
        if (filteredIndex >= 0) {
            const QModelIndex modelIndex = index(filteredIndex, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
    }
}

void DirectoryModel::setPinnedPathSnapshot(const QStringList &paths)
{
    m_pinnedPathKeys.clear();
    for (const QString &path : paths) {
        const QString key = FavoritesStore::normalizedPathKey(path);
        if (!key.isEmpty()) {
            m_pinnedPathKeys.insert(key);
        }
    }

    const QList<int> roles = {IsPinnedRole};
    for (int absoluteIndex = 0; absoluteIndex < m_entries.size(); ++absoluteIndex) {
        FileEntry &entry = m_entries[absoluteIndex];
        const bool isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
        if (entry.isPinned == isPinned) {
            continue;
        }
        entry.isPinned = isPinned;
        const int filteredIndex = m_filteredIndices.indexOf(absoluteIndex);
        if (filteredIndex >= 0) {
            const QModelIndex modelIndex = index(filteredIndex, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
    }
}

void DirectoryModel::updatePinnedPaths(const QStringList &changedPaths, const QStringList &snapshot)
{
    m_pinnedPathKeys.clear();
    for (const QString &path : snapshot) {
        const QString key = FavoritesStore::normalizedPathKey(path);
        if (!key.isEmpty()) {
            m_pinnedPathKeys.insert(key);
        }
    }

    const QList<int> roles = {IsPinnedRole};
    for (const QString &path : changedPaths) {
        const int absoluteIndex = m_pathIndex.value(modelPathKey(path), -1);
        if (absoluteIndex < 0 || absoluteIndex >= m_entries.size()) {
            continue;
        }
        FileEntry &entry = m_entries[absoluteIndex];
        const bool isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
        if (entry.isPinned == isPinned) {
            continue;
        }
        entry.isPinned = isPinned;
        const int filteredIndex = m_filteredIndices.indexOf(absoluteIndex);
        if (filteredIndex >= 0) {
            const QModelIndex modelIndex = index(filteredIndex, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
    }
}

void DirectoryModel::beginBulkWatchSuppression(const QString &path)
{
    if (path.isEmpty()
        || m_currentPath.isEmpty()
        || !sameFilesystemPath(QDir::fromNativeSeparators(m_currentPath),
                               QDir::fromNativeSeparators(path))) {
        return;
    }

    m_bulkWatchSuppressed = true;
    m_bulkWatchDirty = false;
    m_bulkWatchSuppressedPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    m_bulkWatchSuppressedBatches = 0;
    m_bulkWatchSuppressedEvents = 0;
    m_pendingDirectoryEvents.clear();
    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] bulk-suppress-begin"
                 << "path" << m_currentPath;
    }
}

void DirectoryModel::endBulkWatchSuppression(const QString &path)
{
    if (!m_bulkWatchSuppressed) {
        return;
    }
    if (!path.isEmpty()
        && !sameFilesystemPath(QDir::fromNativeSeparators(path),
                               QDir::fromNativeSeparators(m_bulkWatchSuppressedPath))) {
        return;
    }

    if (watchDebugEnabled()) {
        qDebug() << "[DirectoryWatch] bulk-suppress-end"
                 << "path" << m_currentPath
                 << "dirty" << m_bulkWatchDirty
                 << "batches" << m_bulkWatchSuppressedBatches
                 << "events" << m_bulkWatchSuppressedEvents;
    }
    m_bulkWatchSuppressed = false;
    m_bulkWatchDirty = false;
    m_bulkWatchSuppressedPath.clear();
    m_bulkWatchSuppressedBatches = 0;
    m_bulkWatchSuppressedEvents = 0;
    m_pendingDirectoryEvents.clear();
}

void DirectoryModel::notifyCurrentPathUnavailable(const QString &error)
{
    traceDirectoryWatch("unavailable-enter", m_currentPath,
                        QStringLiteral("recovering=%1 error=%2 watched=%3")
                            .arg(m_recoveringUnavailablePath)
                            .arg(error)
                            .arg(m_changeWatcher->watchedPath()));
    if (m_currentPath.isEmpty() || m_recoveringUnavailablePath) {
        return;
    }
    m_recoveringUnavailablePath = true;

    const QString unavailablePath = m_currentPath;
    if (m_provider) {
        m_provider->cancel();
        m_currentScanGeneration = m_provider->currentGeneration();
    }
    m_changeWatcher->stop();
    m_parentChangeWatcher->stop();
    m_deferredWatchRestartPending = false;
    m_deferredWatchRestartPath.clear();
    m_debounceTimer.stop();
    m_directoryEventTimer.stop();
    m_pendingDirectoryEvents.clear();
    m_insertTimer.stop();
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;
    emit visualStructureAboutToChange();
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_foundPaths.clear();
    m_selectedCount = 0;
    endResetModel();
    setLoading(false);
    setError(QStringLiteral("Folder is no longer available"));
    emit countChanged();
    emit selectionChanged();
    emit directoryUnavailable(unavailablePath,
                              error.isEmpty()
                                  ? QStringLiteral("Folder is no longer available")
                                  : error);
}

void DirectoryModel::clearError()
{
    setError({});
    setLastError({});
}

void DirectoryModel::noteLocalMutation()
{
    m_localMutationThrottle.restart();
    m_debounceTimer.stop();
}

void DirectoryModel::suppressNextWatchRestart()
{
    m_suppressNextWatchRestart = true;
    m_deferredWatchRestartPending = false;
    m_deferredWatchRestartPath.clear();
    traceDirectoryWatch("suppress-next-watch", m_currentPath);
}

