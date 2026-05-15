#include "DirectoryModel.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QDebug>
#include <algorithm>

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_scanner, &DirectoryScanner::started, this, &DirectoryModel::onScannerStarted);
    connect(&m_scanner, &DirectoryScanner::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(&m_scanner, &DirectoryScanner::finished, this, &DirectoryModel::onScannerFinished);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryModel::onDirectoryChanged);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &DirectoryModel::onDebounceTimeout);

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
    m_scanner.setShowHidden(show);
    refresh();
    emit showHiddenChanged();
}

bool DirectoryModel::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    m_scanner.setShowHidden(m_showHidden);
    m_scanner.scan(path);
    return true;
}

void DirectoryModel::onScannerStarted()
{
    m_debounceTimer.stop();
    const QString scanPath = m_scanner.currentPath();
    m_freshLoad = (scanPath != m_currentPath);
    m_currentScanGeneration = m_scanner.currentGeneration();

    if (m_freshLoad) {
        // New directory: full clear
        for (FileEntry &entry : m_entries) {
            entry.isSelected = false;
        }
        m_selectedCount = 0;
        m_freshLoadBuffer.clear();

        beginResetModel();
        m_entries.clear();
        m_filteredIndices.clear();
        m_entryIndex.clear();
        m_foundNames.clear();
        endResetModel();
    } else {
        // Incremental refresh: keep existing items, but track what we find
        m_foundNames.clear();
    }

    setLoading(true);
    setError({});
    emit countChanged();
    emit selectionChanged();
}

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

void DirectoryModel::onScannerBatchReady(const QList<FileEntry> &entries, int generation)
{
    if (generation != m_currentScanGeneration) {
        return;
    }

    if (entries.isEmpty()) {
        return;
    }

    if (m_freshLoad) {
        m_freshLoadBuffer.append(entries);
        return;
    }

    for (const FileEntry &entry : entries) {
        const int absoluteIdx = m_entryIndex.value(entry.name, -1);

        if (absoluteIdx >= 0 && absoluteIdx < m_entries.size() && m_entries.at(absoluteIdx).name == entry.name) {
            FileEntry &existing = m_entries[absoluteIdx];
            const bool changed = (existing.size != entry.size
                                  || existing.modified != entry.modified
                                  || existing.isDirectory != entry.isDirectory
                                  || existing.suffix != entry.suffix
                                  || existing.isImage != entry.isImage
                                  || existing.sizeText != entry.sizeText
                                  || existing.modifiedText != entry.modifiedText);

            if (changed) {
                existing = entry;
                const int filteredRow = m_filteredIndices.indexOf(absoluteIdx);
                if (filteredRow != -1) {
                    emit dataChanged(index(filteredRow), index(filteredRow));
                }
            }
            m_foundNames.insert(entry.name);
        } else {
            if (absoluteIdx != -1) {
                m_entryIndex.remove(entry.name);
            }

            const int newAbsoluteIdx = m_entries.size();
            m_entries.append(entry);
            m_entryIndex.insert(entry.name, newAbsoluteIdx);
            m_foundNames.insert(entry.name);

            if (m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive)) {
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

    emit countChanged();
}
void DirectoryModel::onScannerFinished(const QString &path, bool success, int generation, const QString &error)
{
    if (generation != m_currentScanGeneration) {
        setLoading(false);
        return;
    }

    setLoading(false);
    if (success) {
        const bool pathChanged = (m_currentPath != path);

        if (m_freshLoad) {
            QList<FileEntry> orderedEntries = m_freshLoadBuffer;
            std::sort(orderedEntries.begin(), orderedEntries.end(), compareEntries);

            beginResetModel();
            m_entries = orderedEntries;
            m_filteredIndices.clear();
            m_entryIndex.clear();
            m_selectedCount = 0;
            for (int i = 0; i < m_entries.size(); ++i) {
                m_entries[i].isSelected = false;
                m_entryIndex.insert(m_entries[i].name, i);
                if (m_filterText.isEmpty() || m_entries[i].name.contains(m_filterText, Qt::CaseInsensitive)) {
                    m_filteredIndices.append(i);
                }
            }
            endResetModel();
            m_freshLoadBuffer.clear();
            emit countChanged();
            emit selectionChanged();
        } else {
            for (int i = m_entries.size() - 1; i >= 0; --i) {
                if (!m_foundNames.contains(m_entries.at(i).name)) {
                    if (m_entries.at(i).isSelected) {
                        --m_selectedCount;
                    }
                    m_entryIndex.remove(m_entries.at(i).name);

                    int filteredIdx = m_filteredIndices.indexOf(i);
                    if (filteredIdx != -1) {
                        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
                        m_filteredIndices.removeAt(filteredIdx);
                        endRemoveRows();
                    }

                    m_entries.removeAt(i);
                    for (int &idx : m_filteredIndices) {
                        if (idx > i) idx--;
                    }
                }
            }

            m_entryIndex.clear();
            for (int i = 0; i < m_entries.size(); ++i) {
                m_entryIndex.insert(m_entries[i].name, i);
            }
            emit countChanged();
            emit selectionChanged();
        }

        if (pathChanged) {
            if (!m_currentPath.isEmpty()) {
                m_watcher.removePath(m_currentPath);
            }
            m_currentPath = path;
            m_watcher.addPath(m_currentPath);
            emit currentPathChanged();
        }
    } else {
        setError(error);
    }
}
void DirectoryModel::onDirectoryChanged(const QString &path)
{
    if (path == m_currentPath && !m_loading) {
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
        if (m_filterText.isEmpty() || m_entries.at(i).name.contains(m_filterText, Qt::CaseInsensitive)) {
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
        m_scanner.setShowHidden(m_showHidden);
        m_scanner.scan(m_currentPath);
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

    if (QDir::fromNativeSeparators(info.absolutePath()) != QDir::fromNativeSeparators(m_currentPath)) {
        return false;
    }

    const QString name = info.fileName();
    if (m_entryIndex.contains(name)) {
        return false;
    }

    const FileEntry entry = entryFromInfo(info);
    const int newAbsoluteIdx = m_entries.size();
    m_entries.append(entry);
    m_entryIndex.insert(entry.name, newAbsoluteIdx);

    if (m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive)) {
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
    int absoluteIdx = -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (QDir::fromNativeSeparators(m_entries.at(i).path) == normalizedPath) {
            absoluteIdx = i;
            break;
        }
    }
    if (absoluteIdx < 0) {
        return false;
    }

    if (m_entries.at(absoluteIdx).isSelected) {
        --m_selectedCount;
        emit selectionChanged();
    }

    const int filteredIdx = m_filteredIndices.indexOf(absoluteIdx);
    if (filteredIdx != -1) {
        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
        m_filteredIndices.removeAt(filteredIdx);
        endRemoveRows();
    }

    m_entryIndex.remove(m_entries.at(absoluteIdx).name);
    m_entries.removeAt(absoluteIdx);
    for (int &idx : m_filteredIndices) {
        if (idx > absoluteIdx) {
            --idx;
        }
    }
    m_entryIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        m_entryIndex.insert(m_entries[i].name, i);
    }
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

    int absoluteIdx = -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (QDir::fromNativeSeparators(m_entries.at(i).path) == normalizedOldPath) {
            absoluteIdx = i;
            break;
        }
    }
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

    // We need to unselect everything that is currently selected
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected && i != targetActualIdx) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;
            
            // If this item is visible, notify the view
            int filteredRow = m_filteredIndices.indexOf(i);
            if (filteredRow != -1) {
                emit dataChanged(index(filteredRow), index(filteredRow), {IsSelectedRole});
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
    bool selectionChangedOccurred = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;

            int filteredRow = m_filteredIndices.indexOf(i);
            if (filteredRow != -1) {
                emit dataChanged(index(filteredRow), index(filteredRow), {IsSelectedRole});
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
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_entries.at(m_filteredIndices.at(i)).path == path) {
            return i;
        }
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
    if (entry.suffix.isEmpty()) {
        return QStringLiteral("file");
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



