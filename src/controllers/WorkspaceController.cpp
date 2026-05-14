#include "WorkspaceController.h"
#include <QDir>
#include <QFileInfo>

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    connect(&m_operationQueue, &OperationQueue::operationFinished, &m_leftPanel, &FilePanelController::refresh);
    connect(&m_operationQueue, &OperationQueue::operationFinished, &m_rightPanel, &FilePanelController::refresh);

    connect(&m_operationQueue, &OperationQueue::operationFinished, this, [this](auto type, const auto &sources, const auto &destination) {
        HistoryAction::Type historyType;
        switch (type) {
        case OperationQueue::Type::Copy: historyType = HistoryAction::Type::Copy; break;
        case OperationQueue::Type::Move: historyType = HistoryAction::Type::Move; break;
        case OperationQueue::Type::Delete: historyType = HistoryAction::Type::Delete; break;
        default: return;
        }
        
        // Note: For complex Undo, we might need more details (like original paths for move/rename)
        // For now, recording basic action.
        m_historyManager.recordAction({historyType, sources, destination, {}});
    });
}

FilePanelController *WorkspaceController::leftPanel()
{
    return &m_leftPanel;
}

FilePanelController *WorkspaceController::rightPanel()
{
    return &m_rightPanel;
}

PlacesModel *WorkspaceController::placesModel()
{
    return &m_placesModel;
}

OperationQueue *WorkspaceController::operationQueue()
{
    return &m_operationQueue;
}

HistoryManager *WorkspaceController::historyManager()
{
    return &m_historyManager;
}

bool WorkspaceController::splitEnabled() const
{
    return m_splitEnabled;
}

void WorkspaceController::setSplitEnabled(bool enabled)
{
    if (m_splitEnabled == enabled) {
        return;
    }

    if (enabled) {
        FilePanelController *source = m_activePanel == 1 ? &m_rightPanel : &m_leftPanel;
        FilePanelController *target = m_activePanel == 1 ? &m_leftPanel : &m_rightPanel;
        target->openPath(source->currentPath());
    }

    m_splitEnabled = enabled;
    if (!m_splitEnabled && m_activePanel == 1) {
        setActivePanel(0);
    }
    emit splitEnabledChanged();
}

int WorkspaceController::activePanel() const
{
    return m_activePanel;
}

void WorkspaceController::setActivePanel(int panel)
{
    const int normalizedPanel = panel == 1 ? 1 : 0;
    if (m_activePanel == normalizedPanel) {
        return;
    }
    m_activePanel = normalizedPanel;
    emit activePanelChanged();
}

void WorkspaceController::toggleSplit()
{
    setSplitEnabled(!m_splitEnabled);
}

void WorkspaceController::activateLeft()
{
    setActivePanel(0);
}

void WorkspaceController::activateRight()
{
    if (m_splitEnabled) {
        setActivePanel(1);
    }
}

void WorkspaceController::copyActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *source = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    m_operationQueue.copyTo(source->selectedPaths(), destination->currentPath());
}

void WorkspaceController::moveActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *source = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    m_operationQueue.moveTo(source->selectedPaths(), destination->currentPath());
}

void WorkspaceController::deleteActiveSelection()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    m_operationQueue.deletePaths(active->selectedPaths());
}

void WorkspaceController::triggerRename()
{
    emit renameRequested();
}

bool WorkspaceController::hasClipboard() const
{
    return !m_clipboard.isEmpty();
}

void WorkspaceController::copyToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    m_clipboard = active->selectedPaths();
    m_isCut = false;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        QStringLiteral("%1 %2 copied to clipboard")
            .arg(m_clipboard.size())
            .arg(m_clipboard.size() == 1 ? "file" : "files"));
}

void WorkspaceController::cutToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    m_clipboard = active->selectedPaths();
    m_isCut = true;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        QStringLiteral("%1 %2 cut to clipboard")
            .arg(m_clipboard.size())
            .arg(m_clipboard.size() == 1 ? "file" : "files"));
}

void WorkspaceController::pasteFromClipboard()
{
    if (m_clipboard.isEmpty()) {
        return;
    }
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (m_isCut) {
        m_operationQueue.moveTo(m_clipboard, active->currentPath());
        m_clipboard.clear();
        m_isCut = false;
        emit clipboardChanged();
    } else {
        m_operationQueue.copyTo(m_clipboard, active->currentPath());
    }
}

void WorkspaceController::undo()
{
    if (!m_historyManager.canUndo()) return;
    
    HistoryAction action = m_historyManager.takeUndo();
    
    switch (action.type) {
    case HistoryAction::Type::Move: {
        // To undo a move, we need to move files back.
        // This requires knowing the original paths precisely.
        // For now, if sources were paths, we can move from (destination + filename) back to sources.
        QStringList currentPaths;
        for (const QString &src : action.sources) {
            currentPaths.append(QDir(action.destination).filePath(QFileInfo(src).fileName()));
        }
        // Move back to original locations
        for (int i = 0; i < currentPaths.size(); ++i) {
             m_operationQueue.moveTo({currentPaths[i]}, QFileInfo(action.sources[i]).absolutePath());
        }
        break;
    }
    case HistoryAction::Type::Copy: {
        QStringList copiedPaths;
        for (const QString &src : action.sources) {
            copiedPaths.append(QDir(action.destination).filePath(QFileInfo(src).fileName()));
        }
        m_operationQueue.deletePaths(copiedPaths);
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
        m_operationQueue.copyTo(action.sources, action.destination);
        break;
    case HistoryAction::Type::Move:
        m_operationQueue.moveTo(action.sources, action.destination);
        break;
    default:
        break;
    }
}
