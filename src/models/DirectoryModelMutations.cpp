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

bool DirectoryModel::upsertPath(const QString &path)
{
    if (path.isEmpty() || m_currentPath.isEmpty() || ArchiveSupport::isArchivePath(m_currentPath)) {
        return false;
    }

    const QFileInfo info(path);
    const QString normalizedPath = QDir::fromNativeSeparators(info.absoluteFilePath());
    const QString pathKey = modelPathKey(normalizedPath);
    const QString parentPath = QDir::fromNativeSeparators(info.absolutePath());
    const QString currentPath = QDir::fromNativeSeparators(QFileInfo(m_currentPath).absoluteFilePath());

    if (!sameFilesystemPath(parentPath, currentPath)) {
        return false;
    }

    std::optional<FileEntry> maybeEntry = m_provider ? m_provider->entryInfo(normalizedPath) : std::nullopt;
    if (!maybeEntry.has_value()) {
        return removePath(path);
    }

    FileEntry entry = maybeEntry.value();
    entry.isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
    const QString entryPathKey = modelPathKey(entry.path);
    const int absoluteIdx = m_pathIndex.value(pathKey, -1);
    const bool shouldBeVisible = (m_showHidden || !entry.isHidden) && matchesFilter(entry);

    if (absoluteIdx < 0) {
        const int newAbsoluteIdx = m_entries.size();
        m_entries.append(entry);
        m_pathIndex.insert(entryPathKey, newAbsoluteIdx);

        if (shouldBeVisible) {
            auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                [this, &entry](int existingIdx, int) {
                    return compareEntries(m_entries.at(existingIdx), entry);
                });
            const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
            emit visualStructureAboutToChange();
            beginInsertRows(QModelIndex(), row, row);
            m_filteredIndices.insert(row, newAbsoluteIdx);
            endInsertRows();
        }

        emit countChanged();
        return true;
    }

    const int filteredRow = filteredRowForAbsoluteIndex(absoluteIdx);

    FileEntry &existing = m_entries[absoluteIdx];
    const bool wasSelected = existing.isSelected;
    const bool changed = fileEntryMetadataChanged(existing, entry);
    const bool thumbnailChanged = changed && thumbnailIdentityChanged(existing, entry);
    const bool sortOrderChanged = changed && (compareEntries(existing, entry) || compareEntries(entry, existing));
    entry.isSelected = wasSelected;

    if (shouldBeVisible && filteredRow == -1) {
        existing = entry;
        auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
            [this, &entry](int existingIdx, int) {
                return compareEntries(m_entries.at(existingIdx), entry);
            });
        const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
        emit visualStructureAboutToChange();
        beginInsertRows(QModelIndex(), row, row);
        m_filteredIndices.insert(row, absoluteIdx);
        endInsertRows();
        emit countChanged();
        return true;
    }

    if (!shouldBeVisible && filteredRow != -1) {
        existing = entry;
        emit visualStructureAboutToChange();
        beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
        m_filteredIndices.removeAt(filteredRow);
        endRemoveRows();
        emit countChanged();
        return true;
    }

    if (changed) {
        existing = entry;
        if (thumbnailChanged) m_thumbnailRevisions[entry.path] = m_thumbnailRevisions.value(entry.path, 0) + 1;
        if (filteredRow != -1) {
            emit dataChanged(index(filteredRow), index(filteredRow));
            if (sortOrderChanged) {
                sortModel();
            }
        }
        return true;
    }

    return false;
}

bool DirectoryModel::insertPath(const QString &path)
{
    if (path.isEmpty() || m_currentPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
#ifdef Q_OS_WIN
    if (!info.exists() && entryAttributesWindows(info) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
#else
    if (!info.exists()) {
        return false;
    }
#endif

    const QString normPath = modelPathKey(info.absoluteFilePath());
    if (!sameFilesystemPath(QDir::fromNativeSeparators(info.absolutePath()),
                            QDir::fromNativeSeparators(m_currentPath))) {
        return false;
    }
    if (m_pathIndex.contains(normPath)) {
        return false;
    }

    FileEntry entry = entryFromInfo(info);
    entry.isPinned = m_pinnedPathKeys.contains(FavoritesStore::normalizedPathKey(entry.path));
    const int newAbsoluteIdx = m_entries.size();
    m_entries.append(entry);
    m_pathIndex.insert(normPath, newAbsoluteIdx);

    const bool visible = m_showHidden || !entry.isHidden;
    const bool matchesEntryFilter = this->matchesFilter(entry);

    if (visible && matchesEntryFilter) {
        auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
            [&](int existingIdx, int) {
                return this->compareEntries(m_entries.at(existingIdx), entry);
        });
        const int row = std::distance(m_filteredIndices.begin(), it);
        emit visualStructureAboutToChange();
        beginInsertRows(QModelIndex(), row, row);
        m_filteredIndices.insert(row, newAbsoluteIdx);
        endInsertRows();
    }

    emit countChanged();
    return true;
}

bool DirectoryModel::removePath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    QString normalizedPath = modelPathKey(QFileInfo(path).absoluteFilePath());
    int absoluteIdx = m_pathIndex.value(normalizedPath, -1);
    if (absoluteIdx < 0 && isProviderEntryPath(path)) {
        normalizedPath = modelPathKey(path);
        absoluteIdx = m_pathIndex.value(normalizedPath, -1);
    }
    
    if (absoluteIdx < 0) {
        return false;
    }

    if (m_entries.at(absoluteIdx).isSelected) {
        --m_selectedCount;
        emit selectionChanged();
    }

    const int filteredIdx = filteredRowForAbsoluteIndex(absoluteIdx);

    if (filteredIdx != -1) {
        emit visualStructureAboutToChange();
        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
        m_filteredIndices.removeAt(filteredIdx);
        m_pathIndex.remove(normalizedPath);
        m_entries.removeAt(absoluteIdx);

        for (int &idx : m_filteredIndices) {
            if (idx > absoluteIdx) {
                --idx;
            }
        }
        updatePathIndex();
        endRemoveRows();
    } else {
        m_pathIndex.remove(normalizedPath);
        m_entries.removeAt(absoluteIdx);

        for (int &idx : m_filteredIndices) {
            if (idx > absoluteIdx) {
                --idx;
            }
        }
        updatePathIndex();
    }
    
    emit countChanged();
    return true;
}

bool DirectoryModel::renamePath(const QString &oldPath, const QString &newPath)
{
    if (oldPath.isEmpty() || newPath.isEmpty()) {
        return false;
    }

    const QString oldPathKey = modelPathKey(QFileInfo(oldPath).absoluteFilePath());
    const QString newPathKey = modelPathKey(QFileInfo(newPath).absoluteFilePath());
    if (oldPathKey == newPathKey) {
        if (!m_pathIndex.contains(oldPathKey)) {
            return false;
        }
        return upsertPath(newPath) || QFileInfo(newPath).exists();
    }

    const int absoluteIdx = m_pathIndex.value(oldPathKey, -1);
    if (absoluteIdx < 0) {
        return false;
    }

    const bool wasSelected = m_entries.at(absoluteIdx).isSelected;
    if (!removePath(oldPath)) {
        return false;
    }

    const QString normalizedNewPath = QDir::fromNativeSeparators(QFileInfo(newPath).absoluteFilePath());
    const bool inserted = insertPath(normalizedNewPath);
    if (inserted && wasSelected) {
        const int row = indexOfPath(normalizedNewPath);
        if (row >= 0) {
            const int actualIdx = m_filteredIndices.at(row);
            m_entries[actualIdx].isSelected = true;
            ++m_selectedCount;
            emit dataChanged(index(row), index(row), {IsSelectedRole});
            emit selectionChanged();
        }
    }
    return inserted;
}

void DirectoryModel::invalidateThumbnails(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }

    QSet<int> changedRows;
    for (const QString &path : paths) {
        const QString normPath = modelPathKey(path);
        const int absIdx = m_pathIndex.value(normPath, -1);
        if (absIdx < 0 || absIdx >= m_entries.size()) {
            continue;
        }

        const QString entryPath = m_entries.at(absIdx).path;
        m_thumbnailRevisions[entryPath] = m_thumbnailRevisions.value(entryPath, 0) + 1;
        for (int row = 0; row < m_filteredIndices.size(); ++row) {
            if (m_filteredIndices.at(row) == absIdx) {
                changedRows.insert(row);
                break;
            }
        }
    }

    for (int row : changedRows) {
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {ThumbnailRevisionRole});
    }
}

QString DirectoryModel::formatSize(qint64 bytes)
{
    return DriveUtils::formatSize(bytes);
}

QString DirectoryModel::iconNameFor(const FileEntry &entry)
{
    return entry.iconName;
}

void DirectoryModel::processAllPendingInsertsFast()
{
    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        return;
    }

    if (m_freshLoad) {
        if (!m_freshLoadCommitted) {
            commitFreshLoad(m_pendingFreshLoadPath);
        }
        emit visualStructureAboutToChange();
        beginResetModel();
        while (m_pendingInsertOffset < m_pendingInserts.size()) {
            FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
            const QString normalizedPath = modelPathKey(entry.path);

            if (m_pathIndex.contains(normalizedPath)) {
                m_foundPaths.insert(normalizedPath);
                continue;
            }

            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);
        }

        m_filteredIndices.clear();
        m_filteredIndices.reserve(m_entries.size());
        for (int i = 0; i < m_entries.size(); ++i) {
            const FileEntry &entry = m_entries.at(i);
            const bool visible = m_showHidden || !entry.isHidden;
            const bool matchesFilter = this->matchesFilter(entry);
            if (visible && matchesFilter) {
                m_filteredIndices.append(i);
            }
        }
        std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
            [this](int aIdx, int bIdx) {
                return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
            });
        endResetModel();
    } else {
        while (m_pendingInsertOffset < m_pendingInserts.size()) {
            FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
            const QString normalizedPath = modelPathKey(entry.path);
            const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);

            if (absoluteIdx >= 0 && absoluteIdx < m_entries.size()) {
                FileEntry &existing = m_entries[absoluteIdx];
                const bool changed = fileEntryMetadataChanged(existing, entry);
                const bool thumbnailChanged = changed && thumbnailIdentityChanged(existing, entry);
                const bool sortOrderChanged = changed && (compareEntries(existing, entry) || compareEntries(entry, existing));

                const bool visible = m_showHidden || !entry.isHidden;
                const bool matchesFilter = this->matchesFilter(entry);
                const bool shouldBeVisible = visible && matchesFilter;

                int filteredRow = -1;
                for (int i = 0; i < m_filteredIndices.size(); ++i) {
                    if (m_filteredIndices[i] == absoluteIdx) {
                        filteredRow = i;
                        break;
                    }
                }

                if (shouldBeVisible && filteredRow == -1) {
                    auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
                        [this, &entry](int existingIdx, int val) {
                            Q_UNUSED(val);
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
                } else if (shouldBeVisible && filteredRow != -1 && changed) {
                    bool wasSelected = existing.isSelected;
                    existing = entry;
                    existing.isSelected = wasSelected;
                    if (thumbnailChanged) m_thumbnailRevisions[entry.path] = m_thumbnailRevisions.value(entry.path, 0) + 1;
                    emit dataChanged(index(filteredRow), index(filteredRow));
                    if (sortOrderChanged) {
                        sortModel();
                    }
                } else if (changed) {
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

                const bool visible = m_showHidden || !entry.isHidden;
                const bool matchesFilter = this->matchesFilter(entry);
                const bool shouldBeVisible = visible && matchesFilter;

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
    }

    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    emit countChanged();
}

void DirectoryModel::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void DirectoryModel::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void DirectoryModel::setLastError(const QVariantMap &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void DirectoryModel::setScanProgress(double progress, const QString &text)
{
    const double normalized = progress < 0.0 ? -1.0 : std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_scanProgress + 1.0, normalized + 1.0) && m_scanProgressText == text) {
        return;
    }
    m_scanProgress = normalized;
    m_scanProgressText = normalized < 0.0 ? QString{} : text;
    emit scanProgressChanged();
}

