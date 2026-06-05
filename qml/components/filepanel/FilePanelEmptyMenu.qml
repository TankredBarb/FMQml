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

    signal selectAllRequested()

    FilePanelMenuPolicy {
        id: menuPolicy
        controller: root.controller
        workspaceController: root.workspaceController
        favoritesController: root.favoritesController
    }

    function popupEmptyMenu() {
        emptyContextMenu.popup()
    }

    function currentFolderPinned() {
        return menuPolicy.currentFolderPinned()
    }

    function canFavoriteCurrentFolder() {
        return menuPolicy.canFavoriteCurrentFolder()
    }

    ThemedContextMenu {
        id: emptyContextMenu
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: Theme.actionIconColor("terminal")
            visible: Qt.platform.os === "windows"
            enabled: menuPolicy.canOpenTerminal()
            onTriggered: root.controller.openInTerminal()
        }
        ThemedMenuSeparator {
            visible: Qt.platform.os === "windows"
        }
        ThemedMenuItem {
            text: "New Folder"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: Theme.actionIconColor("create")
            enabled: menuPolicy.canCreateInCurrentPath()
            onTriggered: root.controller.createFolder("New Folder")
        }
        ThemedMenuItem {
            text: "New Text File"
            icon.source: "../assets/icons/text-file.svg"
            iconColor: Theme.actionIconColor("text-file")
            enabled: menuPolicy.canCreateInCurrentPath()
            onTriggered: root.controller.createFile("New Text File.txt")
        }
        ThemedMenuItem {
            text: "New File"
            icon.source: "../assets/icons/file-plus.svg"
            iconColor: Theme.actionIconColor("document")
            enabled: menuPolicy.canCreateInCurrentPath()
            onTriggered: root.controller.createFile("New File")
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            iconColor: Theme.actionIconColor("paste")
            enabled: menuPolicy.canPasteFromClipboard()
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard()
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
            text: "Analyze Disk Usage"
            icon.source: "../assets/icons/disk-usage.svg"
            iconColor: Theme.actionIconColor("analyze")
            enabled: menuPolicy.canAnalyzeCurrentFolder()
            onTriggered: if (root.windowObject && root.windowObject.openDiskUsage) root.windowObject.openDiskUsage(root.controller.currentPath)
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: Theme.actionIconColor("info")
            onTriggered: if (root.propertiesController) root.propertiesController.load(root.controller.currentPath)
        }
    }
}
