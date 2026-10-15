#pragma once

#include "DirectoryModel.h"
#include <QBitArray>

namespace DirectoryModelAlgorithms {

QString pathKey(const QString &path);
QStringList selectionWithoutAncestors(const QStringList &paths);

void extendOrTrimSelection(QBitArray &selected, int from, int to);

bool matchesFilter(const FileEntry &entry,
                   const QString &searchText,
                   DirectoryModel::CategoryFilter categoryFilter);

bool lessThan(const FileEntry &left,
              const FileEntry &right,
              bool mixFilesAndFolders,
              DirectoryModel::SortRole sortRole,
              Qt::SortOrder sortOrder);

QList<int> filteredAndSortedIndices(const QList<FileEntry> &entries,
                                    bool showHidden,
                                    const QString &searchText,
                                    DirectoryModel::CategoryFilter categoryFilter,
                                    bool mixFilesAndFolders,
                                    DirectoryModel::SortRole sortRole,
                                    Qt::SortOrder sortOrder);

}
