import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "components"
import "style"

ApplicationWindow {
    id: root

    width: 1120
    height: 720
    minimumWidth: 760
    minimumHeight: 480
    visible: false
    title: "FM"
    color: Theme.bg

    function openDeleteConfirm(paths, label) {
        workspaceOverlays.openDeleteConfirm(paths, label)
    }

    function activePanelController() {
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    property bool previewOnHover: false
    property bool previewPaneVisible: false
    readonly property var workspaceService: workspaceController
    readonly property var quickLookService: quickLookController
    readonly property var propertiesService: propertiesController
    readonly property bool sidebarFocused: sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
    readonly property bool anyOverlayOpen: workspaceOverlays.anyOverlayOpen
                                           || quickLookPopup.opened || quickLookPopup.visible
    readonly property bool workspaceOverlayOpen: workspaceOverlays.workspaceOverlayOpen
                                                 || quickLookPopup.opened || quickLookPopup.visible
    readonly property bool workspaceCommandsEnabled: !root.workspaceOverlayOpen
                                                      && !mainToolbar.textEditingActive
                                                      && !fileWorkspace.isRenaming
    readonly property bool panelShortcutsEnabled: !root.anyOverlayOpen
                                                  && !root.sidebarFocused
                                                  && !mainToolbar.textEditingActive
                                                  && !fileWorkspace.isRenaming
    readonly property bool tabPanelSwitchEnabled: !root.anyOverlayOpen
                                                  && !mainToolbar.textEditingActive
                                                  && !fileWorkspace.isRenaming
                                                  && (!root.sidebarFocused || !sidebar.trapTabNavigation)
    readonly property bool splitViewShortcutEnabled: !root.anyOverlayOpen
                                                    && !mainToolbar.textEditingActive
                                                    && !fileWorkspace.isRenaming
    readonly property bool typeToSearchEnabled: root.panelShortcutsEnabled

    function toggleSplitView() {
        workspaceController.toggleSplit()
    }

    function openCommandPalette() {
        workspaceOverlays.openCommandPalette()
    }

    function goBackInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.goBack()
        }
    }

    function goForwardInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.goForward()
        }
    }

    function goUpInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.goUp()
        }
    }

    function refreshActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.refresh()
        }
    }

    function toggleHiddenFiles() {
        const ctrl = activePanelController()
        if (ctrl) {
            const newValue = !ctrl.directoryModel.showHidden
            ctrl.directoryModel.showHidden = newValue
            workspaceController.treeModel.showHidden = newValue
        }
    }

    function setThemeScheme(scheme) {
        themeController.scheme = scheme
    }

    function openThemeSelector() {
        mainToolbar.openThemeSelector()
    }

    function importThemeFromFile() {
        mainToolbar.openThemeImportDialog()
    }

    function exportCurrentTheme() {
        mainToolbar.openThemeExportDialog()
    }

    function setActiveViewMode(mode) {
        const ctrl = activePanelController()
        if (ctrl) {
            ctrl.viewMode = mode
        }
    }

    function focusActiveSidebar() {
        sidebar.focusSidebar(true)
    }

    function focusActivePath() {
        mainToolbar.focusPath()
    }

    function focusActiveSearch() {
        mainToolbar.focusSearch()
    }

    function createFolderInActivePanel() {
        const ctrl = activePanelController()
        if (ctrl) {
            const path = ctrl.currentPath || ""
            if (path.toLowerCase().startsWith("archive://")
                    || workspaceController.isInsideManagedIsoMount(path)) {
                return
            }
            ctrl.createFolder("New Folder")
        }
    }

    function renameActiveSelection() {
        workspaceController.triggerRename()
    }

    function copyActiveSelection() {
        workspaceController.copyToClipboard()
    }

    function cutActiveSelection() {
        workspaceController.cutToClipboard()
    }

    function pasteClipboardToActivePanel() {
        workspaceController.pasteFromClipboard()
    }

    function requestDeleteActiveSelection() {
        const active = activePanelController()
        if (active) {
            workspaceController.requestDelete(active.selectedPaths(), active.currentPath)
        }
    }

    function showActiveProperties() {
        const ctrl = activePanelController()
        if (!ctrl) {
            return
        }

        const selected = ctrl.selectedPaths()
        if (!selected || selected.length === 0) {
            return
        }

        if (selected.length > 1) {
            propertiesController.loadMultiple(selected)
        } else {
            propertiesController.load(selected[0])
        }
    }

    function showActiveChecksums() {
        const ctrl = activePanelController()
        if (!ctrl) {
            return
        }
        const selected = ctrl.selectedPaths()
        if (selected && selected.length > 0) {
            showChecksums(selected)
        }
    }

    function quickLookActiveTarget() {
        const controller = activePanelController()
        const targetPath = previewTargetFor(controller)
        if (targetPath.length === 0) {
            return
        }

        quickLookController.preview(targetPath)
        quickLookPopup.previewPath = targetPath
        quickLookPopup.open()
    }

    function openHelpDialog() {
        workspaceOverlays.openHelpDialog()
    }

    function previewTargetFor(controller) {
        return previewCoordinator.previewTargetFor(controller)
    }

    function syncPreviewFromActivePanel(immediate) {
        previewCoordinator.syncPreviewFromActivePanel(immediate)
    }

    function scheduleHoverPreview(path) {
        previewCoordinator.scheduleHoverPreview(path)
    }

    function togglePreviewPane() {
        previewCoordinator.togglePreviewPane()
    }

    AppShortcuts {
        id: appShortcuts
        appRoot: root
        workspaceController: root.workspaceService
        quickLookController: root.quickLookService
        propertiesController: root.propertiesService
        sidebar: sidebar
        mainToolbar: mainToolbar
        fileWorkspace: fileWorkspace
        quickLookPopup: quickLookPopup
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.text.length > 0 && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.ShiftModifier)) {
                // Ignore Space, Enter/Return as they are handled by shortcuts or specific components
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                    return;

                if (root.typeToSearchEnabled) {
                     mainToolbar.focusSearch()
                }
            }
        }

        MainToolbar {
            id: mainToolbar
            Layout.fillWidth: true
            workspaceController: root.workspaceService
            previewVisible: root.previewPaneVisible
            onPreviewToggleRequested: (visible) => {
                previewCoordinator.setPreviewPaneVisible(visible)
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Sidebar {
                id: sidebar
                SplitView.preferredWidth: 200
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 300
            }

            FileWorkspace {
                id: fileWorkspace
                SplitView.fillWidth: true
                workspaceController: root.workspaceService
                propertiesController: root.propertiesService
            }

            PreviewPane {
                SplitView.preferredWidth: root.previewPaneVisible ? 340 : 0
                SplitView.minimumWidth: root.previewPaneVisible ? 280 : 0
                SplitView.fillWidth: false
                visible: root.previewPaneVisible || width > 0
                opacity: root.previewPaneVisible ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
            }

            handle: Rectangle {
                implicitWidth: 1
                color: Theme.border
            }
        }
    }

    QuickLook {
        id: quickLookPopup
    }

    CommandRegistry {
        id: commandRegistry
        workspaceCommandsEnabled: root.workspaceCommandsEnabled
        anyOverlayOpen: root.anyOverlayOpen
        workspaceController: root.workspaceService
        activePanelController: root.activePanelController
        goBackInActivePanel: root.goBackInActivePanel
        goForwardInActivePanel: root.goForwardInActivePanel
        goUpInActivePanel: root.goUpInActivePanel
        focusActivePath: root.focusActivePath
        focusActiveSearch: root.focusActiveSearch
        focusActiveSidebar: root.focusActiveSidebar
        toggleSplitView: root.toggleSplitView
        togglePreviewPane: root.togglePreviewPane
        refreshActivePanel: root.refreshActivePanel
        toggleHiddenFiles: root.toggleHiddenFiles
        setThemeScheme: root.setThemeScheme
        openThemeSelector: root.openThemeSelector
        importThemeFromFile: root.importThemeFromFile
        exportCurrentTheme: root.exportCurrentTheme
        createFolderInActivePanel: root.createFolderInActivePanel
        renameActiveSelection: root.renameActiveSelection
        copyActiveSelection: root.copyActiveSelection
        cutActiveSelection: root.cutActiveSelection
        pasteClipboardToActivePanel: root.pasteClipboardToActivePanel
        requestDeleteActiveSelection: root.requestDeleteActiveSelection
        showActiveProperties: root.showActiveProperties
        showActiveChecksums: root.showActiveChecksums
        quickLookActiveTarget: root.quickLookActiveTarget
        openHelpDialog: root.openHelpDialog
    }

    WorkspaceOverlays {
        id: workspaceOverlays
        commandPaletteCommands: commandRegistry.commands
    }

    PreviewCoordinator {
        id: previewCoordinator
        appRoot: root
        workspaceController: root.workspaceService
        quickLookController: root.quickLookService
        quickLookPopup: quickLookPopup
        propertiesController: root.propertiesService
    }

    function showBatchRename(paths) {
        workspaceOverlays.showBatchRename(paths)
    }

    function showChecksums(paths) {
        workspaceOverlays.showChecksums(paths)
    }
}

