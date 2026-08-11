#pragma once

#include "FileProvider.h"

#include <Qt>

namespace FileEntrySortPolicy {

inline bool lessThan(const FileEntry &a, const FileEntry &b, bool mixFilesAndFolders,
                     int sortRole, Qt::SortOrder sortOrder)
{
    const auto isLoadMore = [](const FileEntry &entry) {
        return entry.specialAction == FileEntrySpecialAction::LoadMore;
    };
    const bool aLoadMore = isLoadMore(a);
    const bool bLoadMore = isLoadMore(b);
    if (aLoadMore != bLoadMore) return !aLoadMore;
    if (!mixFilesAndFolders && a.isDirectory != b.isDirectory) return a.isDirectory;

    const auto orderedComparison = [sortOrder](int comparison) {
        return sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
    };
    switch (sortRole) {
    case 0: {
        const int comparison = a.name.compare(b.name, Qt::CaseInsensitive);
        if (comparison != 0) return orderedComparison(comparison);
        break;
    }
    case 1:
        if (a.size != b.size) return sortOrder == Qt::AscendingOrder ? a.size < b.size : a.size > b.size;
        break;
    case 2: {
        const int comparison = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comparison != 0) return orderedComparison(comparison);
        break;
    }
    case 3:
        if (a.modified != b.modified) return sortOrder == Qt::AscendingOrder ? a.modified < b.modified : a.modified > b.modified;
        break;
    case 4:
        if (a.created != b.created) return sortOrder == Qt::AscendingOrder ? a.created < b.created : a.created > b.created;
        break;
    case 5: {
        const int comparison = a.suffix.compare(b.suffix, Qt::CaseInsensitive);
        if (comparison != 0) return orderedComparison(comparison);
        const int nameComparison = a.name.compare(b.name, Qt::CaseInsensitive);
        if (nameComparison != 0) return orderedComparison(nameComparison);
        break;
    }
    default:
        break;
    }

    const int nameComparison = a.name.compare(b.name, Qt::CaseInsensitive);
    if (nameComparison != 0) return orderedComparison(nameComparison);
    return orderedComparison(a.path.compare(b.path, Qt::CaseInsensitive));
}

} // namespace FileEntrySortPolicy
