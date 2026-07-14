#pragma once

#include "DirectoryModel.h"

namespace DirectoryModelInternal {

struct AsyncFreshLoadResult {
    int generation = 0;
    QString path;
    QList<FileEntry> entries;
    QList<int> filteredIndices;
    QHash<QString, int> pathIndex;
    QSet<QString> foundPaths;
    bool showHidden = false;
    bool mixFilesAndFolders = false;
    QString searchText;
    DirectoryModel::CategoryFilter categoryFilter = DirectoryModel::FilterAll;
    DirectoryModel::SortRole sortRole = DirectoryModel::SortByName;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
};

bool fileEntryMetadataChanged(const FileEntry &a, const FileEntry &b);
bool thumbnailIdentityChanged(const FileEntry &a, const FileEntry &b);
void traceDirectoryWatch(const char *stage, const QString &path, const QString &detail = {});
bool sameFilesystemPath(const QString &left, const QString &right);
bool isUriPath(const QString &path);
QString modelPathKey(const QString &path);
bool isProviderEntryPath(const QString &path);
bool pathIsInDirectory(const QString &path, const QString &directoryPath);
void traceDirectoryNav(const char *stage, const QString &path = {}, const QString &detail = {});
bool scannerFailureIndicatesUnavailable(const QString &error);
QString failedNavigationSelectionPath(const QString &failedPath);
AsyncFreshLoadResult buildAsyncFreshLoadResult(int generation,
                                               const QString &path,
                                               QList<FileEntry> baseEntries,
                                               QList<FileEntry> pendingEntries,
                                               qsizetype pendingOffset,
                                               bool showHidden,
                                               const QString &searchText,
                                               DirectoryModel::CategoryFilter categoryFilter,
                                               bool mixFilesAndFolders,
                                               DirectoryModel::SortRole sortRole,
                                               Qt::SortOrder sortOrder);

} // namespace DirectoryModelInternal
