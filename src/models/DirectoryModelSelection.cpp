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

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected && i != targetActualIdx) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;
            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
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

void DirectoryModel::selectRange(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    int start = std::min(from, to);
    int end = std::max(from, to);

    bool selectionChangedOccurred = false;

    for (int i = start; i <= end; ++i) {
        int absIdx = m_filteredIndices.at(i);
        if (!m_entries[absIdx].isSelected) {
            m_entries[absIdx].isSelected = true;
            ++m_selectedCount;
            selectionChangedOccurred = true;
            emit dataChanged(index(i), index(i), {IsSelectedRole});
        }
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::extendOrTrimRange(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_filteredIndices.size() || to >= m_filteredIndices.size()) {
        return;
    }

    const int start = std::min(from, to);
    const int end = std::max(from, to);

    bool rangeAlreadySelected = true;
    for (int row = start; row <= end; ++row) {
        if (!m_entries.at(m_filteredIndices.at(row)).isSelected) {
            rangeAlreadySelected = false;
            break;
        }
    }

    if (!rangeAlreadySelected) {
        selectRange(from, to);
        return;
    }

    int selectedStart = start;
    while (selectedStart > 0 && m_entries.at(m_filteredIndices.at(selectedStart - 1)).isSelected) {
        --selectedStart;
    }

    int selectedEnd = end;
    while (selectedEnd + 1 < m_filteredIndices.size()
           && m_entries.at(m_filteredIndices.at(selectedEnd + 1)).isSelected) {
        ++selectedEnd;
    }

    bool selectionChangedOccurred = false;
    for (int row = selectedStart; row <= selectedEnd; ++row) {
        const bool shouldSelect = row >= start && row <= end;
        const int actualIdx = m_filteredIndices.at(row);
        if (m_entries[actualIdx].isSelected != shouldSelect) {
            m_entries[actualIdx].isSelected = shouldSelect;
            m_selectedCount += shouldSelect ? 1 : -1;
            selectionChangedOccurred = true;
            emit dataChanged(index(row), index(row), {IsSelectedRole});
        }
    }

    if (selectionChangedOccurred) {
        emit selectionChanged();
    }
}

void DirectoryModel::selectRows(const QVariantList &rows)
{
    QSet<int> targetActualIndices;
    targetActualIndices.reserve(rows.size());
    for (const QVariant &rowValue : rows) {
        bool ok = false;
        const int row = rowValue.toInt(&ok);
        if (!ok || row < 0 || row >= m_filteredIndices.size()) {
            continue;
        }
        targetActualIndices.insert(m_filteredIndices.at(row));
    }

    QSet<int> changedActualIndices;
    qsizetype selectedCount = 0;

    for (int i = 0; i < m_entries.size(); ++i) {
        const bool shouldSelect = targetActualIndices.contains(i);
        if (m_entries[i].isSelected != shouldSelect) {
            m_entries[i].isSelected = shouldSelect;
            changedActualIndices.insert(i);
        }
        if (shouldSelect) {
            ++selectedCount;
        }
    }

    if (changedActualIndices.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        if (changedActualIndices.contains(m_filteredIndices.at(row))) {
            emit dataChanged(index(row), index(row), {IsSelectedRole});
        }
    }

    m_selectedCount = static_cast<int>(selectedCount);
    emit selectionChanged();
}

void DirectoryModel::invertSelection()
{
    if (m_filteredIndices.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        const int actualIdx = m_filteredIndices.at(row);
        m_entries[actualIdx].isSelected = !m_entries[actualIdx].isSelected;
        emit dataChanged(index(row), index(row), {IsSelectedRole});
    }

    int selectedCount = 0;
    for (const FileEntry &entry : m_entries) {
        if (entry.isSelected) {
            ++selectedCount;
        }
    }
    m_selectedCount = selectedCount;
    emit selectionChanged();
}

void DirectoryModel::clearSelection()
{
    if (m_selectedCount == 0) return;

    bool selectionChangedOccurred = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isSelected) {
            m_entries[i].isSelected = false;
            --m_selectedCount;
            selectionChangedOccurred = true;

            for (int j = 0; j < m_filteredIndices.size(); ++j) {
                if (m_filteredIndices[j] == i) {
                    emit dataChanged(index(j), index(j), {IsSelectedRole});
                    break;
                }
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

bool DirectoryModel::isShortcutAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return false;
    }
    return m_entries.at(m_filteredIndices.at(row)).isShortcut;
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

