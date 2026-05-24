#include "WorkspaceController.h"
#include <QClipboard>
#include <QGuiApplication>

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    connect(&m_leftPanel, &FilePanelController::contentsChanged, this,
        [this](const QString &path) {
            m_treeModel.refreshPath(path);
        });
    connect(&m_rightPanel, &FilePanelController::contentsChanged, this,
        [this](const QString &path) {
            m_treeModel.refreshPath(path);
        });

    connect(&m_operationQueue, &OperationQueue::operationFinished, this,
        [this](auto type, const auto &sources, const auto &destination) {
            const auto tryUpdatePanel = [](FilePanelController *panel, const QString &sourcePath, const QString &destPath, bool removeSource) {
                if (panel->currentPath().isEmpty()) {
                    return false;
                }

                bool changed = false;
                if (removeSource) {
                    changed |= panel->directoryModel()->removePath(sourcePath);
                }
                if (!destPath.isEmpty()) {
                    changed |= panel->directoryModel()->insertPath(destPath);
                }
                return changed;
            };

            const auto panels = {&m_leftPanel, &m_rightPanel};
            bool needsLeftRefresh = false;
            bool needsRightRefresh = false;
            QStringList treeRefreshPaths;

            const auto addTreeRefreshPath = [&treeRefreshPaths](const QString &path) {
                if (path.isEmpty() || treeRefreshPaths.contains(path)) {
                    return;
                }
                treeRefreshPaths.append(path);
            };

            if (type == OperationQueue::Type::Delete) {
                for (const QString &source : sources) {
                    const QString sourceParent = m_leftPanel.parentPathForPath(source);
                    addTreeRefreshPath(sourceParent);
                    for (FilePanelController *panel : panels) {
                        if (panel->directoryModel()->currentPath() == sourceParent) {
                            if (!panel->directoryModel()->removePath(source)) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }
                    }
                }
            } else {
                for (const QString &source : sources) {
                    FilePanelController *sourcePanel = panelForPath(source);
                    const QString destPath = destination.isEmpty()
                        ? QString()
                        : sourcePanel->childPathForPath(destination, sourcePanel->fileNameForPath(source));
                    const QString sourceParent = sourcePanel->parentPathForPath(source);
                    addTreeRefreshPath(sourceParent);
                    addTreeRefreshPath(destination);

                    for (FilePanelController *panel : panels) {
                        const QString panelPath = panel->directoryModel()->currentPath();
                        const QString destParent = destination;

                        if (type == OperationQueue::Type::Move && panelPath == sourceParent) {
                            if (!panel->directoryModel()->removePath(source)) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }

                        if (panelPath == destParent) {
                            if (!destPath.isEmpty() && !panel->directoryModel()->insertPath(destPath)) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else if (!destPath.isEmpty()) {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }
                    }
                }
            }

            if (needsLeftRefresh) {
                m_leftPanel.refresh();
            }
            if (needsRightRefresh) {
                m_rightPanel.refresh();
            }

            for (const QString &path : treeRefreshPaths) {
                m_treeModel.refreshPath(path);
            }

            if (m_replayingHistory) {
                m_replayingHistory = false;
                return;
            }
            recordOperationHistory(type, sources, destination);
        });

    connect(&m_leftPanel, &FilePanelController::entryRenamed, this,
        [this](const QString &oldPath, const QString &newPath) {
            if (m_replayingHistory) {
                return;
            }
            recordRenameHistory(oldPath, newPath);
        });
    connect(&m_rightPanel, &FilePanelController::entryRenamed, this,
        [this](const QString &oldPath, const QString &newPath) {
            if (m_replayingHistory) {
                return;
            }
            recordRenameHistory(oldPath, newPath);
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

TreeModel *WorkspaceController::treeModel()
{
    return &m_treeModel;
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
        target->syncStateFrom(source);
    } else if (m_activePanel == 1) {
        m_leftPanel.syncStateFrom(&m_rightPanel);
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

void WorkspaceController::focusActivePanel()
{
    emit focusActivePanelRequested();
}

FilePanelController *WorkspaceController::panelForPath(const QString &path)
{
    const QString parentPath = m_leftPanel.parentPathForPath(path);
    if (m_leftPanel.currentPath() == parentPath) {
        return &m_leftPanel;
    }
    if (m_rightPanel.currentPath() == parentPath) {
        return &m_rightPanel;
    }
    return m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
}

void WorkspaceController::recordOperationHistory(OperationQueue::Type type, const QStringList &sources, const QString &destination)
{
    HistoryAction::Type historyType;
    switch (type) {
    case OperationQueue::Type::Copy:
        historyType = HistoryAction::Type::Copy;
        break;
    case OperationQueue::Type::Move:
        historyType = HistoryAction::Type::Move;
        break;
    case OperationQueue::Type::Delete:
        return;
    default:
        return;
    }

    m_historyManager.recordAction({historyType, sources, destination, {}});
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

void WorkspaceController::copyActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *source = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    if (source->isDeviceRoot() || destination->isDeviceRoot()) {
        return;
    }
    m_operationQueue.copyTo(source->selectedPaths(), destination->currentPath());
}

void WorkspaceController::moveActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *source = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    if (source->isDeviceRoot() || destination->isDeviceRoot()) {
        return;
    }
    m_operationQueue.moveTo(source->selectedPaths(), destination->currentPath());
}

void WorkspaceController::deleteActiveSelection()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isDeviceRoot()) {
        return;
    }
    requestDelete(active->selectedPaths(), active->currentPath());
}

void WorkspaceController::requestDelete(const QStringList &paths, const QString &label)
{
    if (paths.isEmpty()) {
        return;
    }
    emit deleteRequested(paths, label);
}

void WorkspaceController::triggerRename()
{
    emit renameRequested();
}

bool WorkspaceController::hasClipboard() const
{
    return !m_clipboard.isEmpty();
}

int WorkspaceController::clipboardCount() const
{
    return m_clipboard.size();
}

bool WorkspaceController::clipboardCut() const
{
    return m_isCut;
}

QString WorkspaceController::clipboardSummary() const
{
    if (m_clipboard.isEmpty()) {
        return {};
    }

    return QStringLiteral("Clipboard: %1 %2 %3")
        .arg(m_clipboard.size())
        .arg(m_clipboard.size() == 1 ? "file" : "files")
        .arg(m_isCut ? "cut" : "copied");
}

void WorkspaceController::copyToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isDeviceRoot()) {
        return;
    }
    m_clipboard = active->selectedPaths();
    m_isCut = false;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::cutToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isDeviceRoot()) {
        return;
    }
    m_clipboard = active->selectedPaths();
    m_isCut = true;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::pasteFromClipboard()
{
    if (m_clipboard.isEmpty()) {
        return;
    }
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isDeviceRoot()) {
        return;
    }
    if (m_isCut) {
        m_operationQueue.moveTo(m_clipboard, active->currentPath());
        m_clipboard.clear();
        m_isCut = false;
        emit clipboardChanged();
    } else {
        m_operationQueue.copyTo(m_clipboard, active->currentPath());
    }
}

void WorkspaceController::copyTextToClipboard(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
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
        QStringList currentPaths;
        FilePanelController *sourcePanel = panelForPath(action.sources.first());
        for (const QString &src : action.sources) {
            currentPaths.append(sourcePanel->childPathForPath(action.destination, sourcePanel->fileNameForPath(src)));
        }
        m_replayingHistory = true;
        m_operationQueue.moveTo(currentPaths, sourcePanel->parentPathForPath(action.sources.first()));
        break;
    }
    case HistoryAction::Type::Copy: {
        if (action.sources.isEmpty()) {
            break;
        }
        QStringList copiedPaths;
        FilePanelController *sourcePanel = panelForPath(action.sources.first());
        for (const QString &src : action.sources) {
            copiedPaths.append(sourcePanel->childPathForPath(action.destination, sourcePanel->fileNameForPath(src)));
        }
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
