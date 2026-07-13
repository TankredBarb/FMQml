import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQml
import ".."
import "../../style"

Item {
    id: root

    property var controller
    property var workspaceController
    property var favoritesController
    property var windowObject
    property var contextRowProvider
    property int contextRowValue: -1
    property string contextPathValue: ""
    property bool contextCanExtractArchive: false
    property bool contextCanMountIso: false
    property bool isCurrentPathArchive: false
    property bool isCurrentPathReadOnlyContainer: false
    property var contextLaunchCapabilities: ({})
    property bool contextOpenWithAvailable: false
    property var contextOpenWithPaths: []
    property var contextOpenWithCandidates: []
    property var customActions: []

    signal renameRequested()
    signal menuOpenChanged(bool open)

    FilePanelMenuPolicy {
        id: menuPolicy
        controller: root.controller
        workspaceController: root.workspaceController
        favoritesController: root.favoritesController
        contextRowProvider: root.contextRowProvider
        contextRowValue: root.contextRowValue
        contextPathValue: root.contextPathValue
        isCurrentPathReadOnlyContainer: root.isCurrentPathReadOnlyContainer
    }

    function popupContextMenu(row, path, canExtractArchive, canMountIso) {
        root.contextRowValue = row === undefined ? -1 : row
        root.contextPathValue = path === undefined ? "" : path
        root.contextCanExtractArchive = canExtractArchive === true
        root.contextCanMountIso = canMountIso === true
        root.contextLaunchCapabilities = root.controller && root.controller.launchCapabilitiesForPath
                ? root.controller.launchCapabilitiesForPath(root.contextPathValue)
                : ({})
        const selectedPaths = root.controller && root.controller.selectedPaths ? root.controller.selectedPaths() : []
        root.contextOpenWithPaths = selectedPaths && selectedPaths.indexOf(root.contextPathValue) >= 0
                ? selectedPaths : [root.contextPathValue]
        root.contextOpenWithAvailable = root.controller && root.controller.openWithAvailableForPaths
                ? root.controller.openWithAvailableForPaths(root.contextOpenWithPaths)
                : false
        root.contextOpenWithCandidates = root.controller && root.controller.openWithCandidatesForPath
                ? root.controller.openWithCandidatesForPath(root.contextPathValue)
                : []
        root.customActions = root.availableCustomActions()
        contextMenu.popup()
    }

    function contextRow() {
        return menuPolicy.contextRow()
    }

    function localPathFromUrl(url) {
        return menuPolicy.localPathFromUrl(url)
    }

    function favoriteMenuPaths() {
        return menuPolicy.favoriteMenuPaths()
    }

    function pathsCanBeFavorited(paths) {
        return menuPolicy.pathsCanBeFavorited(paths)
    }

    function favoriteMenuAvailable() {
        return menuPolicy.favoriteMenuAvailable()
    }

    function favoriteMenuAllPinned() {
        return menuPolicy.favoriteMenuAllPinned()
    }

    function canAnalyzeContextFolder() {
        return menuPolicy.canAnalyzeContextFolder()
    }

    function contextInsideManagedIsoMount() {
        return root.workspaceController
                && root.workspaceController.isInsideManagedIsoMount
                && root.contextPathValue.length > 0
                && root.workspaceController.isInsideManagedIsoMount(root.contextPathValue)
    }

    function canShowLocalMutationBlock() {
        return menuPolicy.canDeleteSelection()
                || !menuPolicy.currentPathIsProvider()
                || root.contextCanMountIso
                || (root.contextCanExtractArchive && menuPolicy.canExtractContextArchive())
    }

    function canUseAdminSingleContextAction() {
        return root.adminModeActive()
                && adminController
                && root.controller
                && !root.controller.isVirtualRoot
                && !menuPolicy.currentPathIsProvider()
                && root.contextRow() >= 0
    }

    function adminModeActive() {
        return Qt.platform.os === "linux"
                && typeof adminController !== "undefined"
                && adminController
                && adminController.adminModeActive
    }

    function canUseAdminSingleSelectionAction() {
        return root.canUseAdminSingleContextAction()
                && root.controller.directoryModel
                && root.controller.directoryModel.selectedCount === 1
    }

    function oppositePanel() {
        if (!root.workspaceController || !root.controller) {
            return null
        }
        if (root.workspaceController.leftPanel === root.controller) {
            return root.workspaceController.rightPanel
        }
        if (root.workspaceController.rightPanel === root.controller) {
            return root.workspaceController.leftPanel
        }
        return root.workspaceController.activePanel === 0
               ? root.workspaceController.rightPanel
               : root.workspaceController.leftPanel
    }

    function customActionDestinationPath() {
        if (!root.workspaceController || !root.workspaceController.splitEnabled) {
            return ""
        }
        const panel = root.oppositePanel()
        const path = panel ? String(panel.currentPath || "") : ""
        return path.indexOf("://") >= 0 ? "" : path
    }

    function customActionContext() {
        const row = root.contextRow()
        const model = menuPolicy.directoryModel()
        return {
            scope: "item",
            currentPath: root.controller ? root.controller.currentPath : "",
            targetPath: root.contextPathValue,
            destinationPath: root.customActionDestinationPath(),
            targetIsDirectory: row >= 0 && model ? model.isDirectoryAt(row) : false,
            selectedPaths: root.controller && root.controller.selectedPaths ? root.controller.selectedPaths() : []
        }
    }

    function availableCustomActions() {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            return []
        }
        return pluginActionController.actionsForContext(root.customActionContext())
    }

    function customActionById(actionId) {
        for (let i = 0; i < root.customActions.length; ++i) {
            if (root.customActions[i].id === actionId) {
                return root.customActions[i]
            }
        }
        return null
    }

    function handleCustomActionResult(result) {
        if (!result) {
            return
        }
        if (result.thumbnailInvalidationPaths
                && root.windowObject
                && root.windowObject.invalidateThumbnailsForPaths) {
            root.windowObject.invalidateThumbnailsForPaths(result.thumbnailInvalidationPaths)
        }
        if (result.refreshCurrentPath === true && root.controller) {
            root.controller.refresh()
        }
        if (result.statusOnly === true) {
            if (root.controller && root.controller.showStatusMessage) {
                root.controller.showStatusMessage(String(result.message || "Plugin action completed."))
            }
            return
        }
        if (root.windowObject && root.windowObject.openPluginActionResult) {
            root.windowObject.openPluginActionResult(result)
        }
    }

    function triggerCustomAction(actionId) {
        if (typeof pluginActionController === "undefined" || !pluginActionController) {
            return
        }
        const action = root.customActionById(actionId)
        if (action && action.asynchronous === true && pluginActionController.triggerActionAsync) {
            const started = pluginActionController.triggerActionAsync(actionId, root.customActionContext())
            if (started && action.text) {
                started.message = String(action.text) + " started."
            }
            root.handleCustomActionResult(started)
            return
        }
        const result = pluginActionController.triggerAction(actionId, root.customActionContext())
        root.handleCustomActionResult(result)
    }

    Connections {
        target: typeof pluginActionController !== "undefined" ? pluginActionController : null
        ignoreUnknownSignals: true
        function onActionFinished(result) {
            root.handleCustomActionResult(result)
        }
    }

    ThemedContextMenu {
        id: contextMenu
        onOpened: root.menuOpenChanged(true)
        onClosed: root.menuOpenChanged(false)
        ThemedMenuItem {
            text: "Open"
            icon.source: "../assets/icons/open.svg"
            iconColor: Theme.actionIconColor("open")
            enabled: menuPolicy.canOpenContextItem()
            onTriggered: root.controller.openItem(contextRow())
        }
        ThemedMenuItem {
            text: "Open With…"
            icon.source: "../assets/icons/open.svg"
            iconColor: Theme.categoryAction
            visible: root.contextOpenWithAvailable
            enabled: visible
            onTriggered: if (root.windowObject && root.windowObject.openOpenWith) {
                root.windowObject.openOpenWith(root.controller, root.contextOpenWithPaths)
            }
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Cut to Clipboard"
            icon.source: "../assets/icons/cut.svg"
            iconColor: Theme.actionIconColor("move")
            visible: !menuPolicy.currentPathIsProvider()
            enabled: menuPolicy.canCutToClipboard()
            onTriggered: if (root.workspaceController) root.workspaceController.cutToClipboard()
        }
        ThemedMenuItem {
            text: "Copy to Clipboard"
            icon.source: "../assets/icons/clipboard-copy.svg"
            iconColor: Theme.actionIconColor("copy")
            enabled: menuPolicy.canCopyToClipboard()
            onTriggered: if (root.workspaceController) root.workspaceController.copyToClipboard()
        }
        ThemedMenuItem {
            text: "Duplicate"
            icon.source: "../assets/icons/duplicate.svg"
            iconColor: Theme.actionIconColor("copy")
            visible: !menuPolicy.currentPathIsProvider()
            enabled: menuPolicy.canDuplicateSelection()
            onTriggered: if (root.workspaceController) root.workspaceController.duplicateActiveSelection()
        }
        ThemedMenuItem {
            text: "Compress as 7zip archive"
            icon.source: "../assets/icons/archive.svg"
            iconColor: Theme.actionIconColor("archive")
            visible: !menuPolicy.currentPathIsProvider()
            enabled: menuPolicy.canCompressSelection()
            onTriggered: if (root.workspaceController) root.workspaceController.compressActiveSelection()
        }
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            iconColor: Theme.actionIconColor("paste")
            enabled: menuPolicy.canPasteFromClipboard()
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard()
        }
        ThemedMenuItem {
            text: "Paste as Administrator"
            icon.source: "../assets/icons/shield.svg"
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
        ThemedMenuSeparator {
            visible: root.favoriteMenuAvailable()
        }
        ThemedMenuItem {
            text: root.favoriteMenuAllPinned() ? "Unpin from Favorites" : "Pin to Favorites"
            icon.source: "../assets/icons/star.svg"
            iconColor: Theme.actionIconColor("favorite")
            visible: root.favoriteMenuAvailable()
            enabled: visible
            onTriggered: {
                if (!root.favoritesController || !root.controller) return
                const selected = root.favoriteMenuPaths()
                if (root.favoriteMenuAllPinned()) {
                    root.favoritesController.unpinPaths(selected)
                } else {
                    root.favoritesController.pinPaths(selected)
                }
            }
        }
        ThemedMenuSeparator {
            visible: root.canShowLocalMutationBlock()
        }
        ThemedMenuItem {
            text: "Mount to..."
            icon.source: "../assets/icons/hard-drive.svg"
            iconColor: Theme.actionIconColor("drive")
            visible: root.contextCanMountIso
            enabled: root.contextCanMountIso
            onTriggered: if (root.workspaceController) root.workspaceController.requestMountIso(root.contextPathValue)
        }
        ThemedMenuSeparator {
            visible: root.contextCanMountIso
        }
        ThemedMenuItem {
            text: "Extract Here"
            icon.source: "../assets/icons/download.svg"
            iconColor: Theme.actionIconColor("extract")
            visible: root.contextCanExtractArchive && menuPolicy.canExtractContextArchive()
            enabled: visible
            onTriggered: if (root.workspaceController) root.workspaceController.extractArchiveHerePath(root.contextPathValue, root.controller.currentPath)
        }
        ThemedMenuItem {
            text: menuPolicy.contextArchiveFolderName.length > 0
                  ? "Extract to " + menuPolicy.contextArchiveFolderName + "/"
                  : "Extract to folder/"
            icon.source: "../assets/icons/folder.svg"
            iconColor: Theme.actionIconColor("extract")
            visible: root.contextCanExtractArchive && menuPolicy.canExtractContextArchive()
            enabled: visible
            onTriggered: if (root.workspaceController) root.workspaceController.extractArchiveToNamedFolderPath(root.contextPathValue, root.controller.currentPath)
        }
        ThemedMenuItem {
            text: "Extract to..."
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: Theme.actionIconColor("extract")
            visible: root.contextCanExtractArchive && menuPolicy.canExtractContextArchive()
            enabled: visible
            onTriggered: extractDestinationDialog.open()
        }
        ThemedMenuSeparator {
            visible: root.contextCanExtractArchive && menuPolicy.canExtractContextArchive()
        }
        ThemedMenuItem {
            text: "Rename"
            icon.source: "../assets/icons/rename.svg"
            iconColor: Theme.actionIconColor("rename")
            visible: !menuPolicy.currentPathIsProvider()
            enabled: menuPolicy.canRenameSelection()
            onTriggered: root.renameRequested()
        }
        ThemedMenuItem {
            text: "Rename as Administrator"
            icon.source: "../assets/icons/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: root.canUseAdminSingleContextAction()
            enabled: visible
            onTriggered: root.renameRequested()
        }
        ThemedMenuItem {
            text: "Delete"
            icon.source: "../assets/icons/delete.svg"
            destructive: true
            iconColor: Theme.actionIconColor("delete")
            visible: menuPolicy.canDeleteSelection()
            enabled: menuPolicy.canDeleteSelection()
            onTriggered: if (root.workspaceController) root.workspaceController.requestDelete(root.controller.selectedPaths(), root.controller.currentPath,
                                                                                              root.controller.selectedItems ? root.controller.selectedItems() : [])
        }
        ThemedMenuItem {
            text: "Delete as Administrator"
            icon.source: "../assets/icons/shield.svg"
            destructive: true
            iconColor: Theme.warning
            active: true
            visible: root.canUseAdminSingleSelectionAction()
            enabled: visible
            onTriggered: {
                if (root.windowObject && root.windowObject.requestDeleteActiveSelectionAsAdministrator) {
                    root.windowObject.requestDeleteActiveSelectionAsAdministrator()
                } else if (root.workspaceController) {
                    root.workspaceController.requestDeleteAsAdministrator(root.controller.selectedPaths(), root.controller.currentPath,
                                                                         root.controller.selectedItems ? root.controller.selectedItems() : [])
                }
            }
        }
        ThemedMenuSeparator {
            visible: root.canShowLocalMutationBlock()
        }
        ThemedMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: Theme.actionIconColor("refresh")
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: "Load More"
            icon.source: "../assets/icons/download.svg"
            iconColor: Theme.actionIconColor("navigation")
            visible: menuPolicy.canLoadMore()
            enabled: visible
            onTriggered: root.controller.openPathPreservingScroll(menuPolicy.loadMorePath())
        }
        ThemedMenuItem {
            text: menuPolicy.revealInOsLabel
            icon.source: "../assets/icons/reveal.svg"
            iconColor: Theme.actionIconColor("navigation")
            visible: menuPolicy.canRevealContextItem()
            enabled: visible
            onTriggered: root.controller.revealInFileManager(contextRow())
        }
        ThemedMenuItem {
            text: "Set as Wallpaper"
            icon.source: "../assets/icons/image.svg"
            iconColor: Theme.actionIconColor("image")
            visible: menuPolicy.canSetContextWallpaper()
            enabled: visible
            onTriggered: root.controller.setAsWallpaper(contextRow())
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: Theme.actionIconColor("info")
            visible: menuPolicy.canShowContextProperties()
            enabled: visible
            onTriggered: root.controller.showProperties(contextRow())
        }
        ThemedMenuItem {
            text: "Edit Access & Ownership as Administrator"
            icon.source: "../assets/icons/shield.svg"
            iconColor: Theme.warning
            active: true
            visible: root.canUseAdminSingleSelectionAction()
                    && menuPolicy.canShowContextProperties()
            enabled: visible
            onTriggered: root.controller.showAccessOwnershipAsAdministrator(contextRow())
        }
        ThemedMenuItem {
            text: "Analyze Disk Usage"
            icon.source: "../assets/icons/disk-usage.svg"
            iconColor: Theme.actionIconColor("analyze")
            visible: root.canAnalyzeContextFolder()
            enabled: visible
            onTriggered: if (root.windowObject && root.windowObject.openDiskUsage) root.windowObject.openDiskUsage(root.contextPathValue)
        }
        ThemedMenuSeparator {
            visible: menuPolicy.canOfferCompareChecksums()
        }
        ThemedMenuItem {
            text: "Compare Checksums (select 2 files)"
            icon.source: "../assets/icons/checksum.svg"
            iconColor: Theme.actionIconColor("info")
            visible: menuPolicy.canOfferCompareChecksums()
            enabled: menuPolicy.canCompareChecksums()
            onTriggered: if (root.windowObject) root.windowObject.showChecksums(root.controller.selectedPaths())
        }
        ThemedMenuSeparator {
            visible: menuPolicy.canOpenTerminal()
        }
        ThemedMenuItem {
            text: Qt.platform.os === "windows" ? "Open in PowerShell" : "Open in Terminal"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: Theme.actionIconColor("terminal")
            visible: menuPolicy.canOpenTerminal()
            enabled: visible
            onTriggered: root.controller.openInTerminal()
        }
        ThemedMenuSeparator {
            visible: root.customActions.length > 0
        }
        Instantiator {
            model: root.customActions
            delegate: ThemedMenuItem {
                text: modelData.text || ""
                icon.source: modelData.iconSource && modelData.iconSource.length > 0
                             ? modelData.iconSource
                             : "../assets/icons/info.svg"
                iconColor: Theme.actionIconColor("info")
                enabled: modelData.enabled !== false
                onTriggered: root.triggerCustomAction(modelData.id)
            }
            onObjectAdded: (index, object) => contextMenu.addItem(object)
            onObjectRemoved: (index, object) => contextMenu.removeItem(object)
        }
    }

    FolderDialog {
        id: extractDestinationDialog
        title: "Extract to Folder"
        currentFolder: menuPolicy.currentFolderUrl()
        onAccepted: {
            if (!root.workspaceController || !root.contextPathValue) {
                return
            }
            const destination = root.localPathFromUrl(selectedFolder)
            if (destination.length > 0) {
                root.workspaceController.extractArchiveTo(root.contextPathValue, destination)
            }
        }
    }

}
