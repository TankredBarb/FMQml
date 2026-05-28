import QtQuick
import QtQuick.Controls
import QtQml

Item {
    id: root
    anchors.fill: parent

    property var commandPaletteCommands: []
    property var appRoot: null

    readonly property bool workspaceOverlayOpen: conflictDialog.opened || conflictDialog.visible
                                                 || helpDialog.opened || helpDialog.visible
                                                 || settingsDialog.opened || settingsDialog.visible
                                                 || propertiesDialog.opened || propertiesDialog.visible
                                                 || isoMountDialog.opened || isoMountDialog.visible
                                                 || deleteConfirmDialog.opened || deleteConfirmDialog.visible
                                                 || batchRenameDialog.opened || batchRenameDialog.visible
                                                 || checksumDialog.opened || checksumDialog.visible
    readonly property bool anyOverlayOpen: root.workspaceOverlayOpen
                                           || commandPalette.opened || commandPalette.visible

    function openDeleteConfirm(paths, label) {
        const list = paths ? Array.from(paths) : []
        if (list.length === 0) {
            return
        }
        const details = workspaceController && workspaceController.deleteRequestDetails
                      ? workspaceController.deleteRequestDetails(list, label || "")
                      : ({})
        deleteConfirmDialog.openFor(list, label || "", details)
    }

    function openCommandPalette() {
        commandPalette.openPalette()
    }

    function openHelpDialog() {
        helpDialog.open()
    }

    function openSettingsDialog() {
        settingsDialog.open()
    }

    function openSettingsImportDialog() {
        settingsDialog.openImportDialog()
    }

    function openSettingsExportDialog() {
        settingsDialog.openExportDialog()
    }

    function showBatchRename(paths) {
        if (!paths || paths.length === 0) return
        batchRenameDialog.sourcePaths = paths
        batchRenameDialog.controller = workspaceController.activePanel === 0
                                       ? workspaceController.leftPanel
                                       : workspaceController.rightPanel
        batchRenameDialog.open()
    }

    function showChecksums(paths) {
        if (!paths || paths.length === 0) return
        checksumDialog.path1 = paths[0]
        checksumDialog.path2 = paths.length > 1 ? paths[1] : ""
        checksumDialog.controller = workspaceController.activePanel === 0
                                     ? workspaceController.leftPanel
                                     : workspaceController.rightPanel
        checksumDialog.open()
    }

    ConflictDialog {
        id: conflictDialog
    }

    HelpDialog {
        id: helpDialog
    }

    SettingsDialog {
        id: settingsDialog
        appRoot: root.appRoot
    }

    PropertiesDialog {
        id: propertiesDialog
    }

    DeleteConfirmDialog {
        id: deleteConfirmDialog
    }

    IsoMountDialog {
        id: isoMountDialog
    }

    BatchRenameDialog {
        id: batchRenameDialog
    }

    ChecksumDialog {
        id: checksumDialog
    }

    CommandPalette {
        id: commandPalette
        commands: root.commandPaletteCommands
    }

    Connections {
        target: workspaceController.operationQueue
        function onConflictDetected(source, destination, sourceSize, sourceModified, destSize, destModified) {
            conflictDialog.sourcePath = source
            conflictDialog.destinationPath = destination
            conflictDialog.sourceSize = sourceSize
            conflictDialog.sourceModified = sourceModified
            conflictDialog.destSize = destSize
            conflictDialog.destModified = destModified
            conflictDialog.open()
        }
    }

    Connections {
        target: workspaceController
        function onDeleteRequested(paths, label) {
            root.openDeleteConfirm(paths, label)
        }
        function onMountIsoRequested(path) {
            isoMountDialog.openFor(path)
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onRevealProperties(paths) {
            propertiesController.loadMultiple(paths)
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onRevealProperties(paths) {
            propertiesController.loadMultiple(paths)
        }
    }
}
