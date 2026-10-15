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
    property string contextPath: controller ? controller.currentPath : ""
    readonly property bool nestedContext: controller && contextPath !== controller.currentPath
    readonly property bool canCreateHere: controller && !controller.remoteMutationPending
        && (nestedContext ? controller.canCreateInPath(contextPath) : controller.canCreateManualItem)
    readonly property bool canPasteHere: workspaceController && workspaceController.hasClipboard
        && !workspaceController.operationQueue.busy && controller
        && (nestedContext ? controller.canPasteIntoPath(contextPath) : controller.canPasteIntoCurrentPath)

    signal selectAllRequested(string path)
    signal refreshFolderRequested(string path)
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
        root.popupEmptyMenuAtPath(root.controller ? root.controller.currentPath : "")
    }

    function popupEmptyMenuAtPath(path) {
        root.contextPath = path || (root.controller ? root.controller.currentPath : "")
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
        const revision = menuPolicy.favoritesPinnedCount
        return revision >= 0 && root.favoritesController && root.favoritesController.isPinned(root.contextPath)
    }

    function canFavoriteCurrentFolder() {
        return root.favoritesController && menuPolicy.actionPolicy.pathCanBeFavorited(root.contextPath)
    }

    function customActionContext() {
        return {
            scope: "folder",
            currentPath: root.contextPath,
            targetPath: root.contextPath,
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
            root.refreshFolderRequested(root.contextPath)
        }
        if (root.windowObject && root.windowObject.openPluginActionResult) {
            root.windowObject.openPluginActionResult(result)
        }
    }

    FmMenu {
        id: emptyContextMenu
        objectName: "filePanelEmptyPopup"
        onOpened: root.menuOpenChanged(true)
        onClosed: root.menuOpenChanged(false)
        FmMenuItem {
            text: Qt.platform.os === "windows" ? "Open in PowerShell" : "Open in Terminal"
            icon.source: "../assets/icons-classic/terminal.svg"
            iconColor: Theme.actionIconColor("terminal")
            visible: menuPolicy.canOpenTerminal()
            enabled: visible
            onTriggered: root.controller.openPathInTerminal(root.contextPath)
        }
        FmMenuSeparator {
            visible: menuPolicy.canOpenTerminal()
        }
        FmMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons-classic/folder-plus.svg"
            iconColor: Theme.actionIconColor("create")
            visible: root.canCreateHere
            enabled: visible
            onTriggered: root.controller.createFolderAtPath(root.contextPath, "New Folder")
        }
        FmMenuItem {
            text: "New Folder as Administrator"
            icon.source: "../assets/icons-classic/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: !root.nestedContext && root.adminModeActive()
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
            visible: root.canCreateHere
            enabled: visible
            onTriggered: root.controller.createFileAtPath(root.contextPath, "New Text File.txt")
        }
        FmMenuItem {
            text: "New File as Administrator"
            icon.source: "../assets/icons-classic/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: !root.nestedContext && root.adminModeActive()
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
            visible: root.canCreateHere
            enabled: visible
            onTriggered: root.controller.createFileAtPath(root.contextPath, "New File")
        }
        FmMenuSeparator {
            visible: root.canCreateHere
        }
        FmMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons-classic/paste.svg"
            iconColor: Theme.actionIconColor("paste")
            enabled: root.canPasteHere
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard(root.contextPath)
        }
        FmMenuItem {
            text: "Paste as Administrator"
            icon.source: "../assets/icons-classic/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: !root.nestedContext && root.adminModeActive()
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
                    root.favoritesController.togglePinned(root.contextPath)
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
            onTriggered: root.selectAllRequested(root.contextPath)
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
            onTriggered: root.refreshFolderRequested(root.contextPath)
        }
        FmMenuItem {
            text: "Load More"
            icon.source: menuPolicy.loadMoreIconSource
            recolorEnabled: false
            visible: !root.nestedContext && menuPolicy.canLoadMore()
            enabled: visible
            onTriggered: root.controller.loadMore()
        }
        FmMenuItem {
            text: "Analyze Disk Usage"
            icon.source: "../assets/icons-classic/disk-usage.svg"
            iconColor: Theme.actionIconColor("analyze")
            visible: menuPolicy.actionPolicy.canAnalyzePath(root.contextPath, true)
            enabled: visible
            onTriggered: if (root.windowObject && root.windowObject.openDiskUsage) root.windowObject.openDiskUsage(root.contextPath)
        }
        FmMenuItem {
            text: "Properties"
            icon.source: "../assets/icons-classic/info.svg"
            iconColor: Theme.actionIconColor("info")
            visible: menuPolicy.actionPolicy.canShowPropertiesPath(root.contextPath)
            enabled: visible
            onTriggered: if (root.propertiesController) root.propertiesController.load(root.contextPath)
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
