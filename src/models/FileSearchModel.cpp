#include "FileSearchModel.h"

#include "../core/DriveUtils.h"

#include <QDir>
#include <QLocale>

#include <algorithm>

FileSearchModel::FileSearchModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileSearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_results.size();
}

QVariant FileSearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }

    const FileSearchResult &result = m_results.at(index.row());
    switch (role) {
    case PathRole:
        return result.path;
    case NameRole:
        return result.name;
    case ParentPathRole:
        return result.parentPath;
    case DisplayPathRole:
        return QDir::toNativeSeparators(result.path);
    case DisplayParentPathRole:
        return QDir::toNativeSeparators(result.parentPath);
    case SizeRole:
        return result.size;
    case SizeTextRole:
        return result.isDirectory ? QStringLiteral("Folder") : DriveUtils::formatSize(result.size);
    case ModifiedTextRole:
        return result.modified.isValid()
            ? QLocale().toString(result.modified, QLocale::ShortFormat)
            : QString();
    case IsDirectoryRole:
        return result.isDirectory;
    case MatchKindRole:
        return result.matchKind;
    case LineNumberRole:
        return result.lineNumber;
    case LineTextRole:
        return result.lineText;
    case LineMatchStartRole:
        return result.lineMatchStart;
    case LineMatchLengthRole:
        return result.lineMatchLength;
    case NameMatchStartRole:
        return result.nameMatchStart;
    case NameMatchLengthRole:
        return result.nameMatchLength;
    case RelevanceScoreRole:
        return result.relevanceScore;
    default:
        return {};
    }
}

QHash<int, QByteArray> FileSearchModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {ParentPathRole, "parentPath"},
        {DisplayPathRole, "displayPath"},
        {DisplayParentPathRole, "displayParentPath"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {ModifiedTextRole, "modifiedText"},
        {IsDirectoryRole, "isDirectory"},
        {MatchKindRole, "matchKind"},
        {LineNumberRole, "lineNumber"},
        {LineTextRole, "lineText"},
        {LineMatchStartRole, "lineMatchStart"},
        {LineMatchLengthRole, "lineMatchLength"},
        {NameMatchStartRole, "nameMatchStart"},
        {NameMatchLengthRole, "nameMatchLength"},
        {RelevanceScoreRole, "relevanceScore"},
    };
}

int FileSearchModel::count() const
{
    return m_results.size();
}

QString FileSearchModel::pathAt(int row) const
{
    if (row < 0 || row >= m_results.size()) {
        return {};
    }
    return m_results.at(row).path;
}

bool FileSearchModel::isDirectoryAt(int row) const
{
    if (row < 0 || row >= m_results.size()) {
        return false;
    }
    return m_results.at(row).isDirectory;
}

int FileSearchModel::indexOfResult(const QString &path, const QString &matchKind, int lineNumber) const
{
    for (int i = 0; i < m_results.size(); ++i) {
        const FileSearchResult &result = m_results.at(i);
        if (result.path == path && result.matchKind == matchKind && result.lineNumber == lineNumber) return i;
    }
    return -1;
}

void FileSearchModel::sort(int mode)
{
    if (m_results.size() < 2) return;
    const auto textCompare = [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive);
    };
    const auto tieBreak = [&](const FileSearchResult &left, const FileSearchResult &right) {
        const int name = textCompare(left.name, right.name);
        if (name != 0) return name < 0;
        const int path = textCompare(left.path, right.path);
        if (path != 0) return path < 0;
        if (left.lineNumber != right.lineNumber) return left.lineNumber < right.lineNumber;
        return left.discoveryOrder < right.discoveryOrder;
    };

    beginResetModel();
    std::stable_sort(m_results.begin(), m_results.end(), [&](const FileSearchResult &left, const FileSearchResult &right) {
        switch (mode) {
        case NameSort: {
            const int value = textCompare(left.name, right.name);
            if (value != 0) return value < 0;
            break;
        }
        case PathSort: {
            const int value = textCompare(left.path, right.path);
            if (value != 0) return value < 0;
            break;
        }
        case SizeSort:
            if (left.size != right.size) return left.size < right.size;
            break;
        case ModifiedSort:
            if (left.modified != right.modified) return left.modified > right.modified;
            break;
        case RelevanceSort:
        default:
            if (left.relevanceScore != right.relevanceScore) return left.relevanceScore > right.relevanceScore;
            break;
        }
        return tieBreak(left, right);
    });
    endResetModel();
}

void FileSearchModel::appendResults(const QList<FileSearchResult> &results)
{
    if (results.isEmpty()) {
        return;
    }

    const int first = m_results.size();
    const int last = first + results.size() - 1;
    beginInsertRows({}, first, last);
    m_results.append(results);
    endInsertRows();
    emit countChanged();
}

void FileSearchModel::clear()
{
    if (m_results.isEmpty()) {
        return;
    }

    beginResetModel();
    m_results.clear();
    endResetModel();
    emit countChanged();
}
