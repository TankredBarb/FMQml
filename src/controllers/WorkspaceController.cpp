#include "WorkspaceController.h"
#include "../core/ArchiveSupport.h"
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTimer>

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    m_placesModel.setIsoMountManager(&m_isoMountManager);
    m_treeModel.setIsoMountManager(&m_isoMountManager);

    connect(&m_leftPanel, &FilePanelController::contentsChanged, this,
        [this](const QString &path) {
            m_treeModel.refreshPath(path);
        });
    connect(&m_rightPanel, &FilePanelController::contentsChanged, this,
        [this](const QString &path) {
            m_treeModel.refreshPath(path);
        });

    connect(&m_leftPanel, &FilePanelController::isoMountRequested, this, &WorkspaceController::requestMountIso);
    connect(&m_rightPanel, &FilePanelController::isoMountRequested, this, &WorkspaceController::requestMountIso);
    connect(&m_isoMountManager, &IsoMountManager::mountFinished, this,
        [this](const QString &, const QString &rootPath, bool success, const QString &) {
            m_placesModel.refresh();
            m_treeModel.refresh();
            QTimer::singleShot(1000, this, [this]() {
                m_placesModel.refresh();
                m_treeModel.refresh();
            });
            if (success && !rootPath.isEmpty()) {
                (m_activePanel == 0 ? &m_leftPanel : &m_rightPanel)->openPath(rootPath);
            }
        });
    connect(&m_isoMountManager, &IsoMountManager::unmountFinished, this,
        [this](const QString &rootPath, bool success, const QString &) {
            m_placesModel.refresh();
            m_treeModel.refresh();
            QTimer::singleShot(1000, this, [this]() {
                m_placesModel.refresh();
                m_treeModel.refresh();
            });
            if (!success) {
                return;
            }
            for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
                const QString current = panel->currentPath();
                if (!current.isEmpty() && current.startsWith(rootPath, Qt::CaseInsensitive)) {
                    panel->openPath(QStringLiteral("devices://"));
                }
            }
        });
    connect(&m_isoMountManager, &IsoMountManager::statusMessage, &m_operationQueue, &OperationQueue::setStatusMessage);

    connect(&m_operationQueue, &OperationQueue::operationFinished, this,
        [this](auto type, const auto &sources, const auto &destination) {
            if (!m_operationQueue.error().isEmpty()) {
                const auto refreshIfShowing = [this](const QString &path) {
                    if (path.isEmpty()) {
                        return;
                    }
                    if (m_leftPanel.directoryModel()->currentPath() == path) {
                        m_leftPanel.refresh();
                    }
                    if (m_rightPanel.directoryModel()->currentPath() == path) {
                        m_rightPanel.refresh();
                    }
                    m_treeModel.refreshPath(path);
                };

                for (const QString &source : sources) {
                    refreshIfShowing(m_leftPanel.parentPathForPath(source));
                }
                if (!destination.isEmpty()) {
                    refreshIfShowing(destination);
                }
                return;
            }

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

            if (type == OperationQueue::Type::Extract) {
                const QString destinationParent = m_leftPanel.parentPathForPath(destination);
                addTreeRefreshPath(destination);
                addTreeRefreshPath(destinationParent);

                for (FilePanelController *panel : panels) {
                    const QString panelPath = panel->directoryModel()->currentPath();
                    if (panelPath == destination || panelPath == destinationParent) {
                        if (panel == &m_leftPanel) needsLeftRefresh = true;
                        if (panel == &m_rightPanel) needsRightRefresh = true;
                    }
                }
            } else if (type == OperationQueue::Type::Delete) {
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
                    const bool sourceIsArchiveEntry = ArchiveSupport::isArchivePath(source);
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
                            if (sourceIsArchiveEntry) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                                continue;
                            }
                            if (!destPath.isEmpty()) {
                                panel->directoryModel()->removePath(destPath + QStringLiteral(".part"));
                            }
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

IsoMountManager *WorkspaceController::isoMountManager()
{
    return &m_isoMountManager;
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
    case OperationQueue::Type::Extract:
        return;
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
    if (!destination->canCreateInCurrentPath()) {
        m_operationQueue.setStatusMessage(QStringLiteral("You do not have permission to write to the destination."));
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
    if (!source->canDeleteSelection() || !destination->canCreateInCurrentPath()) {
        m_operationQueue.setStatusMessage(QStringLiteral("The current selection cannot be moved with the available permissions."));
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
    if (!active->canDeleteSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
        return;
    }
    requestDelete(active->selectedPaths(), active->currentPath());
}

void WorkspaceController::requestDelete(const QStringList &paths, const QString &label)
{
    if (paths.isEmpty()) {
        return;
    }
    FilePanelController *panel = panelForPath(label);
    if (panel && !panel->canDeleteSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
        return;
    }
    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
            m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
            return;
        }
    }
    emit deleteRequested(paths, label);
}

void WorkspaceController::triggerRename()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (!active->canRenameSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("The current item cannot be renamed with the available permissions."));
        return;
    }
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
    if (!active->canDeleteSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be moved from this location."));
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
    if (!active->canPasteIntoCurrentPath()) {
        m_operationQueue.setStatusMessage(QStringLiteral("You do not have permission to write to this location."));
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

void WorkspaceController::extractArchiveTo(const QString &archivePath, const QString &destination)
{
    if (archivePath.isEmpty() || destination.isEmpty()) {
        return;
    }
    m_operationQueue.extractTo(QStringList{archivePath}, destination);
}

bool WorkspaceController::canExtractArchivePath(const QString &archivePath) const
{
    return !archivePath.isEmpty() && m_leftPanel.isArchiveFilePath(archivePath);
}

void WorkspaceController::extractArchiveHerePath(const QString &archivePath, const QString &currentFolder)
{
    if (!canExtractArchivePath(archivePath) || currentFolder.isEmpty()) {
        return;
    }
    extractArchiveTo(archivePath, currentFolder);
}

void WorkspaceController::extractArchiveToNamedFolderPath(const QString &archivePath, const QString &currentFolder)
{
    if (!canExtractArchivePath(archivePath) || currentFolder.isEmpty()) {
        return;
    }

    const QFileInfo info(archivePath);
    const QString folderName = info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
    if (folderName.isEmpty()) {
        return;
    }

    QDir currentDir(currentFolder);
    QString destination = currentDir.filePath(folderName);
    if (QFileInfo::exists(destination)) {
        for (int i = 1; i < 10000; ++i) {
            const QString candidate = currentDir.filePath(QStringLiteral("%1 copy %2").arg(folderName).arg(i));
            if (!QFileInfo::exists(candidate)) {
                destination = candidate;
                break;
            }
        }
    }

    extractArchiveTo(archivePath, destination);
}

bool WorkspaceController::canMountIsoPath(const QString &path) const
{
    return m_isoMountManager.canMountIsoPath(path);
}

void WorkspaceController::requestMountIso(const QString &path)
{
    if (!canMountIsoPath(path)) {
        return;
    }
    const QString mountedRoot = m_isoMountManager.mountedRootForImage(path);
    if (!mountedRoot.isEmpty()) {
        (m_activePanel == 0 ? &m_leftPanel : &m_rightPanel)->openPath(mountedRoot);
        return;
    }
    emit mountIsoRequested(path);
}

void WorkspaceController::mountIsoToLetter(const QString &path, const QString &letter)
{
    m_isoMountManager.mountIsoToLetter(path, letter);
}

void WorkspaceController::mountIsoAutomatically(const QString &path)
{
    m_isoMountManager.mountIsoToLetter(path, {});
}

bool WorkspaceController::isManagedIsoMountRoot(const QString &rootPath) const
{
    return m_isoMountManager.isManagedMountRoot(rootPath);
}

bool WorkspaceController::isInsideManagedIsoMount(const QString &path) const
{
    return m_isoMountManager.isInsideManagedMount(path);
}

void WorkspaceController::unmountIsoRoot(const QString &rootPath)
{
    m_isoMountManager.unmountIsoRoot(rootPath);
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
