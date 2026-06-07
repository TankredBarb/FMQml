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

    Shortcut {
        sequence: "F1"
        enabled: !root.appRoot.anyOverlayOpen
        onActivated: root.appRoot.openHelpDialog()
    }

    Shortcut {
        sequence: "Ctrl+K"
        enabled: (!root.appRoot.anyOverlayOpen || (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible))
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openCommandPalette()
    }

    Shortcut {
        sequence: "Ctrl+Shift+P"
        enabled: (!root.appRoot.anyOverlayOpen || (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible))
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openCommandPalette()
    }

    Shortcut {
        sequence: "F3"
        enabled: root.appRoot.splitViewShortcutEnabled
        onActivated: root.appRoot.toggleSplitView()
    }

    Shortcut {
        sequence: "F4"
        enabled: root.appRoot.splitViewShortcutEnabled
        onActivated: root.appRoot.mirrorActivePanelToOpposite()
    }

    Shortcut {
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
        sequence: "F2"
        enabled: root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const activeCtrl = root.appRoot.activePanelController()
            if (activeCtrl && !root.appRoot.isProviderPath(activeCtrl.currentPath) && activeCtrl.canRenameSelection) {
                root.workspaceController.triggerRename()
            }
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: root.appRoot.fileViewShortcutsEnabled
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
        sequence: "Delete"
        enabled: root.appRoot.fileViewShortcutsEnabled
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
        sequence: "Shift+Delete"
        enabled: root.appRoot.fileViewShortcutsEnabled
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
        sequence: "Escape"
        enabled: root.appRoot.fileViewShortcutsEnabled
                 && ((root.workspaceController.activePanel === 0
                      && (root.workspaceController.leftPanel.directoryModel.selectedCount > 0
                          || (root.fileWorkspace && root.fileWorkspace.leftPanelView && root.fileWorkspace.leftPanelView.invertSelectionActive)))
                     || (root.workspaceController.activePanel === 1
                         && (root.workspaceController.rightPanel.directoryModel.selectedCount > 0
                             || (root.fileWorkspace && root.fileWorkspace.rightPanelView && root.fileWorkspace.rightPanelView.invertSelectionActive))))
        onActivated: {
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
        sequence: "Tab"
        enabled: root.appRoot.tabPanelSwitchEnabled
        onActivated: {
            if (root.workspaceController.splitEnabled) {
                root.workspaceController.activePanel = root.workspaceController.activePanel === 0 ? 1 : 0
                root.workspaceController.focusActivePanel()
            }
        }
    }

    Shortcut {
        sequence: "Alt+Left"
        enabled: root.navigationShortcutsEnabled()
        onActivated: root.appRoot.goBackInActivePanel()
    }

    Shortcut {
        sequence: "Alt+Right"
        enabled: root.navigationShortcutsEnabled()
        onActivated: root.appRoot.goForwardInActivePanel()
    }

    Shortcut {
        sequence: "Alt+Up"
        enabled: root.navigationShortcutsEnabled()
        onActivated: root.appRoot.goUpInActivePanel()
    }

    Shortcut {
        sequence: "Ctrl+L"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.focusActivePath()
    }

    Shortcut {
        sequence: "Ctrl+G"
        enabled: (!root.appRoot.anyOverlayOpen || (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible))
                 && !(root.mainToolbar && root.mainToolbar.textEditingActive)
                 && !(root.fileWorkspace && root.fileWorkspace.isRenaming)
        onActivated: root.appRoot.openCommandPaletteForCommand("nav.goToPath")
    }

    Shortcut {
        sequence: "Ctrl+C"
        enabled: root.appRoot.fileViewShortcutsEnabled
        onActivated: root.workspaceController.copyToClipboard()
    }

    Shortcut {
        sequence: "Ctrl+X"
        enabled: root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl && !root.appRoot.isProviderPath(ctrl.currentPath) && ctrl.canDeleteSelection) {
                root.workspaceController.cutToClipboard()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+V"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl && ctrl.canPasteIntoCurrentPath) {
                root.workspaceController.pasteFromClipboard()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Z"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.undo()
    }

    Shortcut {
        sequence: "Ctrl+Y"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.redo()
    }

    Shortcut {
        sequence: "Ctrl+R"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl) ctrl.refresh()
        }
    }

    Shortcut {
        sequence: "Ctrl+H"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.toggleHiddenFiles()
    }

    Shortcut {
        sequence: "Ctrl+1"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.setActiveViewMode(0)
    }

    Shortcut {
        sequence: "Ctrl+2"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.setActiveViewMode(1)
    }

    Shortcut {
        sequence: "Ctrl+3"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.setActiveViewMode(2)
    }

    Shortcut {
        sequence: "Ctrl+Shift+N"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (root.appRoot.canCreateManualItemInPanel(ctrl)) {
                root.appRoot.createFolderInActivePanel()
            }
        }
    }

    Shortcut {
        sequence: "F7"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (root.appRoot.canCreateManualItemInPanel(ctrl)) {
                root.appRoot.createFolderInActivePanel()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        enabled: root.appRoot.panelShortcutsEnabled
                 && root.shortcutActivePanel
                 && !root.shortcutActivePanel.isFavoritesRoot
        onActivated: root.appRoot.focusActiveSearch()
    }

    Shortcut {
        sequence: "Ctrl+Shift+F"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.openFileSearch()
    }

    Shortcut {
        sequence: "Ctrl+P"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.togglePreviewPane()
    }

    Shortcut {
        sequence: "Ctrl+A"
        enabled: root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const panelView = root.shortcutActivePanelView
            if (panelView) {
                panelView.selectAll()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+I"
        enabled: root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const panelView = root.shortcutActivePanelView
            if (panelView && panelView.canInvertSelection) {
                panelView.toggleInvertSelection()
            }
        }
    }

    Shortcut {
        sequence: "Alt+D"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.focusActivePath()
    }
}
