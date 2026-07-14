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

bool DirectoryModel::openPath(const QString &path)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceDirectoryNav("openPath-begin", path,
                      QStringLiteral("current=%1 provider=%2")
                          .arg(QDir::toNativeSeparators(m_currentPath),
                               m_provider ? m_provider->scheme() : QStringLiteral("<none>")));

    if (path.isEmpty()) {
        traceDirectoryNav("openPath-end", path,
                          QStringLiteral("result=false reason=empty elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    const bool archivePath = ArchiveSupport::isArchivePath(path);
    std::unique_ptr<FileProvider> targetProvider = FileProviderFactory::createProvider(path);
    const QString targetScheme = targetProvider ? targetProvider->scheme() : QStringLiteral("<none>");
    const QString normalizedPath = targetProvider
        ? targetProvider->normalizedPath(path)
        : FileProviderFactory::normalizePath(path);
    traceDirectoryNav("openPath-normalized", normalizedPath,
                      QStringLiteral("archivePath=%1 targetProvider=%2 elapsedMs=%3")
                          .arg(archivePath)
                          .arg(targetScheme)
                          .arg(totalTimer.elapsed()));
    if (normalizedPath.isEmpty()) {
        traceDirectoryNav("openPath-end", path,
                          QStringLiteral("result=false reason=normalize elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    if (!m_provider || !m_provider->canHandle(normalizedPath)) {
        traceDirectoryNav("openPath-replaceProvider", normalizedPath,
                          QStringLiteral("reason=canHandle targetProvider=%1 elapsedMs=%2")
                              .arg(targetScheme)
                              .arg(totalTimer.elapsed()));
        if (targetProvider) {
            replaceProvider(std::move(targetProvider));
        }
    }
    if (!m_provider || !m_provider->canHandle(normalizedPath)) {
        traceDirectoryNav("openPath-end", normalizedPath,
                          QStringLiteral("result=false reason=no-provider elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    m_provider->setShowHidden(m_showHidden);
    const bool pathChanged = isUriPath(normalizedPath) || isUriPath(m_currentPath)
        ? normalizedPath != m_currentPath
        : !sameFilesystemPath(QDir::fromNativeSeparators(normalizedPath),
                              QDir::fromNativeSeparators(m_currentPath));
    if (pathChanged) {
        m_insertTimer.stop();
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        m_pendingScannerFinish = false;
        m_pendingScannerPath.clear();
        m_pendingScannerError.clear();
        m_pendingScannerSuccess = false;
        m_foundPaths.clear();
        traceDirectoryNav("openPath-deferFreshReset", normalizedPath,
                          QStringLiteral("totalMs=%1").arg(totalTimer.elapsed()));
    }
    QElapsedTimer scanTimer;
    scanTimer.start();
    m_provider->scan(normalizedPath);
    traceDirectoryNav("openPath-provider.scan-returned", normalizedPath,
                      QStringLiteral("pathChanged=%1 scanCallMs=%2 totalMs=%3 generation=%4")
                          .arg(pathChanged)
                          .arg(scanTimer.elapsed())
                          .arg(totalTimer.elapsed())
                          .arg(m_provider->currentGeneration()));
    return true;
}

void DirectoryModel::cancelLoading()
{
    if (!m_loading || !m_provider) {
        return;
    }

    m_provider->cancel();
    m_currentScanGeneration = m_provider->currentGeneration();
    m_insertTimer.stop();
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;
    setScanProgress(-1.0);
    setLoading(false);
    setError(QStringLiteral("Archive preparation was cancelled"));
}

void DirectoryModel::clear()
{
    if (m_provider) {
        m_provider->cancel();
    }
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
    m_foundPaths.clear();
    m_previousPath.clear();
    m_pendingFreshLoadPath.clear();
    m_freshLoadCommitted = true;
    m_currentScanGeneration = 0;
    m_selectedCount = 0;

    if (!m_currentPath.isEmpty() && !ArchiveSupport::isArchivePath(m_currentPath)) {
        m_changeWatcher->stop();
        m_parentChangeWatcher->stop();
    }

    emit visualStructureAboutToChange();
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_currentPath.clear();
    endResetModel();

    setLoading(false);
    setError({});
    setLastError({});
    setScanProgress(-1.0);
    emit currentPathChanged();
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::replaceProvider(std::unique_ptr<FileProvider> provider)
{
    if (!provider) {
        return;
    }

    if (m_provider) {
        m_provider->cancel();
        disconnect(m_provider.get(), nullptr, this, nullptr);
    }

    m_provider = std::move(provider);
    connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
    connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(m_provider.get(), &FileProvider::progress, this, &DirectoryModel::onScannerProgress);
    connect(m_provider.get(), &FileProvider::statusMessage, this, &DirectoryModel::providerStatusMessage);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    m_provider->setShowHidden(m_showHidden);
}

void DirectoryModel::onScannerStarted()
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    m_debounceTimer.stop();
    m_directoryEventTimer.stop();
    m_pendingDirectoryEvents.clear();
    m_insertTimer.stop();
    
    const QString scanPath = m_provider->currentPath();
    const QString previousPath = m_currentPath;
    m_previousPath = previousPath;
    m_freshLoad = (scanPath != previousPath);
    m_currentScanGeneration = m_provider->currentGeneration();
    m_recoveringUnavailablePath = false;
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_foundPaths.clear();
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;
    if (m_freshLoad) {
        m_localMutationThrottle.invalidate();
        m_pendingFreshLoadPath = scanPath;
        m_freshLoadCommitted = false;
    } else {
        m_pendingFreshLoadPath.clear();
        m_freshLoadCommitted = true;
    }

    setLoading(true);
    setError({});
    setLastError({});
    setScanProgress(-1.0);
    emit countChanged();
    emit selectionChanged();
    traceDirectoryNav("scannerStarted-end", scanPath,
                      QStringLiteral("previous=%1 fresh=%2 generation=%3 elapsedMs=%4")
                          .arg(QDir::toNativeSeparators(previousPath))
                          .arg(m_freshLoad)
                          .arg(m_currentScanGeneration)
                          .arg(totalTimer.elapsed()));
}

void DirectoryModel::onScannerBatchReady(const QList<FileEntry> &entries, int generation)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    if (entries.isEmpty()) {
        return;
    }

    QList<FileEntry> pinnedEntries = entries;
    for (FileEntry &entry : pinnedEntries) {
        entry.isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
    }
    m_pendingInserts.append(pinnedEntries);
    if (m_freshLoad && m_provider && m_provider->scheme() == QStringLiteral("file")) {
        if (!m_freshLoadCommitted) {
            commitFreshLoad(m_pendingFreshLoadPath);
        }
        return;
    }
    if (!m_insertTimer.isActive()) {
        m_insertTimer.start();
    }
}

void DirectoryModel::onScannerProgress(qint64 processedBytes, qint64 totalBytes, const QString &message, int generation)
{
    if (generation != m_currentScanGeneration || totalBytes <= 0) {
        return;
    }

    const double progress = std::clamp(
        static_cast<double>(processedBytes) / static_cast<double>(totalBytes),
        0.0,
        1.0);
    const QString text = message.isEmpty()
        ? QStringLiteral("%1%").arg(qRound(progress * 100.0))
        : QStringLiteral("%1 %2%").arg(message).arg(qRound(progress * 100.0));
    setScanProgress(progress, text);
}

void DirectoryModel::processPendingInserts()
{
    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        m_insertTimer.stop();
        if (m_pendingScannerFinish) {
            finalizeScannerFinished(m_pendingScannerPath, m_pendingScannerSuccess, m_pendingScannerError);
        }
        return;
    }

    const int chunkSize = 150;
    int processed = 0;
    if (m_freshLoad && !m_freshLoadCommitted) {
        commitFreshLoad(m_pendingFreshLoadPath);
    }

    while (m_pendingInsertOffset < m_pendingInserts.size() && processed < chunkSize) {
        FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
        processed++;

        const QString normalizedPath = modelPathKey(entry.path);
        const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);

        const bool visible = m_showHidden || !entry.isHidden;
        const bool matchesFilter = this->matchesFilter(entry);
        const bool shouldBeVisible = visible && matchesFilter;

        if (absoluteIdx >= 0 && absoluteIdx < m_entries.size()) {
            FileEntry &existing = m_entries[absoluteIdx];
            const bool hasChanged = fileEntryMetadataChanged(existing, entry);
            const bool thumbnailChanged = hasChanged && thumbnailIdentityChanged(existing, entry);
            const bool sortOrderChanged = hasChanged && (compareEntries(existing, entry) || compareEntries(entry, existing));

            int filteredRow = -1;
            for (int i = 0; i < m_filteredIndices.size(); ++i) {
                if (m_filteredIndices[i] == absoluteIdx) {
                    filteredRow = i;
                    break;
                }
            }

            if (shouldBeVisible && filteredRow == -1) {
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
                    [this, &entry](int existingIdx, int) {
                        return this->compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                emit visualStructureAboutToChange();
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, absoluteIdx);
                endInsertRows();
            } else if (!shouldBeVisible && filteredRow != -1) {
                emit visualStructureAboutToChange();
                beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
                m_filteredIndices.removeAt(filteredRow);
                endRemoveRows();
            } else if (shouldBeVisible && filteredRow != -1 && hasChanged) {
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
                if (thumbnailChanged) m_thumbnailRevisions[entry.path] = m_thumbnailRevisions.value(entry.path, 0) + 1;
                emit dataChanged(index(filteredRow), index(filteredRow));
                if (sortOrderChanged) {
                    sortModel();
                }
            } else if (hasChanged) {
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
                if (thumbnailChanged) m_thumbnailRevisions[entry.path] = m_thumbnailRevisions.value(entry.path, 0) + 1;
            }
            m_foundPaths.insert(normalizedPath);
        } else {
            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);

            if (shouldBeVisible) {
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                    [this, &entry](int existingIdx, int) {
                        return this->compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                emit visualStructureAboutToChange();
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, newAbsoluteIdx);
                endInsertRows();
            }
        }
    }

    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        m_insertTimer.stop();
        if (m_pendingScannerFinish) {
            finalizeScannerFinished(m_pendingScannerPath, m_pendingScannerSuccess, m_pendingScannerError);
            return;
        }
    } else if (!m_insertTimer.isActive()) {
        m_insertTimer.start();
    }
    
    emit countChanged();
}

void DirectoryModel::onScannerFinished(const QString &path, bool success, int generation, const QString &error)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    const qsizetype pendingCount = m_pendingInserts.size() - m_pendingInsertOffset;
    if (success
        && m_freshLoad
        && m_provider
        && m_provider->scheme() == QStringLiteral("file")
        && pendingCount >= AsyncFreshLoadThreshold) {
        startAsyncFreshLoad(path);
        return;
    }

    if (success
        && pendingCount > 0
        && (pendingCount <= SmallDirectoryThreshold
            || (m_freshLoad && pendingCount >= LargeDirectoryBulkFinishThreshold))) {
        m_insertTimer.stop();
        processAllPendingInsertsFast();
        finalizeScannerFinished(path, success, error);
        return;
    }

    m_pendingScannerFinish = true;
    m_pendingScannerPath = path;
    m_pendingScannerSuccess = success;
    m_pendingScannerError = error;

    if (m_pendingInsertOffset < m_pendingInserts.size()) {
        if (!m_insertTimer.isActive()) {
            m_insertTimer.start();
        }
        return;
    }

    finalizeScannerFinished(path, success, error);
}

void DirectoryModel::finalizeScannerFinished(const QString &path, bool success, const QString &error)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceDirectoryNav("finalize-begin", path,
                      QStringLiteral("success=%1 fresh=%2 entries=%3 filtered=%4 error=%5")
                          .arg(success)
                          .arg(m_freshLoad)
                          .arg(m_entries.size())
                          .arg(m_filteredIndices.size())
                          .arg(error));
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    setLoading(false);
    setScanProgress(-1.0);
    if (success) {
        if (m_freshLoad && !m_freshLoadCommitted) {
            commitFreshLoad(path);
        }
        if (!m_freshLoad) {
            for (int i = m_entries.size() - 1; i >= 0; --i) {
                const QString normPath = modelPathKey(m_entries.at(i).path);
                if (!m_foundPaths.contains(normPath)) {
                    if (m_entries.at(i).isSelected) {
                        --m_selectedCount;
                    }
                    
                    int filteredIdx = -1;
                    for (int j = 0; j < m_filteredIndices.size(); ++j) {
                        if (m_filteredIndices[j] == i) {
                            filteredIdx = j;
                            break;
                        }
                    }

                    if (filteredIdx != -1) {
                        emit visualStructureAboutToChange();
                        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
                        m_filteredIndices.removeAt(filteredIdx);
                        m_entries.removeAt(i);
                        for (int &idx : m_filteredIndices) {
                            if (idx > i) idx--;
                        }
                        endRemoveRows();
                    } else {
                        m_entries.removeAt(i);
                        for (int &idx : m_filteredIndices) {
                            if (idx > i) idx--;
                        }
                    }
                }
            }
            updatePathIndex();
            emit selectionChanged();
        }
        emit countChanged();
        if (m_freshLoad) {
            QElapsedTimer watchTimer;
            watchTimer.start();
            restartChangeWatcherForCurrentPath();
            traceDirectoryNav("finalize-watch-restart", path,
                              QStringLiteral("elapsedMs=%1").arg(watchTimer.elapsed()));
        }
        if (m_deferredWatchRestartPending
            && sameFilesystemPath(QDir::fromNativeSeparators(path),
                                  QDir::fromNativeSeparators(m_deferredWatchRestartPath))) {
            scheduleDeferredWatchRestart();
        }
    } else {
        if (sameFilesystemPath(QDir::fromNativeSeparators(path), QDir::fromNativeSeparators(m_currentPath))
            && scannerFailureIndicatesUnavailable(error)) {
            notifyCurrentPathUnavailable(error);
            m_previousPath.clear();
            return;
        }

        if (m_freshLoad) {
            if (!m_freshLoadCommitted) {
                m_currentPath = m_previousPath;
                emit currentPathChanged();
            }
            selectFailedNavigationTarget(path);
            restoreProviderForCurrentPathLater();
        }
        setError(error);
        setLastError(FileError::classify(error, path, QStringLiteral("open")));
        emit directoryUnavailable(path, error);
    }
    m_previousPath.clear();
    m_pendingFreshLoadPath.clear();
    m_freshLoadCommitted = true;
    traceDirectoryNav("finalize-end", path,
                      QStringLiteral("success=%1 elapsedMs=%2 entries=%3 filtered=%4 loading=%5")
                          .arg(success)
                          .arg(totalTimer.elapsed())
                          .arg(m_entries.size())
                          .arg(m_filteredIndices.size())
                          .arg(m_loading));
}

void DirectoryModel::commitFreshLoad(const QString &path)
{
    if (!m_freshLoad || m_freshLoadCommitted) {
        return;
    }

    const QString targetPath = path.isEmpty() ? m_pendingFreshLoadPath : path;
    m_changeWatcher->stop();
    m_parentChangeWatcher->stop();
    emit visualStructureAboutToChange();
    beginResetModel();
    m_entries.clear();
    m_filteredIndices.clear();
    m_pathIndex.clear();
    m_selectedCount = 0;
    m_currentPath = targetPath;
    endResetModel();

    m_freshLoadCommitted = true;
    m_pendingFreshLoadPath.clear();
    emit currentPathChanged();
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::startAsyncFreshLoad(const QString &path)
{
    const int generation = m_currentScanGeneration;
    const bool showHidden = m_showHidden;
    const bool mixFilesAndFolders = m_mixFilesAndFolders;
    const QString searchText = m_searchText;
    const CategoryFilter categoryFilter = m_categoryFilter;
    const SortRole sortRole = m_sortRole;
    const Qt::SortOrder sortOrder = m_sortOrder;

    QList<FileEntry> baseEntries;
    if (m_freshLoadCommitted) {
        baseEntries = m_entries;
    }

    QList<FileEntry> pendingEntries = std::move(m_pendingInserts);
    const qsizetype pendingOffset = m_pendingInsertOffset;
    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_insertTimer.stop();
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    if (!m_freshLoadCommitted) {
        commitFreshLoad(path);
    }

    auto *watcher = new QFutureWatcher<AsyncFreshLoadResult>(this);
    connect(watcher, &QFutureWatcher<AsyncFreshLoadResult>::finished, this, [this, watcher]() {
        AsyncFreshLoadResult result = watcher->result();
        watcher->deleteLater();

        if (result.generation != m_currentScanGeneration || !m_freshLoad) {
            return;
        }
        if (!sameFilesystemPath(QDir::fromNativeSeparators(result.path),
                                QDir::fromNativeSeparators(m_currentPath))) {
            return;
        }

        const bool policyChanged = result.showHidden != m_showHidden
            || result.mixFilesAndFolders != m_mixFilesAndFolders
            || result.searchText != m_searchText
            || result.categoryFilter != m_categoryFilter
            || result.sortRole != m_sortRole
            || result.sortOrder != m_sortOrder;
        if (policyChanged) {
            m_pendingInserts = std::move(result.entries);
            m_pendingInsertOffset = 0;
            startAsyncFreshLoad(result.path);
            return;
        }

        emit visualStructureAboutToChange();
        beginResetModel();
        m_entries = std::move(result.entries);
        m_filteredIndices = std::move(result.filteredIndices);
        m_pathIndex = std::move(result.pathIndex);
        m_foundPaths = std::move(result.foundPaths);
        m_selectedCount = 0;
        endResetModel();

        emit countChanged();
        emit selectionChanged();
        finalizeScannerFinished(result.path, true, {});
    });

    watcher->setFuture(QtConcurrent::run([generation,
                                          path,
                                          baseEntries = std::move(baseEntries),
                                          pendingEntries = std::move(pendingEntries),
                                          pendingOffset,
                                          showHidden,
                                          searchText,
                                          categoryFilter,
                                          mixFilesAndFolders,
                                          sortRole,
                                          sortOrder]() mutable {
        return buildAsyncFreshLoadResult(generation,
                                         path,
                                         std::move(baseEntries),
                                         std::move(pendingEntries),
                                         pendingOffset,
                                         showHidden,
                                         searchText,
                                         categoryFilter,
                                         mixFilesAndFolders,
                                         sortRole,
                                         sortOrder);
    }));
}

bool DirectoryModel::selectFailedNavigationTarget(const QString &failedPath)
{
    const QString targetPath = failedNavigationSelectionPath(failedPath);
    const int targetIdx = m_pathIndex.value(modelPathKey(targetPath), -1);
    if (targetIdx < 0 || targetIdx >= m_entries.size()) {
        return false;
    }

    const int row = indexOfPath(targetPath);
    if (row < 0) {
        return false;
    }

    selectOnly(row);
    return true;
}

void DirectoryModel::restoreProviderForCurrentPathLater()
{
    const QString path = m_currentPath;
    if (path.isEmpty() || (m_provider && m_provider->canHandle(path))) {
        return;
    }

    QTimer::singleShot(0, this, [this, path]() {
        if (path == m_currentPath && (!m_provider || !m_provider->canHandle(path))) {
            replaceProvider(FileProviderFactory::createProvider(path));
        }
    });
}

void DirectoryModel::updatePathIndex()
{
    m_pathIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        m_pathIndex.insert(modelPathKey(m_entries[i].path), i);
    }
}

int DirectoryModel::filteredRowForAbsoluteIndex(int absoluteIdx) const
{
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_filteredIndices.at(i) == absoluteIdx) {
            return i;
        }
    }
    return -1;
}

