import QtQuick

QtObject {
    id: root

    property bool appActive: false
    property bool appVisible: false
    property int logicalActivePanel: 0
    property bool anyOverlayOpen: false
    property bool workspaceOverlayOpen: false
    property bool commandPaletteOpen: false
    property bool quickLookOpen: false
    property bool sidebarFocused: false
    property bool sidebarTrapTabNavigation: false
    property bool pathEditorActive: false
    property bool quickSearchActive: false
    property bool renameEditorActive: false
    property bool splitEnabled: false
    property bool operationBusy: false
    property bool activePanelValid: false
    property bool activePanelFavoritesRoot: false
    property bool activePanelProviderPath: false
    property bool activePanelCanDeleteSelection: false
    property bool activePanelCanRenameSelection: false
    property bool activePanelCanPasteIntoCurrentPath: false
    property int activePanelSelectedCount: 0
    property bool logEnabled: false

    readonly property bool editorActive: root.pathEditorActive
                                         || root.quickSearchActive
                                         || root.renameEditorActive
    readonly property string currentContext: {
        if (root.commandPaletteOpen) return "commandPalette"
        if (root.quickLookOpen) return "quickLook"
        if (root.workspaceOverlayOpen) return "workspaceOverlay"
        if (root.renameEditorActive) return "renameEditor"
        if (root.pathEditorActive) return "pathEditor"
        if (root.quickSearchActive) return "quickSearch"
        if (root.sidebarFocused) return "sidebar"
        if (!root.appVisible) return "startup"
        return "panel"
    }

    readonly property bool canUsePanelShortcuts: root.appVisible
                                                 && !root.anyOverlayOpen
                                                 && !root.editorActive
    readonly property bool canUseFileViewShortcuts: root.canUsePanelShortcuts
                                                    && !root.sidebarFocused
    readonly property bool canSwitchPanel: root.canUsePanelShortcuts
                                           && (!root.sidebarFocused || !root.sidebarTrapTabNavigation)
    readonly property bool canTypeToSearch: root.canUseFileViewShortcuts
                                            && root.activePanelValid
                                            && !root.activePanelFavoritesRoot
    readonly property bool canTransferToOpposite: root.canUsePanelShortcuts
                                                  && root.splitEnabled
                                                  && !root.operationBusy
                                                  && root.activePanelSelectedCount > 0

    function blockReason(actionName) {
        if (!root.appVisible) return "app-hidden"
        if (root.commandPaletteOpen) return "command-palette"
        if (root.quickLookOpen) return "quick-look"
        if (root.workspaceOverlayOpen) return "workspace-overlay"
        if (root.renameEditorActive) return "rename-editor"
        if (root.pathEditorActive) return "path-editor"
        if (root.quickSearchActive) return "quick-search"

        switch (actionName) {
        case "switchPanel":
            if (root.sidebarFocused && root.sidebarTrapTabNavigation) return "sidebar-traps-tab"
            break
        case "typeToSearch":
        case "focusQuickSearch":
            if (root.sidebarFocused) return "sidebar"
            if (!root.activePanelValid) return "no-active-panel"
            if (root.activePanelFavoritesRoot) return "favorites-root"
            break
        case "renameSelection":
            if (root.sidebarFocused) return "sidebar"
            if (!root.activePanelValid) return "no-active-panel"
            if (root.activePanelProviderPath) return "provider-path"
            if (!root.activePanelCanRenameSelection) return "rename-unavailable"
            break
        case "copySelection":
        case "selectAll":
        case "invertSelection":
        case "previewSelection":
            if (root.sidebarFocused) return "sidebar"
            if (!root.activePanelValid) return "no-active-panel"
            break
        case "cutSelection":
            if (root.sidebarFocused) return "sidebar"
            if (!root.activePanelValid) return "no-active-panel"
            if (root.activePanelProviderPath) return "provider-path"
            if (!root.activePanelCanDeleteSelection) return "cut-unavailable"
            break
        case "deleteSelection":
            if (root.sidebarFocused) return "sidebar"
            if (!root.activePanelValid) return "no-active-panel"
            if (!root.activePanelCanDeleteSelection) return "delete-unavailable"
            if (root.operationBusy) return "operation-busy"
            break
        case "pasteIntoPanel":
            if (!root.activePanelValid) return "no-active-panel"
            if (!root.activePanelCanPasteIntoCurrentPath) return "paste-unavailable"
            break
        case "copyToOpposite":
        case "moveToOpposite":
            if (!root.splitEnabled) return "split-disabled"
            if (root.operationBusy) return "operation-busy"
            if (root.activePanelSelectedCount <= 0) return "empty-selection"
            break
        default:
            break
        }

        if (!root.canUsePanelShortcuts) return "panel-shortcuts-disabled"
        return ""
    }

    function canRun(actionName) {
        switch (actionName) {
        case "openHelp":
            return root.appVisible && root.appActive && !root.anyOverlayOpen
        case "openCommandPalette":
            return root.appVisible && root.appActive && !root.editorActive && !root.renameEditorActive
        case "switchPanel":
            return root.canSwitchPanel
        case "toggleSplit":
        case "mirrorPanel":
        case "navigatePanel":
        case "focusPathEditor":
        case "closeOrCancel":
        case "undoPanel":
        case "redoPanel":
        case "refreshPanel":
        case "toggleHiddenFiles":
        case "setViewMode":
        case "createFolder":
        case "openFileSearch":
        case "togglePreviewPane":
            return root.canUsePanelShortcuts
        case "typeToSearch":
        case "focusQuickSearch":
            return root.canTypeToSearch
        case "renameSelection":
            return root.canUseFileViewShortcuts
                && root.activePanelValid
                && !root.activePanelProviderPath
                && root.activePanelCanRenameSelection
        case "copySelection":
        case "selectAll":
        case "invertSelection":
        case "previewSelection":
            return root.canUseFileViewShortcuts
                && root.activePanelValid
        case "cutSelection":
            return root.canUseFileViewShortcuts
                && root.activePanelValid
                && !root.activePanelProviderPath
                && root.activePanelCanDeleteSelection
        case "deleteSelection":
            return root.canUseFileViewShortcuts
                && root.activePanelValid
                && root.activePanelCanDeleteSelection
                && !root.operationBusy
        case "pasteIntoPanel":
            return root.canUsePanelShortcuts
                && root.activePanelValid
                && root.activePanelCanPasteIntoCurrentPath
        case "copyToOpposite":
        case "moveToOpposite":
            return root.canTransferToOpposite
        default:
            return root.canUsePanelShortcuts
        }
    }

    function traceDecision(actionName) {
        if (!root.logEnabled) {
            return
        }
        const allowed = root.canRun(actionName)
        console.log("[InputRouting]",
                    "action=" + actionName,
                    "context=" + root.currentContext,
                    "activePanel=" + root.logicalActivePanel,
                    "overlay=" + root.anyOverlayOpen,
                    "editor=" + root.editorActive,
                    "allowed=" + allowed,
                    "reason=" + (allowed ? "allowed" : root.blockReason(actionName)))
    }

}
