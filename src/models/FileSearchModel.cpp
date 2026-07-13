#include "FileSearchModel.h"

#include "../core/DriveUtils.h"

#include <QDir>
#include <QLocale>

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
