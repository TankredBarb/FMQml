import QtQml

QtObject {
    id: root

    property bool workspaceCommandsEnabled: false
    property bool anyOverlayOpen: false
    property var workspaceController
    property var activePanelController

    property var goBackInActivePanel
    property var goForwardInActivePanel
    property var goUpInActivePanel
    property var focusActivePath
    property var focusActiveSearch
    property var focusActiveSidebar
    property var toggleSplitView
    property var mirrorActivePanelToOpposite
    property var togglePreviewPane
    property var refreshActivePanel
    property var toggleHiddenFiles
    property var setThemeScheme
    property var openThemeSelector
    property var createFolderInActivePanel
    property var renameActiveSelection
    property var copyActiveSelection
    property var cutActiveSelection
    property var pasteClipboardToActivePanel
    property var requestDeleteActiveSelection
    property var showActiveProperties
    property var showActiveChecksums
    property var quickLookActiveTarget
    property var openHelpDialog
    property var openSettingsDialog
    property var openThemeEditorDialog
    property var openSettingsImportDialog
    property var openSettingsExportDialog
    property var openSettingsDataFolder
    property var resetSavedWorkspaceState
    property var relaunchAsAdmin

    function isReadOnlyContainerPath(path) {
        if (!path) return false
        if (path.toLowerCase().startsWith("archive://")) return true
        return root.workspaceController && root.workspaceController.isInsideManagedIsoMount(path)
    }

    readonly property var commands: [
        {
            id: "nav.goBack",
            title: "Go back",
            subtitle: "Return to the previous folder",
            shortcut: "Alt+Left",
            keywords: ["back", "history", "previous"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.goBackInActivePanel) root.goBackInActivePanel() }
        },
        {
            id: "nav.goForward",
            title: "Go forward",
            subtitle: "Move to the next folder in history",
            shortcut: "Alt+Right",
            keywords: ["forward", "history", "next"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.goForwardInActivePanel) root.goForwardInActivePanel() }
        },
        {
            id: "nav.goUp",
            title: "Go up",
            subtitle: "Open the parent folder",
            shortcut: "Alt+Up",
            keywords: ["up", "parent", "folder"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.goUpInActivePanel) root.goUpInActivePanel() }
        },
        {
            id: "nav.focusPath",
            title: "Focus path bar",
            subtitle: "Edit the current path manually",
            shortcut: "Ctrl+L",
            keywords: ["path", "location", "address"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.focusActivePath) root.focusActivePath() }
        },
        {
            id: "nav.focusSearch",
            title: "Focus search field",
            subtitle: "Quick-search the active panel by name",
            shortcut: "Ctrl+F",
            keywords: ["search", "find", "name"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.focusActiveSearch) root.focusActiveSearch() }
        },
        {
            id: "nav.focusSidebar",
            title: "Focus sidebar",
            subtitle: "Switch keyboard focus to places and folders",
            shortcut: "F9",
            keywords: ["sidebar", "places", "folders"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.focusActiveSidebar) root.focusActiveSidebar() }
        },
        {
            id: "nav.toggleSplit",
            title: "Toggle split view",
            subtitle: "Show or hide the second file panel",
            shortcut: "F3",
            keywords: ["split", "dual", "panels"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.toggleSplitView) root.toggleSplitView() }
        },
        {
            id: "nav.mirrorActivePanelToOtherPanel",
            title: "Mirror active panel",
            subtitle: "Copy the active panel path, view, sort, and filters to the other panel",
            shortcut: "F4",
            keywords: ["split", "dual", "panel", "mirror", "folder", "view"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.mirrorActivePanelToOpposite) root.mirrorActivePanelToOpposite() }
        },
        {
            id: "view.togglePreview",
            title: "Toggle preview pane",
            subtitle: "Show or hide the quick preview panel",
            shortcut: "Ctrl+P",
            keywords: ["preview", "pane", "quicklook"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.togglePreviewPane) root.togglePreviewPane() }
        },
        {
            id: "view.refresh",
            title: "Refresh active panel",
            subtitle: "Reload the current directory listing",
            shortcut: "F5",
            keywords: ["refresh", "reload", "update"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.refreshActivePanel) root.refreshActivePanel() }
        },
        {
            id: "view.toggleHidden",
            title: "Toggle hidden files",
            subtitle: "Show or hide hidden entries",
            shortcut: "Ctrl+H",
            keywords: ["hidden", "visibility", "system"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.toggleHiddenFiles) root.toggleHiddenFiles() }
        },
        {
            id: "view.catppuccinLatte",
            title: "Switch to Catppuccin Latte",
            subtitle: "Apply the soft light Catppuccin scheme",
            shortcut: "",
            keywords: ["theme", "appearance", "light", "catppuccin", "latte"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(0) }
        },
        {
            id: "view.auroraGlass",
            title: "Switch to Aurora Glass",
            subtitle: "Apply the colorful dark premium scheme",
            shortcut: "",
            keywords: ["theme", "appearance", "dark", "premium", "aurora"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(1) }
        },
        {
            id: "view.oxideGarden",
            title: "Switch to Oxide Garden",
            subtitle: "Apply the earthy paper-and-patina scheme",
            shortcut: "",
            keywords: ["theme", "appearance", "light", "earth", "oxide", "garden", "patina"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(2) }
        },
        {
            id: "view.emberLuxe",
            title: "Switch to Ember Luxe",
            subtitle: "Apply the warm dark premium scheme",
            shortcut: "",
            keywords: ["theme", "appearance", "dark", "premium", "ember"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(3) }
        },
        {
            id: "theme.openSelector",
            title: "Open theme selector",
            subtitle: "Choose the active color scheme",
            shortcut: "",
            keywords: ["theme", "appearance", "selector", "palette", "schemes"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openThemeSelector) root.openThemeSelector() }
        },
        {
            id: "view.details",
            title: "Set details view",
            subtitle: "Switch active panel to details mode",
            shortcut: "Ctrl+1",
            keywords: ["details", "table", "list"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.activePanelController) root.activePanelController().viewMode = 0 }
        },
        {
            id: "view.grid",
            title: "Set grid view",
            subtitle: "Switch active panel to grid mode",
            shortcut: "Ctrl+2",
            keywords: ["grid", "tiles", "icons"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.activePanelController) root.activePanelController().viewMode = 1 }
        },
        {
            id: "view.brief",
            title: "Set brief view",
            subtitle: "Switch active panel to brief mode",
            shortcut: "Ctrl+3",
            keywords: ["brief", "compact", "two-column"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.activePanelController) root.activePanelController().viewMode = 2 }
        },
        {
            id: "file.rename",
            title: "Rename selection",
            subtitle: "Rename the focused item or batch rename multiple items",
            shortcut: "F2",
            keywords: ["rename", "batch", "edit"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.canRenameSelection
            },
            run: function() { if (root.renameActiveSelection) root.renameActiveSelection() }
        },
        {
            id: "file.newFolder",
            title: "Create folder",
            subtitle: "Create a new folder in the active directory",
            shortcut: "F7",
            keywords: ["folder", "new", "create"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.canCreateInCurrentPath
            },
            run: function() { if (root.createFolderInActivePanel) root.createFolderInActivePanel() }
        },
        {
            id: "file.copy",
            title: "Copy selection",
            subtitle: "Copy selected files to clipboard",
            shortcut: "Ctrl+C",
            keywords: ["copy", "clipboard"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
            },
            run: function() { if (root.copyActiveSelection) root.copyActiveSelection() }
        },
        {
            id: "file.cut",
            title: "Cut selection",
            subtitle: "Cut selected files to clipboard",
            shortcut: "Ctrl+X",
            keywords: ["cut", "clipboard"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.canDeleteSelection
            },
            run: function() { if (root.cutActiveSelection) root.cutActiveSelection() }
        },
        {
            id: "file.paste",
            title: "Paste clipboard",
            subtitle: "Paste items into the active directory",
            shortcut: "Ctrl+V",
            keywords: ["paste", "clipboard"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.directoryModel && root.workspaceController && root.workspaceController.hasClipboard
                    && ctrl.canPasteIntoCurrentPath
            },
            run: function() { if (root.pasteClipboardToActivePanel) root.pasteClipboardToActivePanel() }
        },
        {
            id: "file.delete",
            title: "Delete selection",
            subtitle: "Move selected items to the delete flow",
            shortcut: "Delete",
            keywords: ["delete", "remove", "trash"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled || !root.workspaceController || root.workspaceController.operationQueue.busy) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.canDeleteSelection
            },
            run: function() { if (root.requestDeleteActiveSelection) root.requestDeleteActiveSelection() }
        },
        {
            id: "inspect.properties",
            title: "Show properties",
            subtitle: "Open the properties dialog for the selected items",
            shortcut: "Space",
            keywords: ["properties", "info", "details"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
            },
            run: function() { if (root.showActiveProperties) root.showActiveProperties() }
        },
        {
            id: "inspect.checksums",
            title: "Calculate checksums",
            subtitle: "Open the checksum dialog for selected items",
            shortcut: "",
            keywords: ["checksum", "hash", "compare"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0
            },
            run: function() { if (root.showActiveChecksums) root.showActiveChecksums() }
        },
        {
            id: "inspect.preview",
            title: "Preview current item",
            subtitle: "Open quick look or properties for the current target",
            shortcut: "Space",
            keywords: ["preview", "quicklook", "inspect"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.quickLookActiveTarget) root.quickLookActiveTarget() }
        },
        {
            id: "settings.open",
            title: "Open settings",
            subtitle: "Adjust workspace, appearance, and maintenance options",
            shortcut: "",
            keywords: ["settings", "preferences", "workspace", "persistence"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsDialog) root.openSettingsDialog() }
        },
        {
            id: "theme.editor",
            title: "Open Theme Editor",
            subtitle: "Create or adjust a custom theme draft",
            shortcut: "",
            keywords: ["theme", "editor", "palette", "draft", "colors", "appearance"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openThemeEditorDialog) root.openThemeEditorDialog() }
        },
        {
            id: "settings.resetWorkspace",
            title: "Reset saved workspace",
            subtitle: "Clear saved layout and folder state for the next launch",
            shortcut: "",
            keywords: ["settings", "workspace", "reset", "session", "layout"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.resetSavedWorkspaceState) root.resetSavedWorkspaceState() }
        },
        {
            id: "settings.export",
            title: "Export settings",
            subtitle: "Save workspace, panels, theme, and app preferences to JSON",
            shortcut: "",
            keywords: ["settings", "export", "json", "backup", "save"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsExportDialog) root.openSettingsExportDialog() }
        },
        {
            id: "settings.import",
            title: "Import settings",
            subtitle: "Restore workspace, panels, theme, and app preferences from JSON",
            shortcut: "",
            keywords: ["settings", "import", "json", "restore", "load"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsImportDialog) root.openSettingsImportDialog() }
        },
        {
            id: "settings.openDataFolder",
            title: "Open app data folder",
            subtitle: "Reveal the FM app data location in Explorer",
            shortcut: "",
            keywords: ["settings", "data", "folder", "appdata", "storage", "open"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsDataFolder) root.openSettingsDataFolder() }
        },
        {
            id: "app.rerunAsAdmin",
            title: "Rerun as administrator",
            subtitle: "Restart FM with elevated privileges",
            shortcut: "",
            keywords: ["admin", "administrator", "elevate", "elevation", "runas", "privileges", "uac"],
            enabled: function() {
                return root.workspaceCommandsEnabled
                    && typeof adminController !== "undefined"
                    && adminController
                    && !adminController.isElevated
            },
            run: function() { if (root.relaunchAsAdmin) root.relaunchAsAdmin() }
        },
        {
            id: "help.shortcuts",
            title: "Show keyboard help",
            subtitle: "Open the shortcuts reference",
            shortcut: "F1",
            keywords: ["help", "shortcuts", "reference"],
            enabled: function() { return !root.anyOverlayOpen },
            run: function() { if (root.openHelpDialog) root.openHelpDialog() }
        }
    ]
}
