#include "DirectoryModel.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QDebug>

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_scanner, &DirectoryScanner::started, this, &DirectoryModel::onScannerStarted);
    connect(&m_scanner, &DirectoryScanner::batchReady, this, &DirectoryModel::onScannerBatchReady);
    connect(&m_scanner, &DirectoryScanner::finished, this, &DirectoryModel::onScannerFinished);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryModel::onDirectoryChanged);

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
        return entry.isDirectory ? QStringLiteral("Folder") : formatSize(entry.size);
    case ModifiedTextRole:
        return QLocale().toString(entry.modified, QLocale::ShortFormat);
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
    int count = 0;
    for (int idx : m_filteredIndices) {
        if (m_entries.at(idx).isSelected) {
            ++count;
        }
    }
    return count;
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
    const QString scanPath = m_scanner.currentPath();
    if (scanPath == m_currentPath) {
        // Incremental refresh: keep existing items, but track what we find
        m_foundNames.clear();
    } else {
        // New directory: full clear
        for (FileEntry &entry : m_entries) {
            entry.isSelected = false;
        }

        beginResetModel();
        m_entries.clear();
        m_filteredIndices.clear();
        m_foundNames.clear();
        endResetModel();
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
}

void DirectoryModel::onScannerBatchReady(const QList<FileEntry> &entries)
{
    if (entries.isEmpty()) {
        return;
    }

    for (const FileEntry &entry : entries) {
        m_foundNames.insert(entry.name);
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const FileEntry &e) {
            return e.name == entry.name;
        });

        if (it != m_entries.end()) {
            // Item exists, check if it changed
            int absoluteIdx = std::distance(m_entries.begin(), it);
            bool changed = (it->size != entry.size || it->modified != entry.modified || it->isDirectory != entry.isDirectory);
            
            if (changed || entry.isDirectory) {
                *it = entry;
                int filteredRow = m_filteredIndices.indexOf(absoluteIdx);
                if (filteredRow != -1) {
                    emit dataChanged(index(filteredRow), index(filteredRow));
                }
            }
        } else {
            // New item
            int absoluteIdx = m_entries.size();
            m_entries.append(entry);
            
            if (m_filterText.isEmpty() || entry.name.contains(m_filterText, Qt::CaseInsensitive)) {
                // Find correct insertion point to maintain sorting
                auto it_insert = std::lower_bound(m_filteredIndices.begin(), m_filteredIndices.end(), absoluteIdx,
                    [&](int existingIdx, int) {
                        return compareEntries(m_entries.at(existingIdx), entry);
                    });
                
                int row = std::distance(m_filteredIndices.begin(), it_insert);
                beginInsertRows(QModelIndex(), row, row);
                m_filteredIndices.insert(row, absoluteIdx);
                endInsertRows();
            }
        }
    }

    emit countChanged();
}

void DirectoryModel::onScannerFinished(const QString &path, bool success, const QString &error)
{
    setLoading(false);
    if (success) {
        if (m_currentPath == path) {
            // Remove items that are no longer present
            for (int i = m_entries.size() - 1; i >= 0; --i) {
                if (!m_foundNames.contains(m_entries.at(i).name)) {
                    int filteredIdx = m_filteredIndices.indexOf(i);
                    if (filteredIdx != -1) {
                        beginRemoveRows(QModelIndex(), filteredIdx, filteredIdx);
                        m_filteredIndices.removeAt(filteredIdx);
                        endRemoveRows();
                    }
                    
                    m_entries.removeAt(i);
                    // Update m_filteredIndices because absolute indices shifted
                    for (int &idx : m_filteredIndices) {
                        if (idx > i) idx--;
                    }
                }
            }
            emit countChanged();
        } else {
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
        refresh();
    }
}

void DirectoryModel::applyFilter()
{
    // Clear selection when filtering to avoid "ghost" selections in filtered view
    for (FileEntry &entry : m_entries) {
        entry.isSelected = false;
    }

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
        openPath(m_currentPath);
    }
}

void DirectoryModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return;
    }
    const int actualIdx = m_filteredIndices.at(row);
    m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
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
