#include "DirectoryModel.h"

#include "../core/FileProviderFactory.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QDebug>
#include <algorithm>

namespace {
FileEntry entryFromInfo(const QFileInfo &fileInfo)
{
    FileEntry entry;
    entry.name = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.suffix = fileInfo.suffix();
    entry.size = fileInfo.size();
    entry.modified = fileInfo.lastModified();
    entry.created = fileInfo.birthTime().isValid() ? fileInfo.birthTime() : fileInfo.lastModified();
    entry.isDirectory = fileInfo.isDir();
    entry.isHidden = fileInfo.isHidden();
    entry.isReadOnly = !fileInfo.isWritable();
    entry.isSystem = fileInfo.isSymLink();

    QLocale loc;
    entry.sizeText = entry.isDirectory
        ? QString()
        : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
    entry.createdText  = loc.toString(entry.created,  QLocale::ShortFormat);

    // Build attributes string
    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden)    attrs += QLatin1Char('H');
    if (entry.isReadOnly)  attrs += QLatin1Char('R');
    if (fileInfo.isSymLink()) attrs += QLatin1Char('L');
    entry.attributesText = attrs;

    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("gif"),
        QStringLiteral("bmp"),
        QStringLiteral("webp"),
        QStringLiteral("ico")
    };
    static const QStringList mediaSuffixes = {
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("ogg"),
        QStringLiteral("m4a"),
        QStringLiteral("mp4"),
        QStringLiteral("m4b"),
        QStringLiteral("wav"),
        QStringLiteral("wma"),
        QStringLiteral("avi"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("wmv"),
        QStringLiteral("pdf"),
        QStringLiteral("svg"),
        QStringLiteral("svgz"),
        QStringLiteral("ttf"),
        QStringLiteral("otf"),
        QStringLiteral("woff"),
        QStringLiteral("woff2")
    };
    entry.isImage = !entry.isDirectory && imageSuffixes.contains(entry.suffix.toLower());
    entry.hasThumbnail = entry.isImage || (!entry.isDirectory && mediaSuffixes.contains(entry.suffix.toLower()));
    return entry;
}
}

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_provider(FileProviderFactory::createProvider(QString(), nullptr))
{
    connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
    connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryModel::onDirectoryChanged);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &DirectoryModel::onDebounceTimeout);
    m_localMutationThrottle.invalidate();

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
    case CreatedTextRole:
        return entry.createdText;
    case AttributesRole:
        return entry.attributesText;
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
    case HasThumbnailRole:
        return entry.hasThumbnail;
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
        {CreatedTextRole, "createdText"},
        {AttributesRole, "attributesText"},
        {IsDirectoryRole, "isDirectory"},
        {IsHiddenRole, "isHidden"},
        {IsSelectedRole, "isSelected"},
        {IconNameRole, "iconName"},
        {SuffixRole, "suffix"},
        {IsImageRole, "isImage"},
        {HasThumbnailRole, "hasThumbnail"},
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
    
    // Immediately update the filtered indices for items we already have.
    // This is instant and uses beginResetModel, which is safe and prevents artifacts.
    // We keep selection because toggling hidden files shouldn't clear it.
    applyFilterInternal(true);
    
    refresh();
    emit showHiddenChanged();
}

bool DirectoryModel::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    if (!m_provider->canHandle(path)) {
        disconnect(m_provider.get(), nullptr, this, nullptr);
        m_provider = FileProviderFactory::createProvider(path, nullptr);
        connect(m_provider.get(), &FileProvider::started, this, &DirectoryModel::onScannerStarted);
        connect(m_provider.get(), &FileProvider::batchReady, this, &DirectoryModel::onScannerBatchReady);
        connect(m_provider.get(), &FileProvider::finished, this, &DirectoryModel::onScannerFinished);
    }
#ifdef FM_DEBUG_LOAD_TIMING
    m_loadTimingTimer.start();
    m_loadTimingFirstRowInserted = false;
    m_loadTimingRailShown = false;
    qDebug("[FM_TIMING] openPath() called for: %s", qUtf8Printable(path));
#endif
    m_provider->setShowHidden(m_showHidden);
    m_provider->scan(path);
    return true;
}

void DirectoryModel::onScannerStarted()
{
#ifdef FM_DEBUG_LOAD_TIMING
    qDebug("[FM_TIMING] started() signal received, elapsed: %lld ms",
           m_loadTimingTimer.elapsed());
#endif
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
        m_localMutationThrottle.invalidate();
    }

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

#ifdef FM_DEBUG_LOAD_TIMING
    qDebug("[FM_TIMING] batchReady() received %d entries, elapsed: %lld ms",
           entries.size(), m_loadTimingTimer.elapsed());
#endif
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
    
    // If we are doing a lot of visibility changes (e.g. toggling hidden files),
    // it's safer and faster to just reset the model at the end of the chunk
    // if the number of insertions/removals is high.
    bool manyChanges = false;
    int changeCount = 0;

    while (m_pendingInsertOffset < m_pendingInserts.size() && processed < chunkSize) {
        FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
        processed++;

        const QString normalizedPath = QDir::fromNativeSeparators(entry.path);
        const int absoluteIdx = m_pathIndex.value(normalizedPath, -1);

        const bool visible = m_showHidden || !entry.isHidden;
        const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);
        const bool shouldBeVisible = visible && matchesFilter;

        if (absoluteIdx >= 0 && absoluteIdx < m_entries.size()) {
            FileEntry &existing = m_entries[absoluteIdx];
            const bool hasChanged = (existing.size != entry.size
                                  || existing.modified != entry.modified
                                  || existing.isDirectory != entry.isDirectory
                                  || existing.suffix != entry.suffix
                                  || existing.isImage != entry.isImage
                                  || existing.sizeText != entry.sizeText
                                  || existing.modifiedText != entry.modifiedText
                                  || existing.createdText != entry.createdText
                                  || existing.attributesText != entry.attributesText);

            int filteredRow = -1;
            for (int i = 0; i < m_filteredIndices.size(); ++i) {
                if (m_filteredIndices[i] == absoluteIdx) {
                    filteredRow = i;
                    break;
                }
            }

            if (shouldBeVisible && filteredRow == -1) {
                // Was hidden, now visible: INSERT
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
                    [this, &entry](int existingIdx, int val) {
                        Q_UNUSED(val);
                        return this->compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, absoluteIdx);
                endInsertRows();
                changeCount++;
            } else if (!shouldBeVisible && filteredRow != -1) {
                // Was visible, now hidden: REMOVE
                beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
                m_filteredIndices.removeAt(filteredRow);
                endRemoveRows();
                changeCount++;
            } else if (shouldBeVisible && filteredRow != -1 && hasChanged) {
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
                emit dataChanged(index(filteredRow), index(filteredRow));
            } else if (hasChanged) {
                bool wasSelected = existing.isSelected;
                existing = entry;
                existing.isSelected = wasSelected;
            }
            m_foundPaths.insert(normalizedPath);
        } else {
            // New entry
            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);

            if (shouldBeVisible) {
                auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                    [this, &entry](int existingIdx, int val) {
                        Q_UNUSED(val);
                        return this->compareEntries(m_entries.at(existingIdx), entry);
                    });
                const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, newAbsoluteIdx);
                endInsertRows();
                changeCount++;
            }
        }
        
        if (changeCount > 50) {
            manyChanges = true;
            // If we have too many changes in one chunk, it's better to just finish the chunk
            // and do a reset if we encounter even more, or just continue one-by-one.
            // Actually, for now let's stick to one-by-one but be careful.
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

#ifdef FM_DEBUG_LOAD_TIMING
    qDebug("[FM_TIMING] finished() signal received, pending=%d, elapsed: %lld ms",
           static_cast<int>(m_pendingInserts.size() - m_pendingInsertOffset),
           m_loadTimingTimer.elapsed());
#endif

    // Small-directory fast path: if the scan finished and pending entries
    // are below the threshold, process them all in one synchronous commit
    // instead of draining through the 16ms insert timer.
    const qsizetype pendingCount = m_pendingInserts.size() - m_pendingInsertOffset;
    if (pendingCount > 0 && pendingCount <= SmallDirectoryThreshold) {
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
    m_pendingScannerFinish = false;
    m_pendingScannerPath.clear();
    m_pendingScannerError.clear();
    m_pendingScannerSuccess = false;

#ifdef FM_DEBUG_LOAD_TIMING
    qDebug("[FM_TIMING] finalizeScannerFinished() called, elapsed: %lld ms, rows=%d, railShown=%d",
           m_loadTimingTimer.elapsed(), m_filteredIndices.size(), m_loadTimingRailShown);
#endif
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

        if (!m_selectOnLoad.isEmpty()) {
            selectPath(m_selectOnLoad);
            m_selectOnLoad.clear();
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
        if (m_localMutationThrottle.isValid() && m_localMutationThrottle.elapsed() < 250) {
            return;
        }
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
    applyFilterInternal(false);
}

void DirectoryModel::applyFilterInternal(bool keepSelection)
{
    if (!keepSelection) {
        for (FileEntry &entry : m_entries) {
            entry.isSelected = false;
        }
        m_selectedCount = 0;
    }

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
    std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [this](int aIdx, int bIdx) {
            return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
        });
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

void DirectoryModel::noteLocalMutation()
{
    m_localMutationThrottle.restart();
    m_debounceTimer.stop();
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
                return this->compareEntries(m_entries.at(existingIdx), entry);
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

void DirectoryModel::selectPath(const QString &path)
{
    const int row = indexOfPath(path);
    if (row >= 0) {
        selectOnly(row);
    } else {
        clearSelection();
    }
}

void DirectoryModel::selectRange(int from, int to){
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    int start = std::min(from, to);
    int end = std::max(from, to);

    bool selectionChangedOccurred = false;

    // First clear anything outside the range if needed? 
    // Usually Shift+Click means "select everything between last selected and this one".
    // If Ctrl is NOT pressed, we usually clear first. 
    // But let's assume UI handles clearing if needed, OR we just ADD to selection.
    // Standard Windows behavior: Shift+Click clears current selection and selects the range.
    
    // Let's clear first for simplicity and common behavior.
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

    for (int i = start; i <= end; ++i) {
        int absIdx = m_filteredIndices.at(i);
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            selectionChangedOccurred = true;
            emit dataChanged(index(i), index(i), {IsSelectedRole});
        }
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

void DirectoryModel::processAllPendingInsertsFast()
{
    // Fast path for small directories: drain all pending inserts in one
    // synchronous GUI-thread commit. This avoids the per-frame insert timer
    // overhead and makes small directories appear instantly.
    //
    // For fresh loads we batch all new entries into a single
    // beginInsertRows/endInsertRows pair, which is significantly faster than
    // inserting one-by-one with sorted positioning (O(N log N) vs O(N^2)).
    //
    // For non-fresh loads (refreshes), we fall through to the per-entry
    // update/insert logic because existing entries may need updating.

    if (m_pendingInsertOffset >= m_pendingInserts.size()) {
        m_pendingInserts.clear();
        m_pendingInsertOffset = 0;
        return;
    }

    if (m_freshLoad) {
        // Fast fresh load: collect all entries, sort once, then do a single
        // batch insert notification.
        QList<FileEntry> newEntries;
        newEntries.reserve(m_pendingInserts.size() - m_pendingInsertOffset);

        while (m_pendingInsertOffset < m_pendingInserts.size()) {
            FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
            const QString normalizedPath = QDir::fromNativeSeparators(entry.path);

            // Skip duplicates (shouldn't happen on fresh load, but be safe)
            if (m_pathIndex.contains(normalizedPath)) {
                m_foundPaths.insert(normalizedPath);
                continue;
            }

            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
            m_foundPaths.insert(normalizedPath);

            const bool visible = m_showHidden || !entry.isHidden;
            const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);
            if (visible && matchesFilter) {
                newEntries.append(entry);
            }
        }

        // Sort the new visible entries, then compute their filtered indices
        if (!newEntries.isEmpty()) {
            std::sort(newEntries.begin(), newEntries.end(), [this](const FileEntry &a, const FileEntry &b) {
                return this->compareEntries(a, b);
            });

            // Build the list of absolute indices for visible new entries in sorted order
            QList<int> sortedNewAbsoluteIndices;
            sortedNewAbsoluteIndices.reserve(newEntries.size());
            for (const FileEntry &entry : newEntries) {
                const QString normPath = QDir::fromNativeSeparators(entry.path);
                sortedNewAbsoluteIndices.append(m_pathIndex.value(normPath));
            }

            // Merge sortedNewAbsoluteIndices into m_filteredIndices
            // Since existing m_filteredIndices is empty for fresh loads, this is trivial
            const int firstRow = m_filteredIndices.size();
            const int lastRow = firstRow + sortedNewAbsoluteIndices.size() - 1;

            beginInsertRows(QModelIndex(), firstRow, lastRow);
            m_filteredIndices.append(sortedNewAbsoluteIndices);
            endInsertRows();

#ifdef FM_DEBUG_LOAD_TIMING
            if (!m_loadTimingFirstRowInserted) {
                m_loadTimingFirstRowInserted = true;
                qDebug("[FM_TIMING] First rows inserted (fast path, count=%d), elapsed: %lld ms",
                       sortedNewAbsoluteIndices.size(), m_loadTimingTimer.elapsed());
            }
#endif
        }
    } else {
        // Non-fresh load (refresh): process entries one by one, same as
        // processPendingInserts but without chunking.
        while (m_pendingInsertOffset < m_pendingInserts.size()) {
            FileEntry entry = m_pendingInserts.at(m_pendingInsertOffset++);
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
                                      || existing.modifiedText != entry.modifiedText
                                      || existing.createdText != entry.createdText
                                      || existing.attributesText != entry.attributesText);

                const bool visible = m_showHidden || !entry.isHidden;
                const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);
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
                    beginInsertRows(QModelIndex(), row, row);
                    m_filteredIndices.insert(row, absoluteIdx);
                    endInsertRows();
                } else if (!shouldBeVisible && filteredRow != -1) {
                    beginRemoveRows(QModelIndex(), filteredRow, filteredRow);
                    m_filteredIndices.removeAt(filteredRow);
                    endRemoveRows();
                } else if (shouldBeVisible && filteredRow != -1 && changed) {
                    bool wasSelected = existing.isSelected;
                    existing = entry;
                    existing.isSelected = wasSelected;
                    emit dataChanged(index(filteredRow), index(filteredRow));
                } else if (changed) {
                    bool wasSelected = existing.isSelected;
                    existing = entry;
                    existing.isSelected = wasSelected;
                }
                m_foundPaths.insert(normalizedPath);
            } else {
                const int newAbsoluteIdx = m_entries.size();
                m_entries.append(entry);
                m_pathIndex.insert(normalizedPath, newAbsoluteIdx);
                m_foundPaths.insert(normalizedPath);

                const bool visible = m_showHidden || !entry.isHidden;
                const bool matchesFilter = m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive);
                const bool shouldBeVisible = visible && matchesFilter;

                if (shouldBeVisible) {
                    auto it = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), newAbsoluteIdx,
                        [this, &entry](int existingIdx, int val) {
                            Q_UNUSED(val);
                            return this->compareEntries(m_entries.at(existingIdx), entry);
                        });
                    const int row = static_cast<int>(std::distance(m_filteredIndices.begin(), it));
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

#ifdef FM_DEBUG_LOAD_TIMING
void DirectoryModel::dumpLoadTiming() const
{
    qDebug("[FM_TIMING] Dump: elapsed=%lld ms, rows=%d, firstRowInserted=%d, railShown=%d",
           m_loadTimingTimer.elapsed(), m_filteredIndices.size(),
           m_loadTimingFirstRowInserted, m_loadTimingRailShown);
}
#endif

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

DirectoryModel::SortRole DirectoryModel::sortRole() const
{
    return m_sortRole;
}

void DirectoryModel::setSortRole(SortRole role)
{
    if (m_sortRole == role) {
        return;
    }
    m_sortRole = role;
    sortModel();
    emit sortRoleChanged();
}

Qt::SortOrder DirectoryModel::sortOrder() const
{
    return m_sortOrder;
}

void DirectoryModel::setSortOrder(Qt::SortOrder order)
{
    if (m_sortOrder == order) {
        return;
    }
    m_sortOrder = order;
    sortModel();
    emit sortOrderChanged();
}

bool DirectoryModel::compareEntries(const FileEntry &a, const FileEntry &b) const
{
    if (a.isDirectory != b.isDirectory) {
        return a.isDirectory; // Directories always come first
    }

    switch (m_sortRole) {
    case SortByName: {
        int comp = a.name.compare(b.name, Qt::CaseInsensitive);
        if (comp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        break;
    }
    case SortBySize: {
        if (a.size != b.size) {
            return m_sortOrder == Qt::AscendingOrder ? (a.size < b.size) : (a.size > b.size);
        }
        break;
    }
    case SortByType: {
        int comp = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        break;
    }
    case SortByDate: {
        if (a.modified != b.modified) {
            return m_sortOrder == Qt::AscendingOrder ? (a.modified < b.modified) : (a.modified > b.modified);
        }
        break;
    }
    case SortByDateCreated: {
        if (a.created != b.created) {
            return m_sortOrder == Qt::AscendingOrder ? (a.created < b.created) : (a.created > b.created);
        }
        break;
    }
    case SortByExtension: {
        int comp = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (comp < 0) : (comp > 0);
        }
        // Secondary: sort by name within same extension
        int nameComp = a.name.compare(b.name, Qt::CaseInsensitive);
        if (nameComp != 0) {
            return m_sortOrder == Qt::AscendingOrder ? (nameComp < 0) : (nameComp > 0);
        }
        break;
    }
    }

    // Default stable sorting fallback
    int nameComp = a.name.compare(b.name, Qt::CaseInsensitive);
    if (nameComp != 0) {
        return nameComp < 0;
    }
    return a.path.compare(b.path) < 0;
}

void DirectoryModel::sortModel()
{
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    beginResetModel();
    std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [this](int aIdx, int bIdx) {
            return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
        });
    endResetModel();
}
