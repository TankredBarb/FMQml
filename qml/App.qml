import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
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
    color: Theme.panelSurface

    function openDeleteConfirm(paths, label) {
        workspaceOverlays.openDeleteConfirm(paths, label)
    }

    function activePanelController() {
        if (fileWorkspace && workspaceController.splitEnabled) {
            if (fileWorkspace.leftPanelView && fileWorkspace.leftPanelView.containsActiveFocus) {
                if (workspaceController.activePanel !== 0) {
                    workspaceController.activePanel = 0
                }
                return workspaceController.leftPanel
            }
            if (fileWorkspace.rightPanelView && fileWorkspace.rightPanelView.containsActiveFocus) {
                if (workspaceController.activePanel !== 1) {
                    workspaceController.activePanel = 1
                }
                return workspaceController.rightPanel
            }
        }
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    function activePanelView() {
        return fileWorkspace ? fileWorkspace.activePanelView() : null
    }

    function navigateActivePanel(path) {
        const panelView = activePanelView()
        const ctrl = activePanelController()
        if ((panelView || ctrl) && path && path.trim().length > 0) {
            const opened = panelView && panelView.openPath
                         ? panelView.openPath(path.trim())
                         : ctrl.openPath(path.trim())
            if (!opened) {
                showTransientInfo("Path is invalid, unavailable, or not a folder.")
                return false
            }
            return true
        }
        showTransientInfo("Enter a valid folder path.")
        return false
    }

    function quitApplication() {
        root.forceQuitRequested = true
        workspaceStateSaveTimer.stop()
        saveWorkspaceStateNow(true)
        Qt.quit()
    }

    property bool previewPaneVisible: false
    property bool workspaceStateRestored: false
    property bool workspaceStateSavePaused: false
    property bool workspaceStateRestoreActive: false
    property bool forceQuitRequested: false
    property int workspaceStateRestoreGeneration: 0
    property bool mainSplitResizing: false
    property bool previewPaneTransitionActive: false
    property string transientInfoMessage: ""
    property real sidebarStoredWidth: 200
    property real previewPaneStoredWidth: 340
    property real sidebarPreferredWidth: 200
    property real previewPanePreferredWidth: 0
    property var previewPanePendingWorkspaceSplitState: null
    readonly property real transientInfoBottomInset: 20 + Math.max(
                                                         fileWorkspace && fileWorkspace.leftPanel
                                                             ? fileWorkspace.leftPanel.footerHeight
                                                             : 32,
                                                         fileWorkspace && fileWorkspace.rightPanel
                                                             ? fileWorkspace.rightPanel.footerHeight
                                                             : 32) + 10
    readonly property bool anyLiveResize: root.mainSplitResizing || fileWorkspace.splitResizing
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
                                                  && !mainToolbar.textEditingActive
                                                  && !fileWorkspace.isRenaming
    readonly property bool fileViewShortcutsEnabled: root.panelShortcutsEnabled
                                                     && !root.sidebarFocused
    readonly property bool tabPanelSwitchEnabled: !root.anyOverlayOpen
                                                  && !mainToolbar.textEditingActive
                                                  && !fileWorkspace.isRenaming
                                                  && (!root.sidebarFocused || !sidebar.trapTabNavigation)
    readonly property bool splitViewShortcutEnabled: !root.anyOverlayOpen
                                                    && !mainToolbar.textEditingActive
                                                    && !fileWorkspace.isRenaming
    readonly property bool typeToSearchEnabled: root.fileViewShortcutsEnabled

    function toggleSplitView() {
        if (workspaceController.splitEnabled) {
            workspaceController.toggleSplit()
            Qt.callLater(() => fileWorkspace.expandSinglePanel())
        } else {
            workspaceController.toggleSplit()
            Qt.callLater(() => fileWorkspace.splitEvenly())
        }
    }

    function mirrorActivePanelToOpposite() {
        const wasSplit = workspaceController.splitEnabled
        workspaceController.mirrorActivePanelToOpposite()
        if (!wasSplit) {
            Qt.callLater(() => fileWorkspace.splitEvenly())
        }
    }

    function scheduleWorkspaceStateSave() {
        if (root.workspaceStateRestored && !root.workspaceStateSavePaused) {
            workspaceStateSaveTimer.restart()
        }
    }

    function saveWorkspaceStateNow(includePanelLayout) {
        if (!appSettings || !root.workspaceStateRestored || root.workspaceStateSavePaused) {
            return
        }

        const activeCtrl = activePanelController()
        const state = {
            windowX: root.x,
            windowY: root.y,
            windowWidth: root.width,
            windowHeight: root.height,
            windowMaximized: root.visibility === Window.Maximized,
            splitEnabled: workspaceController.splitEnabled,
            activePanel: workspaceController.activePanel,
            previewPaneVisible: root.previewPaneVisible,
            leftPath: workspaceController.leftPanel.currentPath,
            rightPath: workspaceController.rightPanel.currentPath,
            leftViewMode: workspaceController.leftPanel.viewMode,
            rightViewMode: workspaceController.rightPanel.viewMode,
            leftGridIconSize: fileWorkspace.leftPanelView.gridIconSize,
            rightGridIconSize: fileWorkspace.rightPanelView.gridIconSize,
            leftBriefRowHeight: fileWorkspace.leftPanelView.briefRowHeight,
            rightBriefRowHeight: fileWorkspace.rightPanelView.briefRowHeight,
            leftShowActionBar: fileWorkspace.leftPanelView.showActionBar,
            rightShowActionBar: fileWorkspace.rightPanelView.showActionBar,
            leftDetailsVisualState: fileWorkspace.leftPanelView.detailsVisualState(),
            rightDetailsVisualState: fileWorkspace.rightPanelView.detailsVisualState(),
            leftSortRole: workspaceController.leftPanel.panelSortRole,
            rightSortRole: workspaceController.rightPanel.panelSortRole,
            leftSortOrder: workspaceController.leftPanel.panelSortOrder,
            rightSortOrder: workspaceController.rightPanel.panelSortOrder,
            leftMixFilesAndFolders: workspaceController.leftPanel.directoryModel.mixFilesAndFolders,
            rightMixFilesAndFolders: workspaceController.rightPanel.directoryModel.mixFilesAndFolders,
            showHidden: activeCtrl ? activeCtrl.directoryModel.showHidden
                                   : workspaceController.leftPanel.directoryModel.showHidden
        }

        if (includePanelLayout) {
            state.sidebarWidth = Math.round(Math.max(140, root.sidebarStoredWidth))
            state.previewPaneWidth = Math.round(Math.max(280, root.previewPaneStoredWidth))
            state.fileWorkspaceSplitState = fileWorkspace.saveSplitState()
        }

        appSettings.saveWorkspaceState(state)
    }

    function stopWorkspaceStatePersistenceTimers() {
        workspaceStateSaveTimer.stop()
        sidebarWidthCommitTimer.stop()
        previewPaneWidthCommitTimer.stop()
        previewPaneTransitionTimer.stop()
    }

    function restoreWorkspaceState() {
        if (!appSettings) {
            root.workspaceStateRestored = true
            return
        }

        const state = appSettings.workspaceState()
        root.restoreWorkspaceStateFrom(state)
    }

    function restoreWorkspaceStateFrom(state) {
        if (!state) {
            root.workspaceStateRestored = true
            return
        }

        const restoreGeneration = ++root.workspaceStateRestoreGeneration
        const showHidden = !!state.showHidden

        function applyPanelState() {
            if (workspaceController.leftPanel.currentPath !== state.leftPath) {
                workspaceController.leftPanel.openPath(state.leftPath)
            }
            if (workspaceController.rightPanel.currentPath !== state.rightPath) {
                workspaceController.rightPanel.openPath(state.rightPath)
            }
            workspaceController.leftPanel.viewMode = state.leftViewMode
            workspaceController.rightPanel.viewMode = state.rightViewMode
            workspaceController.leftPanel.setPanelSortPolicy(state.leftSortRole, state.leftSortOrder)
            workspaceController.rightPanel.setPanelSortPolicy(state.rightSortRole, state.rightSortOrder)
            workspaceController.leftPanel.directoryModel.mixFilesAndFolders = state.leftMixFilesAndFolders === true
            workspaceController.rightPanel.directoryModel.mixFilesAndFolders = state.rightMixFilesAndFolders === true
        }

        stopWorkspaceStatePersistenceTimers()
        root.workspaceStateRestoreActive = true
        root.workspaceStateSavePaused = true
        root.workspaceStateRestored = false
        root.previewPaneTransitionActive = false
        root.previewPanePendingWorkspaceSplitState = null
        previewCoordinator.clearPreviewTimers()

        const geometry = appSettings.sanitizedWindowGeometry(state, 1120, 720)
        if (geometry.valid) {
            if (root.visibility === Window.Maximized) {
                root.visibility = Window.Windowed
            }
            root.x = geometry.x
            root.y = geometry.y
            root.width = geometry.width
            root.height = geometry.height
        }

        root.sidebarStoredWidth = state.sidebarWidth
        root.previewPaneStoredWidth = state.previewPaneWidth
        root.sidebarPreferredWidth = root.sidebarStoredWidth
        root.previewPanePreferredWidth = !!state.previewPaneVisible ? Math.max(280, root.previewPaneStoredWidth) : 0

        workspaceController.leftPanel.directoryModel.showHidden = showHidden
        workspaceController.rightPanel.directoryModel.showHidden = showHidden
        workspaceController.treeModel.showHidden = showHidden
        workspaceController.splitEnabled = !!state.splitEnabled
        applyPanelState()

        fileWorkspace.leftPanelView.gridIconSize = state.leftGridIconSize
        fileWorkspace.rightPanelView.gridIconSize = state.rightGridIconSize
        fileWorkspace.leftPanelView.briefRowHeight = state.leftBriefRowHeight
        fileWorkspace.rightPanelView.briefRowHeight = state.rightBriefRowHeight
        fileWorkspace.leftPanelView.showActionBar = state.leftShowActionBar !== false
        fileWorkspace.rightPanelView.showActionBar = state.rightShowActionBar !== false
        fileWorkspace.leftPanelView.restoreDetailsVisualState(state.leftDetailsVisualState)
        fileWorkspace.rightPanelView.restoreDetailsVisualState(state.rightDetailsVisualState)
        previewCoordinator.setPreviewPaneVisible(!!state.previewPaneVisible)
        root.applyPreviewPaneWidth()

        Qt.callLater(() => {
            if (restoreGeneration !== root.workspaceStateRestoreGeneration) {
                return
            }
            root.sidebarStoredWidth = state.sidebarWidth
            root.previewPaneStoredWidth = state.previewPaneWidth
            root.sidebarPreferredWidth = root.sidebarStoredWidth
            applyPanelState()
            root.applyPreviewPaneWidth()
            if (workspaceController.splitEnabled) {
                fileWorkspace.restoreSplitState(state.fileWorkspaceSplitState)
            } else {
                fileWorkspace.expandSinglePanel()
            }

            Qt.callLater(() => {
                if (restoreGeneration !== root.workspaceStateRestoreGeneration) {
                    return
                }
                applyPanelState()
                workspaceController.activePanel = workspaceController.splitEnabled ? state.activePanel : 0
                root.applyPreviewPaneWidth()
                if (workspaceController.splitEnabled) {
                    fileWorkspace.restoreSplitState(state.fileWorkspaceSplitState)
                }

                Qt.callLater(() => {
                    if (restoreGeneration !== root.workspaceStateRestoreGeneration) {
                        return
                    }
                    if (root.visible) {
                        if (state.windowMaximized) {
                            root.visibility = Window.Maximized
                        } else if (root.visibility === Window.Maximized) {
                            root.visibility = Window.Windowed
                        }
                    }
                    previewCoordinator.syncPreviewFromActivePanel(true)
                    root.workspaceStateRestoreActive = false
                    root.workspaceStateSavePaused = false
                    root.workspaceStateRestored = true
                })
            })
        })
    }

    function applyPreviewPaneWidth() {
        if (!previewPane) {
            return
        }
        root.previewPanePreferredWidth = root.previewPaneVisible
            ? Math.max(280, root.previewPaneStoredWidth)
            : 0
    }

    function beginPreviewPaneTransition() {
        if (workspaceController.splitEnabled) {
            root.previewPanePendingWorkspaceSplitState = fileWorkspace.saveSplitState()
        } else {
            root.previewPanePendingWorkspaceSplitState = null
        }
        root.previewPaneTransitionActive = true
        previewPaneTransitionTimer.restart()
    }

    function finishPreviewPaneTransition() {
        if (root.previewPanePendingWorkspaceSplitState !== null
                && root.previewPanePendingWorkspaceSplitState !== undefined
                && workspaceController.splitEnabled) {
            fileWorkspace.restoreSplitState(root.previewPanePendingWorkspaceSplitState)
        }
        root.previewPanePendingWorkspaceSplitState = null
        root.previewPaneTransitionActive = false
    }

    function openCommandPalette() {
        workspaceOverlays.openCommandPalette()
    }

    function openCommandPaletteForCommand(commandId) {
        workspaceOverlays.openCommandPaletteForCommand(commandId)
    }

    function goBackInActivePanel() {
        const panelView = activePanelView()
        const ctrl = activePanelController()
        if (panelView && panelView.goBack) {
            panelView.goBack()
        } else if (ctrl) {
            ctrl.goBack()
        }
    }

    function goForwardInActivePanel() {
        const panelView = activePanelView()
        const ctrl = activePanelController()
        if (panelView && panelView.goForward) {
            panelView.goForward()
        } else if (ctrl) {
            ctrl.goForward()
        }
    }

    function goUpInActivePanel() {
        const panelView = activePanelView()
        const ctrl = activePanelController()
        if (panelView && panelView.goUp) {
            panelView.goUp()
        } else if (ctrl) {
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
            workspaceController.leftPanel.directoryModel.showHidden = newValue
            workspaceController.rightPanel.directoryModel.showHidden = newValue
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

    function setActiveViewMode(mode) {
        const ctrl = activePanelController()
        if (ctrl && !ctrl.isFavoritesRoot) {
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
        const ctrl = activePanelController()
        if (ctrl && ctrl.isFavoritesRoot) {
            return
        }
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

    function copyActiveSelectionToOpposite() {
        workspaceController.copyActiveSelectionToOpposite()
    }

    function moveActiveSelectionToOpposite() {
        workspaceController.moveActiveSelectionToOpposite()
    }

    function duplicateActiveSelection() {
        workspaceController.duplicateActiveSelection()
    }

    function compressActiveSelection(format) {
        workspaceController.compressActiveSelection(format || "7z")
    }

    function cutActiveSelection() {
        workspaceController.cutToClipboard()
    }

    function pasteClipboardToActivePanel() {
        workspaceController.pasteFromClipboard()
    }

    function addSelectionToFavorites() {
        const ctrl = activePanelController()
        if (!ctrl || ctrl.isVirtualRoot) {
            return
        }
        const selected = ctrl.selectedPaths()
        if (!selected || selected.length === 0 || !favoritesController) {
            return
        }
        for (let i = 0; i < selected.length; ++i) {
            if (String(selected[i]).toLowerCase().startsWith("archive://")) {
                return
            }
        }
        const changed = favoritesController.pinPaths(selected)
        showTransientInfo(changed > 0
                          ? (changed + (changed === 1 ? " item pinned to Favorites" : " items pinned to Favorites"))
                          : "Selection is already pinned to Favorites")
    }

    function requestDeleteActiveSelection() {
        const active = activePanelController()
        if (active) {
            workspaceController.requestDelete(active.selectedPaths(), active.currentPath)
        }
    }

    function showActiveProperties(tabIndex) {
        const ctrl = activePanelController()
        if (!ctrl) {
            return
        }

        const selected = ctrl.selectedPaths()
        if (!selected || selected.length === 0) {
            return
        }

        if (workspaceOverlays.propertiesDialog) {
            workspaceOverlays.propertiesDialog.suppressDialog = false
            if (typeof tabIndex === "number") {
                workspaceOverlays.propertiesDialog.requestedTab = tabIndex
            }
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
        if (!selected || selected.length === 0) {
            return
        }
        if (selected.length === 1) {
            showActiveProperties(3)
            return
        }
        if (selected.length === 2) {
            showChecksums(selected)
        }
    }

    function quickLookActiveTarget() {
        const controller = activePanelController()
        const targetPath = previewTargetFor(controller)
        if (targetPath.length === 0) {
            return
        }

        const selected = controller ? controller.selectedPaths() : []
        if (targetPath === "selection://" && selected && selected.length > 1) {
            quickLookController.previewSelection(selected)
        } else {
            quickLookController.preview(targetPath)
        }
        quickLookPopup.previewPath = targetPath
        quickLookPopup.open()
    }

    function openHelpDialog() {
        workspaceOverlays.openHelpDialog()
    }

    function openSettingsDialog() {
        workspaceOverlays.openSettingsDialog()
    }

    function systemTrayModeActive() {
        return typeof systemTrayController !== "undefined"
            && systemTrayController
            && systemTrayController.active
    }

    function openSettingsImportDialog() {
        workspaceOverlays.openSettingsImportDialog()
    }

    function openSettingsExportDialog() {
        workspaceOverlays.openSettingsExportDialog()
    }

    function openDiskUsage(path) {
        const target = path && path.length > 0
                     ? path
                     : (activePanelController() ? activePanelController().currentPath : "")
        workspaceOverlays.openDiskUsage(target)
    }

    function openFileSearch() {
        const ctrl = activePanelController()
        const path = ctrl && ctrl.currentPath ? String(ctrl.currentPath) : ""
        const lowerPath = path.toLowerCase()
        if (!ctrl || path.length === 0 || ctrl.isVirtualRoot
                || lowerPath.startsWith("archive://")
                || lowerPath.startsWith("devices://")
                || lowerPath.startsWith("favorites://")) {
            showTransientInfo("Open a regular folder before searching.")
            return
        }
        workspaceOverlays.openFileSearch(path,
                                         ctrl.directoryModel ? ctrl.directoryModel.showHidden : false)
    }

    function showTransientInfo(message) {
        if (!message || message.length === 0) {
            return
        }
        root.transientInfoMessage = message
        transientInfoBannerTimer.restart()
    }

    function resetSavedWorkspaceState() {
        if (appSettings) {
            stopWorkspaceStatePersistenceTimers()
            appSettings.resetWorkspaceState()
            root.workspaceStateSavePaused = true
            showTransientInfo("Saved workspace and theme will reset on the next launch.")
        }
    }

    function resetCommandUsageStats() {
        if (appSettings) {
            appSettings.resetCommandUsageStats()
            showTransientInfo("Command palette usage history was cleared.")
        }
    }

    function openSettingsDataFolder() {
        if (appSettings) {
            appSettings.openAppDataFolder()
        }
    }

    function previewTargetFor(controller) {
        return previewCoordinator.previewTargetFor(controller)
    }

    function syncPreviewFromActivePanel(immediate) {
        previewCoordinator.syncPreviewFromActivePanel(immediate)
    }

    function releasePreviewForPaths(paths) {
        if (!root.quickLookService || !paths || paths.length === 0) {
            return
        }

        const previewPath = root.quickLookService.path || ""
        const previewAbsolutePath = root.quickLookService.absolutePath || ""
        for (let i = 0; i < paths.length; ++i) {
            const path = paths[i] || ""
            if (path.length === 0) {
                continue
            }
            const normalizedPath = path.toLowerCase()
            if (previewPath === path || previewAbsolutePath === path
                    || previewPath.toLowerCase() === normalizedPath
                    || previewAbsolutePath.toLowerCase() === normalizedPath) {
                previewCoordinator.clearPreviewTimers()
                root.quickLookService.preview("")
                return
            }
        }
    }

    function togglePreviewPane() {
        root.setPreviewPaneVisible(!root.previewPaneVisible)
    }

    function setPreviewPaneVisible(visible) {
        if (root.previewPaneVisible === visible) {
            return
        }
        beginPreviewPaneTransition()
        previewCoordinator.setPreviewPaneVisible(visible)
    }

    function relaunchAsAdmin() {
        if (typeof adminController === "undefined" || !adminController) {
            return false
        }
        saveWorkspaceStateNow(true)
        return adminController.relaunchAsAdmin()
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

    Timer {
        id: workspaceStateSaveTimer
        interval: 350
        repeat: false
        onTriggered: root.saveWorkspaceStateNow(false)
    }

    Timer {
        id: sidebarWidthCommitTimer
        interval: 140
        repeat: false
        onTriggered: {
            root.sidebarPreferredWidth = Math.max(140, Math.min(300, root.sidebarStoredWidth))
        }
    }

    Timer {
        id: previewPaneWidthCommitTimer
        interval: 140
        repeat: false
        onTriggered: {
            if (root.previewPaneVisible) {
                root.previewPanePreferredWidth = Math.max(280, root.previewPaneStoredWidth)
            }
        }
    }

    Timer {
        id: previewPaneTransitionTimer
        interval: 180
        repeat: false
        onTriggered: root.finishPreviewPaneTransition()
    }

    Timer {
        id: transientInfoBannerTimer
        interval: 5000
        repeat: false
        onTriggered: root.transientInfoMessage = ""
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.text.length > 0 && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.ShiftModifier)) {
                // Ignore navigation/command keys that may still deliver text on some platforms.
                if (event.key === Qt.Key_Space
                        || event.key === Qt.Key_Return
                        || event.key === Qt.Key_Enter
                        || event.key === Qt.Key_Delete)
                    return;

                if (root.typeToSearchEnabled && mainToolbar.focusSearch(event.text)) {
                    event.accepted = true
                }
            }
        }

            MainToolbar {
            id: mainToolbar
            Layout.fillWidth: true
            appRoot: root
            workspaceController: root.workspaceService
            activePanelView: root.activePanelView()
            previewVisible: root.previewPaneVisible
            searchReturnVisible: workspaceOverlays.searchReturnAvailable && !root.anyOverlayOpen
            onPreviewToggleRequested: (visible) => {
                root.setPreviewPaneVisible(visible)
            }
            onSearchReturnRequested: workspaceOverlays.reopenFileSearchResults()
        }

        Item {
            id: mainArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle {
                anchors.fill: parent
                color: Theme.panelSurface
            }

            SplitView {
                id: mainSplitView
                anchors.fill: parent
                orientation: Qt.Horizontal

            Sidebar {
                id: sidebar
                SplitView.preferredWidth: root.sidebarPreferredWidth
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 300
                activePanelViewProvider: function() { return root.activePanelView() }
                liveResizeActive: root.anyLiveResize
                onWidthChanged: {
                    if (!root.workspaceStateRestoreActive && width >= 140) {
                        root.sidebarStoredWidth = width
                        if (Math.abs(root.sidebarPreferredWidth - width) > 0.5) {
                            sidebarWidthCommitTimer.restart()
                        }
                    }
                }
            }

            FileWorkspace {
                id: fileWorkspace
                SplitView.fillWidth: true
                liveResizeActive: root.anyLiveResize
                externalScrollActive: sidebar.treeScrollActive
                workspaceController: root.workspaceService
                propertiesController: root.propertiesService
                quickLookPopup: quickLookPopup
                onPanelVisualStateChanged: root.scheduleWorkspaceStateSave()
            }

            PreviewPane {
                id: previewPane
                SplitView.preferredWidth: root.previewPanePreferredWidth
                SplitView.minimumWidth: root.previewPaneVisible ? 280 : 0
                SplitView.fillWidth: false
                liveResizeActive: root.anyLiveResize || root.previewPaneTransitionActive || fileWorkspace.previewScrollActive
                scrollPauseActive: fileWorkspace.previewScrollActive && !root.anyLiveResize && !root.previewPaneTransitionActive
                previewPending: previewCoordinator.previewPending
                pendingPreviewPath: previewCoordinator.pendingPreviewPath
                visible: root.previewPaneVisible || width > 0
                opacity: root.previewPaneVisible ? 1.0 : 0.0
                onWidthChanged: {
                    if (!root.workspaceStateRestoreActive && root.previewPaneVisible && width >= 280) {
                        root.previewPaneStoredWidth = width
                        if (Math.abs(root.previewPanePreferredWidth - width) > 0.5) {
                            previewPaneWidthCommitTimer.restart()
                        }
                    }
                }

                Behavior on SplitView.preferredWidth {
                    enabled: root.workspaceStateRestored
                    NumberAnimation {
                        duration: 120
                        easing.type: Easing.OutQuad
                    }
                }

                Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
            }

                handle: Rectangle {
                    implicitWidth: 4
                    color: "transparent"
                    readonly property bool handleActive: SplitHandle.hovered || SplitHandle.pressed

                    SplitHandle.onPressedChanged: {
                        root.mainSplitResizing = SplitHandle.pressed
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSm
                        color: Theme.accent
                        opacity: SplitHandle.pressed ? 0.10 : (SplitHandle.hovered ? 0.05 : 0.0)

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 120
                                easing.type: Easing.OutQuad
                            }
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.handleActive ? 2 : 1
                        height: Math.max(0, parent.height - Theme.panelRadius * 2)
                        radius: width / 2
                        color: parent.handleActive
                               ? Theme.accent
                               : Theme.panelStrokeSubtle
                        opacity: SplitHandle.pressed ? 0.78 : (SplitHandle.hovered ? 0.44 : (themeController.isDark ? 0.16 : 0.34))

                        Behavior on width {
                            NumberAnimation {
                                duration: 100
                                easing.type: Easing.OutQuad
                            }
                        }

                        Behavior on color { ColorAnimation { duration: 120 } }
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }
                }
            }
        }
    }

    Rectangle {
        id: transientInfoBanner
        visible: root.transientInfoMessage.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.transientInfoBottomInset
        width: Math.min(parent.width - 32, infoBannerLabel.implicitWidth + 32)
        height: infoBannerLabel.implicitHeight + 18
        radius: Theme.radiusSm
        color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.18 : 0.12)
        border.width: 1
        border.color: Theme.withAlpha(Theme.categoryInfo, 0.40)
        opacity: visible ? 1 : 0
        z: 1000

        Behavior on opacity {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutQuad
            }
        }

        Label {
            id: infoBannerLabel
            anchors.fill: parent
            anchors.margins: 9
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            text: root.transientInfoMessage
            color: Theme.textPrimary
            font.pixelSize: 12
            font.weight: Font.DemiBold
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
        mirrorActivePanelToOpposite: root.mirrorActivePanelToOpposite
        togglePreviewPane: root.togglePreviewPane
        refreshActivePanel: root.refreshActivePanel
        toggleHiddenFiles: root.toggleHiddenFiles
        setThemeScheme: root.setThemeScheme
        openThemeSelector: root.openThemeSelector
        createFolderInActivePanel: root.createFolderInActivePanel
        renameActiveSelection: root.renameActiveSelection
        copyActiveSelection: root.copyActiveSelection
        copyActiveSelectionToOpposite: root.copyActiveSelectionToOpposite
        moveActiveSelectionToOpposite: root.moveActiveSelectionToOpposite
        duplicateActiveSelection: root.duplicateActiveSelection
        compressActiveSelection: root.compressActiveSelection
        cutActiveSelection: root.cutActiveSelection
        pasteClipboardToActivePanel: root.pasteClipboardToActivePanel
        addSelectionToFavorites: root.addSelectionToFavorites
        requestDeleteActiveSelection: root.requestDeleteActiveSelection
        showActiveProperties: root.showActiveProperties
        showActiveChecksums: root.showActiveChecksums
        quickLookActiveTarget: root.quickLookActiveTarget
        openHelpDialog: root.openHelpDialog
        openSettingsDialog: root.openSettingsDialog
        openThemeEditorDialog: workspaceOverlays.openThemeEditorDialog
        openSettingsImportDialog: root.openSettingsImportDialog
        openSettingsExportDialog: root.openSettingsExportDialog
        openSettingsDataFolder: root.openSettingsDataFolder
        openDiskUsage: root.openDiskUsage
        openFileSearch: root.openFileSearch
        resetSavedWorkspaceState: root.resetSavedWorkspaceState
        resetCommandUsageStats: root.resetCommandUsageStats
        relaunchAsAdmin: root.relaunchAsAdmin
        quitApplication: root.quitApplication
        copyPropertiesToClipboard: workspaceOverlays.copyPropertiesToClipboard
        exportPropertiesToFile: workspaceOverlays.exportPropertiesToFile
        navigateActivePanel: root.navigateActivePanel
    }

    WorkspaceOverlays {
        id: workspaceOverlays
        appRoot: root
        commandPaletteCommands: commandRegistry.commands
    }

    PreviewCoordinator {
        id: previewCoordinator
        appRoot: root
        workspaceController: root.workspaceService
        quickLookController: root.quickLookService
        quickLookPopup: quickLookPopup
        propertiesController: root.propertiesService
        previewSuppressed: fileWorkspace.externalPreviewScrollActive
    }

    Connections {
        target: root
        function onXChanged() { root.scheduleWorkspaceStateSave() }
        function onYChanged() { root.scheduleWorkspaceStateSave() }
        function onWidthChanged() { root.scheduleWorkspaceStateSave() }
        function onHeightChanged() { root.scheduleWorkspaceStateSave() }
        function onVisibilityChanged() { root.scheduleWorkspaceStateSave() }
    }

    Connections {
        target: root.workspaceService
        function onSplitEnabledChanged() { root.scheduleWorkspaceStateSave() }
        function onActivePanelChanged() { root.scheduleWorkspaceStateSave() }
    }

    Connections {
        target: root.workspaceService.leftPanel
        function onCurrentPathChanged() { root.scheduleWorkspaceStateSave() }
        function onViewModeChanged() { root.scheduleWorkspaceStateSave() }
    }

    Connections {
        target: root.workspaceService.rightPanel
        function onCurrentPathChanged() { root.scheduleWorkspaceStateSave() }
        function onViewModeChanged() { root.scheduleWorkspaceStateSave() }
    }

    Connections {
        target: root.workspaceService.leftPanel.directoryModel
        function onShowHiddenChanged() { root.scheduleWorkspaceStateSave() }
        function onSortRoleChanged() { root.scheduleWorkspaceStateSave() }
        function onSortOrderChanged() { root.scheduleWorkspaceStateSave() }
        function onMixFilesAndFoldersChanged() { root.scheduleWorkspaceStateSave() }
    }

    Connections {
        target: root.workspaceService.rightPanel.directoryModel
        function onShowHiddenChanged() { root.scheduleWorkspaceStateSave() }
        function onSortRoleChanged() { root.scheduleWorkspaceStateSave() }
        function onSortOrderChanged() { root.scheduleWorkspaceStateSave() }
        function onMixFilesAndFoldersChanged() { root.scheduleWorkspaceStateSave() }
    }

    Connections {
        target: typeof systemTrayController !== "undefined" ? systemTrayController : null
        function onOptionsRequested() {
            systemTrayController.showWindow()
            Qt.callLater(root.openSettingsDialog)
        }
        function onExitRequested() {
            root.quitApplication()
        }
    }

    function showBatchRename(paths) {
        workspaceOverlays.showBatchRename(paths)
    }

    function showChecksums(paths) {
        workspaceOverlays.showChecksums(paths)
    }

    onPreviewPaneVisibleChanged: {
        applyPreviewPaneWidth()
        scheduleWorkspaceStateSave()
    }
    onClosing: function(close) {
        workspaceStateSaveTimer.stop()
        saveWorkspaceStateNow(true)
        if (!root.forceQuitRequested && root.systemTrayModeActive()) {
            close.accepted = false
            systemTrayController.hideWindow()
        }
    }

    Component.onCompleted: restoreWorkspaceState()
}
