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
#include <QBitArray>
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

void DirectoryModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return;
    }
    const int actualIdx = m_filteredIndices.at(row);
    if (m_entries.at(actualIdx).specialAction != FileEntrySpecialAction::None) {
        if (m_entries.at(actualIdx).isSelected) {
            m_entries[actualIdx].isSelected = false;
            --m_selectedCount;
            emit dataChanged(index(row), index(row), {IsSelectedRole});
            emit selectionChanged();
        }
        return;
    }
    m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
    m_selectedCount += m_entries[actualIdx].isSelected ? 1 : -1;
    emit dataChanged(index(row), index(row), {IsSelectedRole});
    emit selectionChanged();
}

void DirectoryModel::selectOnly(int row)
{
    if (m_mapSelectedPaths.isEmpty() && m_selectedCount == 1 && row >= 0 && row < m_filteredIndices.size()) {
        const FileEntry &entry = m_entries.at(m_filteredIndices.at(row));
        if (entry.isSelected && entry.specialAction == FileEntrySpecialAction::None) return;
    }
    selectRows({row});
}

void DirectoryModel::selectMapPath(const QString &path, bool toggle)
{
    if (path.isEmpty()) return;
    if (!toggle) {
        selectMapPaths({path});
        return;
    }
    QStringList paths = selectedPaths();
    if (!paths.removeOne(path)) {
        const QString key = DirectoryModelAlgorithms::pathKey(path);
        const QString prefix = key.endsWith(QLatin1Char('/')) ? key : key + QLatin1Char('/');
        // Explicitly selecting a folder replaces its selected descendants.
        paths.removeIf([&](const QString &selected) {
            return DirectoryModelAlgorithms::pathKey(selected).startsWith(prefix);
        });
        paths.append(path);
    }
    selectMapPaths(paths);
}

void DirectoryModel::selectMapPaths(const QStringList &paths)
{
    const QStringList safePaths = DirectoryModelAlgorithms::selectionWithoutAncestors(paths);
    if (selectedPaths() == safePaths) return;
    QVariantList rows;
    QStringList nested;
    for (const QString &path : safePaths) {
        const int row = indexOfPath(path);
        if (row >= 0) rows.append(row);
        else if (!nested.contains(path)) nested.append(path);
    }
    selectRows(rows);
    if (nested.isEmpty()) return;
    m_mapSelectedPaths = nested;
    emit selectionChanged();
}

void DirectoryModel::selectMapRange(const QStringList &orderedPaths, const QString &path)
{
    const int target = orderedPaths.indexOf(path);
    if (target < 0) return;
    QStringList result = selectedPaths();
    const QSet<QString> selected(result.cbegin(), result.cend());
    QBitArray mask(orderedPaths.size());
    for (int i = 0; i < orderedPaths.size(); ++i) {
        if (!selected.contains(orderedPaths.at(i))) continue;
        mask.setBit(i);
    }
    DirectoryModelAlgorithms::extendOrTrimSelection(mask, 0, target);
    for (int i = 0; i < orderedPaths.size(); ++i) {
        const QString &itemPath = orderedPaths.at(i);
        if (!mask.testBit(i)) result.removeOne(itemPath);
        else if (!selected.contains(itemPath)) result.append(itemPath);
    }
    selectMapPaths(result);
}

void DirectoryModel::notifySelectionRowsChanged(const QList<int> &rows)
{
    // Callers supply changed visible rows in ascending order, after updating all state.
    for (qsizetype i = 0; i < rows.size(); ++i) {
        const int first = rows.at(i);
        int last = first;
        while (i + 1 < rows.size() && rows.at(i + 1) == last + 1) {
            last = rows.at(++i);
        }
        emit dataChanged(index(first), index(last), {IsSelectedRole});
    }
}

void DirectoryModel::selectRange(int from, int to)
{
    if (!m_mapSelectedPaths.isEmpty()) clearSelection();
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    int start = std::min(from, to);
    int end = std::max(from, to);

    QList<int> changedRows;

    for (int i = start; i <= end; ++i) {
        int absIdx = m_filteredIndices.at(i);
        if (m_entries.at(absIdx).specialAction != FileEntrySpecialAction::None) {
            if (m_entries.at(absIdx).isSelected) {
                m_entries[absIdx].isSelected = false;
                --m_selectedCount;
                changedRows.append(i);
            }
            continue;
        }
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            changedRows.append(i);
        }
    }

    if (!changedRows.isEmpty()) {
        notifySelectionRowsChanged(changedRows);
        emit selectionChanged();
    }
}

void DirectoryModel::extendOrTrimRange(int from, int to)
{
    if (!m_mapSelectedPaths.isEmpty()) clearSelection();
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    QBitArray selected(m_filteredIndices.size());
    for (int row = 0; row < m_filteredIndices.size(); ++row)
        selected.setBit(row, m_entries.at(m_filteredIndices.at(row)).isSelected);
    DirectoryModelAlgorithms::extendOrTrimSelection(selected, from, to);

    QList<int> changedRows;
    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        const int actualIdx = m_filteredIndices.at(row);
        const bool shouldSelect = selected.testBit(row)
            && m_entries.at(actualIdx).specialAction == FileEntrySpecialAction::None;
        if (m_entries[actualIdx].isSelected != shouldSelect) {
            m_entries[actualIdx].isSelected = shouldSelect;
            m_selectedCount += shouldSelect ? 1 : -1;
            changedRows.append(row);
        }
    }

    if (!changedRows.isEmpty()) {
        notifySelectionRowsChanged(changedRows);
        emit selectionChanged();
    }
}

void DirectoryModel::selectRows(const QVariantList &rows)
{
    const bool hadMapSelection = !m_mapSelectedPaths.isEmpty();
    m_mapSelectedPaths.clear();
    QBitArray targetActualIndices(m_entries.size());
    for (const QVariant &rowValue : rows) {
        bool ok = false;
        const int row = rowValue.toInt(&ok);
        if (!ok || row < 0 || row >= m_filteredIndices.size()) {
            continue;
        }
        const int actualIdx = m_filteredIndices.at(row);
        if (m_entries.at(actualIdx).specialAction == FileEntrySpecialAction::None) {
            targetActualIndices.setBit(actualIdx);
        }
    }

    QBitArray changedActualIndices(m_entries.size());
    bool changed = false;
    qsizetype selectedCount = 0;

    for (int i = 0; i < m_entries.size(); ++i) {
        const bool shouldSelect = targetActualIndices.testBit(i);
        if (m_entries.at(i).isSelected != shouldSelect) {
            m_entries[i].isSelected = shouldSelect;
            changedActualIndices.setBit(i);
            changed = true;
        }
        if (shouldSelect) {
            ++selectedCount;
        }
    }

    if (!changed) {
        if (hadMapSelection) emit selectionChanged();
        return;
    }

    QList<int> changedRows;
    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        if (changedActualIndices.testBit(m_filteredIndices.at(row))) {
            changedRows.append(row);
        }
    }

    m_selectedCount = static_cast<int>(selectedCount);
    notifySelectionRowsChanged(changedRows);
    emit selectionChanged();
}

void DirectoryModel::invertSelection()
{
    if (!m_mapSelectedPaths.isEmpty()) clearSelection();
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    QList<int> changedRows;
    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        const int actualIdx = m_filteredIndices.at(row);
        if (m_entries.at(actualIdx).specialAction != FileEntrySpecialAction::None) {
            if (m_entries.at(actualIdx).isSelected) {
                m_entries[actualIdx].isSelected = false;
                changedRows.append(row);
            }
            continue;
        }
        m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
        changedRows.append(row);
    }

    int selectedCount = 0;
    for (const FileEntry &entry : m_entries) {
        if (entry.isSelected) {
            ++selectedCount;
        }
    }
    m_selectedCount = selectedCount;
    notifySelectionRowsChanged(changedRows);
    emit selectionChanged();
}

void DirectoryModel::clearSelection()
{
    if (m_selectedCount == 0 && m_mapSelectedPaths.isEmpty()) return;
    selectRows({});
}

void DirectoryModel::selectAll()
{
    if (!m_mapSelectedPaths.isEmpty()) clearSelection();
    QList<int> changedRows;
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        int absIdx = m_filteredIndices[i];
        if (m_entries.at(absIdx).specialAction != FileEntrySpecialAction::None) {
            if (m_entries.at(absIdx).isSelected) {
                m_entries[absIdx].isSelected = false;
                --m_selectedCount;
                changedRows.append(i);
            }
            continue;
        }
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            changedRows.append(i);
        }
    }
    if (!changedRows.isEmpty()) {
        notifySelectionRowsChanged(changedRows);
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

bool DirectoryModel::isShortcutAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).isShortcut;
}

int DirectoryModel::specialActionAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) return static_cast<int>(FileEntrySpecialAction::None);
    return static_cast<int>(m_entries.at(m_filteredIndices.at(row)).specialAction);
}

bool DirectoryModel::isSpecialActionPath(const QString &path) const
{
    const int actualIdx = m_pathIndex.value(modelPathKey(path), -1);
    return actualIdx >= 0
        && m_entries.at(actualIdx).specialAction != FileEntrySpecialAction::None
        && filteredRowForAbsoluteIndex(actualIdx) >= 0;
}

QString DirectoryModel::shortcutTargetPathAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return {};
    }
    return m_entries.at(m_filteredIndices.at(row)).shortcutTargetPath;
}

QString DirectoryModel::shortcutOpenPathAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return {};
    }
    return m_entries.at(m_filteredIndices.at(row)).shortcutOpenPath;
}

bool DirectoryModel::shortcutTargetIsDirectoryAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).shortcutTargetIsDirectory;
}

int DirectoryModel::indexOfPath(const QString &path) const
{
    const QString normPath = modelPathKey(path);
    const int absIdx = m_pathIndex.value(normPath, -1);
    if (absIdx == -1) return -1;
    
    for (int i = 0; i < m_filteredIndices.size(); ++i) {
        if (m_filteredIndices[i] == absIdx) return i;
    }
    return -1;
}

int DirectoryModel::indexOfSpecialAction(int action) const
{
    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        if (static_cast<int>(m_entries.at(m_filteredIndices.at(row)).specialAction) == action) return row;
    }
    return -1;
}

QStringList DirectoryModel::selectedPaths() const
{
    QStringList paths = m_mapSelectedPaths;
    for (const FileEntry &entry : m_entries) {
        if (entry.isSelected && entry.specialAction == FileEntrySpecialAction::None) {
            paths.append(entry.path);
        }
    }
    return paths;
}
