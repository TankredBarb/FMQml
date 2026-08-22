import QtQuick
import QtQml
import ".."
import "../framework"
import "../../style"

Item {
    id: root

    property var controller
    property var workspaceController
    property var propertiesController
    property var favoritesController
    property var windowObject
    property bool isCurrentPathArchive: false
    property bool isCurrentPathReadOnlyContainer: false
    property bool showActionBar: true
    property bool showSelectionBadges: true
    property bool showMediaHoverPreviews: false
    property bool showFolderHoverPreviews: false
    property bool folderPeekEnabled: false
    property var customActions: []

    signal selectAllRequested()
    signal menuOpenChanged(bool open)
    signal actionBarVisibilityRequested(bool visible)
    signal selectionBadgesVisibilityRequested(bool visible)
    signal mediaHoverPreviewsVisibilityRequested(bool visible)
    signal folderHoverPreviewsVisibilityRequested(bool visible)
    signal folderPeekEnabledRequested(bool enabled)

    FilePanelMenuPolicy {
        id: menuPolicy
        controller: root.controller
        workspaceController: root.workspaceController
        favoritesController: root.favoritesController
    }

    function popupEmptyMenu() {
        root.customActions = root.availableCustomActions()
        emptyContextMenu.popup()
    }

    function adminModeActive() {
        return Qt.platform.os === "linux"
                && typeof adminController !== "undefined"
                && adminController
                && adminController.adminModeActive
    }

    function currentFolderPinned() {
        return menuPolicy.currentFolderPinned()
    }

    function canFavoriteCurrentFolder() {
        return menuPolicy.canFavoriteCurrentFolder()
    }

    function customActionContext() {
        return {
            scope: "folder",
            currentPath: root.controller ? root.controller.currentPath : "",
            targetPath: root.controller ? root.controller.currentPath : "",
            targetIsDirectory: true,
            selectedPaths: root.controller && root.controller.selectedPaths ? root.controller.selectedPaths() : []
        }
    }

    function availableCustomActions() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            return []
        }
        return pluginActionController.actionsForContext(root.customActionContext())
    }

    function triggerCustomAction(actionId) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            return
        }
        const result = pluginActionController.triggerAction(actionId, root.customActionContext())
        if (result && result.ok === true && result.signedOutProviderPrefix && root.workspaceController) {
            const providerPrefix = String(result.signedOutProviderPrefix)
            const panels = [root.workspaceController.leftPanel, root.workspaceController.rightPanel]
            for (let i = 0; i < panels.length; ++i) {
                const panel = panels[i]
                if (panel && String(panel.currentPath || "").startsWith(providerPrefix)) {
                    panel.openPath("devices://")
                }
            }
        }
        if (result && result.ok === true && result.refreshCurrentPath === true && root.controller) {
            root.controller.refresh()
        }
        if (root.windowObject && root.windowObject.openPluginActionResult) {
            root.windowObject.openPluginActionResult(result)
        }
    }

    FmMenu {
        id: emptyContextMenu
        onOpened: root.menuOpenChanged(true)
        onClosed: root.menuOpenChanged(false)
        FmMenuItem {
            text: Qt.platform.os === "windows" ? "Open in PowerShell" : "Open in Terminal"
            icon.source: "../assets/icons-classic/terminal.svg"
            iconColor: Theme.actionIconColor("terminal")
            visible: menuPolicy.canOpenTerminal()
            enabled: visible
            onTriggered: root.controller.openInTerminal()
        }
        FmMenuSeparator {
            visible: menuPolicy.canOpenTerminal()
        }
        FmMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons-classic/folder-plus.svg"
            iconColor: Theme.actionIconColor("create")
            visible: menuPolicy.canCreateInCurrentPath()
            enabled: visible
            onTriggered: root.controller.createFolder("New Folder")
        }
        FmMenuItem {
            text: "New Folder as Administrator"
            icon.source: "../assets/icons-classic/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: root.adminModeActive()
                     && root.controller
                     && !root.controller.isVirtualRoot
                     && !menuPolicy.currentPathIsProvider()
            enabled: visible
            onTriggered: {
                if (root.windowObject && root.windowObject.createFolderInActivePanelAsAdministrator) {
                    root.windowObject.createFolderInActivePanelAsAdministrator()
                } else if (root.controller && root.controller.createFolderAsAdministrator) {
                    root.controller.createFolderAsAdministrator("New Folder")
                }
            }
        }
        FmMenuItem {
            text: "New Text File"
            icon.source: "../assets/icons-classic/text-file.svg"
            iconColor: Theme.actionIconColor("text-file")
            visible: menuPolicy.canCreateInCurrentPath()
            enabled: visible
            onTriggered: root.controller.createFile("New Text File.txt")
        }
        FmMenuItem {
            text: "New File as Administrator"
            icon.source: "../assets/icons-classic/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: root.adminModeActive()
                     && root.controller
                     && !root.controller.isVirtualRoot
                     && !menuPolicy.currentPathIsProvider()
            enabled: visible
            onTriggered: {
                if (root.windowObject && root.windowObject.createFileInActivePanelAsAdministrator) {
                    root.windowObject.createFileInActivePanelAsAdministrator("New File")
                } else if (root.controller && root.controller.createFileAsAdministrator) {
                    root.controller.createFileAsAdministrator("New File")
                }
            }
        }
        FmMenuItem {
            text: "New File"
            icon.source: "../assets/icons-classic/file-plus.svg"
            iconColor: Theme.actionIconColor("document")
            visible: menuPolicy.canCreateInCurrentPath()
            enabled: visible
            onTriggered: root.controller.createFile("New File")
        }
        FmMenuSeparator {
            visible: menuPolicy.canCreateInCurrentPath()
        }
        FmMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons-classic/paste.svg"
            iconColor: Theme.actionIconColor("paste")
            enabled: menuPolicy.canPasteFromClipboard()
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard()
        }
        FmMenuItem {
            text: "Paste as Administrator"
            icon.source: "../assets/icons-classic/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: root.adminModeActive()
                     && root.controller
                     && !root.controller.isVirtualRoot
                     && !menuPolicy.currentPathIsProvider()
            enabled: visible
                     && root.workspaceController
                     && root.workspaceController.hasClipboard
                     && !root.workspaceController.clipboardCut
            onTriggered: {
                if (root.windowObject && root.windowObject.pasteClipboardToActivePanelAsAdministrator) {
                    root.windowObject.pasteClipboardToActivePanelAsAdministrator()
                } else if (root.workspaceController) {
                    root.workspaceController.pasteFromClipboardAsAdministrator()
                }
            }
        }
        FmMenuSeparator {}
        FmMenuItem {
            text: root.currentFolderPinned()
                  ? "Unpin Current Folder from Favorites"
                  : "Pin Current Folder to Favorites"
            icon.source: "../assets/icons-classic/star.svg"
            iconColor: Theme.actionIconColor("favorite")
            visible: root.canFavoriteCurrentFolder()
            enabled: visible
            onTriggered: {
                if (root.favoritesController && root.controller) {
                    root.favoritesController.togglePinned(root.controller.currentPath)
                }
            }
        }
        FmMenuSeparator {
            visible: root.canFavoriteCurrentFolder()
        }
        FmMenuItem {
            text: "Select All"
            icon.source: "../assets/icons-classic/select-all.svg"
            iconColor: Theme.actionIconColor("primary")
            onTriggered: root.selectAllRequested()
        }
        FmMenuToggleItem {
            text: root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            icon.source: root.controller.directoryModel.showHidden ? "../assets/icons-classic/eye-off.svg" : "../assets/icons-classic/eye.svg"
            iconColor: Theme.actionIconColor("hidden")
            onTriggered: {
                const newValue = !root.controller.directoryModel.showHidden
                root.controller.directoryModel.showHidden = newValue
                root.workspaceController.treeModel.showHidden = newValue
            }
        }
        FmMenu {
            title: "Panel Appearance"
            width: 270
            icon.source: "../assets/icons-classic/columns-2.svg"
            itemIconColor: Theme.actionIconColor("view-grid")

            FmMenuToggleItem {
                text: root.showActionBar ? "Hide Action Bar" : "Show Action Bar"
                active: root.showActionBar
                icon.source: "../assets/icons-classic/operation-drawer-compact.svg"
                iconColor: Theme.actionIconColor("view-details")
                onTriggered: root.actionBarVisibilityRequested(!root.showActionBar)
            }
            FmMenuToggleItem {
                text: root.showSelectionBadges ? "Hide Selection Badges" : "Show Selection Badges"
                active: root.showSelectionBadges
                icon.source: "../assets/icons-classic/select-all.svg"
                iconColor: Theme.actionIconColor("primary")
                onTriggered: root.selectionBadgesVisibilityRequested(!root.showSelectionBadges)
            }
        }
        FmMenu {
            title: "Hover & Peek"
            width: 270
            icon.source: "../assets/icons-classic/duplicate.svg"
            itemIconColor: Theme.actionIconColor("info")

            FmMenuToggleItem {
                text: root.showMediaHoverPreviews ? "Hide Media Hover Previews" : "Show Media Hover Previews"
                active: root.showMediaHoverPreviews
                icon.source: "../assets/icons-classic/image.svg"
                iconColor: Theme.categoryInfo
                onTriggered: root.mediaHoverPreviewsVisibilityRequested(!root.showMediaHoverPreviews)
            }
            FmMenuToggleItem {
                text: root.showFolderHoverPreviews ? "Hide Folder Hover Previews" : "Show Folder Hover Previews"
                active: root.showFolderHoverPreviews
                icon.source: "../assets/icons-classic/folder-open.svg"
                iconColor: Theme.categoryNavigation
                onTriggered: root.folderHoverPreviewsVisibilityRequested(!root.showFolderHoverPreviews)
            }
            FmMenuToggleItem {
                text: root.folderPeekEnabled ? "Hide Folder Peek" : "Show Folder Peek"
                active: root.folderPeekEnabled
                icon.source: "../assets/icons-classic/panel-open.svg"
                iconColor: Theme.categoryAction
                onTriggered: root.folderPeekEnabledRequested(!root.folderPeekEnabled)
            }
        }
        FmMenuSeparator {}
        FmMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons-classic/refresh.svg"
            iconColor: Theme.actionIconColor("refresh")
            onTriggered: root.controller.refresh()
        }
        FmMenuItem {
            text: "Load More"
            icon.source: menuPolicy.loadMoreIconSource
            recolorEnabled: false
            visible: menuPolicy.canLoadMore()
            enabled: visible
            onTriggered: root.controller.loadMore()
        }
        FmMenuItem {
            text: "Analyze Disk Usage"
            icon.source: "../assets/icons-classic/disk-usage.svg"
            iconColor: Theme.actionIconColor("analyze")
            visible: menuPolicy.canAnalyzeCurrentFolder()
            enabled: visible
            onTriggered: if (root.windowObject && root.windowObject.openDiskUsage) root.windowObject.openDiskUsage(root.controller.currentPath)
        }
        FmMenuItem {
            text: "Properties"
            icon.source: "../assets/icons-classic/info.svg"
            iconColor: Theme.actionIconColor("info")
            visible: menuPolicy.canShowCurrentFolderProperties()
            enabled: visible
            onTriggered: if (root.propertiesController) root.propertiesController.load(root.controller.currentPath)
        }
        FmMenuSeparator {
            visible: root.customActions.length > 0
        }
        Instantiator {
            model: root.customActions
            delegate: FmMenuItem {
                text: modelData.text || ""
                icon.source: modelData.iconSource && modelData.iconSource.length > 0
                             ? modelData.iconSource
                             : "../assets/icons-classic/info.svg"
                iconColor: Theme.actionIconColor("info")
                enabled: modelData.enabled !== false
                onTriggered: root.triggerCustomAction(modelData.id)
            }
            onObjectAdded: (index, object) => emptyContextMenu.addItem(object)
            onObjectRemoved: (index, object) => emptyContextMenu.removeItem(object)
        }
    }

}
