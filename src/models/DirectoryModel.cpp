#include "DirectoryModel.h"

#include "../core/LocalFileProvider.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QDebug>
#include <algorithm>

namespace {
bool compareEntries(const FileEntry &a, const FileEntry &b)
{
    if (a.isDirectory != b.isDirectory) {
        return a.isDirectory; // Directories come first
    }
    return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
}

FileEntry entryFromInfo(const QFileInfo &fileInfo)
{
    FileEntry entry;
    entry.name = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.suffix = fileInfo.suffix();
    entry.size = fileInfo.size();
    entry.modified = fileInfo.lastModified();
    entry.isDirectory = fileInfo.isDir();
    entry.isHidden = fileInfo.isHidden();

    QLocale loc;
    entry.sizeText = entry.isDirectory
        ? QStringLiteral("Folder")
        : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);

    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("gif"),
        QStringLiteral("bmp"),
        QStringLiteral("webp"),
        QStringLiteral("ico")
    };
    entry.isImage = !entry.isDirectory && imageSuffixes.contains(entry.suffix.toLower());
    return entry;
}
}

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_provider(std::make_unique<LocalFileProvider>())
{
    connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
    connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryModel::onDirectoryChanged);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &DirectoryModel::onDebounceTimeout);

    m_insertTimer.setInterval(16);
    connect(&m_insertTimer, &QTimer::timeout, this, &DirectoryModel::processPendingInserts);

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    openPath(home.isEmpty() ? QDir::homePath() : home);
}

int DirectoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_filteredIndices.size();
}

QVariant DirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size()) {
        return {};
    }

    const FileEntry &entry = m_entries.at(m_filteredIndices.at(index.row()));
    switch (role) {
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case SizeRole:
        return entry.size;
    case SizeTextRole:
        return entry.sizeText;
    case ModifiedTextRole:
        return entry.modifiedText;
    case IsDirectoryRole:
        return entry.isDirectory;
    case IsHiddenRole:
        return entry.isHidden;
    case IsSelectedRole:
        return entry.isSelected;
    case IconNameRole:
        return iconNameFor(entry);
    case SuffixRole:
        return entry.suffix;
    case IsImageRole:
        return entry.isImage;
    default:
        return {};
    }
}

QHash<int, QByteArray> DirectoryModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {ModifiedTextRole, "modifiedText"},
        {IsDirectoryRole, "isDirectory"},
        {IsHiddenRole, "isHidden"},
        {IsSelectedRole, "isSelected"},
        {IconNameRole, "iconName"},
        {SuffixRole, "suffix"},
        {IsImageRole, "isImage"},
    };
}

QString DirectoryModel::currentPath() const
{
    return m_currentPath;
}

bool DirectoryModel::loading() const
{
    return m_loading;
}

QString DirectoryModel::error() const
{
    return m_error;
}

int DirectoryModel::count() const
{
    return m_filteredIndices.size();
}

int DirectoryModel::selectedCount() const
{
    return m_selectedCount;
}

QString DirectoryModel::filterText() const
{
    return m_filterText;
}

void DirectoryModel::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    m_filterText = text;
    applyFilter();
    emit filterTextChanged();
}

bool DirectoryModel::showHidden() const
{
    return m_showHidden;
}

void DirectoryModel::setShowHidden(bool show)
{
    if (m_showHidden == show) {
        return;
    }
    m_showHidden = show;
    m_provider->setShowHidden(show);
    refresh();
    emit showHiddenChanged();
}

bool DirectoryModel::openPath(const QString &path)
{
    if (path.isEmpty() || !m_provider->canHandle(path)) {
        return false;
    }
    m_provider->setShowHidden(m_showHidden);
    m_provider->scan(path);
    return true;
}

void DirectoryModel::onScannerStarted()
{
    m_debounceTimer.stop();
    m_insertTimer.stop();
    
    const QString scanPath = m_provider->currentPath();
    const QString previousPath = m_currentPath;
    m_previousPath = previousPath;
    m_freshLoad = (scanPath != previousPath);
    m_currentScanGeneration = m_provider->currentGeneration();

    m_pendingInserts.clear();
    m_pendingInsertOffset = 0;
    m_foundPaths.clear();
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    if (m_freshLoad) {
        m_selectedCount = 0;
        beginResetModel();
        m_entries.clear();
        m_filteredIndices.clear();
        m_pathIndex.clear();
        endResetModel();

        if (!previousPath.isEmpty()) {
            m_watcher.removePath(previousPath);
        }
        m_currentPath = scanPath;
        m_watcher.addPath(m_currentPath);
        emit currentPathChanged();
    }

    setLoading(true);
    setError({});
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::onScannerBatchReady(const QList<FileEntry> &entries, int generation)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    if (entries.isEmpty()) {
        return;
    }

    m_pendingInserts.append(entries);
    if (!m_insertTimer.isActive()) {
        m_insertTimer.start();
    }
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

    // Process in chunks to keep GUI responsive
    const int chunkSize = 150;
    int processed = 0;
    
    while (m_pendingInsertOffset < m_pendingInserts.size() && processed < chunkSize) {
        FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
        processed++;

        const QString normalizedPath = QDir::fromNativeSeparators(entry.path);
        const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);

        if (absoluteIdx >= 0 && absoluteIdx < m_entries.size()) {
            FileEntry &existing = m_entries[absoluteIdx];
            const bool changed = (existing.size != entry.size
                                  || existing.modified != entry.modified
                                  || existing.isDirectory != entry.isDirectory
                                  || existing.suffix != entry.suffix
                                  || existing.isImage != entry.isImage
                                  || existing.sizeText != entry.sizeText
                                  || existing.modifiedText != entry.modifiedText);

            if (changed) {
                // Preserving selection state
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
                
                int filteredRow = -1;
                // For fresh load, we don't have filteredIndices yet or it's being built
                // But if it's an update, we need to find the row.
                // indexOf is O(N), but we only do it for updates.
                for (int i = 0; i < m_filteredIndices.size(); ++i) {
                    if (m_filteredIndices[i] == absoluteIdx) {
                        filteredRow = i;
                        break;
                    }
                }
                
                if (filteredRow != -1) {
                    emit dataChanged(index(filteredRow), index(filteredRow));
                }
            }
            m_foundPaths.insert(normalizedPath);
        } else {
            // New entry
            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);

            const bool visible = m_showHidden || !entry.isHidden;
            const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);

            if (visible && matchesFilter) {
                // If we are in a fresh load, we might want to just append for speed 
                // and sort at the very end, OR insert into sorted position.
                // Inserting into sorted position is O(N) per insert, total O(N^2).
                // For 10k items, that's slow.
                // Let's use std::lower_bound to find insertion point.
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                    [&](int existingIdx, int) {
                        return compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = std::distance(m_filteredIndices.begin(), it);
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
        setLoading(false);
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
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

    setLoading(false);
    if (success) {
        // Remove items that were not found in the scan (unless it was a fresh load where we cleared everything)
        if (!m_freshLoad) {
            for (int i = m_entries.size() - 1; i >= 0; --i) {
                const QString normPath = QDir::fromNativeSeparators(m_entries.at(i).path);
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
                        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
                        m_filteredIndices.removeAt(filteredIdx);
                        endRemoveRows();
                    }

                    m_entries.removeAt(i);
                    // Update indices in m_filteredIndices
                    for (int &idx : m_filteredIndices) {
                        if (idx > i) idx--;
                    }
                }
            }
            updatePathIndex();
            emit selectionChanged();
        }
        emit countChanged();
    } else {
        if (m_freshLoad) {
            if (!m_currentPath.isEmpty()) {
                m_watcher.removePath(m_currentPath);
            }
            m_currentPath = m_previousPath;
            if (!m_currentPath.isEmpty()) {
                m_watcher.addPath(m_currentPath);
            }
            emit currentPathChanged();

            beginResetModel();
            m_entries.clear();
            m_filteredIndices.clear();
            m_pathIndex.clear();
            m_selectedCount = 0;
            endResetModel();
            emit countChanged();
            emit selectionChanged();
        }
        setError(error);
        emit directoryUnavailable(path, error);
    }

    m_previousPath.clear();
}

void DirectoryModel::updatePathIndex()
{
    m_pathIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        m_pathIndex.insert(QDir::fromNativeSeparators(m_entries[i].path), i);
    }
}

void DirectoryModel::onDirectoryChanged(const QString &path)
{
    if (path == m_currentPath && !m_loading) {
        if (!QFileInfo::exists(m_currentPath) || !QFileInfo(m_currentPath).isDir()) {
            m_watcher.removePath(m_currentPath);
        }
        m_debounceTimer.start();
    }
}

void DirectoryModel::onDebounceTimeout()
{
    if (!m_currentPath.isEmpty() && !m_loading) {
        refresh();
    }
}

void DirectoryModel::applyFilter()
{
    // Clear selection when filtering to avoid "ghost" selections in filtered view
    for (FileEntry &entry : m_entries) {
        entry.isSelected = false;
    }
    m_selectedCount = 0;

    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        const FileEntry &entry = m_entries.at(i);
        const bool visible = m_showHidden || !entry.isHidden;
        const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);
        
        if (visible && matchesFilter) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
    emit countChanged();
    emit selectionChanged();
}

void DirectoryModel::refresh()
{
    if (!m_currentPath.isEmpty()) {
        m_provider->setShowHidden(m_showHidden);
        m_provider->scan(m_currentPath);
    }
}

bool DirectoryModel::insertPath(const QString &path)
{
    if (path.isEmpty() || m_currentPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }

    const QString normPath = QDir::fromNativeSeparators(info.absoluteFilePath());
    if (QDir::fromNativeSeparators(info.absolutePath()) != QDir::fromNativeSeparators(m_currentPath)) {
        return false;
    }
    if (m_pathIndex.contains(normPath)) {
        return false;
    }

    const FileEntry entry = entryFromInfo(info);
    const int newAbsoluteIdx = m_entries.size();
    m_entries.append(entry);
    m_pathIndex.insert(normPath, newAbsoluteIdx);

    const bool visible = m_showHidden || !entry.isHidden;
    const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);

    if (visible && matchesFilter) {
        auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
            [&](int existingIdx, int) {
                return compareEntries(m_entries.at(existingIdx), entry);
            });
        const int row = std::distance(m_filteredIndices.begin(), it);
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

    const QString normalizedPath = QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
    const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);
    
    if (absoluteIdx < 0) {
        return false;
    }

    if (m_entries.at(absoluteIdx).isSelected) {
        --m_selectedCount;
        emit selectionChanged();
    }

    int filteredIdx = -1;
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_filteredIndices[i] == absoluteIdx) {
            filteredIdx = i;
            break;
        }
    }

    if (filteredIdx != -1) {
        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
        m_filteredIndices.removeAt(filteredIdx);
        endRemoveRows();
    }

    m_pathIndex.remove(normalizedPath);
    m_entries.removeAt(absoluteIdx);
    
    // Update indices in m_filteredIndices and m_pathIndex
    for (int &idx : m_filteredIndices) {
        if (idx > absoluteIdx) {
            --idx;
        }
    }
    updatePathIndex();
    
    emit countChanged();
    return true;
}

bool DirectoryModel::renamePath(const QString &oldPath, const QString &newPath)
{
    if (oldPath.isEmpty() || newPath.isEmpty()) {
        return false;
    }

    const QString normalizedOldPath = QDir::fromNativeSeparators(QFileInfo(oldPath).absoluteFilePath());
    const QString normalizedNewPath = QDir::fromNativeSeparators(QFileInfo(newPath).absoluteFilePath());
    if (normalizedOldPath == normalizedNewPath) {
        return true;
    }

    const int absoluteIdx = m_pathIndex.value(normalizedOldPath, -1);
    if (absoluteIdx < 0) {
        return false;
    }

    const bool wasSelected = m_entries.at(absoluteIdx).isSelected;
    if (!removePath(oldPath)) {
        return false;
    }

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

void DirectoryModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return;
    }
    const int actualIdx = m_filteredIndices.at(row);
    m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
    m_selectedCount += m_entries[actualIdx].isSelected ? 1 : -1;
    emit dataChanged(index(row), index(row), {IsSelectedRole});
    emit selectionChanged();
}

void DirectoryModel::selectOnly(int row)
{
    const int targetActualIdx = (row >= 0 && row < m_filteredIndices.size()) 
        ? m_filteredIndices.at(row) 
        : -1;

    bool selectionChangedOccurred = false;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected && i != targetActualIdx) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;
            
            // Optimization: we don't need to search if we know it's not visible,
            // but for simplicity we'll do it. O(N) but only for selected items.
            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
            }
        }
    }

    if (targetActualIdx != -1 && !m_entries[targetActualIdx].isSelected) {
        m_entries[targetActualIdx].isSelected = true;
        ++m_selectedCount;
        selectionChangedOccurred = true;
        emit dataChanged(index(row), index(row), {IsSelectedRole});
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::clearSelection()
{
    if (m_selectedCount == 0) return;

    bool selectionChangedOccurred = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;

            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
            }
        }
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::selectAll()
{
    bool changed = false;
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        int absIdx = m_filteredIndices[i];
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            changed = true;
            emit dataChanged(index(i), index(i), {IsSelectedRole});
        }
    }
    if (changed)
        emit selectionChanged();
}

QString DirectoryModel::pathAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return {};
    }
    return m_entries.at(m_filteredIndices.at(row)).path;
}

bool DirectoryModel::isDirectoryAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).isDirectory;
}

int DirectoryModel::indexOfPath(const QString &path) const
{
    const QString normPath = QDir::fromNativeSeparators(path);
    const int absIdx = m_pathIndex.value(normPath, -1);
    if (absIdx == -1) return -1;
    
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_filteredIndices[i] == absIdx) return i;
    }
    return -1;
}

QStringList DirectoryModel::selectedPaths() const
{
    QStringList paths;
    for (const FileEntry &entry : m_entries) {
        if (entry.isSelected) {
            paths.append(entry.path);
        }
    }
    return paths;
}

QString DirectoryModel::formatSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString DirectoryModel::iconNameFor(const FileEntry &entry)
{
    if (entry.isDirectory) {
        return QStringLiteral("folder");
    }
    return QStringLiteral("file");
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
