#include "WorkspaceController.h"
#include "../core/ArchiveSupport.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include "../core/FileProviderPluginRegistry.h"
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#endif
#include "WorkspaceControllerInternal.h"

using namespace WorkspaceControllerInternal;

void WorkspaceController::recordOperationHistory(OperationQueue::Type type, const QStringList &sources,
                                                 const QString &destination, const QStringList &resultPaths)
{
    if (destination.isEmpty()) return;
    HistoryAction::Type historyType;
    switch (type) {
    case OperationQueue::Type::Copy:
        historyType = HistoryAction::Type::Copy;
        break;
    case OperationQueue::Type::Duplicate:
        return;
    case OperationQueue::Type::Move:
        historyType = HistoryAction::Type::Move;
        break;
    case OperationQueue::Type::Extract:
        return;
    case OperationQueue::Type::Compress:
        return;
    case OperationQueue::Type::Delete:
        return;
    case OperationQueue::Type::CreateFolder:
        return;
    default:
        return;
    }

    m_historyManager.recordAction({historyType, sources, destination, resultPaths});
}

void WorkspaceController::recordRenameHistory(const QString &oldPath, const QString &newPath)
{
    if (oldPath.isEmpty() || newPath.isEmpty()) {
        return;
    }
    m_historyManager.recordAction({HistoryAction::Type::Rename, {oldPath}, newPath, {oldPath}});
}

void WorkspaceController::finishHistoryReplay()
{
    m_replayingHistory = false;
}

void WorkspaceController::undo()
{
    if (!m_historyManager.canUndo()) return;

    HistoryAction action = m_historyManager.takeUndo();

    switch (action.type) {
    case HistoryAction::Type::Move: {
        if (action.sources.isEmpty()) {
            break;
        }
        const QStringList currentPaths = !action.originalPaths.isEmpty()
            ? action.originalPaths
            : QStringList{};
        if (currentPaths.isEmpty()) break;
        FilePanelController *sourcePanel = panelForPath(action.sources.first());
        m_replayingHistory = true;
        m_operationQueue.moveTo(currentPaths, sourcePanel->parentPathForPath(action.sources.first()));
        break;
    }
    case HistoryAction::Type::Copy: {
        if (action.sources.isEmpty()) {
            break;
        }
        const QStringList copiedPaths = action.originalPaths;
        if (copiedPaths.isEmpty()) break;
        m_replayingHistory = true;
        m_operationQueue.deletePaths(copiedPaths);
        break;
    }
    case HistoryAction::Type::Rename: {
        if (action.sources.isEmpty() || action.destination.isEmpty()) {
            break;
        }
        const QString oldPath = action.sources.first();
        const QString newPath = action.destination;
        FilePanelController *panel = panelForPath(oldPath);
        const QString oldName = panel->fileNameForPath(oldPath);
        m_replayingHistory = true;
        if (!panel->renamePath(newPath, oldName)) {
            finishHistoryReplay();
        }
        break;
    }
    default:
        break;
    }
}

void WorkspaceController::redo()
{
    if (!m_historyManager.canRedo()) return;

    HistoryAction action = m_historyManager.takeRedo();
    switch (action.type) {
    case HistoryAction::Type::Copy:
        m_replayingHistory = true;
        m_operationQueue.copyTo(action.sources, action.destination);
        break;
    case HistoryAction::Type::Move:
        m_replayingHistory = true;
        m_operationQueue.moveTo(action.sources, action.destination);
        break;
    case HistoryAction::Type::Rename: {
        if (action.sources.isEmpty() || action.destination.isEmpty()) {
            break;
        }
        const QString oldPath = action.sources.first();
        const QString newPath = action.destination;
        FilePanelController *panel = panelForPath(oldPath);
        const QString newName = panel->fileNameForPath(newPath);
        m_replayingHistory = true;
        if (!panel->renamePath(oldPath, newName)) {
            finishHistoryReplay();
        }
        break;
    }
    default:
        break;
    }
}
