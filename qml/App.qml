import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "components"
import "style"

ApplicationWindow {
    id: root

    width: 1120
    height: 720
    minimumWidth: 760
    minimumHeight: 480
    visible: false
    title: "FM"
    color: Theme.bg

    function openDeleteConfirm(paths, label) {
        const list = paths ? Array.from(paths) : []
        if (list.length === 0) {
            return
        }
        deleteConfirmDialog.openFor(list, label || "")
    }

    function activePanelController() {
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    function activePanelScrolling() {
        const controller = activePanelController()
        return controller ? controller.scrolling : false
    }

    property bool previewOnHover: false
    property bool previewPaneVisible: false
    readonly property bool sidebarFocused: sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
    readonly property bool anyOverlayOpen: conflictDialog.opened || conflictDialog.visible
                                           || helpDialog.opened || helpDialog.visible
                                           || propertiesDialog.opened || propertiesDialog.visible
                                           || quickLookPopup.opened || quickLookPopup.visible
                                           || deleteConfirmDialog.opened || deleteConfirmDialog.visible
                                           || batchRenameDialog.opened || batchRenameDialog.visible
                                           || checksumDialog.opened || checksumDialog.visible
                                           || commandPalette.opened || commandPalette.visible
    readonly property bool workspaceOverlayOpen: conflictDialog.opened || conflictDialog.visible
                                                 || helpDialog.opened || helpDialog.visible
                                                 || propertiesDialog.opened || propertiesDialog.visible
                                                 || quickLookPopup.opened || quickLookPopup.visible
                                                 || deleteConfirmDialog.opened || deleteConfirmDialog.visible
                                                 || batchRenameDialog.opened || batchRenameDialog.visible
                                                 || checksumDialog.opened || checksumDialog.visible
    readonly property bool workspaceCommandsEnabled: !root.workspaceOverlayOpen
                                                      && !mainToolbar.textEditingActive
                                                      && !fileWorkspace.isRenaming
    readonly property bool panelShortcutsEnabled: !root.anyOverlayOpen
                                                  && !root.sidebarFocused
                                                  && !mainToolbar.textEditingActive
                                                  && !fileWorkspace.isRenaming
    readonly property bool tabPanelSwitchEnabled: !root.anyOverlayOpen
                                                  && !mainToolbar.textEditingActive
                                                  && !fileWorkspace.isRenaming
                                                  && (!root.sidebarFocused || !sidebar.trapTabNavigation)
    readonly property bool typeToSearchEnabled: root.panelShortcutsEnabled

    function previewTargetFor(controller) {
        if (!controller) {
            return ""
        }

        if (root.previewOnHover && controller.hoveredPath && controller.hoveredPath.length > 0) {
            return controller.hoveredPath
        }

        const selected = controller.selectedPaths()
        if (selected.length > 0) {
            return selected[0]
        }

        return controller.currentPath || ""
    }

    property string pendingPreviewPath: ""
    property int hoverPreviewDelayMs: 250

    Timer {
        id: previewSyncTimer
        interval: 250
        repeat: false
        onTriggered: {
            if (!root.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                previewSyncTimer.restart()
                return
            }
            quickLookController.preview(root.pendingPreviewPath)
        }
    }

    Timer {
        id: hoverPreviewTimer
        interval: root.hoverPreviewDelayMs
        repeat: false
        onTriggered: {
            if (!root.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                hoverPreviewTimer.restart()
                return
            }
            quickLookController.preview(root.pendingPreviewPath)
        }
    }

    function syncPreviewFromActivePanel(immediate) {
        if (!root.previewPaneVisible) {
            return
        }
        const controller = activePanelController()
        const targetPath = previewTargetFor(controller)
        if (immediate !== true && activePanelScrolling()) {
            pendingPreviewPath = targetPath
            return
        }
        if (immediate === true) {
            previewSyncTimer.stop()
            hoverPreviewTimer.stop()
            pendingPreviewPath = targetPath
            quickLookController.preview(targetPath)
            return
        }

        pendingPreviewPath = targetPath
        previewSyncTimer.restart()
    }

    function scheduleHoverPreview(path) {
        if (!root.previewPaneVisible) {
            return
        }

        if (activePanelScrolling()) {
            pendingPreviewPath = path
            return
        }

        if (hoverPreviewDelayMs <= 0) {
            hoverPreviewTimer.stop()
            return
        }

        pendingPreviewPath = path
        hoverPreviewTimer.restart()
    }

    function toggleSplitView() {
        workspaceController.toggleSplit()
    }

    function openCommandPalette() {
        commandPalette.openPalette()
    }

    function goBackInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.goBack()
        }
    }

    function goForwardInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.goForward()
        }
    }

    function goUpInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.goUp()
        }
    }

    function togglePreviewPane() {
        const visible = !root.previewPaneVisible
        root.previewPaneVisible = visible
        quickLookController.visible = visible
        if (visible) {
            syncPreviewFromActivePanel(true)
        } else {
            quickLookController.preview("")
        }
    }

    function refreshActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.refresh()
        }
    }

    function toggleHiddenFiles() {
        const ctrl = activePanelController()
        if (ctrl) {
            const newValue = !ctrl.directoryModel.showHidden
            ctrl.directoryModel.showHidden = newValue
            workspaceController.treeModel.showHidden = newValue
        }
    }

    function setThemeScheme(scheme) {
        themeController.scheme = scheme
    }

    function openThemeSelector() {
        mainToolbar.openThemeSelector()
    }

    function importThemeFromFile() {
        mainToolbar.openThemeImportDialog()
    }

    function exportCurrentTheme() {
        mainToolbar.openThemeExportDialog()
    }

    function setActiveViewMode(mode) {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.viewMode = mode
        }
    }

    function focusActiveSidebar() {
        sidebar.focusSidebar(true)
    }

    function focusActivePath() {
        mainToolbar.focusPath()
    }

    function focusActiveSearch() {
        mainToolbar.focusSearch()
    }

    function createFolderInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.createFolder("New Folder")
        }
    }

    function renameActiveSelection() {
        workspaceController.triggerRename()
    }

    function copyActiveSelection() {
        workspaceController.copyToClipboard()
    }

    function cutActiveSelection() {
        workspaceController.cutToClipboard()
    }

    function pasteClipboardToActivePanel() {
        workspaceController.pasteFromClipboard()
    }

    function requestDeleteActiveSelection() {
        const active = activePanelController()
        if (active) {
            workspaceController.requestDelete(active.selectedPaths(), active.currentPath)
        }
    }

    function showActiveProperties() {
        const ctrl = activePanelController()
        if (!ctrl) {
            return
        }

        const selected = ctrl.selectedPaths()
        if (!selected || selected.length === 0) {
            return
        }

        if (selected.length > 1) {
            propertiesController.loadMultiple(selected)
        } else {
            propertiesController.load(selected[0])
        }
    }

    function showActiveChecksums() {
        const ctrl = activePanelController()
        if (!ctrl) {
            return
        }
        const selected = ctrl.selectedPaths()
        if (selected && selected.length > 0) {
            showChecksums(selected)
        }
    }

    function quickLookActiveTarget() {
        const controller = activePanelController()
        const targetPath = previewTargetFor(controller)
        if (targetPath.length === 0) {
            return
        }

        const row = controller.directoryModel.indexOfPath(targetPath)
        const isDir = row >= 0 ? controller.directoryModel.isDirectoryAt(row) : false

        if (isDir) {
            propertiesController.load(targetPath)
            return
        }

        const previewableTypes = ["text", "image", "pdf", "svg", "font", "audio", "video", "executable"]
        if (root.previewPaneVisible) {
            if (previewableTypes.includes(quickLookController.type)) {
                quickLookPopup.previewPath = targetPath
                quickLookPopup.open()
            } else {
                propertiesController.load(targetPath)
            }
        } else {
            quickLookController.preview(targetPath)
            if (previewableTypes.includes(quickLookController.type)) {
                quickLookPopup.previewPath = targetPath
                quickLookPopup.open()
            } else {
                quickLookController.preview("")
                propertiesController.load(targetPath)
            }
        }
    }

    function commandPaletteCommands() {
        return [
            {
                id: "nav.goBack",
                title: "Go back",
                subtitle: "Return to the previous folder",
                shortcut: "Alt+Left",
                keywords: ["back", "history", "previous"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.goBackInActivePanel() }
            },
            {
                id: "nav.goForward",
                title: "Go forward",
                subtitle: "Move to the next folder in history",
                shortcut: "Alt+Right",
                keywords: ["forward", "history", "next"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.goForwardInActivePanel() }
            },
            {
                id: "nav.goUp",
                title: "Go up",
                subtitle: "Open the parent folder",
                shortcut: "Alt+Up",
                keywords: ["up", "parent", "folder"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.goUpInActivePanel() }
            },
            {
                id: "nav.focusPath",
                title: "Focus path bar",
                subtitle: "Edit the current path manually",
                shortcut: "Ctrl+L",
                keywords: ["path", "location", "address"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.focusActivePath() }
            },
            {
                id: "nav.focusSearch",
                title: "Focus search field",
                subtitle: "Filter the active panel",
                shortcut: "Ctrl+F",
                keywords: ["search", "filter", "find"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.focusActiveSearch() }
            },
            {
                id: "nav.focusSidebar",
                title: "Focus sidebar",
                subtitle: "Switch keyboard focus to places and folders",
                shortcut: "F9",
                keywords: ["sidebar", "places", "folders"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.focusActiveSidebar() }
            },
            {
                id: "nav.toggleSplit",
                title: "Toggle split view",
                subtitle: "Show or hide the second file panel",
                shortcut: "F3",
                keywords: ["split", "dual", "panels"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.toggleSplitView() }
            },
            {
                id: "view.togglePreview",
                title: "Toggle preview pane",
                subtitle: "Show or hide the quick preview panel",
                shortcut: "Ctrl+P",
                keywords: ["preview", "pane", "quicklook"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.togglePreviewPane() }
            },
            {
                id: "view.refresh",
                title: "Refresh active panel",
                subtitle: "Reload the current directory listing",
                shortcut: "F5",
                keywords: ["refresh", "reload", "update"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.refreshActivePanel() }
            },
            {
                id: "view.toggleHidden",
                title: "Toggle hidden files",
                subtitle: "Show or hide hidden entries",
                shortcut: "Ctrl+H",
                keywords: ["hidden", "visibility", "system"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.toggleHiddenFiles() }
            },
            {
                id: "view.neonCarbon",
                title: "Switch to Neon Carbon",
                subtitle: "Apply the default dark premium scheme",
                shortcut: "",
                keywords: ["theme", "appearance", "dark", "premium", "carbon"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setThemeScheme(0) }
            },
            {
                id: "view.auroraGlass",
                title: "Switch to Aurora Glass",
                subtitle: "Apply the colorful dark premium scheme",
                shortcut: "",
                keywords: ["theme", "appearance", "dark", "premium", "aurora"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setThemeScheme(1) }
            },
            {
                id: "view.porcelainSpectrum",
                title: "Switch to Porcelain Spectrum",
                subtitle: "Apply the premium light scheme",
                shortcut: "",
                keywords: ["theme", "appearance", "light", "premium", "porcelain"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setThemeScheme(2) }
            },
            {
                id: "view.emberLuxe",
                title: "Switch to Ember Luxe",
                subtitle: "Apply the warm dark premium scheme",
                shortcut: "",
                keywords: ["theme", "appearance", "dark", "premium", "ember"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setThemeScheme(3) }
            },
            {
                id: "theme.openSelector",
                title: "Open theme selector",
                subtitle: "Choose a built-in scheme or load a JSON theme",
                shortcut: "",
                keywords: ["theme", "appearance", "selector", "palette", "schemes"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.openThemeSelector() }
            },
            {
                id: "theme.import",
                title: "Import theme from file",
                subtitle: "Load a theme JSON from disk",
                shortcut: "",
                keywords: ["theme", "import", "json", "file", "load"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.importThemeFromFile() }
            },
            {
                id: "theme.export",
                title: "Export current theme",
                subtitle: "Save the active palette to JSON",
                shortcut: "",
                keywords: ["theme", "export", "json", "file", "save"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.exportCurrentTheme() }
            },
            {
                id: "view.details",
                title: "Set details view",
                subtitle: "Switch active panel to details mode",
                shortcut: "Ctrl+1",
                keywords: ["details", "table", "list"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setActiveViewMode(0) }
            },
            {
                id: "view.grid",
                title: "Set grid view",
                subtitle: "Switch active panel to grid mode",
                shortcut: "Ctrl+2",
                keywords: ["grid", "tiles", "icons"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setActiveViewMode(1) }
            },
            {
                id: "view.brief",
                title: "Set brief view",
                subtitle: "Switch active panel to brief mode",
                shortcut: "Ctrl+3",
                keywords: ["brief", "compact", "two-column"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.setActiveViewMode(2) }
            },
            {
                id: "file.rename",
                title: "Rename selection",
                subtitle: "Rename the focused item or batch rename multiple items",
                shortcut: "F2",
                keywords: ["rename", "batch", "edit"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.currentPath ? !ctrl.currentPath.toLowerCase().startsWith("archive://") : true
                },
                run: function() { root.renameActiveSelection() }
            },
            {
                id: "file.newFolder",
                title: "Create folder",
                subtitle: "Create a new folder in the active directory",
                shortcut: "F7",
                keywords: ["folder", "new", "create"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.currentPath ? !ctrl.currentPath.toLowerCase().startsWith("archive://") : true
                },
                run: function() { root.createFolderInActivePanel() }
            },
            {
                id: "file.copy",
                title: "Copy selection",
                subtitle: "Copy selected files to clipboard",
                shortcut: "Ctrl+C",
                keywords: ["copy", "clipboard"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
                },
                run: function() { root.copyActiveSelection() }
            },
            {
                id: "file.cut",
                title: "Cut selection",
                subtitle: "Cut selected files to clipboard",
                shortcut: "Ctrl+X",
                keywords: ["cut", "clipboard"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
                },
                run: function() { root.cutActiveSelection() }
            },
            {
                id: "file.paste",
                title: "Paste clipboard",
                subtitle: "Paste items into the active directory",
                shortcut: "Ctrl+V",
                keywords: ["paste", "clipboard"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.directoryModel && workspaceController.hasClipboard
                },
                run: function() { root.pasteClipboardToActivePanel() }
            },
            {
                id: "file.delete",
                title: "Delete selection",
                subtitle: "Move selected items to the delete flow",
                shortcut: "Delete",
                keywords: ["delete", "remove", "trash"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled || workspaceController.operationQueue.busy) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0 && !(ctrl.currentPath ? ctrl.currentPath.toLowerCase().startsWith("archive://") : false)
                },
                run: function() { root.requestDeleteActiveSelection() }
            },
            {
                id: "inspect.properties",
                title: "Show properties",
                subtitle: "Open the properties dialog for the selected items",
                shortcut: "Space",
                keywords: ["properties", "info", "details"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
                },
                run: function() { root.showActiveProperties() }
            },
            {
                id: "inspect.checksums",
                title: "Calculate checksums",
                subtitle: "Open the checksum dialog for selected items",
                shortcut: "",
                keywords: ["checksum", "hash", "compare"],
                enabled: function() {
                    if (!root.workspaceCommandsEnabled) return false
                    const ctrl = activePanelController()
                    return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
                },
                run: function() { root.showActiveChecksums() }
            },
            {
                id: "inspect.preview",
                title: "Preview current item",
                subtitle: "Open quick look or properties for the current target",
                shortcut: "Space",
                keywords: ["preview", "quicklook", "inspect"],
                enabled: function() { return root.workspaceCommandsEnabled },
                run: function() { root.quickLookActiveTarget() }
            },
            {
                id: "help.shortcuts",
                title: "Show keyboard help",
                subtitle: "Open the shortcuts reference",
                shortcut: "F1",
                keywords: ["help", "shortcuts", "reference"],
                enabled: function() { return !root.anyOverlayOpen },
                run: function() { helpDialog.open() }
            }
        ]
    }

    Shortcut {
        sequence: "F1"
        enabled: !root.anyOverlayOpen
        onActivated: helpDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+K"
        enabled: !root.anyOverlayOpen && !mainToolbar.textEditingActive && !fileWorkspace.isRenaming
        onActivated: root.openCommandPalette()
    }

    Shortcut {
        sequence: "Ctrl+Shift+P"
        enabled: !root.anyOverlayOpen && !mainToolbar.textEditingActive && !fileWorkspace.isRenaming
        onActivated: root.openCommandPalette()
    }

    Shortcut {
        sequence: "F3"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.toggleSplit()
    }

    Shortcut {
        sequence: "F9"
        enabled: !root.anyOverlayOpen && !mainToolbar.textEditingActive && !fileWorkspace.isRenaming
        onActivated: {
            if (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus) {
                sidebar.trapTabNavigation = false
                workspaceController.focusActivePanel()
            } else {
                sidebar.focusSidebar(true)
            }
        }
    }

    Shortcut {
        sequence: "F2"
        enabled: {
            if (!root.panelShortcutsEnabled) return false
            let activeCtrl = workspaceController.activePanel === 0 
                             ? workspaceController.leftPanel 
                             : workspaceController.rightPanel
            let isArchive = activeCtrl.currentPath ? activeCtrl.currentPath.toLowerCase().startsWith("archive://") : false
            return !isArchive
        }
        onActivated: {
            workspaceController.triggerRename()
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const controller = activePanelController()
            const targetPath = previewTargetFor(controller)
            if (targetPath.length > 0) {
                const row = controller.directoryModel.indexOfPath(targetPath)
                const isDir = row >= 0 ? controller.directoryModel.isDirectoryAt(row) : false
                
                if (isDir) {
                    propertiesController.load(targetPath)
                } else {
                    const previewableTypes = ["text", "image", "pdf", "svg", "font", "audio", "video", "executable"]
                    if (root.previewPaneVisible) {
                        if (previewableTypes.includes(quickLookController.type)) {
                            quickLookPopup.previewPath = targetPath
                            quickLookPopup.open()
                        } else {
                            propertiesController.load(targetPath)
                        }
                    } else {
                        quickLookController.preview(targetPath)
                        if (previewableTypes.includes(quickLookController.type)) {
                            quickLookPopup.previewPath = targetPath
                            quickLookPopup.open()
                        } else {
                            quickLookController.preview("")
                            propertiesController.load(targetPath)
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Delete"
        enabled: {
            if (!root.panelShortcutsEnabled
                || workspaceController.operationQueue.busy) {
                return false
            }
            let activeCtrl = workspaceController.activePanel === 0 
                             ? workspaceController.leftPanel 
                             : workspaceController.rightPanel
            let isArchive = activeCtrl.currentPath ? activeCtrl.currentPath.toLowerCase().startsWith("archive://") : false
            if (isArchive) {
                return false
            }
            return activeCtrl.directoryModel.selectedCount > 0
        }
        onActivated: {
            const active = workspaceController.activePanel === 0
                           ? workspaceController.leftPanel
                           : workspaceController.rightPanel
            workspaceController.requestDelete(active.selectedPaths(), active.currentPath)
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.panelShortcutsEnabled
                 && ((workspaceController.activePanel === 0
                      && workspaceController.leftPanel.directoryModel.selectedCount > 0)
                     || (workspaceController.activePanel === 1
                         && workspaceController.rightPanel.directoryModel.selectedCount > 0))
        onActivated: {
            const active = workspaceController.activePanel === 0
                           ? workspaceController.leftPanel
                           : workspaceController.rightPanel
            active.directoryModel.clearSelection()
            workspaceController.focusActivePanel()
        }
    }

    Shortcut {
        sequence: "Tab"
        enabled: root.tabPanelSwitchEnabled
        onActivated: {
            if (workspaceController.splitEnabled) {
                workspaceController.activePanel = workspaceController.activePanel === 0 ? 1 : 0
                workspaceController.focusActivePanel()
            }
        }
    }

    Shortcut {
        sequence: "Alt+Left"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.goBack()
                     : workspaceController.rightPanel.goBack()
    }

    Shortcut {
        sequence: "Alt+Right"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.goForward()
                     : workspaceController.rightPanel.goForward()
    }

    Shortcut {
        sequence: "Alt+Up"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.activePanel === 0
                     ? workspaceController.leftPanel.goUp()
                     : workspaceController.rightPanel.goUp()
    }

    Shortcut {
        sequence: "Ctrl+L"
        enabled: root.panelShortcutsEnabled
        onActivated: mainToolbar.focusPath()
    }

    Shortcut {
        sequence: "Ctrl+C"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.copyToClipboard()
    }

    Shortcut {
        sequence: "Ctrl+X"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.cutToClipboard()
    }

    Shortcut {
        sequence: "Ctrl+V"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.pasteFromClipboard()
    }

    Shortcut {
        sequence: "Ctrl+Z"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.undo()
    }

    Shortcut {
        sequence: "Ctrl+Y"
        enabled: root.panelShortcutsEnabled
        onActivated: workspaceController.redo()
    }

    Shortcut {
        sequence: "F5"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (workspaceController.splitEnabled && ctrl && ctrl.directoryModel.selectedCount > 0 && !workspaceController.operationQueue.busy) {
                workspaceController.copyActiveSelectionToOpposite()
            } else if (ctrl) {
                ctrl.refresh()
            }
        }
    }

    Shortcut {
        sequence: "F6"
        enabled: root.panelShortcutsEnabled && workspaceController.splitEnabled
                 && activePanelController() 
                 && activePanelController().directoryModel.selectedCount > 0
                 && !workspaceController.operationQueue.busy
        onActivated: {
            workspaceController.moveActiveSelectionToOpposite()
        }
    }

    Shortcut {
        sequence: "Ctrl+R"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.refresh()
        }
    }

    Shortcut {
        sequence: "Ctrl+H"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) {
                const newValue = !ctrl.directoryModel.showHidden
                ctrl.directoryModel.showHidden = newValue
                workspaceController.treeModel.showHidden = newValue
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+1"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.viewMode = 0
        }
    }

    Shortcut {
        sequence: "Ctrl+2"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.viewMode = 1
        }
    }

    Shortcut {
        sequence: "Ctrl+3"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.viewMode = 2
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+N"
        enabled: {
            if (!root.panelShortcutsEnabled) return false
            const ctrl = activePanelController()
            return ctrl && ctrl.currentPath ? !ctrl.currentPath.toLowerCase().startsWith("archive://") : true
        }
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.createFolder("New Folder")
        }
    }

    Shortcut {
        sequence: "F7"
        enabled: {
            if (!root.panelShortcutsEnabled) return false
            const ctrl = activePanelController()
            return ctrl && ctrl.currentPath ? !ctrl.currentPath.toLowerCase().startsWith("archive://") : true
        }
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl) ctrl.createFolder("New Folder")
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        enabled: root.panelShortcutsEnabled
        onActivated: mainToolbar.focusSearch()
    }

    Shortcut {
        sequence: "Ctrl+P"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const visible = !root.previewPaneVisible
            root.previewPaneVisible = visible
            quickLookController.visible = visible
            if (visible) {
                syncPreviewFromActivePanel(true)
            } else {
                quickLookController.preview("")
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+A"
        enabled: root.panelShortcutsEnabled
        onActivated: {
            const ctrl = activePanelController()
            if (ctrl && ctrl.directoryModel) {
                ctrl.directoryModel.selectAll()
            }
        }
    }

    Shortcut {
        sequence: "Alt+D"
        enabled: root.panelShortcutsEnabled
        onActivated: mainToolbar.focusPath()
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.text.length > 0 && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.ShiftModifier)) {
                // Ignore Space, Enter/Return as they are handled by shortcuts or specific components
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                    return;

                if (root.typeToSearchEnabled) {
                     mainToolbar.focusSearch()
                }
            }
        }

        MainToolbar {
            id: mainToolbar
            Layout.fillWidth: true
            previewVisible: root.previewPaneVisible
            onPreviewToggleRequested: (visible) => {
                root.previewPaneVisible = visible
                quickLookController.visible = visible
                if (visible) {
                    syncPreviewFromActivePanel(true)
                }
                else {
                    quickLookController.preview("")
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Sidebar {
                id: sidebar
                SplitView.preferredWidth: 200
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 300
            }

            FileWorkspace {
                id: fileWorkspace
                SplitView.fillWidth: true
            }

            PreviewPane {
                SplitView.preferredWidth: root.previewPaneVisible ? 340 : 0
                SplitView.minimumWidth: root.previewPaneVisible ? 280 : 0
                SplitView.fillWidth: false
                visible: root.previewPaneVisible || width > 0
                opacity: root.previewPaneVisible ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
            }

            handle: Rectangle {
                implicitWidth: 1
                color: Theme.border
            }
        }
    }

    ConflictDialog {
        id: conflictDialog
    }

    HelpDialog {
        id: helpDialog
    }

    PropertiesDialog {
        id: propertiesDialog
    }

    QuickLook {
        id: quickLookPopup
    }

    DeleteConfirmDialog {
        id: deleteConfirmDialog
    }

    CommandPalette {
        id: commandPalette
        commands: root.commandPaletteCommands()
    }

    Connections {
        target: workspaceController.operationQueue
        function onConflictDetected(source, destination, sourceSize, sourceModified, destSize, destModified) {
            conflictDialog.sourcePath = source
            conflictDialog.destinationPath = destination
            conflictDialog.sourceSize = sourceSize
            conflictDialog.sourceModified = sourceModified
            conflictDialog.destSize = destSize
            conflictDialog.destModified = destModified
            conflictDialog.open()
        }
    }

    Connections {
        target: workspaceController
        function onDeleteRequested(paths, label) {
            openDeleteConfirm(paths, label)
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onRevealProperties(paths) {
            propertiesController.loadMultiple(paths)
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onRevealProperties(paths) {
            propertiesController.loadMultiple(paths)
        }
    }

    Connections {
        target: workspaceController
        function onActivePanelChanged() {
            if (root.previewPaneVisible) {
                syncPreviewFromActivePanel(true)
            }
            workspaceController.treeModel.showHidden = activePanelController().directoryModel.showHidden
        }
    }

    Connections {
        target: quickLookController
        function onVisibleChanged() {
            if (root.previewPaneVisible !== quickLookController.visible) {
                root.previewPaneVisible = quickLookController.visible
            }
            if (!quickLookController.visible) {
                previewSyncTimer.stop()
                hoverPreviewTimer.stop()
                quickLookController.preview("")
            }
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onHoveredPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0 && root.previewOnHover) {
                if (workspaceController.leftPanel.hoveredPath.length > 0) {
                    scheduleHoverPreview(workspaceController.leftPanel.hoveredPath)
                } else {
                    hoverPreviewTimer.stop()
                }
            }
        }
        function onCurrentPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0) {
                syncPreviewFromActivePanel(!workspaceController.leftPanel.scrolling)
            }
        }
        function onScrollingChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0
                    && workspaceController.leftPanel.scrolling) {
                previewSyncTimer.stop()
                hoverPreviewTimer.stop()
            }
            if (root.previewPaneVisible && workspaceController.activePanel === 0
                    && !workspaceController.leftPanel.scrolling) {
                syncPreviewFromActivePanel(false)
            }
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onHoveredPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1 && root.previewOnHover) {
                if (workspaceController.rightPanel.hoveredPath.length > 0) {
                    scheduleHoverPreview(workspaceController.rightPanel.hoveredPath)
                } else {
                    hoverPreviewTimer.stop()
                }
            }
        }
        function onCurrentPathChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1) {
                syncPreviewFromActivePanel(!workspaceController.rightPanel.scrolling)
            }
        }
        function onScrollingChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1
                    && workspaceController.rightPanel.scrolling) {
                previewSyncTimer.stop()
                hoverPreviewTimer.stop()
            }
            if (root.previewPaneVisible && workspaceController.activePanel === 1
                    && !workspaceController.rightPanel.scrolling) {
                syncPreviewFromActivePanel(false)
            }
        }
    }

    function showBatchRename(paths) {
        if (!paths || paths.length === 0) return;
        batchRenameDialog.sourcePaths = paths
        batchRenameDialog.controller = workspaceController.activePanel === 0 
                                       ? workspaceController.leftPanel 
                                       : workspaceController.rightPanel
        batchRenameDialog.open()
    }

    BatchRenameDialog {
        id: batchRenameDialog
    }

    function showChecksums(paths) {
        if (!paths || paths.length === 0) return;
        checksumDialog.path1 = paths[0]
        checksumDialog.path2 = paths.length > 1 ? paths[1] : ""
        checksumDialog.controller = workspaceController.activePanel === 0 
                                     ? workspaceController.leftPanel 
                                     : workspaceController.rightPanel
        checksumDialog.open()
    }

    ChecksumDialog {
        id: checksumDialog
    }

    Connections {
        target: workspaceController.leftPanel.directoryModel
        function onSelectionChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 0) {
                syncPreviewFromActivePanel(!workspaceController.leftPanel.scrolling)
            }
        }
    }

    Connections {
        target: workspaceController.rightPanel.directoryModel
        function onSelectionChanged() {
            if (root.previewPaneVisible && workspaceController.activePanel === 1) {
                syncPreviewFromActivePanel(!workspaceController.rightPanel.scrolling)
            }
        }
    }
}

