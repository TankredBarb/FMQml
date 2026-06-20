import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var appRoot
    property var workspaceController
    property var quickLookController
    property var propertiesController
    property var sidebar
    property var mainToolbar
    property var fileWorkspace
    property var quickLookPopup
    property var inputCoordinator
    readonly property var shortcutActivePanel: !root.workspaceController
                                              ? null
                                              : (root.workspaceController.activePanel === 0
                                                 ? root.workspaceController.leftPanel
                                                 : root.workspaceController.rightPanel)
    readonly property var shortcutActivePanelView: !root.workspaceController || !root.fileWorkspace
                                                  ? null
                                                  : (root.workspaceController.activePanel === 0
                                                     ? root.fileWorkspace.leftPanelView
                                                     : root.fileWorkspace.rightPanelView)

    function isReadOnlyContainerPath(path) {
        if (!path) return false
        if (path.toLowerCase().startsWith("archive://")) return true
        return root.workspaceController && root.workspaceController.isInsideManagedIsoMount(path)
    }

    function activePanelAcceptsFileDelete() {
        return Boolean(root.shortcutActivePanel
                       && !root.shortcutActivePanel.isVirtualRoot
                       && root.appRoot
                       && root.shortcutActivePanel.canDeleteSelection)
    }

    function navigationShortcutsEnabled() {
        return root.appRoot
                && !root.appRoot.anyOverlayOpen
                && !(root.mainToolbar && root.mainToolbar.textEditingActive)
    }

    function traceShortcut(actionName) {
        if (root.appRoot && root.appRoot.inputRoutingLog) {
            root.appRoot.inputRoutingLog("shortcut-activated", actionName)
        }
        if (root.inputCoordinator) {
            root.inputCoordinator.traceDecision(actionName)
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "F1"
        enabled: !root.appRoot.anyOverlayOpen
        onActivated: root.appRoot.openHelpDialog()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+K"
        enabled: (!root.appRoot.anyOverlayOpen || (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible))
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openCommandPalette()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+Shift+P"
        enabled: (!root.appRoot.anyOverlayOpen || (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible))
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openCommandPalette()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+Alt+Shift+D"
        enabled: !root.appRoot.anyOverlayOpen
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openDebugInformationDialog()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "F3"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("toggleSplit")
                 : root.appRoot.splitViewShortcutEnabled
        onActivated: {
            root.traceShortcut("toggleSplit")
            root.appRoot.toggleSplitView()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "F4"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("mirrorPanel")
                 : root.appRoot.splitViewShortcutEnabled
        onActivated: root.appRoot.mirrorActivePanelToOpposite()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "F9"
        enabled: !root.appRoot.anyOverlayOpen
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: {
            if (root.sidebar.placesList.activeFocus || root.sidebar.foldersTree.activeFocus) {
                root.sidebar.trapTabNavigation = false
                root.workspaceController.focusActivePanel()
            } else {
                root.appRoot.focusActiveSidebar()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "F2"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("renameSelection")
                 : root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const activeCtrl = root.appRoot.activePanelController()
            if (activeCtrl && !root.appRoot.isProviderPath(activeCtrl.currentPath) && activeCtrl.canRenameSelection) {
                root.workspaceController.triggerRename()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Space"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("previewSelection")
                 : root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const controller = root.appRoot.activePanelController()
            const panelView = root.shortcutActivePanelView
            if (controller && controller.isFavoritesRoot) {
                if (panelView && panelView.quickLookCurrentFavorite) {
                    panelView.quickLookCurrentFavorite()
                }
                return
            }

            if (!controller) {
                return
            }
            const targetPath = root.appRoot.previewTargetFor(controller)
            if (targetPath.length === 0) {
                return
            }

            root.quickLookController.preview(targetPath)
            root.quickLookPopup.previewPath = targetPath
            root.quickLookPopup.open()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Delete"
        enabled: (root.inputCoordinator
                  ? root.inputCoordinator.canRun("deleteSelection")
                  : root.appRoot.fileViewShortcutsEnabled)
                 && root.activePanelAcceptsFileDelete()
                 && !root.workspaceController.operationQueue.busy
        onActivated: {
            const activeCtrl = root.appRoot.activePanelController()
            if (activeCtrl && activeCtrl.directoryModel && activeCtrl.directoryModel.selectedCount > 0) {
                root.appRoot.requestDeleteActiveSelection()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Shift+Delete"
        enabled: (root.inputCoordinator
                  ? root.inputCoordinator.canRun("deleteSelection")
                  : root.appRoot.fileViewShortcutsEnabled)
                 && root.activePanelAcceptsFileDelete()
                 && !root.workspaceController.operationQueue.busy
        onActivated: {
            const activeCtrl = root.appRoot.activePanelController()
            if (activeCtrl && activeCtrl.directoryModel && activeCtrl.directoryModel.selectedCount > 0) {
                root.appRoot.requestDeleteActiveSelection()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Escape"
        enabled: (root.inputCoordinator
                  ? root.inputCoordinator.canRun("closeOrCancel")
                  : root.appRoot.fileViewShortcutsEnabled)
                 && ((root.fileWorkspace
                      && ((root.fileWorkspace.leftPanelView && root.fileWorkspace.leftPanelView.dropMenuOpen())
                          || (root.fileWorkspace.rightPanelView && root.fileWorkspace.rightPanelView.dropMenuOpen())))
                     || (root.workspaceController.activePanel === 0
                      && (root.workspaceController.leftPanel.directoryModel.selectedCount > 0
                          || (root.fileWorkspace && root.fileWorkspace.leftPanelView && root.fileWorkspace.leftPanelView.invertSelectionActive)))
                     || (root.workspaceController.activePanel === 1
                         && (root.workspaceController.rightPanel.directoryModel.selectedCount > 0
                             || (root.fileWorkspace && root.fileWorkspace.rightPanelView && root.fileWorkspace.rightPanelView.invertSelectionActive))))
        onActivated: {
            if (root.fileWorkspace) {
                if (root.fileWorkspace.leftPanelView) {
                    root.fileWorkspace.leftPanelView.cancelDropMenu()
                }
                if (root.fileWorkspace.rightPanelView) {
                    root.fileWorkspace.rightPanelView.cancelDropMenu()
                }
            }
            const panelView = root.shortcutActivePanelView
            if (panelView) {
                panelView.clearSelection()
            } else {
                const active = root.workspaceController.activePanel === 0
                               ? root.workspaceController.leftPanel
                               : root.workspaceController.rightPanel
                active.directoryModel.clearSelection()
            }
            root.workspaceController.focusActivePanel()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Tab"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("switchPanel")
                 : root.appRoot.tabPanelSwitchEnabled
        onActivated: {
            root.traceShortcut("switchPanel")
            if (root.workspaceController.splitEnabled) {
                root.workspaceController.activePanel = root.workspaceController.activePanel === 0 ? 1 : 0
                root.workspaceController.focusActivePanel()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Alt+Left"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("navigatePanel")
                 : root.navigationShortcutsEnabled()
        onActivated: root.appRoot.goBackInActivePanel()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Alt+Right"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("navigatePanel")
                 : root.navigationShortcutsEnabled()
        onActivated: root.appRoot.goForwardInActivePanel()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Alt+Up"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("navigatePanel")
                 : root.navigationShortcutsEnabled()
        onActivated: root.appRoot.goUpInActivePanel()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+L"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("focusPathEditor")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: {
            root.traceShortcut("focusPathEditor")
            root.appRoot.focusActivePath()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+G"
        enabled: (!root.appRoot.anyOverlayOpen || (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible))
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openCommandPaletteForCommand("nav.goToPath")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+C"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("copySelection")
                 : root.appRoot.fileViewShortcutsEnabled
        onActivated: root.workspaceController.copyToClipboard()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+X"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("cutSelection")
                 : root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl && !root.appRoot.isProviderPath(ctrl.currentPath) && ctrl.canDeleteSelection) {
                root.workspaceController.cutToClipboard()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+V"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("pasteIntoPanel")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl && ctrl.canPasteIntoCurrentPath) {
                root.workspaceController.pasteFromClipboard()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+Z"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("undoPanel")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.undo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+Y"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("redoPanel")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.redo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+R"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("refreshPanel")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl) ctrl.refresh()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+H"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("toggleHiddenFiles")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.toggleHiddenFiles()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+1"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("setViewMode")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.setActiveViewMode(0)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+2"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("setViewMode")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.setActiveViewMode(1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+3"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("setViewMode")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.setActiveViewMode(2)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+Shift+N"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("createFolder")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (root.appRoot.canCreateManualItemInPanel(ctrl)) {
                root.appRoot.createFolderInActivePanel()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "F7"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("createFolder")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (root.appRoot.canCreateManualItemInPanel(ctrl)) {
                root.appRoot.createFolderInActivePanel()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+F"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("focusQuickSearch")
                 : root.appRoot.panelShortcutsEnabled
                   && root.shortcutActivePanel
                   && !root.shortcutActivePanel.isFavoritesRoot
        onActivated: {
            root.traceShortcut("focusQuickSearch")
            root.appRoot.focusActiveSearch()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+Shift+F"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("openFileSearch")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.openFileSearch()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+P"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("togglePreviewPane")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.togglePreviewPane()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+A"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("selectAll")
                 : root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const panelView = root.shortcutActivePanelView
            if (panelView) {
                panelView.selectAll()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Ctrl+I"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("invertSelection")
                 : root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const panelView = root.shortcutActivePanelView
            if (panelView && panelView.canInvertSelection) {
                panelView.toggleInvertSelection()
            }
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Alt+D"
        enabled: root.inputCoordinator
                 ? root.inputCoordinator.canRun("focusPathEditor")
                 : root.appRoot.panelShortcutsEnabled
        onActivated: {
            root.traceShortcut("focusPathEditor")
            root.appRoot.focusActivePath()
        }
    }
}
