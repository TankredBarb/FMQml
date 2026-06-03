import QtQuick
import QtQuick.Controls
import QtQml

Item {
    id: root
    anchors.fill: parent

    property var commandPaletteCommands: []
    property var appRoot: null
    property alias propertiesDialog: propertiesDialog
    property bool searchReturnAvailable: false

    readonly property bool workspaceOverlayOpen: conflictDialog.opened || conflictDialog.visible
                                                 || helpDialog.opened || helpDialog.visible
                                                 || settingsDialog.opened || settingsDialog.visible
                                                 || themeEditorDialog.opened || themeEditorDialog.visible
                                                 || propertiesDialog.opened || propertiesDialog.visible
                                                 || isoMountDialog.opened || isoMountDialog.visible
                                                 || nestedArchiveDialog.opened || nestedArchiveDialog.visible
                                                 || archivePasswordDialog.opened || archivePasswordDialog.visible
                                                 || deleteConfirmDialog.opened || deleteConfirmDialog.visible
                                                 || diskUsageDialog.opened || diskUsageDialog.visible
                                                 || fileSearchDialog.opened || fileSearchDialog.visible
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

    function openCommandPaletteForCommand(commandId) {
        commandPalette.openCommandArgument(commandId)
    }

    function openHelpDialog() {
        helpDialog.open()
    }

    function openSettingsDialog() {
        settingsDialog.open()
    }

    function openThemeEditorDialog() {
        themeEditorDialog.open()
    }

    function copyPropertiesToClipboard() {
        if (propertiesDialog.visible) {
            propertiesDialog.copyAll()
        } else {
            const ctrl = appRoot ? appRoot.activePanelController() : null
            if (!ctrl) return
            const selected = ctrl.selectedPaths()
            if (!selected || selected.length === 0) return

            propertiesDialog.suppressDialog = true
            if (selected.length > 1) {
                propertiesController.loadMultiple(selected)
            } else {
                propertiesController.load(selected[0])
            }
            if (workspaceController) {
                workspaceController.copyTextToClipboard(propertiesController.exportableText())
                if (appRoot) {
                    appRoot.showTransientInfo("Properties copied to clipboard")
                }
            }
            propertiesController.visible = false
            propertiesDialog.suppressDialog = false
        }
    }

    function exportPropertiesToFile(format) {
        if (propertiesDialog.visible) {
            propertiesDialog.openExportMenu()
        } else {
            const ctrl = appRoot ? appRoot.activePanelController() : null
            if (!ctrl) return
            const selected = ctrl.selectedPaths()
            if (!selected || selected.length === 0) return

            propertiesDialog.suppressDialog = true
            propertiesDialog.exportDialogPending = true
            if (selected.length > 1) {
                propertiesController.loadMultiple(selected)
            } else {
                propertiesController.load(selected[0])
            }
            propertiesDialog.silentExport(format || "json")
        }
    }

    function closeTopOverlay() {
        if ((themeEditorDialog.opened || themeEditorDialog.visible) && !themeEditorDialog.childDialogOpen()) {
            themeEditorDialog.closeEditor()
            return true
        }
        if (settingsDialog.opened || settingsDialog.visible) {
            settingsDialog.accept()
            return true
        }
        if (helpDialog.opened || helpDialog.visible) {
            helpDialog.close()
            return true
        }
        if (propertiesDialog.opened || propertiesDialog.visible) {
            propertiesDialog.close()
            return true
        }
        if (isoMountDialog.opened || isoMountDialog.visible) {
            isoMountDialog.close()
            return true
        }
        if (nestedArchiveDialog.opened || nestedArchiveDialog.visible) {
            nestedArchiveDialog.close()
            return true
        }
        if (archivePasswordDialog.opened || archivePasswordDialog.visible) {
            archivePasswordDialog.close()
            return true
        }
        if (deleteConfirmDialog.opened || deleteConfirmDialog.visible) {
            deleteConfirmDialog.close()
            return true
        }
        if (diskUsageDialog.opened || diskUsageDialog.visible) {
            diskUsageDialog.accept()
            return true
        }
        if (fileSearchDialog.opened || fileSearchDialog.visible) {
            fileSearchDialog.accept()
            return true
        }
        if (batchRenameDialog.opened || batchRenameDialog.visible) {
            batchRenameDialog.reject()
            return true
        }
        if (checksumDialog.opened || checksumDialog.visible) {
            checksumDialog.accept()
            return true
        }
        return false
    }

    function openSettingsImportDialog() {
        settingsDialog.openImportDialog()
    }

    function openSettingsExportDialog() {
        settingsDialog.openExportDialog()
    }

    function openDiskUsage(path) {
        if (!path || path.length === 0) return
        diskUsageDialog.openFor(path)
    }

    function openFileSearch(path, includeHidden) {
        if (!path || path.length === 0) return
        root.searchReturnAvailable = false
        fileSearchDialog.openFor(path, includeHidden === true)
    }

    function openNestedArchive(controller, path, displayName, sizeText) {
        nestedArchiveDialog.openFor(controller, path, displayName || "", sizeText || "")
    }

    function openArchivePassword(controller, path, displayName, message) {
        archivePasswordDialog.openFor(controller, path, displayName || "", message || "")
    }

    function reopenFileSearchResults() {
        if (!root.searchReturnAvailable) {
            return
        }
        fileSearchDialog.reopenResults()
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
        onThemeEditorRequested: root.openThemeEditorDialog()
    }

    ThemeEditorDialog {
        id: themeEditorDialog
        parent: Overlay.overlay
    }

    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        enabled: (themeEditorDialog.opened || themeEditorDialog.visible)
                 && !themeEditorDialog.childDialogOpen()
        onActivated: root.closeTopOverlay()
        onActivatedAmbiguously: root.closeTopOverlay()
    }

    PropertiesDialog {
        id: propertiesDialog
        appRoot: root.appRoot
    }

    DeleteConfirmDialog {
        id: deleteConfirmDialog
    }

    IsoMountDialog {
        id: isoMountDialog
    }

    NestedArchiveDialog {
        id: nestedArchiveDialog
    }

    ArchivePasswordDialog {
        id: archivePasswordDialog
    }

    DiskUsageDialog {
        id: diskUsageDialog
        appRoot: root.appRoot
    }

    FileSearchDialog {
        id: fileSearchDialog
        appRoot: root.appRoot
        onResultOpened: {
            root.searchReturnAvailable = true
        }
        onSearchContextReset: {
            root.searchReturnAvailable = false
        }
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
        activePanelController: root.appRoot ? root.appRoot.activePanelController : null
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
        function onArchivePasswordRequested(path, displayName, message) {
            root.openArchivePassword(workspaceController, path, displayName, message)
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onNestedArchiveOpenRequested(path, displayName, sizeText) {
            root.openNestedArchive(workspaceController.leftPanel, path, displayName, sizeText)
        }

        function onArchivePasswordRequested(path, displayName, message) {
            root.openArchivePassword(workspaceController.leftPanel, path, displayName, message)
        }

        function onRevealProperties(paths) {
            propertiesDialog.suppressDialog = false
            propertiesController.loadMultiple(paths)
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onNestedArchiveOpenRequested(path, displayName, sizeText) {
            root.openNestedArchive(workspaceController.rightPanel, path, displayName, sizeText)
        }

        function onArchivePasswordRequested(path, displayName, message) {
            root.openArchivePassword(workspaceController.rightPanel, path, displayName, message)
        }

        function onRevealProperties(paths) {
            propertiesDialog.suppressDialog = false
            propertiesController.loadMultiple(paths)
        }
    }
}
