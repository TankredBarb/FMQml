import QtQuick
import QtQuick.Controls
import QtQml
import "../filepanel"

Item {
    id: root
    anchors.fill: parent

    property var commandPaletteCommands: []
    property var appRoot: null
    property var conflictDialog: null
    property var helpDialog: null
    property var settingsDialog: null
    property var pluginManagerDialog: null
    property var themeEditorDialog: null
    property var propertiesDialog: null
    property var deleteConfirmDialog: null
    property var isoMountDialog: null
    property var nestedArchiveDialog: null
    property var archivePasswordDialog: null
    property var diskUsageDialog: null
    property var fileSearchDialog: null
    property var batchRenameDialog: null
    property var checksumDialog: null
    property var debugInformationDialog: null
    property var commandPalette: null
    property var pluginActionResultDialog: null
    property bool searchReturnAvailable: false

    function isOpen(item) {
        return !!item && (item.opened || item.visible)
    }

    function ensureConflictDialog() {
        if (!root.conflictDialog) root.conflictDialog = conflictDialogComponent.createObject(root)
        return root.conflictDialog
    }

    function ensureHelpDialog() {
        if (!root.helpDialog) root.helpDialog = helpDialogComponent.createObject(root)
        return root.helpDialog
    }

    function ensureSettingsDialog() {
        if (!root.settingsDialog) root.settingsDialog = settingsDialogComponent.createObject(root)
        return root.settingsDialog
    }

    function ensurePluginManagerDialog() {
        if (!root.pluginManagerDialog) root.pluginManagerDialog = pluginManagerDialogComponent.createObject(root)
        return root.pluginManagerDialog
    }

    function ensureThemeEditorDialog() {
        if (!root.themeEditorDialog) root.themeEditorDialog = themeEditorDialogComponent.createObject(root)
        return root.themeEditorDialog
    }

    function ensurePropertiesDialog() {
        if (!root.propertiesDialog) root.propertiesDialog = propertiesDialogComponent.createObject(root)
        return root.propertiesDialog
    }

    function ensureDeleteConfirmDialog() {
        if (!root.deleteConfirmDialog) root.deleteConfirmDialog = deleteConfirmDialogComponent.createObject(root)
        return root.deleteConfirmDialog
    }

    function ensureIsoMountDialog() {
        if (!root.isoMountDialog) root.isoMountDialog = isoMountDialogComponent.createObject(root)
        return root.isoMountDialog
    }

    function ensureNestedArchiveDialog() {
        if (!root.nestedArchiveDialog) root.nestedArchiveDialog = nestedArchiveDialogComponent.createObject(root)
        return root.nestedArchiveDialog
    }

    function ensureArchivePasswordDialog() {
        if (!root.archivePasswordDialog) root.archivePasswordDialog = archivePasswordDialogComponent.createObject(root)
        return root.archivePasswordDialog
    }

    function ensureDiskUsageDialog() {
        if (!root.diskUsageDialog) root.diskUsageDialog = diskUsageDialogComponent.createObject(root)
        return root.diskUsageDialog
    }

    function ensureFileSearchDialog() {
        if (!root.fileSearchDialog) root.fileSearchDialog = fileSearchDialogComponent.createObject(root)
        return root.fileSearchDialog
    }

    function ensureBatchRenameDialog() {
        if (!root.batchRenameDialog) root.batchRenameDialog = batchRenameDialogComponent.createObject(root)
        return root.batchRenameDialog
    }

    function ensureChecksumDialog() {
        if (!root.checksumDialog) root.checksumDialog = checksumDialogComponent.createObject(root)
        return root.checksumDialog
    }

    function ensureDebugInformationDialog() {
        if (!root.debugInformationDialog) root.debugInformationDialog = debugInformationDialogComponent.createObject(root)
        return root.debugInformationDialog
    }

    function ensureCommandPalette() {
        if (!root.commandPalette) root.commandPalette = commandPaletteComponent.createObject(root)
        return root.commandPalette
    }

    readonly property bool workspaceOverlayOpen: root.isOpen(root.conflictDialog)
                                                 || root.isOpen(root.helpDialog)
                                                 || root.isOpen(root.settingsDialog)
                                                 || root.isOpen(root.pluginManagerDialog)
                                                 || root.isOpen(root.themeEditorDialog)
                                                 || root.isOpen(root.propertiesDialog)
                                                 || root.isOpen(root.isoMountDialog)
                                                 || root.isOpen(root.nestedArchiveDialog)
                                                 || root.isOpen(root.archivePasswordDialog)
                                                 || root.isOpen(root.deleteConfirmDialog)
                                                 || root.isOpen(root.diskUsageDialog)
                                                 || root.isOpen(root.fileSearchDialog)
                                                 || root.isOpen(root.batchRenameDialog)
                                                 || root.isOpen(root.checksumDialog)
                                                 || root.isOpen(root.debugInformationDialog)
                                                 || root.isOpen(root.pluginActionResultDialog)
    readonly property bool anyOverlayOpen: root.workspaceOverlayOpen
                                           || root.isOpen(root.commandPalette)

    function openDeleteConfirm(paths, label, items) {
        const list = paths ? Array.from(paths) : []
        if (list.length === 0) {
            return
        }
        const details = workspaceController && workspaceController.deleteRequestDetails
                      ? workspaceController.deleteRequestDetails(list, label || "")
                      : ({})
        root.ensureDeleteConfirmDialog().openFor(list, label || "", details, items || [])
    }

    function openCommandPalette() {
        root.ensureCommandPalette().openPalette()
    }

    function openCommandPaletteForCommand(commandId) {
        root.ensureCommandPalette().openCommandArgument(commandId)
    }

    function openHelpDialog() {
        root.ensureHelpDialog().open()
    }

    function openSettingsDialog() {
        root.ensureSettingsDialog().open()
    }

    function openPluginManagerDialog() {
        root.ensurePluginManagerDialog().open()
    }

    function openThemeEditorDialog() {
        root.ensureThemeEditorDialog().open()
    }

    function openDebugInformationDialog() {
        root.ensureDebugInformationDialog().open()
    }

    function copyPropertiesToClipboard() {
        if (root.propertiesDialog && root.propertiesDialog.visible) {
            root.propertiesDialog.copyAll()
        } else {
            const ctrl = appRoot ? appRoot.activePanelController() : null
            if (!ctrl) return
            const selected = ctrl.selectedPaths()
            if (!selected || selected.length === 0) return

            const dialog = root.ensurePropertiesDialog()
            dialog.suppressDialog = true
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
            dialog.suppressDialog = false
        }
    }

    function exportPropertiesToFile(format) {
        if (root.propertiesDialog && root.propertiesDialog.visible) {
            root.propertiesDialog.openExportMenu()
        } else {
            const ctrl = appRoot ? appRoot.activePanelController() : null
            if (!ctrl) return
            const selected = ctrl.selectedPaths()
            if (!selected || selected.length === 0) return

            const dialog = root.ensurePropertiesDialog()
            dialog.suppressDialog = true
            dialog.exportDialogPending = true
            if (selected.length > 1) {
                propertiesController.loadMultiple(selected)
            } else {
                propertiesController.load(selected[0])
            }
            dialog.silentExport(format || "json")
        }
    }

    function closeTopOverlay() {
        if (root.isOpen(root.themeEditorDialog) && !root.themeEditorDialog.childDialogOpen()) {
            root.themeEditorDialog.closeEditor()
            return true
        }
        if (root.isOpen(root.pluginManagerDialog)) {
            root.pluginManagerDialog.accept()
            return true
        }
        if (root.isOpen(root.settingsDialog)) {
            root.settingsDialog.accept()
            return true
        }
        if (root.isOpen(root.helpDialog)) {
            root.helpDialog.close()
            return true
        }
        if (root.isOpen(root.propertiesDialog)) {
            root.propertiesDialog.close()
            return true
        }
        if (root.isOpen(root.isoMountDialog)) {
            root.isoMountDialog.close()
            return true
        }
        if (root.isOpen(root.nestedArchiveDialog)) {
            root.nestedArchiveDialog.close()
            return true
        }
        if (root.isOpen(root.archivePasswordDialog)) {
            root.archivePasswordDialog.close()
            return true
        }
        if (root.isOpen(root.deleteConfirmDialog)) {
            root.deleteConfirmDialog.close()
            return true
        }
        if (root.isOpen(root.diskUsageDialog)) {
            root.diskUsageDialog.accept()
            return true
        }
        if (root.isOpen(root.fileSearchDialog)) {
            root.fileSearchDialog.accept()
            return true
        }
        if (root.isOpen(root.batchRenameDialog)) {
            root.batchRenameDialog.reject()
            return true
        }
        if (root.isOpen(root.checksumDialog)) {
            root.checksumDialog.accept()
            return true
        }
        if (root.isOpen(root.debugInformationDialog)) {
            root.debugInformationDialog.close()
            return true
        }
        if (root.isOpen(root.pluginActionResultDialog)) {
            root.pluginActionResultDialog.close()
            return true
        }
        return false
    }

    function openSettingsImportDialog() {
        root.ensureSettingsDialog().openImportDialog()
    }

    function openSettingsExportDialog() {
        root.ensureSettingsDialog().openExportDialog()
    }

    function openDiskUsage(path) {
        if (!path || path.length === 0) return
        root.ensureDiskUsageDialog().openFor(path)
    }

    function openFileSearch(path, includeHidden) {
        if (!path || path.length === 0) return
        root.searchReturnAvailable = false
        root.ensureFileSearchDialog().openFor(path, includeHidden === true)
    }

    function openNestedArchive(controller, path, displayName, sizeText) {
        root.ensureNestedArchiveDialog().openFor(controller, path, displayName || "", sizeText || "")
    }

    function openArchivePassword(controller, path, displayName, message) {
        root.ensureArchivePasswordDialog().openFor(controller, path, displayName || "", message || "")
    }

    function reopenFileSearchResults() {
        if (!root.searchReturnAvailable) {
            return
        }
        root.ensureFileSearchDialog().reopenResults()
    }

    function showBatchRename(paths) {
        if (!paths || paths.length === 0) return
        if (root.appRoot && root.appRoot.beginRenamePreviewSuppression) {
            root.appRoot.beginRenamePreviewSuppression(paths)
        }
        const dialog = root.ensureBatchRenameDialog()
        dialog.sourcePaths = paths
        dialog.controller = workspaceController.activePanel === 0
                                       ? workspaceController.leftPanel
                                       : workspaceController.rightPanel
        dialog.open()
    }

    function showChecksums(paths) {
        if (!paths || paths.length === 0) return
        const dialog = root.ensureChecksumDialog()
        dialog.path1 = paths[0]
        dialog.path2 = paths.length > 1 ? paths[1] : ""
        dialog.controller = workspaceController.activePanel === 0
                                     ? workspaceController.leftPanel
                                     : workspaceController.rightPanel
        dialog.open()
    }

    function openPluginActionResult(result) {
        if (!root.pluginActionResultDialog) {
            root.pluginActionResultDialog = pluginActionResultDialogComponent.createObject(root)
        }
        root.pluginActionResultDialog.showResult(result)
    }

    Component {
        id: conflictDialogComponent
        ConflictDialog {}
    }

    Component {
        id: helpDialogComponent
        HelpDialog {}
    }

    Component {
        id: settingsDialogComponent
        SettingsDialog {
            appRoot: root.appRoot
            onThemeEditorRequested: root.openThemeEditorDialog()
            onPluginManagerRequested: root.openPluginManagerDialog()
        }
    }

    Component {
        id: pluginManagerDialogComponent
        PluginManagerDialog {}
    }

    Component {
        id: themeEditorDialogComponent
        ThemeEditorDialog {
            parent: Overlay.overlay
        }
    }

    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        enabled: root.isOpen(root.themeEditorDialog)
                 && !root.themeEditorDialog.childDialogOpen()
        onActivated: root.closeTopOverlay()
        onActivatedAmbiguously: root.closeTopOverlay()
    }

    Component {
        id: propertiesDialogComponent
        PropertiesDialog {
            appRoot: root.appRoot
        }
    }

    Component {
        id: deleteConfirmDialogComponent
        DeleteConfirmDialog {
            appRoot: root.appRoot
        }
    }

    Component {
        id: isoMountDialogComponent
        IsoMountDialog {}
    }

    Component {
        id: nestedArchiveDialogComponent
        NestedArchiveDialog {}
    }

    Component {
        id: archivePasswordDialogComponent
        ArchivePasswordDialog {}
    }

    Component {
        id: diskUsageDialogComponent
        DiskUsageDialog {
            appRoot: root.appRoot
        }
    }

    Component {
        id: fileSearchDialogComponent
        FileSearchDialog {
            appRoot: root.appRoot
            onResultOpened: {
                root.searchReturnAvailable = true
            }
            onSearchContextReset: {
                root.searchReturnAvailable = false
            }
        }
    }

    Component {
        id: batchRenameDialogComponent
        BatchRenameDialog {
            appRoot: root.appRoot
        }
    }

    Component {
        id: checksumDialogComponent
        ChecksumDialog {}
    }

    Component {
        id: debugInformationDialogComponent
        DebugInformationDialog {
            appRoot: root.appRoot
        }
    }

    Component {
        id: commandPaletteComponent
        CommandPalette {
            commands: root.commandPaletteCommands
            activePanelController: root.appRoot ? root.appRoot.activePanelController : null
        }
    }

    Component {
        id: pluginActionResultDialogComponent
        PluginActionResultDialog {}
    }

    Connections {
        target: workspaceController.operationQueue
        function onConflictDetected(source, destination, sourceSize, sourceModified, destSize, destModified) {
            const dialog = root.ensureConflictDialog()
            dialog.sourcePath = source
            dialog.destinationPath = destination
            dialog.sourceSize = sourceSize
            dialog.sourceModified = sourceModified
            dialog.destSize = destSize
            dialog.destModified = destModified
            dialog.open()
        }
    }

    Connections {
        target: propertiesController
        function onVisibleChanged() {
            if (propertiesController.visible) {
                root.ensurePropertiesDialog()
            }
        }
    }

    Connections {
        target: workspaceController
        function onDeleteRequested(paths, label, items) {
            root.openDeleteConfirm(paths, label, items)
        }
        function onMountIsoRequested(path) {
            root.ensureIsoMountDialog().openFor(path)
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
            root.ensurePropertiesDialog().suppressDialog = false
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
            root.ensurePropertiesDialog().suppressDialog = false
            propertiesController.loadMultiple(paths)
        }
    }
}
