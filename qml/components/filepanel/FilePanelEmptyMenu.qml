import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    readonly property int favoritesPinnedCount: root.favoritesController ? root.favoritesController.pinnedCount : -1

    signal selectAllRequested()

    function popupEmptyMenu() {
        emptyContextMenu.popup()
    }

    function currentFolderPinned() {
        const revision = root.favoritesPinnedCount
        if (revision < 0 || !root.favoritesController || !root.controller) {
            return false
        }
        return root.favoritesController.isPinned(root.controller.currentPath)
    }

    ThemedContextMenu {
        id: emptyContextMenu
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: "#6366f1"
            visible: Qt.platform.os === "windows"
            enabled: root.controller.currentPath.length > 0
            onTriggered: root.controller.openInTerminal()
        }
        ThemedMenuSeparator {
            visible: Qt.platform.os === "windows"
        }
        ThemedMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: "#22c55e"
            enabled: root.controller && root.controller.canCreateInCurrentPath
            onTriggered: root.controller.createFolder("New Folder")
        }
        ThemedMenuItem {
            text: "New Text File"
            icon.source: "../assets/icons/document.svg"
            iconColor: "#f59e0b"
            enabled: root.controller && root.controller.canCreateInCurrentPath
            onTriggered: root.controller.createFile("New Text File.txt")
        }
        ThemedMenuItem {
            text: "New File"
            icon.source: "../assets/icons/document.svg"
            iconColor: "#60a5fa"
            enabled: root.controller && root.controller.canCreateInCurrentPath
            onTriggered: root.controller.createFile("New File")
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            iconColor: "#14b8a6"
            enabled: Boolean(root.workspaceController
                     && root.workspaceController.operationQueue
                     && root.workspaceController.hasClipboard
                     && !root.workspaceController.operationQueue.busy
                     && root.controller
                     && root.controller.canPasteIntoCurrentPath)
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard()
        }
        ThemedMenuItem {
            text: root.currentFolderPinned()
                  ? "Unpin Current Folder from Favorites"
                  : "Pin Current Folder to Favorites"
            icon.source: "../assets/icons/star.svg"
            iconColor: Theme.accent
            enabled: Boolean(root.favoritesController
                     && root.controller
                     && root.controller.currentPath.length > 0
                     && !root.controller.isVirtualRoot)
            onTriggered: {
                if (root.favoritesController && root.controller) {
                    root.favoritesController.togglePinned(root.controller.currentPath)
                }
            }
        }
        ThemedMenuItem {
            text: "Select All"
            icon.source: "../assets/icons/select-all.svg"
            iconColor: "#8b5cf6"
            onTriggered: root.selectAllRequested()
        }
        ThemedMenuItem {
            text: root.controller.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            icon.source: root.controller.directoryModel.showHidden ? "../assets/icons/eye-off.svg" : "../assets/icons/eye.svg"
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
            iconColor: "#14b8a6"
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: "Analyze Disk Usage"
            icon.source: "../assets/icons/hard-drive.svg"
            iconColor: Theme.accent
            enabled: Boolean(root.controller
                     && root.controller.currentPath.length > 0
                     && typeof diskUsageController !== "undefined"
                     && diskUsageController
                     && diskUsageController.canAnalyzePath(root.controller.currentPath))
            onTriggered: if (root.windowObject && root.windowObject.openDiskUsage) root.windowObject.openDiskUsage(root.controller.currentPath)
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: "#0ea5e9"
            onTriggered: if (root.propertiesController) root.propertiesController.load(root.controller.currentPath)
        }
    }
}
