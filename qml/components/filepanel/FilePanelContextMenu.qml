import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

Item {
    id: root

    property var controller
    property var workspaceController
    property var windowObject
    property var contextRowProvider
    property bool isCurrentPathArchive: false

    signal renameRequested()

    function popupContextMenu() {
        contextMenu.popup()
    }

    function contextRow() {
        return root.contextRowProvider ? root.contextRowProvider() : -1
    }

    readonly property string revealInOsLabel: Qt.platform.os === "windows" ? "Show in Explorer"
            : Qt.platform.os === "osx" ? "Reveal in Finder"
            : "Open Containing Folder"

    ThemedContextMenu {
        id: contextMenu
        ThemedMenuItem {
            text: "Open"
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: "#22c55e"
            enabled: contextRow() >= 0
            onTriggered: root.controller.openItem(contextRow())
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Cut to Clipboard"
            icon.source: "../assets/icons/move.svg"
            iconColor: "#f59e0b"
            enabled: Boolean(root.controller.directoryModel.selectedCount > 0
                     && root.workspaceController
                     && root.workspaceController.operationQueue
                     && !root.workspaceController.operationQueue.busy)
            onTriggered: if (root.workspaceController) root.workspaceController.cutToClipboard()
        }
        ThemedMenuItem {
            text: "Copy to Clipboard"
            icon.source: "../assets/icons/copy.svg"
            iconColor: "#3b82f6"
            enabled: Boolean(root.controller.directoryModel.selectedCount > 0
                     && root.workspaceController
                     && root.workspaceController.operationQueue
                     && !root.workspaceController.operationQueue.busy)
            onTriggered: if (root.workspaceController) root.workspaceController.copyToClipboard()
        }
        ThemedMenuItem {
            text: "Paste from Clipboard"
            icon.source: "../assets/icons/paste.svg"
            iconColor: "#14b8a6"
            enabled: Boolean(root.workspaceController
                     && root.workspaceController.operationQueue
                     && root.workspaceController.hasClipboard
                     && !root.workspaceController.operationQueue.busy)
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Rename"
            icon.source: "../assets/icons/rename.svg"
            iconColor: "#a855f7"
            enabled: contextRow() >= 0 && !root.isCurrentPathArchive
            onTriggered: root.renameRequested()
        }
        ThemedMenuItem {
            text: "Delete"
            icon.source: "../assets/icons/delete.svg"
            destructive: true
            iconColor: "#ef4444"
            enabled: Boolean(root.controller.directoryModel.selectedCount > 0
                     && root.workspaceController
                     && root.workspaceController.operationQueue
                     && !root.workspaceController.operationQueue.busy
                     && !root.isCurrentPathArchive)
            onTriggered: if (root.workspaceController) root.workspaceController.requestDelete(root.controller.selectedPaths(), root.controller.currentPath)
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Refresh"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: "#14b8a6"
            onTriggered: root.controller.refresh()
        }
        ThemedMenuItem {
            text: revealInOsLabel
            icon.source: "../assets/icons/reveal.svg"
            iconColor: "#3b82f6"
            enabled: contextRow() >= 0
            onTriggered: root.controller.revealInFileManager(contextRow())
        }
        ThemedMenuItem {
            text: "Properties"
            icon.source: "../assets/icons/info.svg"
            iconColor: "#0ea5e9"
            enabled: contextRow() >= 0
            onTriggered: root.controller.showProperties(contextRow())
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Checksums"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: "#14b8a6"
            enabled: root.controller.directoryModel.selectedCount === 1
                     && !root.controller.directoryModel.isDirectoryAt(root.controller.directoryModel.indexOfPath(root.controller.selectedPaths()[0]))
            onTriggered: if (root.windowObject) root.windowObject.showChecksums(root.controller.selectedPaths())
        }
        ThemedMenuItem {
            text: "Compare Files"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: "#3b82f6"
            enabled: root.controller.directoryModel.selectedCount === 2
            onTriggered: if (root.windowObject) root.windowObject.showChecksums(root.controller.selectedPaths())
        }
        ThemedMenuSeparator {
            visible: Qt.platform.os === "windows"
        }
        ThemedMenuItem {
            text: "Open in PowerShell"
            icon.source: "../assets/icons/terminal.svg"
            iconColor: "#6366f1"
            visible: Qt.platform.os === "windows"
            enabled: root.controller.currentPath.length > 0
            onTriggered: root.controller.openInTerminal()
        }
    }
}
