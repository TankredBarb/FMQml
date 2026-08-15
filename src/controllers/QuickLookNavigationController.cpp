#include "QuickLookNavigationController.h"

#include "FilePanelController.h"
#include "FolderPeekController.h"
#include "QuickLookController.h"
#include "../models/DirectoryModel.h"

#include <QAbstractItemModel>
#include <QVariantMap>

#include <utility>

QuickLookNavigationController::QuickLookNavigationController(QObject *parent)
    : QObject(parent)
{
}

void QuickLookNavigationController::setQuickLookController(QuickLookController *controller)
{
    m_quickLook = controller;
}

bool QuickLookNavigationController::beginPanel(QObject *panelObject, const QString &path)
{
    endSession();
    auto *panel = qobject_cast<FilePanelController *>(panelObject);
    if (!panel || !panel->directoryModel()) return false;
    const int row = panel->directoryModel()->indexOfPath(path);
    if (row < 0 || panel->directoryModel()->specialActionAt(row) != 0) return false;

    m_panel = panel;
    m_originKind = PanelOrigin;
    m_directoryIdentity = panel->currentPath();
    m_requestedPath = path;
    m_requestedRow = row;
    DirectoryModel *model = panel->directoryModel();
    m_connections += connect(panel, &FilePanelController::currentPathChanged, this, &QuickLookNavigationController::refresh);
    m_connections += connect(panel, &QObject::destroyed, this, &QuickLookNavigationController::endSession);
    m_connections += connect(model, &DirectoryModel::countChanged, this, &QuickLookNavigationController::refresh);
    m_connections += connect(model, &QAbstractItemModel::modelReset, this, &QuickLookNavigationController::refresh);
    m_connections += connect(model, &QAbstractItemModel::layoutChanged, this, &QuickLookNavigationController::refresh);
    refresh();
    return true;
}

bool QuickLookNavigationController::beginFolderPeek(QObject *peekObject, const QString &path)
{
    endSession();
    auto *peek = qobject_cast<FolderPeekController *>(peekObject);
    if (!peek || !peek->isOpen()) return false;
    m_peek = peek;
    m_originKind = FolderPeekOrigin;
    m_directoryIdentity = peek->currentPath();
    m_requestedPath = path;
    m_requestedRow = indexOfPath(path);
    if (m_requestedRow < 0) {
        endSession();
        return false;
    }
    m_connections += connect(peek, &FolderPeekController::entriesChanged, this, &QuickLookNavigationController::refresh);
    m_connections += connect(peek, &FolderPeekController::currentPathChanged, this, &QuickLookNavigationController::refresh);
    m_connections += connect(peek, &FolderPeekController::openChanged, this, &QuickLookNavigationController::refresh);
    m_connections += connect(peek, &QObject::destroyed, this, &QuickLookNavigationController::endSession);
    refresh();
    return true;
}

void QuickLookNavigationController::disconnectOrigin()
{
    for (const auto &connection : std::as_const(m_connections)) disconnect(connection);
    m_connections.clear();
}

void QuickLookNavigationController::endSession()
{
    const bool changed = active() || m_canGoPrevious || m_canGoNext;
    disconnectOrigin();
    m_panel.clear();
    m_peek.clear();
    m_originKind = NoOrigin;
    m_directoryIdentity.clear();
    m_requestedPath.clear();
    m_requestedRow = -1;
    m_canGoPrevious = false;
    m_canGoNext = false;
    ++m_revision;
    if (changed) emit stateChanged();
}

int QuickLookNavigationController::count() const
{
    if (m_originKind == PanelOrigin && m_panel) return m_panel->directoryModel()->rowCount();
    if (m_originKind == FolderPeekOrigin && m_peek) return m_peek->entries().size();
    return 0;
}

QString QuickLookNavigationController::pathAt(int row) const
{
    if (row < 0 || row >= count()) return {};
    if (m_originKind == PanelOrigin) return m_panel->directoryModel()->pathAt(row);
    return m_peek->entries().at(row).toMap().value(QStringLiteral("path")).toString();
}

bool QuickLookNavigationController::ordinaryAt(int row) const
{
    return row >= 0 && row < count()
        && (m_originKind != PanelOrigin || m_panel->directoryModel()->specialActionAt(row) == 0);
}

int QuickLookNavigationController::indexOfPath(const QString &path) const
{
    if (m_originKind == PanelOrigin && m_panel) return m_panel->directoryModel()->indexOfPath(path);
    for (int row = 0; row < count(); ++row) if (pathAt(row) == path) return row;
    return -1;
}

int QuickLookNavigationController::adjacentRow(int fromRow, int direction) const
{
    for (int row = fromRow + direction; row >= 0 && row < count(); row += direction) {
        if (ordinaryAt(row) && !pathAt(row).isEmpty()) return row;
    }
    return -1;
}

bool QuickLookNavigationController::originStillValid() const
{
    if (m_originKind == PanelOrigin) return m_panel && m_panel->currentPath() == m_directoryIdentity;
    if (m_originKind == FolderPeekOrigin) return m_peek && m_peek->isOpen() && m_peek->currentPath() == m_directoryIdentity;
    return false;
}

void QuickLookNavigationController::refresh()
{
    if (!active()) return;
    if (!originStillValid()) {
        endSession();
        return;
    }
    if (count() <= 0) {
        m_canGoPrevious = m_canGoNext = false;
        emit stateChanged();
        return;
    }
    int row = indexOfPath(m_requestedPath);
    if (row < 0) {
        row = qBound(0, m_requestedRow, count() - 1);
        if (!ordinaryAt(row) || pathAt(row).isEmpty()) {
            const int forward = adjacentRow(row - 1, 1);
            row = forward >= 0 ? forward : adjacentRow(row + 1, -1);
        }
        if (row >= 0) commitRow(row);
        return;
    }
    m_requestedRow = row;
    m_canGoPrevious = adjacentRow(row, -1) >= 0;
    m_canGoNext = adjacentRow(row, 1) >= 0;
    emit stateChanged();
}

bool QuickLookNavigationController::commitRow(int row)
{
    if (!originStillValid() || !ordinaryAt(row)) return false;
    const QString path = pathAt(row);
    if (path.isEmpty()) return false;
    m_requestedRow = row;
    m_requestedPath = path;
    ++m_revision;
    m_canGoPrevious = adjacentRow(row, -1) >= 0;
    m_canGoNext = adjacentRow(row, 1) >= 0;
    if (m_quickLook) m_quickLook->preview(path);
    emit stateChanged();
    emit targetCommitted(path, m_originKind, m_revision);
    return true;
}

bool QuickLookNavigationController::navigate(int direction)
{
    if (!active() || (direction != -1 && direction != 1)) return false;
    refresh();
    const int row = indexOfPath(m_requestedPath);
    if (row < 0) return false;
    const int target = adjacentRow(row, direction);
    return target >= 0 && commitRow(target);
}
