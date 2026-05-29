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

    function isReadOnlyContainerPath(path) {
        if (!path) return false
        if (path.toLowerCase().startsWith("archive://")) return true
        return root.workspaceController && root.workspaceController.isInsideManagedIsoMount(path)
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
            if (activeCtrl && activeCtrl.canRenameSelection) {
                root.workspaceController.triggerRename()
            }
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: root.appRoot.fileViewShortcutsEnabled
        onActivated: {
            const controller = root.appRoot.activePanelController()
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
        enabled: root.appRoot.fileViewShortcutsEnabled && !root.workspaceController.operationQueue.busy
        onActivated: {
            const activeCtrl = root.appRoot.activePanelController()
            if (activeCtrl && activeCtrl.directoryModel && activeCtrl.directoryModel.selectedCount > 0) {
                root.appRoot.requestDeleteActiveSelection()
            }
        }
    }

    Shortcut {
        sequence: "Shift+Delete"
        enabled: root.appRoot.fileViewShortcutsEnabled && !root.workspaceController.operationQueue.busy
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
                      && root.workspaceController.leftPanel.directoryModel.selectedCount > 0)
                     || (root.workspaceController.activePanel === 1
                         && root.workspaceController.rightPanel.directoryModel.selectedCount > 0))
        onActivated: {
            const active = root.workspaceController.activePanel === 0
                           ? root.workspaceController.leftPanel
                           : root.workspaceController.rightPanel
            active.directoryModel.clearSelection()
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
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.activePanel === 0
                     ? root.workspaceController.leftPanel.goBack()
                     : root.workspaceController.rightPanel.goBack()
    }

    Shortcut {
        sequence: "Alt+Right"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.activePanel === 0
                     ? root.workspaceController.leftPanel.goForward()
                     : root.workspaceController.rightPanel.goForward()
    }

    Shortcut {
        sequence: "Alt+Up"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.workspaceController.activePanel === 0
                     ? root.workspaceController.leftPanel.goUp()
                     : root.workspaceController.rightPanel.goUp()
    }

    Shortcut {
        sequence: "Ctrl+L"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.focusActivePath()
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
            if (ctrl && ctrl.canDeleteSelection) {
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
        sequence: "F5"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            const opposite = root.workspaceController.activePanel === 0
                             ? root.workspaceController.rightPanel
                             : root.workspaceController.leftPanel
            if (!root.appRoot.sidebarFocused
                    && root.workspaceController.splitEnabled
                    && ctrl
                    && ctrl.directoryModel.selectedCount > 0
                    && opposite
                    && opposite.canCreateInCurrentPath
                    && !root.workspaceController.operationQueue.busy) {
                root.workspaceController.copyActiveSelectionToOpposite()
            } else if (ctrl) {
                ctrl.refresh()
            }
        }
    }

    Shortcut {
        sequence: "F6"
        enabled: root.appRoot.fileViewShortcutsEnabled && root.workspaceController.splitEnabled && !root.workspaceController.operationQueue.busy
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            const opposite = root.workspaceController.activePanel === 0
                             ? root.workspaceController.rightPanel
                             : root.workspaceController.leftPanel
            if (ctrl && ctrl.canDeleteSelection && opposite && opposite.canCreateInCurrentPath) {
                root.workspaceController.moveActiveSelectionToOpposite()
            }
        }
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
            if (ctrl && ctrl.canCreateInCurrentPath) {
                root.appRoot.createFolderInActivePanel()
            }
        }
    }

    Shortcut {
        sequence: "F7"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: {
            const ctrl = root.appRoot.activePanelController()
            if (ctrl && ctrl.canCreateInCurrentPath) {
                root.appRoot.createFolderInActivePanel()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.focusActiveSearch()
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
            const ctrl = root.appRoot.activePanelController()
            if (ctrl && ctrl.directoryModel) {
                ctrl.directoryModel.selectAll()
            }
        }
    }

    Shortcut {
        sequence: "Alt+D"
        enabled: root.appRoot.panelShortcutsEnabled
        onActivated: root.appRoot.focusActivePath()
    }
}
