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
    property var copyActiveSelectionToOpposite
    property var moveActiveSelectionToOpposite
    property var duplicateActiveSelection
    property var compressActiveSelection
    property var cutActiveSelection
    property var pasteClipboardToActivePanel
    property var addSelectionToFavorites
    property var requestDeleteActiveSelection
    property var showActiveProperties
    property var showActiveChecksums
    property var quickLookActiveTarget
    property var openHelpDialog
    property var openSettingsDialog
    property var openPluginManagerDialog
    property var openThemeEditorDialog
    property var openSettingsImportDialog
    property var openSettingsExportDialog
    property var openSettingsDataFolder
    property var openDiskUsage
    property var openFileSearch
    property var resetSavedWorkspaceState
    property var resetCommandUsageStats
    property var relaunchAsAdmin
    property var quitApplication
    property var copyPropertiesToClipboard
    property var exportPropertiesToFile
    property var navigateActivePanel

    function explicitScheme(path) {
        const value = String(path || "").trim()
        const index = value.indexOf("://")
        if (index <= 0) return ""
        const scheme = value.substring(0, index).toLowerCase()
        if (scheme.length === 0 || !/[a-z]/.test(scheme.charAt(0))) return ""
        for (let i = 0; i < scheme.length; ++i) {
            const ch = scheme.charAt(i)
            if (!/[a-z0-9+.-]/.test(ch)) return ""
        }
        return scheme
    }

    function hasExplicitNonLocalScheme(path) {
        const scheme = explicitScheme(path)
        return scheme.length > 0 && scheme !== "file"
    }

    function isProviderPath(path) {
        const scheme = explicitScheme(path)
        return scheme.length > 0
            && scheme !== "file"
            && scheme !== "archive"
            && scheme !== "devices"
            && scheme !== "favorites"
    }

    function pathCanShowProperties(path) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        return value.length > 0
            && !hasExplicitNonLocalScheme(value)
            && lower !== "devices://"
            && lower !== "favorites://"
    }

    function pathCanBeFavorited(path) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        return value.length > 0
            && !hasExplicitNonLocalScheme(value)
            && lower !== "devices://"
            && lower !== "favorites://"
    }

    function panelPathIsDirectory(ctrl, path) {
        if (!ctrl || !ctrl.directoryModel || !path) return false
        const row = ctrl.directoryModel.indexOfPath(path)
        return row >= 0 && ctrl.directoryModel.isDirectoryAt(row)
    }

    function canAnalyzePanelPath(ctrl) {
        if (!ctrl || !ctrl.currentPath || ctrl.currentPath.length === 0 || ctrl.isVirtualRoot) return false
        if (String(ctrl.currentPath).toLowerCase().startsWith("archive://")) return false
        if (hasExplicitNonLocalScheme(ctrl.currentPath)) return false
        return typeof diskUsageController !== "undefined" && diskUsageController
    }

    function canSearchPanelPath(ctrl) {
        if (!ctrl || !ctrl.currentPath || ctrl.currentPath.length === 0 || ctrl.isVirtualRoot) return false
        const path = String(ctrl.currentPath).toLowerCase()
        return !path.startsWith("archive://")
            && !path.startsWith("devices://")
            && !path.startsWith("favorites://")
            && !hasExplicitNonLocalScheme(ctrl.currentPath)
    }

    function canCreateManualItem(ctrl) {
        return Boolean(ctrl
                       && !isProviderPath(ctrl.currentPath)
                       && ctrl.canCreateInCurrentPath)
    }

    function oppositePanelFor(ctrl) {
        if (!root.workspaceController || !root.workspaceController.splitEnabled || !ctrl) return null
        if (root.workspaceController.leftPanel === ctrl) return root.workspaceController.rightPanel
        if (root.workspaceController.rightPanel === ctrl) return root.workspaceController.leftPanel
        return root.workspaceController.activePanel === 0
            ? root.workspaceController.rightPanel
            : root.workspaceController.leftPanel
    }

    function canCopySelectionToOpposite(ctrl) {
        const destination = oppositePanelFor(ctrl)
        return Boolean(root.workspaceController
                       && !root.workspaceController.operationQueue.busy
                       && ctrl
                       && destination
                       && ctrl.directoryModel
                       && ctrl.directoryModel.selectedCount > 0
                       && !ctrl.isVirtualRoot
                       && ctrl.canCopySelection
                       && !destination.isVirtualRoot
                       && destination.canCreateInCurrentPath)
    }

    function canMoveSelectionToOpposite(ctrl) {
        const destination = oppositePanelFor(ctrl)
        return canCopySelectionToOpposite(ctrl)
               && ctrl
               && destination
               && !isProviderPath(ctrl.currentPath)
               && !isProviderPath(destination.currentPath)
               && ctrl.canDeleteSelection
    }

    function canShowPropertiesSelection(ctrl) {
        if (!ctrl || !ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return false
        const selected = ctrl.selectedPaths ? ctrl.selectedPaths() : []
        if (!selected || selected.length === 0) return false
        for (let i = 0; i < selected.length; ++i) {
            if (!pathCanShowProperties(selected[i])) {
                return false
            }
        }
        return true
    }

    function canPinPanelSelection(ctrl) {
        if (!ctrl || !ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0 || ctrl.isVirtualRoot) return false
        const selected = ctrl.selectedPaths ? ctrl.selectedPaths() : []
        if (!selected || selected.length === 0) return false
        for (let i = 0; i < selected.length; ++i) {
            if (!pathCanBeFavorited(selected[i])) {
                return false
            }
        }
        return true
    }

    readonly property var commands: [
        {
            id: "nav.goBack",
            title: "Go back",
            subtitle: "Return to the previous folder",
            category: "Navigation",
            shortcut: "Alt+Left",
            keywords: ["back", "history", "previous"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.goBackInActivePanel) root.goBackInActivePanel() }
        },
        {
            id: "nav.goForward",
            title: "Go forward",
            subtitle: "Move to the next folder in history",
            category: "Navigation",
            shortcut: "Alt+Right",
            keywords: ["forward", "history", "next"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.goForwardInActivePanel) root.goForwardInActivePanel() }
        },
        {
            id: "nav.goUp",
            title: "Go up",
            subtitle: "Open the parent folder",
            category: "Navigation",
            shortcut: "Alt+Up",
            keywords: ["up", "parent", "folder"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.goUpInActivePanel) root.goUpInActivePanel() }
        },
        {
            id: "nav.focusPath",
            title: "Focus path bar",
            subtitle: "Edit the current path manually",
            category: "Navigation",
            shortcut: "Ctrl+L",
            keywords: ["path", "location", "address"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.focusActivePath) root.focusActivePath() }
        },
        {
            id: "nav.focusSearch",
            title: "Focus search field",
            subtitle: "Quick-search the active panel by name",
            category: "Navigation",
            shortcut: "Ctrl+F",
            keywords: ["search", "find", "name"],
            enabled: function() {
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.workspaceCommandsEnabled && ctrl && !ctrl.isFavoritesRoot
            },
            run: function() { if (root.focusActiveSearch) root.focusActiveSearch() }
        },
        {
            id: "nav.focusSidebar",
            title: "Focus sidebar",
            subtitle: "Switch keyboard focus to places and folders",
            category: "Navigation",
            shortcut: "F9",
            keywords: ["sidebar", "places", "folders"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.focusActiveSidebar) root.focusActiveSidebar() }
        },
        {
            id: "nav.goToPath",
            title: "Go to path",
            subtitle: "Navigate the active panel to a specific folder",
            category: "Navigation",
            shortcut: "Ctrl+G",
            keywords: ["go", "path", "folder", "navigate", "open", "directory"],
            aliases: ["goto", "open folder", "cd"],
            acceptsArgument: true,
            suggestionKind: "path",
            argumentLabel: "Folder path (e.g. C:\\Users or C:/Users)...",
            enabled: function() { return root.workspaceCommandsEnabled },
            validateArgument: function(arg) {
                const path = arg ? arg.trim() : ""
                if (path.length === 0) return "Enter a folder path."
                if (typeof root.activePanelController !== "function") return "No active panel is available."
                const ctrl = root.activePanelController()
                if (!ctrl) return "No active panel is available."
                return ""
            },
            runWithArgument: function(arg) {
                const path = arg.trim()
                if (path.length > 0 && root.navigateActivePanel) {
                    const opened = root.navigateActivePanel(path)
                    if (opened && root.workspaceController && root.workspaceController.focusActivePanel) {
                        Qt.callLater(() => root.workspaceController.focusActivePanel())
                    }
                }
            },
            run: function() {}
        },
        {
            id: "nav.openFavorites",
            title: "Open Favorites",
            subtitle: "Open pinned paths, frequent folders, and tags",
            category: "Navigation",
            shortcut: "",
            keywords: ["favorites", "fav", "bookmarks", "pinned", "tags"],
            aliases: ["favorites", "fav", "bookmarks", "pinned"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() {
                if (root.navigateActivePanel) {
                    root.navigateActivePanel("favorites://")
                }
            }
        },
        {
            id: "favorites.addSelection",
            title: "Pin selection to Favorites",
            subtitle: "Pin selected files or folders",
            category: "Navigation",
            shortcut: "",
            keywords: ["favorites", "pin", "bookmark", "selection", "add"],
            aliases: ["pin selection", "bookmark selection", "add favorite"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canPinPanelSelection(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (ctrl.isVirtualRoot) return "Favorites cannot pin virtual locations"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!root.canPinPanelSelection(ctrl)) return "This location cannot be pinned"
                return ""
            },
            run: function() {
                if (root.addSelectionToFavorites) {
                    root.addSelectionToFavorites()
                }
            }
        },
        {
            id: "nav.toggleSplit",
            title: "Toggle split view",
            subtitle: "Show or hide the second file panel",
            category: "Navigation",
            shortcut: "F3",
            keywords: ["split", "dual", "panels"],
            aliases: ["two panels", "split layout", "dual view"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.toggleSplitView) root.toggleSplitView() }
        },
        {
            id: "nav.mirrorActivePanelToOtherPanel",
            title: "Mirror active panel",
            subtitle: "Copy the active panel path, view, sort, and filters to the other panel",
            category: "Navigation",
            shortcut: "F4",
            keywords: ["split", "dual", "panel", "mirror", "folder", "view"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.mirrorActivePanelToOpposite) root.mirrorActivePanelToOpposite() }
        },
        {
            id: "view.togglePreview",
            title: "Toggle preview pane",
            subtitle: "Show or hide the quick preview panel",
            category: "View",
            shortcut: "Ctrl+P",
            keywords: ["preview", "pane", "quicklook"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.togglePreviewPane) root.togglePreviewPane() }
        },
        {
            id: "view.refresh",
            title: "Refresh active panel",
            subtitle: "Reload the current directory listing",
            category: "View",
            shortcut: "Ctrl+R",
            keywords: ["refresh", "reload", "update"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.refreshActivePanel) root.refreshActivePanel() }
        },
        {
            id: "view.toggleHidden",
            title: "Toggle hidden files",
            subtitle: "Show or hide hidden entries",
            category: "View",
            shortcut: "Ctrl+H",
            keywords: ["hidden", "visibility", "system"],
            aliases: ["show hidden", "system files", "dotfiles"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.toggleHiddenFiles) root.toggleHiddenFiles() }
        },
        {
            id: "view.catppuccinLatte",
            title: "Switch to Catppuccin Latte",
            subtitle: "Apply the soft light Catppuccin scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "light", "catppuccin", "latte"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(0) }
        },
        {
            id: "view.auroraGlass",
            title: "Switch to Aurora Glass",
            subtitle: "Apply the colorful dark premium scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "dark", "premium", "aurora"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(1) }
        },
        {
            id: "view.porcelainBloom",
            title: "Switch to Porcelain Bloom",
            subtitle: "Apply the bright white-and-rose light scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "light", "white", "red", "rose", "porcelain", "bloom", "coral"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(2) }
        },
        {
            id: "view.emberLuxe",
            title: "Switch to Ember Luxe",
            subtitle: "Apply the warm dark premium scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "dark", "premium", "ember"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(3) }
        },
        {
            id: "view.graphiteSage",
            title: "Switch to Graphite Sage",
            subtitle: "Apply the graphite, sage, and brass scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "dark", "graphite", "sage", "brass"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(4) }
        },
        {
            id: "view.velvetExcess",
            title: "Switch to Velvet Excess",
            subtitle: "Apply the velvet, orchid, and gold scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "dark", "velvet", "excess", "orchid", "gold"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.setThemeScheme) root.setThemeScheme(5) }
        },
        {
            id: "theme.openSelector",
            title: "Open theme selector",
            subtitle: "Choose the active color scheme",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "selector", "palette", "schemes"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openThemeSelector) root.openThemeSelector() }
        },
        {
            id: "theme.switch",
            title: "Switch theme by name",
            subtitle: "Catppuccin Latte / Aurora Glass / Porcelain Bloom / Ember Luxe / Graphite Sage / Velvet Excess",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "appearance", "scheme", "switch", "change", "catppuccin", "aurora", "porcelain", "bloom", "rose", "ember", "graphite", "sage", "velvet", "excess"],
            aliases: ["set theme", "change theme", "apply theme"],
            acceptsArgument: true,
            argumentLabel: "Theme name (e.g. Porcelain Bloom, Aurora Glass)...",
            enabled: function() { return root.workspaceCommandsEnabled },
            getSuggestions: function(input) {
                const builtins = [
                    { title: "Catppuccin Latte", value: "Catppuccin Latte", subtitle: "Built-in soft light scheme", previewColor: "#EFF1F5" },
                    { title: "Aurora Glass", value: "Aurora Glass", subtitle: "Built-in dark glass scheme", previewColor: "#08111F" },
                    { title: "Porcelain Bloom", value: "Porcelain Bloom", subtitle: "Built-in white-and-rose light scheme", previewColor: "#FAFBFA" },
                    { title: "Ember Luxe", value: "Ember Luxe", subtitle: "Built-in dark luxury scheme", previewColor: "#100C0A" },
                    { title: "Graphite Sage", value: "Graphite Sage", subtitle: "Built-in graphite, sage, and brass scheme", previewColor: "#111715" },
                    { title: "Velvet Excess", value: "Velvet Excess", subtitle: "Built-in velvet, orchid, and gold scheme", previewColor: "#160817" }
                ]
                let list = builtins
                if (typeof themeController !== "undefined" && themeController) {
                    try {
                        const customs = themeController.availableCustomThemes()
                        for (let i = 0; i < customs.length; ++i) {
                            const c = customs[i]
                            list.push({
                                title: c.name || c.fileName,
                                value: "custom:" + c.filePath,
                                subtitle: "Custom theme (" + c.fileName + " - " + (c.mode || "dark") + ")",
                                previewColor: c.colors && c.colors.accent ? c.colors.accent : "#2DD4BF"
                            })
                        }
                    } catch (e) {
                        console.log("Error loading custom themes: " + e)
                    }
                }
                const term = input.toLowerCase().trim()
                if (term.length === 0) return list
                const filtered = []
                for (let j = 0; j < list.length; ++j) {
                    const item = list[j]
                    if (item.title.toLowerCase().indexOf(term) >= 0 || 
                        item.subtitle.toLowerCase().indexOf(term) >= 0) {
                        filtered.push(item)
                    }
                }
                return filtered
            },
            runWithArgument: function(arg) {
                if (arg.startsWith("custom:")) {
                    if (typeof themeController !== "undefined" && themeController) {
                        const filePath = arg.substring(7)
                        themeController.loadThemeFromFile(filePath)
                    }
                    return
                }
                if (!root.setThemeScheme) return
                const name = arg.trim().toLowerCase()
                if (name.indexOf("latte") >= 0 || name.indexOf("catppuccin") >= 0) {
                    root.setThemeScheme(0)
                } else if (name.indexOf("aurora") >= 0 || name.indexOf("glass") >= 0) {
                    root.setThemeScheme(1)
                } else if (name.indexOf("porcelain") >= 0 || name.indexOf("bloom") >= 0 || name.indexOf("rose") >= 0) {
                    root.setThemeScheme(2)
                } else if (name.indexOf("ember") >= 0 || name.indexOf("luxe") >= 0) {
                    root.setThemeScheme(3)
                } else if (name.indexOf("graphite") >= 0 || name.indexOf("sage") >= 0) {
                    root.setThemeScheme(4)
                } else if (name.indexOf("velvet") >= 0 || name.indexOf("excess") >= 0) {
                    root.setThemeScheme(5)
                }
            },
            run: function() {}
        },
        {
            id: "view.details",
            title: "Set details view",
            subtitle: "Switch active panel to details mode",
            category: "View",
            shortcut: "Ctrl+1",
            keywords: ["details", "table", "list"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.activePanelController) root.activePanelController().viewMode = 0 }
        },
        {
            id: "view.grid",
            title: "Set grid view",
            subtitle: "Switch active panel to grid mode",
            category: "View",
            shortcut: "Ctrl+2",
            keywords: ["grid", "tiles", "icons"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.activePanelController) root.activePanelController().viewMode = 1 }
        },
        {
            id: "view.brief",
            title: "Set brief view",
            subtitle: "Switch active panel to brief mode",
            category: "View",
            shortcut: "Ctrl+3",
            keywords: ["brief", "compact", "two-column"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.activePanelController) root.activePanelController().viewMode = 2 }
        },
        {
            id: "file.rename",
            title: "Rename selection",
            subtitle: "Rename the focused item or batch rename multiple items",
            category: "File",
            shortcut: "F2",
            keywords: ["rename", "batch", "edit"],
            aliases: ["change name", "rename files"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && !root.isProviderPath(ctrl.currentPath) && ctrl.canRenameSelection
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (root.isProviderPath(ctrl.currentPath)) return "Rename is unavailable for this provider"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!ctrl.canRenameSelection) return "Cannot rename current selection"
                return ""
            },
            run: function() { if (root.renameActiveSelection) root.renameActiveSelection() }
        },
        {
            id: "file.newItem",
            title: "Create new item",
            subtitle: "Choose a folder, file, or text file to create",
            category: "File",
            keywords: ["folder", "file", "text", "new", "create"],
            aliases: ["new folder", "new file", "new text file"],
            acceptsArgument: true,
            argumentLabel: "Choose item type...",
            suggestions: [
                { title: "New Folder", subtitle: "Create a new folder in the active directory", value: "New Folder", category: "File" },
                { title: "New File", subtitle: "Create a file without an extension", value: "New File", category: "File" },
                { title: "New Text File", subtitle: "Create a .txt file", value: "New Text File", category: "File" }
            ],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canCreateManualItem(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (root.isProviderPath(ctrl.currentPath)) return "Manual item creation is unavailable for this provider"
                if (!ctrl.canCreateInCurrentPath) return "Current folder is read-only"
                return ""
            },
            validateArgument: function(value) {
                if (value === "New Folder" || value === "New File" || value === "New Text File") {
                    return ""
                }
                return "Choose New Folder, New File, or New Text File"
            },
            runWithArgument: function(value) {
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!root.canCreateManualItem(ctrl)) {
                    return
                }
                if (value === "New Folder") {
                    ctrl.createFolder("New Folder")
                } else if (value === "New File") {
                    ctrl.createFile("New File")
                } else if (value === "New Text File") {
                    ctrl.createFile("New Text File.txt")
                }
            }
        },
        {
            id: "file.copy",
            title: "Copy selection",
            subtitle: "Copy selected files to clipboard",
            category: "File",
            shortcut: "Ctrl+C",
            keywords: ["copy", "clipboard"],
            aliases: ["copy files", "clipboard copy"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.directoryModel && ctrl.directoryModel.selectedCount > 0 && ctrl.canCopySelection
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!ctrl.canCopySelection) return "Selected items cannot be copied"
                return ""
            },
            run: function() { if (root.copyActiveSelection) root.copyActiveSelection() }
        },
        {
            id: "file.copyToOtherPanel",
            title: "Copy selection to other panel",
            subtitle: "Copy selected items to the opposite panel",
            category: "File",
            shortcut: "F5",
            keywords: ["copy", "panel", "other panel", "opposite panel"],
            aliases: ["copy to panel", "copy to other panel", "copy opposite"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled || !root.workspaceController || root.workspaceController.operationQueue.busy) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return canCopySelectionToOpposite(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                if (!root.workspaceController) return "No workspace"
                if (root.workspaceController.operationQueue.busy) return "Operation queue is busy"
                if (!root.workspaceController.splitEnabled) return "Split view is disabled"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!ctrl.canCopySelection) return "Selected items cannot be copied"
                const destination = oppositePanelFor(ctrl)
                if (!destination || destination.isVirtualRoot) return "No writable opposite panel"
                if (!destination.canCreateInCurrentPath) return "Cannot write to the opposite panel"
                return ""
            },
            run: function() { if (root.copyActiveSelectionToOpposite) root.copyActiveSelectionToOpposite() }
        },
        {
            id: "file.moveToOtherPanel",
            title: "Move selection to other panel",
            subtitle: "Move selected items to the opposite panel",
            category: "File",
            shortcut: "Shift+F5",
            keywords: ["move", "panel", "other panel", "opposite panel"],
            aliases: ["move to panel", "move to other panel", "move opposite"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled || !root.workspaceController || root.workspaceController.operationQueue.busy) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return canMoveSelectionToOpposite(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                if (!root.workspaceController) return "No workspace"
                if (root.workspaceController.operationQueue.busy) return "Operation queue is busy"
                if (!root.workspaceController.splitEnabled) return "Split view is disabled"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                const destination = oppositePanelFor(ctrl)
                if (!destination || destination.isVirtualRoot) return "No writable opposite panel"
                if (!destination.canCreateInCurrentPath) return "Cannot write to the opposite panel"
                if (!ctrl.canDeleteSelection) return "Cannot move current selection"
                return ""
            },
            run: function() { if (root.moveActiveSelectionToOpposite) root.moveActiveSelectionToOpposite() }
        },
        {
            id: "file.duplicate",
            title: "Duplicate selection",
            subtitle: "Copy selected items in the current folder",
            category: "File",
            keywords: ["duplicate", "copy here", "copy in place"],
            aliases: ["clone", "copy current file"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled || !root.workspaceController || root.workspaceController.operationQueue.busy) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && !root.isProviderPath(ctrl.currentPath) && ctrl.canDuplicateSelection
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                if (root.workspaceController && root.workspaceController.operationQueue.busy) return "Operation queue is busy"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (root.isProviderPath(ctrl.currentPath)) return "Duplicate is unavailable for this provider"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!ctrl.canDuplicateSelection) return "Select one writable file"
                return ""
            },
            run: function() { if (root.duplicateActiveSelection) root.duplicateActiveSelection() }
        },
        {
            id: "file.compress7z",
            title: "Compress as archive",
            subtitle: "Create a 7z, zip, gzip, bzip2, or xz archive in the current folder",
            category: "File",
            shortcut: "",
            keywords: ["compress", "archive", "7zip", "7z", "zip", "pack"],
            aliases: ["compress selection", "create 7z", "archive selection"],
            acceptsArgument: true,
            argumentLabel: "Archive format...",
            suggestions: [
                { title: "7zip (.7z)", value: "7z", subtitle: "Default 7-Zip archive", previewColor: "#8B5CF6" },
                { title: "ZIP (.zip)", value: "zip", subtitle: "Portable zip archive", previewColor: "#3B82F6" },
                { title: "GZip (.gz)", value: "gz", subtitle: "Single-file gzip stream", previewColor: "#14B8A6" },
                { title: "BZip2 (.bz2)", value: "bz2", subtitle: "Single-file bzip2 stream", previewColor: "#F59E0B" },
                { title: "XZ (.xz)", value: "xz", subtitle: "Single-file xz stream", previewColor: "#22C55E" }
            ],
            enabled: function() {
                if (!root.workspaceCommandsEnabled || !root.workspaceController || root.workspaceController.operationQueue.busy) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && !root.isProviderPath(ctrl.currentPath) && ctrl.canCompressSelection
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                if (root.workspaceController && root.workspaceController.operationQueue.busy) return "Operation queue is busy"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (root.isProviderPath(ctrl.currentPath)) return "Archive creation is unavailable for this provider"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!ctrl.canCreateInCurrentPath) return "Cannot write to current folder"
                if (!ctrl.canCompressSelection) return "7-Zip is unavailable or this selection cannot be compressed"
                return ""
            },
            validateArgument: function(arg) {
                const value = (arg || "").trim().toLowerCase()
                if (value.length === 0) return ""
                if (value === "tar.gz" || value === "tgz") return "tar.gz requires a two-step tar + gzip flow."
                if (["7z", "7zip", "7-zip", "zip", "gz", "gzip", "bz2", "bzip2", "xz", "zx"].indexOf(value) < 0) {
                    return "Choose 7zip, zip, gzip, bzip2, or xz."
                }
                if (["gz", "gzip", "bz2", "bzip2", "xz", "zx"].indexOf(value) >= 0) {
                    const ctrl = root.activePanelController ? root.activePanelController() : null
                    if (!ctrl || !ctrl.directoryModel || ctrl.directoryModel.selectedCount !== 1) {
                        return "This format supports one selected file only."
                    }
                    const selected = ctrl.selectedPaths()
                    if (!selected || selected.length !== 1) return "Select one file."
                    const idx = ctrl.directoryModel.indexOfPath(selected[0])
                    if (idx < 0 || ctrl.directoryModel.isDirectoryAt(idx)) return "This format supports files, not folders."
                }
                return ""
            },
            runWithArgument: function(arg) {
                if (root.compressActiveSelection) {
                    root.compressActiveSelection((arg || "7z").trim())
                }
            },
            run: function() { if (root.compressActiveSelection) root.compressActiveSelection("7z") }
        },
        {
            id: "file.cut",
            title: "Cut selection",
            subtitle: "Cut selected files to clipboard",
            category: "File",
            shortcut: "Ctrl+X",
            keywords: ["cut", "clipboard"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && !root.isProviderPath(ctrl.currentPath) && ctrl.canDeleteSelection
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (root.isProviderPath(ctrl.currentPath)) return "Cut is unavailable for this provider"
                if (!ctrl.canDeleteSelection) return "Cannot cut current selection"
                return ""
            },
            run: function() { if (root.cutActiveSelection) root.cutActiveSelection() }
        },
        {
            id: "file.paste",
            title: "Paste clipboard",
            subtitle: "Paste items into the active directory",
            category: "File",
            shortcut: "Ctrl+V",
            keywords: ["paste", "clipboard"],
            aliases: ["insert", "paste files", "clipboard paste"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.directoryModel && root.workspaceController && root.workspaceController.hasClipboard
                    && ctrl.canPasteIntoCurrentPath
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!root.workspaceController || !root.workspaceController.hasClipboard) return "Clipboard is empty"
                if (!ctrl.canPasteIntoCurrentPath) return "Cannot paste into current folder"
                return ""
            },
            run: function() { if (root.pasteClipboardToActivePanel) root.pasteClipboardToActivePanel() }
        },
        {
            id: "file.delete",
            title: "Delete selection",
            subtitle: "Move selected items to the delete flow",
            category: "File",
            shortcut: "Delete",
            keywords: ["delete", "remove", "trash"],
            aliases: ["remove", "trash", "erase"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled || !root.workspaceController || root.workspaceController.operationQueue.busy) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return ctrl && ctrl.canDeleteSelection
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                if (root.workspaceController && root.workspaceController.operationQueue.busy) return "Operation queue is busy"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.canDeleteSelection) return "Cannot delete current selection"
                return ""
            },
            run: function() { if (root.requestDeleteActiveSelection) root.requestDeleteActiveSelection() }
        },
        {
            id: "inspect.properties",
            title: "Show properties",
            subtitle: "Open the properties dialog for the selected items",
            category: "Inspect",
            shortcut: "Space",
            keywords: ["properties", "info", "details"],
            aliases: ["info", "size", "permissions"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canShowPropertiesSelection(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!root.canShowPropertiesSelection(ctrl)) return "Properties are available for local files only"
                return ""
            },
            run: function() { if (root.showActiveProperties) root.showActiveProperties() }
        },
        {
            id: "inspect.properties.hashes",
            title: "Calculate / view hashes (Checksums)",
            subtitle: "Open the properties dialog directly to the Hashes tab",
            category: "Inspect",
            shortcut: "",
            keywords: ["properties", "checksum", "hashes", "md5", "sha1", "sha256"],
            aliases: ["md5", "sha256", "checksum", "hash file"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl || !ctrl.directoryModel || ctrl.directoryModel.selectedCount !== 1) return false
                const selected = ctrl.selectedPaths()
                if (!selected || selected.length !== 1) return false
                if (hasExplicitNonLocalScheme(selected[0])) return false
                return !root.panelPathIsDirectory(ctrl, selected[0])
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                const selected = ctrl.selectedPaths()
                if (!selected || selected.length === 0) return "No items selected"
                if (selected.length > 1) return "Select exactly one file"
                if (root.hasExplicitNonLocalScheme(selected[0])) return "Checksums require a local file"
                if (root.panelPathIsDirectory(ctrl, selected[0])) {
                    return "Hashes are not supported for folders"
                }
                return ""
            },
            run: function() { if (root.showActiveProperties) root.showActiveProperties(3) }
        },
        {
            id: "inspect.properties.security",
            title: "Show access properties (Security)",
            subtitle: "Open the properties dialog directly to the Access/Security tab",
            category: "Inspect",
            shortcut: "",
            keywords: ["properties", "access", "security", "permissions", "attributes"],
            aliases: ["permissions", "owner", "access permissions"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canShowPropertiesSelection(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!root.canShowPropertiesSelection(ctrl)) return "Properties are available for local files only"
                return ""
            },
            run: function() { if (root.showActiveProperties) root.showActiveProperties(2) }
        },
        {
            id: "inspect.copyProperties",
            title: "Copy all properties",
            subtitle: "Copy all current item properties to clipboard",
            category: "Inspect",
            shortcut: "",
            keywords: ["properties", "copy", "clipboard"],
            enabled: function() {
                if (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible) {
                    return true
                }
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canShowPropertiesSelection(ctrl)
            },
            disabledReason: function() {
                if (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible) {
                    return ""
                }
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!root.canShowPropertiesSelection(ctrl)) return "Properties are available for local files only"
                return ""
            },
            run: function() { if (root.copyPropertiesToClipboard) root.copyPropertiesToClipboard() }
        },
        {
            id: "inspect.exportProperties",
            title: "Export properties to file",
            subtitle: "Save properties of the selected items to a Text or JSON file",
            category: "Inspect",
            shortcut: "",
            keywords: ["properties", "export", "file", "save", "text", "json"],
            aliases: ["export properties", "save properties", "properties file"],
            acceptsArgument: true,
            argumentLabel: "Export format (Text or JSON)...",
            suggestions: [
                { title: "JSON Format (.json)", value: "json", subtitle: "Structured machine-readable data", previewColor: "#2DD4BF" },
                { title: "Text Format (.txt)", value: "txt", subtitle: "Plain text layout of all properties", previewColor: "#A3E635" }
            ],
            enabled: function() {
                if (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible) {
                    return true
                }
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canShowPropertiesSelection(ctrl)
            },
            disabledReason: function() {
                if (typeof propertiesController !== "undefined" && propertiesController && propertiesController.visible) {
                    return ""
                }
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel || ctrl.directoryModel.selectedCount === 0) return "No items selected"
                if (!root.canShowPropertiesSelection(ctrl)) return "Properties are available for local files only"
                return ""
            },
            runWithArgument: function(arg) {
                if (root.exportPropertiesToFile) {
                    const fmt = (arg && arg.toLowerCase().indexOf("txt") >= 0) ? "txt" : "json"
                    root.exportPropertiesToFile(fmt)
                }
            },
            run: function() { if (root.exportPropertiesToFile) root.exportPropertiesToFile("json") }
        },
        {
            id: "inspect.checksums",
            title: "Compare file checksums (select 2 files)",
            subtitle: "Compare hashes for exactly two selected files",
            category: "Inspect",
            shortcut: "",
            keywords: ["checksum", "hash", "compare", "diff", "verify"],
            aliases: ["compare hashes", "verify files", "checksum compare"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl || !ctrl.directoryModel) return false
                const count = ctrl.directoryModel.selectedCount
                if (count !== 2) return false
                const paths = ctrl.selectedPaths()
                if (paths.length !== count) return false
                for (let i = 0; i < count; i++) {
                    if (root.hasExplicitNonLocalScheme(paths[i])) return false
                    const idx = ctrl.directoryModel.indexOfPath(paths[i])
                    if (idx < 0 || ctrl.directoryModel.isDirectoryAt(idx)) return false
                }
                return true
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!ctrl.directoryModel) return "No directory model"
                const count = ctrl.directoryModel.selectedCount
                if (count === 0) return "No items selected"
                if (count === 1) return "Use Properties > Hashes for a single file"
                if (count !== 2) return "Select exactly two files to compare checksums"
                const paths = ctrl.selectedPaths()
                if (paths.length !== count) return "Invalid selection state"
                for (let i = 0; i < count; i++) {
                    if (root.hasExplicitNonLocalScheme(paths[i])) return "Checksums require local files"
                    const idx = ctrl.directoryModel.indexOfPath(paths[i])
                    if (idx < 0) return "File not found"
                    if (ctrl.directoryModel.isDirectoryAt(idx)) return "Directories cannot have checksums computed"
                }
                return ""
            },
            run: function() { if (root.showActiveChecksums) root.showActiveChecksums() }
        },
        {
            id: "inspect.preview",
            title: "Preview current item",
            subtitle: "Open quick look or properties for the current target",
            category: "Inspect",
            shortcut: "Space",
            keywords: ["preview", "quicklook", "inspect"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.quickLookActiveTarget) root.quickLookActiveTarget() }
        },
        {
            id: "tools.diskUsage",
            title: "Analyze disk usage",
            subtitle: "Find the largest files and folders in the current location",
            category: "Tools",
            shortcut: "",
            keywords: ["disk", "usage", "space", "size", "storage", "largest", "folders", "files"],
            aliases: ["space usage", "folder sizes", "what uses space"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canAnalyzePanelPath(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (typeof diskUsageController === "undefined" || !diskUsageController) return "Disk usage analyzer is unavailable"
                if (!root.canAnalyzePanelPath(ctrl)) return "Current location cannot be analyzed"
                return ""
            },
            run: function() {
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (ctrl && root.openDiskUsage) {
                    root.openDiskUsage(ctrl.currentPath)
                }
            }
        },
        {
            id: "tools.fileSearch",
            title: "Search files in current folder",
            subtitle: "Find files and folders under the active panel path",
            category: "Tools",
            shortcut: "Ctrl+Shift+F",
            keywords: ["search", "find", "files", "folders", "recursive", "contents"],
            aliases: ["global search", "file search", "find files", "recursive search", "search contents"],
            enabled: function() {
                if (!root.workspaceCommandsEnabled) return false
                const ctrl = root.activePanelController ? root.activePanelController() : null
                return root.canSearchPanelPath(ctrl)
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                const ctrl = root.activePanelController ? root.activePanelController() : null
                if (!ctrl) return "No active panel"
                if (!root.canSearchPanelPath(ctrl)) return "Current location cannot be searched"
                return ""
            },
            run: function() { if (root.openFileSearch) root.openFileSearch() }
        },
        {
            id: "settings.open",
            title: "Open settings",
            subtitle: "Adjust workspace, appearance, and maintenance options",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "preferences", "workspace", "persistence"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsDialog) root.openSettingsDialog() }
        },
        {
            id: "settings.fonts",
            title: "Font settings",
            subtitle: "Adjust the app font family and text scale",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "fonts", "typography", "readability", "scale", "size"],
            aliases: ["font size", "text size", "ui scale", "readability"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsDialog) root.openSettingsDialog() }
        },
        {
            id: "settings.plugins",
            title: "Open Plugin Manager",
            subtitle: "View, load, and disable loaded plugins",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "plugins", "extensions", "providers", "ftp", "mock", "load"],
            aliases: ["plugins", "extensions", "plugin manager", "manage plugins"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openPluginManagerDialog) root.openPluginManagerDialog() }
        },
        {
            id: "theme.editor",
            title: "Open Theme Editor",
            subtitle: "Create or adjust a custom theme draft",
            category: "Theme",
            shortcut: "",
            keywords: ["theme", "editor", "palette", "draft", "colors", "appearance"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openThemeEditorDialog) root.openThemeEditorDialog() }
        },
        {
            id: "settings.resetWorkspace",
            title: "Reset saved workspace",
            subtitle: "Clear saved layout and folder state for the next launch",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "workspace", "reset", "session", "layout"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.resetSavedWorkspaceState) root.resetSavedWorkspaceState() }
        },
        {
            id: "settings.resetCommandHistory",
            title: "Reset command palette history",
            subtitle: "Clear recent and frequent command ranking data",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "palette", "command", "history", "recent", "usage", "reset"],
            aliases: ["reset palette", "clear command history", "clear recent commands"],
            enabled: function() { return root.workspaceCommandsEnabled },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                return ""
            },
            run: function() { if (root.resetCommandUsageStats) root.resetCommandUsageStats() }
        },
        {
            id: "settings.export",
            title: "Export settings",
            subtitle: "Save workspace, panels, theme, and app preferences to JSON",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "export", "json", "backup", "save"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsExportDialog) root.openSettingsExportDialog() }
        },
        {
            id: "settings.import",
            title: "Import settings",
            subtitle: "Restore workspace, panels, theme, and app preferences from JSON",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "import", "json", "restore", "load"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsImportDialog) root.openSettingsImportDialog() }
        },
        {
            id: "settings.openDataFolder",
            title: "Open app data folder",
            subtitle: "Reveal the FM app data location in Explorer",
            category: "Settings",
            shortcut: "",
            keywords: ["settings", "data", "folder", "appdata", "storage", "open"],
            enabled: function() { return root.workspaceCommandsEnabled },
            run: function() { if (root.openSettingsDataFolder) root.openSettingsDataFolder() }
        },
        {
            id: "app.rerunAsAdmin",
            title: "Rerun as administrator",
            subtitle: "Restart FM with elevated privileges",
            category: "Admin",
            shortcut: "",
            keywords: ["admin", "administrator", "elevate", "elevation", "runas", "privileges", "uac"],
            enabled: function() {
                return root.workspaceCommandsEnabled
                    && typeof adminController !== "undefined"
                    && adminController
                    && adminController.canRelaunchAsAdmin
                    && !adminController.isElevated
            },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                if (typeof adminController !== "undefined" && adminController && !adminController.canRelaunchAsAdmin) return "Administrator relaunch is not supported on this platform"
                if (typeof adminController !== "undefined" && adminController && adminController.isElevated) return "Already running as administrator"
                return ""
            },
            run: function() { if (root.relaunchAsAdmin) root.relaunchAsAdmin() }
        },
        {
            id: "app.quit",
            title: "Quit",
            subtitle: "Exit FM",
            category: "App",
            shortcut: "",
            keywords: ["quit", "exit", "close", "app", "application"],
            aliases: ["exit app", "close app", "close window"],
            enabled: function() { return true },
            run: function() { if (root.quitApplication) root.quitApplication() }
        },
        {
            id: "help.shortcuts",
            title: "Show keyboard help",
            category: "Help",
            shortcut: "F1",
            keywords: ["help", "shortcuts", "reference"],
            enabled: function() { return root.workspaceCommandsEnabled },
            disabledReason: function() {
                if (!root.workspaceCommandsEnabled) return "Overlays are open"
                return ""
            },
            run: function() { if (root.openHelpDialog) root.openHelpDialog() }
        }
    ]
}
