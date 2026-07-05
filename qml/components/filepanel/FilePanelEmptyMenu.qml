import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import ".."
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
    property var customActions: []

    signal selectAllRequested()

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
        if (result && result.ok === true && result.refreshCurrentPath === true && root.controller) {
            root.controller.refresh()
        }
        if (root.windowObject && root.windowObject.openPluginActionResult) {
            root.windowObject.openPluginActionResult(result)
        }
    }

    ThemedContextMenu {
        id: emptyContextMenu
        ThemedMenuItem {
            text: Qt.platform.os === "windows" ? "Open in PowerShell" : "Open in Terminal"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: Theme.actionIconColor("terminal")
            visible: menuPolicy.canOpenTerminal()
            enabled: visible
            onTriggered: root.controller.openInTerminal()
        }
        ThemedMenuSeparator {
            visible: menuPolicy.canOpenTerminal()
        }
        ThemedMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: Theme.actionIconColor("create")
            visible: menuPolicy.canCreateInCurrentPath()
            enabled: visible
            onTriggered: root.controller.createFolder("New Folder")
        }
        ThemedMenuItem {
            text: "New Folder as Administrator"
            icon.source: "../assets/icons/shield.svg"
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
        ThemedMenuItem {
            text: "New Text File"
            icon.source: "../assets/icons/text-file.svg"
            iconColor: Theme.actionIconColor("text-file")
            visible: menuPolicy.canCreateInCurrentPath()
            enabled: visible
            onTriggered: root.controller.createFile("New Text File.txt")
        }
        ThemedMenuItem {
            text: "New File as Administrator"
            icon.source: "../assets/icons/shield.svg"
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
        ThemedMenuItem {
            text: "New File"
            icon.source: "../assets/icons/file-plus.svg"
            iconColor: Theme.actionIconColor("document")
            visible: menuPolicy.canCreateInCurrentPath()
            enabled: visible
            onTriggered: root.controller.createFile("New File")
        }
        ThemedMenuSeparator {
            visible: menuPolicy.canCreateInCurrentPath()
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
                     && root.workspaceController
                     && root.workspaceController.hasClipboard
                     && !root.workspaceController.clipboardCut
                     && root.controller
                     && !root.controller.isVirtualRoot
                     && !menuPolicy.currentPathIsProvider()
            enabled: visible
            onTriggered: {
                if (root.windowObject && root.windowObject.pasteClipboardToActivePanelAsAdministrator) {
                    root.windowObject.pasteClipboardToActivePanelAsAdministrator()
                } else if (root.workspaceController) {
                    root.workspaceController.pasteFromClipboardAsAdministrator()
                }
            }
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: root.currentFolderPinned()
                  ? "Unpin Current Folder from Favorites"
                  : "Pin Current Folder to Favorites"
            icon.source: "../assets/icons/star.svg"
            iconColor: Theme.actionIconColor("favorite")
            visible: root.canFavoriteCurrentFolder()
            enabled: visible
            onTriggered: {
                if (root.favoritesController && root.controller) {
                    root.favoritesController.togglePinned(root.controller.currentPath)
                }
            }
        }
        ThemedMenuSeparator {
            visible: root.canFavoriteCurrentFolder()
        }
        ThemedMenuItem {
            text: "Select All"
            icon.source: "../assets/icons/select-all.svg"
            iconColor: Theme.actionIconColor("primary")
            onTriggered: root.selectAllRequested()
        }
        ThemedMenuItem {
            text: root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            icon.source: root.controller.directoryModel.showHidden ? "../assets/icons/eye-off.svg" : "../assets/icons/eye.svg"
            iconColor: Theme.actionIconColor("hidden")
            onTriggered: {
                const newValue = !root.controller.directoryModel.showHidden
                root.controller.directoryModel.showHidden = newValue
                root.workspaceController.treeModel.showHidden = newValue
            }
        }
        ThemedMenuSeparator {}
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
            text: "Analyze Disk Usage"
            icon.source: "../assets/icons/disk-usage.svg"
            iconColor: Theme.actionIconColor("analyze")
            visible: menuPolicy.canAnalyzeCurrentFolder()
            enabled: visible
            onTriggered: if (root.windowObject && root.windowObject.openDiskUsage) root.windowObject.openDiskUsage(root.controller.currentPath)
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: Theme.actionIconColor("info")
            visible: menuPolicy.canShowCurrentFolderProperties()
            enabled: visible
            onTriggered: if (root.propertiesController) root.propertiesController.load(root.controller.currentPath)
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
            onObjectAdded: (index, object) => emptyContextMenu.addItem(object)
            onObjectRemoved: (index, object) => emptyContextMenu.removeItem(object)
        }
    }

}
