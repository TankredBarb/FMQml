import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import ".."
import "../../style"

Item {
    id: root

    property var controller
    property var workspaceController
    property var windowObject
    property var contextRowProvider
    property int contextRowValue: -1
    property string contextPathValue: ""
    property bool contextCanExtractArchive: false
    property bool contextCanMountIso: false
    property bool isCurrentPathArchive: false
    property bool isCurrentPathReadOnlyContainer: false
    readonly property string contextArchiveFolderName: {
        if (!root.contextPathValue || root.contextPathValue.length === 0) {
            return ""
        }
        const normalized = String(root.contextPathValue).replace(/\\/g, "/")
        const fileName = normalized.split("/").filter(part => part.length > 0).pop() || ""
        if (fileName.length === 0) {
            return ""
        }
        const dot = fileName.lastIndexOf(".")
        return dot > 0 ? fileName.substring(0, dot) : fileName
    }

    signal renameRequested()

    function popupContextMenu(row, path, canExtractArchive, canMountIso) {
        root.contextRowValue = row === undefined ? -1 : row
        root.contextPathValue = path === undefined ? "" : path
        root.contextCanExtractArchive = canExtractArchive === true
        root.contextCanMountIso = canMountIso === true
        contextMenu.popup()
    }

    function contextRow() {
        return root.contextRowValue >= 0 ? root.contextRowValue
                                         : (root.contextRowProvider ? root.contextRowProvider() : -1)
    }

    function localPathFromUrl(url) {
        let value = url ? url.toString() : ""
        if (value.startsWith("file:///")) {
            value = decodeURIComponent(value.substring(8))
            if (Qt.platform.os === "windows" && value.length >= 3 && value[1] === ":") {
                return value
            }
            return "/" + value
        }
        if (value.startsWith("file://")) {
            return decodeURIComponent(value.substring(7))
        }
        return decodeURIComponent(value)
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
                     && !root.workspaceController.operationQueue.busy
                     && !root.isCurrentPathReadOnlyContainer)
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
                     && !root.workspaceController.operationQueue.busy
                     && root.controller
                     && root.controller.canPasteIntoCurrentPath)
            onTriggered: if (root.workspaceController) root.workspaceController.pasteFromClipboard()
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: "Mount to..."
            icon.source: "../assets/icons/hard-drive.svg"
            iconColor: "#14b8a6"
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
            iconColor: "#14b8a6"
            visible: root.contextCanExtractArchive
            enabled: root.contextCanExtractArchive
            onTriggered: if (root.workspaceController) root.workspaceController.extractArchiveHerePath(root.contextPathValue, root.controller.currentPath)
        }
        ThemedMenuItem {
            text: root.contextArchiveFolderName.length > 0
                  ? "Extract to " + root.contextArchiveFolderName + "/"
                  : "Extract to folder/"
            icon.source: "../assets/icons/folder.svg"
            iconColor: "#14b8a6"
            visible: root.contextCanExtractArchive
            enabled: root.contextCanExtractArchive
            onTriggered: if (root.workspaceController) root.workspaceController.extractArchiveToNamedFolderPath(root.contextPathValue, root.controller.currentPath)
        }
        ThemedMenuItem {
            text: "Extract to..."
            icon.source: "../assets/icons/folder-plus.svg"
            iconColor: "#14b8a6"
            visible: root.contextCanExtractArchive
            enabled: root.contextCanExtractArchive
            onTriggered: extractDestinationDialog.open()
        }
        ThemedMenuSeparator {
            visible: root.contextCanExtractArchive
        }
        ThemedMenuItem {
            text: "Rename"
            icon.source: "../assets/icons/rename.svg"
            iconColor: "#a855f7"
            enabled: contextRow() >= 0
                     && root.controller
                     && root.controller.canRenameSelection
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
                     && root.controller
                     && root.controller.canDeleteSelection)
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
            text: "Compare Checksums (select 2 files)"
            icon.source: "../assets/icons/refresh.svg"
            iconColor: Theme.categoryInfo
            enabled: {
                if (!root.controller || !root.controller.directoryModel) return false
                if (root.controller.directoryModel.selectedCount !== 2) return false
                const paths = root.controller.selectedPaths()
                if (paths.length !== 2) return false
                const idx1 = root.controller.directoryModel.indexOfPath(paths[0])
                const idx2 = root.controller.directoryModel.indexOfPath(paths[1])
                return idx1 >= 0 && idx2 >= 0
                    && !root.controller.directoryModel.isDirectoryAt(idx1)
                    && !root.controller.directoryModel.isDirectoryAt(idx2)
            }
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

    FolderDialog {
        id: extractDestinationDialog
        title: "Extract to Folder"
        currentFolder: root.controller && root.controller.currentPath.length > 0
                       ? "file:///" + root.controller.currentPath.replace(/\\/g, "/")
                       : "file:///"
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
