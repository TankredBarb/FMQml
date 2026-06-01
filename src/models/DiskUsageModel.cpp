#include "DiskUsageModel.h"

#include "../core/DriveUtils.h"

#include <QLocale>
#include <algorithm>

namespace {
QString formatSizeDetailed(qint64 bytes)
{
    constexpr qint64 KB = 1024LL;
    constexpr qint64 MB = KB * 1024LL;
    constexpr qint64 GB = MB * 1024LL;
    constexpr qint64 TB = GB * 1024LL;

    if (bytes >= TB) {
        return QStringLiteral("%1 TB").arg(QString::number(static_cast<double>(bytes) / TB, 'f', 2));
    }
    if (bytes >= GB) {
        return QStringLiteral("%1 GB").arg(QString::number(static_cast<double>(bytes) / GB, 'f', 2));
    }
    if (bytes >= MB) {
        return QStringLiteral("%1 MB").arg(QString::number(static_cast<double>(bytes) / MB, 'f', 1));
    }
    return DriveUtils::formatSize(bytes);
}
}

DiskUsageModel::DiskUsageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DiskUsageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant DiskUsageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const DiskUsageEntry &entry = m_entries.at(index.row());
    switch (role) {
    case PathRole:
        return entry.path;
    case NameRole:
        return entry.name;
    case SizeRole:
        return entry.size;
    case SizeTextRole:
        return DriveUtils::formatSize(entry.size);
    case SizeDetailTextRole:
        return formatSizeDetailed(entry.size);
    case ExactSizeTextRole:
        return QLocale().toString(entry.size) + QStringLiteral(" bytes");
    case PercentOfLargestRole:
        return m_largestSize > 0 ? static_cast<double>(entry.size) / static_cast<double>(m_largestSize) : 0.0;
    case PercentOfRootRole:
        return m_rootTotalBytes > 0 ? static_cast<double>(entry.size) / static_cast<double>(m_rootTotalBytes) : 0.0;
    case PercentOfRootTextRole:
        return m_rootTotalBytes > 0
            ? QStringLiteral("%1%").arg(QString::number((static_cast<double>(entry.size) / m_rootTotalBytes) * 100.0, 'f', 1))
            : QString();
    case IsDirectoryRole:
        return entry.isDirectory;
    case FileCountRole:
        return entry.fileCount;
    case FolderCountRole:
        return entry.folderCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> DiskUsageModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {SizeDetailTextRole, "sizeDetailText"},
        {ExactSizeTextRole, "exactSizeText"},
        {PercentOfLargestRole, "percentOfLargest"},
        {PercentOfRootRole, "percentOfRoot"},
        {PercentOfRootTextRole, "percentOfRootText"},
        {IsDirectoryRole, "isDirectory"},
        {FileCountRole, "fileCount"},
        {FolderCountRole, "folderCount"},
    };
}

int DiskUsageModel::count() const
{
    return m_entries.size();
}

int DiskUsageModel::sortKey() const
{
    return m_sortKey;
}

bool DiskUsageModel::sortAscending() const
{
    return m_sortAscending;
}

void DiskUsageModel::setSort(int key, bool ascending)
{
    if (m_sortKey == key && m_sortAscending == ascending) {
        return;
    }
    beginResetModel();
    m_sortKey = key;
    m_sortAscending = ascending;
    applySort();
    endResetModel();
    emit sortChanged();
}

void DiskUsageModel::setEntries(const QList<DiskUsageEntry> &entries, qint64 rootTotalBytes)
{
    beginResetModel();
    m_entries = entries;
    m_rootTotalBytes = rootTotalBytes;
    m_largestSize = 0;
    for (const DiskUsageEntry &entry : std::as_const(m_entries)) {
        m_largestSize = std::max(m_largestSize, entry.size);
    }
    applySort();
    endResetModel();
    emit countChanged();
}

void DiskUsageModel::clear()
{
    if (m_entries.isEmpty() && m_largestSize == 0) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    m_largestSize = 0;
    m_rootTotalBytes = 0;
    endResetModel();
    emit countChanged();
}

void DiskUsageModel::applySort()
{
    const int key = m_sortKey;
    const bool ascending = m_sortAscending;
    std::sort(m_entries.begin(), m_entries.end(), [key, ascending](const DiskUsageEntry &left, const DiskUsageEntry &right) {
        int comparison = 0;
        switch (key) {
        case 1:
            comparison = QString::compare(left.name, right.name, Qt::CaseInsensitive);
            break;
        case 2: {
            const int leftItems = left.fileCount + left.folderCount;
            const int rightItems = right.fileCount + right.folderCount;
            comparison = leftItems == rightItems ? 0 : (leftItems < rightItems ? -1 : 1);
            break;
        }
        case 3:
        case 0:
        default:
            comparison = left.size == right.size ? 0 : (left.size < right.size ? -1 : 1);
            break;
        }
        if (comparison == 0) {
            comparison = QString::compare(left.path, right.path, Qt::CaseInsensitive);
        }
        return ascending ? comparison < 0 : comparison > 0;
    });
}
