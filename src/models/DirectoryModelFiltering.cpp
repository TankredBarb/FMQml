#include "DirectoryModel.h"
#include "DirectoryModelAlgorithms.h"
#include "DirectoryWatchPolicy.h"

#include "../core/ArchiveSupport.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include "../core/FileError.h"
#include "../core/FileProviderFactory.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"
#include "../core/LocalFileBadgeResolver.h"
#include "../core/LocalMountPointIndex.h"
#include "../core/FavoritesStore.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QLocale>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QtGlobal>
#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "DirectoryModelInternal.h"

using namespace DirectoryModelInternal;

void DirectoryModel::applyFilter()
{
    applyFilterInternal(false);
}

bool DirectoryModel::matchesFilter(const FileEntry &entry) const
{
    return entryMatchesFilterSnapshot(entry, m_searchText, m_categoryFilter);
}

void DirectoryModel::notifyFiltersChanged()
{
    emit filtersChanged();
}

void DirectoryModel::applyFilterInternal(bool keepSelection)
{
    if (!keepSelection) {
        for (FileEntry &entry : m_entries) {
            entry.isSelected = false;
        }
        m_selectedCount = 0;
    }

    emit visualStructureAboutToChange();
    beginResetModel();
    m_filteredIndices = DirectoryModelAlgorithms::filteredAndSortedIndices(
        m_entries,
        m_showHidden,
        m_searchText,
        m_categoryFilter,
        m_mixFilesAndFolders,
        m_sortRole,
        m_sortOrder);
    endResetModel();
    emit countChanged();
    emit selectionChanged();
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

void DirectoryModel::setSortPolicy(SortRole role, Qt::SortOrder order)
{
    const bool roleChanged = m_sortRole != role;
    const bool orderChanged = m_sortOrder != order;
    if (!roleChanged && !orderChanged) {
        return;
    }

    m_sortRole = role;
    m_sortOrder = order;
    sortModel();

    if (roleChanged) {
        emit sortRoleChanged();
    }
    if (orderChanged) {
        emit sortOrderChanged();
    }
}

bool DirectoryModel::compareEntries(const FileEntry &a, const FileEntry &b) const
{
    return compareEntriesForPolicy(a, b, m_mixFilesAndFolders, m_sortRole, m_sortOrder);
}

void DirectoryModel::sortModel()
{
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    emit visualStructureAboutToChange();
    emit layoutAboutToBeChanged();
    std::stable_sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [this](int aIdx, int bIdx) {
            return compareEntries(m_entries.at(aIdx), m_entries.at(bIdx));
        });
    emit layoutChanged();
}
