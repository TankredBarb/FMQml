import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window
import QtQml
import FM
import "components"
import "components/common"
import "components/dialogs"
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
    property var pendingAdminAction: null

    function openDeleteConfirm(paths, label, items) {
        workspaceOverlays.openDeleteConfirm(paths, label, items)
    }

    function ensureQuickLookPopup() {
        if (!root.quickLookPopupItem) {
            root.quickLookPopupItem = quickLookPopupComponent.createObject(root)
        }
        return root.quickLookPopupItem
    }

    function openQuickLookPath(targetPath) {
        const popup = root.ensureQuickLookPopup()
        popup.previewPath = targetPath
        popup.open()
    }

    function activePanelController() {
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    function activePanelView() {
        return fileWorkspace ? fileWorkspace.activePanelView() : null
    }

    function inputRoutingLog(stage, detail) {
        if (typeof inputRoutingLogEnabled === "undefined" || !inputRoutingLogEnabled) {
            return
        }
        const panelView = root.inputRoutingObjectsReady ? root.activePanelView() : null
        console.log("[InputRouting]",
                    "stage=" + stage,
                    "detail=" + (detail || ""),
                    "visible=" + root.visible,
                    "active=" + root.active,
                    "focusItem=" + (!!root.activeFocusItem),
                    "panelFocus=" + (!!panelView && panelView.containsActiveFocus),
                    "initialApplied=" + root.initialPanelFocusApplied,
                    "context=" + (root.inputRoutingObjectsReady ? inputCoordinator.currentContext : "building"),
                    "canTab=" + (root.inputRoutingObjectsReady ? inputCoordinator.canRun("switchPanel") : "building"),
                    "canF3=" + (root.inputRoutingObjectsReady ? inputCoordinator.canRun("toggleSplit") : "building"),
                    "canType=" + (root.inputRoutingObjectsReady ? inputCoordinator.canRun("typeToSearch") : "building"),
                    "blockTab=" + (root.inputRoutingObjectsReady ? inputCoordinator.blockReason("switchPanel") : "building"),
                    "blockF3=" + (root.inputRoutingObjectsReady ? inputCoordinator.blockReason("toggleSplit") : "building"),
                    "blockType=" + (root.inputRoutingObjectsReady ? inputCoordinator.blockReason("typeToSearch") : "building"))
    }

    function scheduleInitialPanelFocus(reason) {
        if (root.initialPanelFocusApplied) {
            root.inputRoutingLog("scheduleInitialPanelFocus-skip", reason || "already-applied")
            return
        }
        root.inputRoutingLog("scheduleInitialPanelFocus", reason || "unspecified")
        initialPanelFocusRequest.reason = reason || "unspecified"
        initialPanelFocusRequest.interval = 0
        initialPanelFocusRequest.restart()
    }

    function initialPanelFocusBlocked() {
        return !root.visible
            || !fileWorkspace
            || root.anyOverlayOpen
            || mainToolbar.textEditingActive
            || fileWorkspace.isRenaming
            || root.sidebarFocused
    }

    function applyInitialPanelFocus(reason) {
        if (root.initialPanelFocusApplied || root.initialPanelFocusBlocked()) {
            root.inputRoutingLog("applyInitialPanelFocus-blocked", reason || "")
            return
        }

        root.inputRoutingLog("applyInitialPanelFocus-request", reason || "")
        if (!root.activeFocusItem && appContent) {
            root.inputRoutingLog("applyInitialPanelFocus-anchor", reason || "")
            appContent.forceActiveFocus(Qt.OtherFocusReason)
        }
        workspaceController.focusActivePanel()
        Qt.callLater(() => {
            if (root.initialPanelFocusApplied || root.initialPanelFocusBlocked()) {
                root.inputRoutingLog("applyInitialPanelFocus-verify-blocked", reason || "")
                return
            }
            const panelView = root.activePanelView()
            if (panelView && panelView.containsActiveFocus) {
                root.initialPanelFocusApplied = true
                root.inputRoutingLog("applyInitialPanelFocus-success", reason || "")
            } else {
                root.inputRoutingLog("applyInitialPanelFocus-missed", reason || "")
            }
        })
    }

    function explicitPathScheme(path) {
        const value = String(path || "").trim()
        const index = value.indexOf("://")
        if (index <= 0) return ""
        const scheme = value.substring(0, index).toLowerCase()
        if (scheme.length === 0 || !/[a-z]/.test(scheme.charAt(0))) return ""
        for (let i = 0; i < scheme.length; ++i) {
            const ch = scheme.charAt(i)
            if (!/[a-z0-9+.-]/.test(ch)) return ""
        }
        return scheme
    }

    function pathCanBeFavorited(path) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        const scheme = explicitPathScheme(value)
        return value.length > 0
            && (scheme.length === 0 || scheme === "file")
            && lower !== "devices://"
            && lower !== "favorites://"
    }

    function isProviderPath(path) {
        const scheme = explicitPathScheme(path)
        return scheme.length > 0
            && scheme !== "file"
            && scheme !== "archive"
            && scheme !== "devices"
            && scheme !== "favorites"
    }

    function pathCanShowProperties(path) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        const scheme = explicitPathScheme(value)
        return value.length > 0
            && (scheme.length === 0
                || scheme === "file"
                || root.isProviderPath(value))
            && lower !== "devices://"
            && lower !== "favorites://"
    }

    function pathsCanShowProperties(paths) {
        if (!paths || paths.length === 0) {
            return false
        }
        for (let i = 0; i < paths.length; ++i) {
            if (!root.pathCanShowProperties(paths[i])) {
                return false
            }
        }
        return true
    }

    function canCreateManualItemInPanel(ctrl) {
        return Boolean(ctrl
                       && !root.isProviderPath(ctrl.currentPath)
                       && ctrl.canCreateInCurrentPath)
    }

    function oppositePanelController(ctrl) {
        if (!workspaceController || !workspaceController.splitEnabled || !ctrl) {
            return null
        }
        if (workspaceController.leftPanel === ctrl) {
            return workspaceController.rightPanel
        }
        if (workspaceController.rightPanel === ctrl) {
            return workspaceController.leftPanel
        }
        return workspaceController.activePanel === 0
            ? workspaceController.rightPanel
            : workspaceController.leftPanel
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
    property bool startupWorkspaceRestoreDeferred: false
    property bool startupShellFirstRestoreActive: false
    property bool initialPanelFocusApplied: false
    property bool inputRoutingObjectsReady: false
    property bool forceQuitRequested: false
    property int workspaceStateRestoreGeneration: 0
    property bool mainSplitResizing: false
    property bool previewPaneTransitionActive: false
    property bool operationPreviewSuppressed: false
    property bool renamePreviewSuppressed: false
    property bool deletePreviewReleaseActive: false
    property var deletePreviewReleasePaths: []
    property string transientInfoMessage: ""
    property real sidebarStoredWidth: 200
    property real previewPaneStoredWidth: 340
    property real sidebarPreferredWidth: 200
    property real previewPanePreferredWidth: 0
    property var previewPanePendingWorkspaceSplitState: null
    property var quickLookPopupItem: null
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
    readonly property bool shellFirstQmlRestoreEnabled: typeof appSettings !== "undefined"
                                                        && appSettings
                                                        && appSettings.shellFirstQmlRestore

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
            leftShowSelectionBadges: fileWorkspace.leftPanelView.showSelectionBadges,
            rightShowSelectionBadges: fileWorkspace.rightPanelView.showSelectionBadges,
            leftShowHoverPreviews: fileWorkspace.leftPanelView.showHoverPreviews,
            rightShowHoverPreviews: fileWorkspace.rightPanelView.showHoverPreviews,
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

    function startupShellReady() {
        if (!root.startupWorkspaceRestoreDeferred
                || root.workspaceStateRestoreActive
                || root.workspaceStateRestored) {
            return
        }
        root.startupWorkspaceRestoreDeferred = false
        root.startupShellFirstRestoreActive = true
        Qt.callLater(() => {
            restoreWorkspaceState()
        })
    }

    function restoreWorkspaceStateFrom(state) {
        if (!state) {
            root.workspaceStateRestored = true
            root.startupShellFirstRestoreActive = false
            return
        }

        const restoreGeneration = ++root.workspaceStateRestoreGeneration
        const showHidden = !!state.showHidden
        let leftOpenRequested = false
        let rightOpenRequested = false

        function applyPanelState() {
            if (!leftOpenRequested && workspaceController.leftPanel.currentPath !== state.leftPath) {
                if (root.startupShellFirstRestoreActive) {
                    leftOpenRequested = true
                } else {
                    leftOpenRequested = workspaceController.leftPanel.openPath(state.leftPath)
                }
            }
            if (!rightOpenRequested && workspaceController.rightPanel.currentPath !== state.rightPath) {
                if (root.startupShellFirstRestoreActive) {
                    rightOpenRequested = true
                } else {
                    rightOpenRequested = workspaceController.rightPanel.openPath(state.rightPath)
                }
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

        const restoreWindowState = !root.startupShellFirstRestoreActive
        if (restoreWindowState) {
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
        fileWorkspace.leftPanelView.showSelectionBadges = state.leftShowSelectionBadges !== false
        fileWorkspace.rightPanelView.showSelectionBadges = state.rightShowSelectionBadges !== false
        fileWorkspace.leftPanelView.showHoverPreviews = state.leftShowHoverPreviews === true
        fileWorkspace.rightPanelView.showHoverPreviews = state.rightShowHoverPreviews === true
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
                    if (root.visible && restoreWindowState) {
                        if (state.windowMaximized) {
                            root.visibility = Window.Maximized
                        } else if (root.visibility === Window.Maximized) {
                            root.visibility = Window.Windowed
                        }
                    }
                    previewCoordinator.syncPreviewFromActivePanel(true)
                    root.startupShellFirstRestoreActive = false
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
        if (root.canCreateManualItemInPanel(ctrl)) {
            ctrl.createFolder("New Folder")
        }
    }

    function createFolderInActivePanelAsAdministrator() {
        if (!root.adminModeActive()) {
            root.showTransientInfo("Unlock administrator mode first")
            return
        }
        const ctrl = activePanelController()
        if (!ctrl || !ctrl.createFolderAsAdministrator) return
        ctrl.createFolderAsAdministrator("New Folder")
    }

    function createFileInActivePanelAsAdministrator(name) {
        if (!root.adminModeActive()) {
            root.showTransientInfo("Unlock administrator mode first")
            return
        }
        const ctrl = activePanelController()
        if (!ctrl || !ctrl.createFileAsAdministrator) return
        ctrl.createFileAsAdministrator(name || "New File")
    }

    function renameActiveSelection() {
        const ctrl = activePanelController()
        if (ctrl && !root.isProviderPath(ctrl.currentPath)) {
            workspaceController.triggerRename()
        }
    }

    function copyActiveSelection() {
        workspaceController.copyToClipboard()
    }

    function copyActiveSelectionToOpposite() {
        workspaceController.copyActiveSelectionToOpposite()
    }

    function moveActiveSelectionToOpposite() {
        const ctrl = activePanelController()
        const destination = root.oppositePanelController(ctrl)
        if (ctrl && destination
                && !root.isProviderPath(ctrl.currentPath)
                && !root.isProviderPath(destination.currentPath)) {
            workspaceController.moveActiveSelectionToOpposite()
        }
    }

    function duplicateActiveSelection() {
        const ctrl = activePanelController()
        if (ctrl && !root.isProviderPath(ctrl.currentPath)) {
            workspaceController.duplicateActiveSelection()
        }
    }

    function compressActiveSelection(format) {
        const ctrl = activePanelController()
        if (ctrl && !root.isProviderPath(ctrl.currentPath)) {
            workspaceController.compressActiveSelection(format || "7z")
        }
    }

    function cutActiveSelection() {
        const ctrl = activePanelController()
        if (ctrl && !root.isProviderPath(ctrl.currentPath)) {
            workspaceController.cutToClipboard()
        }
    }

    function pasteClipboardToActivePanel() {
        workspaceController.pasteFromClipboard()
    }

    function pasteClipboardToActivePanelAsAdministrator() {
        if (!root.adminModeActive()) {
            root.showTransientInfo("Unlock administrator mode first")
            return
        }
        if (!workspaceController) return
        workspaceController.pasteFromClipboardAsAdministrator()
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
            if (!root.pathCanBeFavorited(selected[i])) {
                showTransientInfo("This location cannot be pinned to Favorites")
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
        if (active && active.canDeleteSelection) {
            workspaceController.requestDelete(active.selectedPaths(), active.currentPath,
                                              active.selectedItems ? active.selectedItems() : [])
        }
    }

    function requestDeleteActiveSelectionAsAdministrator() {
        if (!root.adminModeActive()) {
            root.showTransientInfo("Unlock administrator mode first")
            return
        }
        const active = activePanelController()
        if (!active || !workspaceController) return
        workspaceController.requestDeleteAsAdministrator(active.selectedPaths(), active.currentPath,
                                                         active.selectedItems ? active.selectedItems() : [])
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
        if (!root.pathsCanShowProperties(selected)) {
            showTransientInfo("Properties are not available for this location")
            return
        }

        let providerCount = 0
        for (let i = 0; i < selected.length; ++i) {
            if (root.isProviderPath(selected[i])) {
                ++providerCount
            }
        }
        if (providerCount > 0) {
            if (providerCount !== selected.length) {
                showTransientInfo("Mixed local and provider properties are not supported yet")
                return
            }
            if (selected.length > 1) {
                showTransientInfo("Provider properties support one selected item for now")
                return
            }
            const overlay = workspaceOverlays.ensureProviderPropertiesOverlay()
            providerPropertiesController.load(selected[0])
            providerPropertiesController.visible = true
            if (overlay) {
                overlay.open()
            }
            return
        }

        const propertiesDialog = workspaceOverlays.ensurePropertiesDialog()
        if (propertiesDialog) {
            propertiesDialog.suppressDialog = false
            if (typeof tabIndex === "number") {
                propertiesDialog.requestedTab = tabIndex
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
        root.openQuickLookPath(targetPath)
    }

    function openHelpDialog() {
        workspaceOverlays.openHelpDialog()
    }

    function openSettingsDialog() {
        workspaceOverlays.openSettingsDialog()
    }

    function openTextColorOverridesOverlay() {
        workspaceOverlays.openTextColorOverridesOverlay()
    }

    function resetTextColorOverrides() {
        if (appSettings) {
            appSettings.resetAll()
            showTransientInfo("Text color overrides have been reset.")
        }
    }

    function openPluginManagerDialog() {
        workspaceOverlays.openPluginManagerDialog()
    }

    function openDebugInformationDialog() {
        workspaceOverlays.openDebugInformationDialog()
    }

    function openPluginActionResult(result) {
        workspaceOverlays.openPluginActionResult(result)
    }

    function openSteamProtonLaunch(controller, path) {
        workspaceOverlays.openSteamProtonLaunch(controller, path)
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
        const scheme = root.explicitPathScheme(target)
        const lowerPath = String(target || "").toLowerCase()
        if (!target || target.length === 0
                || (scheme.length > 0 && scheme !== "file")
                || lowerPath === "devices://"
                || lowerPath === "favorites://"
                || (workspaceController && workspaceController.isInsideManagedIsoMount(target))) {
            showTransientInfo("Open a regular folder before analyzing disk usage.")
            return
        }
        workspaceOverlays.openDiskUsage(target)
    }

    function openFileSearch() {
        const ctrl = activePanelController()
        const path = ctrl && ctrl.currentPath ? String(ctrl.currentPath) : ""
        const lowerPath = path.toLowerCase()
        const scheme = root.explicitPathScheme(path)
        if (!ctrl || path.length === 0 || ctrl.isVirtualRoot
                || (scheme.length > 0 && scheme !== "file")
                || lowerPath.startsWith("devices://")
                || lowerPath.startsWith("favorites://")) {
            showTransientInfo("Open a regular folder before searching.")
            return
        }
        workspaceOverlays.openFileSearch(path, true)
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

    function samePathList(left, right) {
        if (!left || !right || left.length !== right.length) {
            return false
        }
        for (let i = 0; i < left.length; ++i) {
            if (String(left[i] || "") !== String(right[i] || "")) {
                return false
            }
        }
        return true
    }

    function finishOperationPreviewSuppression() {
        operationPreviewSuppressionTimer.stop()
        root.operationPreviewSuppressed = false
        root.deletePreviewReleaseActive = false
        root.deletePreviewReleasePaths = []
    }

    function clearPreviewForPaths(paths, forceRelease) {
        if (!root.quickLookService || !paths || paths.length === 0) {
            return
        }
        if (forceRelease === true) {
            previewCoordinator.clearPreviewTimers()
            quickLookPopup.close()
            quickLookPopup.previewPath = ""
            root.quickLookService.preview("")
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
                quickLookPopup.close()
                quickLookPopup.previewPath = ""
                root.quickLookService.preview("")
                return
            }
        }
    }

    function releasePreviewForPaths(paths, forceRelease) {
        const force = forceRelease === true
        if (force) {
            root.operationPreviewSuppressed = true
            root.deletePreviewReleaseActive = true
            root.deletePreviewReleasePaths = paths ? Array.from(paths) : []
            operationPreviewSuppressionTimer.restart()
        }
        root.clearPreviewForPaths(paths, force)
    }

    function beginRenamePreviewSuppression(paths) {
        root.renamePreviewSuppressed = true
        root.clearPreviewForPaths(paths, true)
    }

    function finishRenamePreviewSuppression(restorePreview) {
        const wasSuppressed = root.renamePreviewSuppressed
        root.renamePreviewSuppressed = false
        if (restorePreview === true && wasSuppressed && !root.operationPreviewSuppressed) {
            previewCoordinator.syncPreviewFromActivePanel(true)
        }
    }

    function previewPathBelongsToVolumeRoot(path, rootPath) {
        return path && path.length > 0
            && root.workspaceService
            && root.workspaceService.pathBelongsToVolumeRoot
            && root.workspaceService.pathBelongsToVolumeRoot(path, rootPath)
    }

    function releasePreviewForVolumeRoot(rootPath) {
        if (!rootPath || rootPath.length === 0) {
            return
        }

        const previewPath = root.quickLookService ? (root.quickLookService.path || "") : ""
        const previewAbsolutePath = root.quickLookService ? (root.quickLookService.absolutePath || "") : ""
        const popupPath = quickLookPopup.previewPath || ""
        const previewMatches = root.previewPathBelongsToVolumeRoot(previewPath, rootPath)
            || root.previewPathBelongsToVolumeRoot(previewAbsolutePath, rootPath)
        const popupMatches = root.previewPathBelongsToVolumeRoot(popupPath, rootPath)

        if (previewMatches && root.quickLookService) {
            previewCoordinator.clearPreviewTimers()
            root.quickLookService.preview("devices://")
        }
        if (popupMatches) {
            quickLookPopup.close()
            quickLookPopup.previewPath = ""
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

    function unlockAdminMode() {
        if (typeof adminController === "undefined" || !adminController) {
            return false
        }
        if (adminController.shouldShowAdminSafetyWarning) {
            adminSafetyDialog.open()
            return false
        }
        const unlocked = adminController.unlockAdminMode()
        if (!unlocked && adminController.adminModeUnavailableReason) {
            root.showTransientInfo(adminController.adminModeUnavailableReason)
        }
        return unlocked
    }

    function adminModeActive() {
        return typeof adminController !== "undefined"
                && adminController
                && adminController.adminModeActive
    }

    function ensureAdminModeForAction(action) {
        if (typeof action !== "function") {
            return false
        }
        if (root.adminModeActive()) {
            action()
            return true
        }
        root.pendingAdminAction = action
        const waitingForSafetyDialog = typeof adminController !== "undefined"
                && adminController
                && adminController.shouldShowAdminSafetyWarning
        const unlocked = root.unlockAdminMode()
        if (unlocked && root.adminModeActive() && root.pendingAdminAction) {
            const pending = root.pendingAdminAction
            root.pendingAdminAction = null
            pending()
            return true
        }
        if (!waitingForSafetyDialog) {
            root.pendingAdminAction = null
        }
        return false
    }

    function confirmAdminSafetyAndUnlock() {
        if (typeof adminController === "undefined" || !adminController) {
            return false
        }
        adminController.acknowledgeAdminSafetyWarning()
        adminSafetyDialog.close()
        const unlocked = root.unlockAdminMode()
        if (unlocked && root.adminModeActive() && root.pendingAdminAction) {
            const pending = root.pendingAdminAction
            root.pendingAdminAction = null
            pending()
        }
        return unlocked
    }

    function lockAdminMode() {
        if (typeof adminController === "undefined" || !adminController) {
            return
        }
        root.pendingAdminAction = null
        adminController.lockAdminMode()
        root.showTransientInfo("Administrator mode locked")
    }

    function showAdminModeStatus() {
        if (typeof adminController === "undefined" || !adminController) {
            return
        }
        let message = "Administrator mode: " + adminController.adminModeStateName
        if (adminController.adminModeRemainingSeconds > 0) {
            message += " (" + adminController.adminModeRemainingSeconds + "s remaining)"
        } else if (adminController.adminModeUnavailableReason) {
            message += " - " + adminController.adminModeUnavailableReason
        }
        root.showTransientInfo(message)
    }

    InputCoordinator {
        id: inputCoordinator
        appActive: root.active
        appVisible: root.visible
        logicalActivePanel: root.workspaceService.activePanel
        anyOverlayOpen: root.anyOverlayOpen
        workspaceOverlayOpen: root.workspaceOverlayOpen
        commandPaletteOpen: workspaceOverlays.commandPalette
                            && (workspaceOverlays.commandPalette.opened
                                || workspaceOverlays.commandPalette.visible)
        quickLookOpen: quickLookPopup.opened || quickLookPopup.visible
        sidebarFocused: root.sidebarFocused
        sidebarTrapTabNavigation: sidebar.trapTabNavigation
        pathEditorActive: mainToolbar.pathEditing
        quickSearchActive: mainToolbar.textEditingActive && !mainToolbar.pathEditing
        renameEditorActive: fileWorkspace.isRenaming
        splitEnabled: root.workspaceService.splitEnabled
        operationBusy: root.workspaceService.operationQueue.busy
        logEnabled: typeof inputRoutingLogEnabled !== "undefined" && inputRoutingLogEnabled
        activePanelValid: !!(root.workspaceService.activePanel === 0
                              ? root.workspaceService.leftPanel
                              : root.workspaceService.rightPanel)
        activePanelFavoritesRoot: {
            const ctrl = root.workspaceService.activePanel === 0
                ? root.workspaceService.leftPanel
                : root.workspaceService.rightPanel
            return !!ctrl && ctrl.isFavoritesRoot
        }
        activePanelProviderPath: {
            const ctrl = root.workspaceService.activePanel === 0
                ? root.workspaceService.leftPanel
                : root.workspaceService.rightPanel
            return !!ctrl && root.isProviderPath(ctrl.currentPath)
        }
        activePanelCanDeleteSelection: {
            const ctrl = root.workspaceService.activePanel === 0
                ? root.workspaceService.leftPanel
                : root.workspaceService.rightPanel
            return !!ctrl && ctrl.canDeleteSelection
        }
        activePanelCanRenameSelection: {
            const ctrl = root.workspaceService.activePanel === 0
                ? root.workspaceService.leftPanel
                : root.workspaceService.rightPanel
            return !!ctrl && ctrl.canRenameSelection
        }
        activePanelCanPasteIntoCurrentPath: {
            const ctrl = root.workspaceService.activePanel === 0
                ? root.workspaceService.leftPanel
                : root.workspaceService.rightPanel
            return !!ctrl && ctrl.canPasteIntoCurrentPath
        }
        activePanelSelectedCount: {
            const ctrl = root.workspaceService.activePanel === 0
                ? root.workspaceService.leftPanel
                : root.workspaceService.rightPanel
            return ctrl && ctrl.directoryModel ? ctrl.directoryModel.selectedCount : 0
        }
    }

    readonly property bool canTransferToOpposite: inputCoordinator.canTransferToOpposite

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
        inputCoordinator: inputCoordinator
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
        id: operationPreviewSuppressionTimer
        interval: 10000
        repeat: false
        onTriggered: {
            if (root.workspaceService.operationQueue.busy) {
                operationPreviewSuppressionTimer.restart()
                return
            }
            root.finishOperationPreviewSuppression()
        }
    }

    Timer {
        id: transientInfoBannerTimer
        interval: 5000
        repeat: false
        onTriggered: root.transientInfoMessage = ""
    }

    Timer {
        id: initialPanelFocusRequest
        property string reason: ""
        interval: 0
        repeat: false
        onTriggered: root.applyInitialPanelFocus(reason)
    }

    onVisibleChanged: {
        root.inputRoutingLog("window-visible-changed", visible)
        if (visible) {
            root.scheduleInitialPanelFocus("window-visible")
        }
    }

    onActiveChanged: {
        root.inputRoutingLog("window-active-changed", active)
        if (active) {
            root.scheduleInitialPanelFocus("window-active")
        }
    }

    onActiveFocusItemChanged: root.inputRoutingLog("activeFocusItem-changed", !!activeFocusItem)

    onWorkspaceStateRestoredChanged: {
        root.inputRoutingLog("workspaceStateRestored-changed", workspaceStateRestored)
        if (workspaceStateRestored) {
            root.scheduleInitialPanelFocus("workspace-restored")
        }
    }


    ColumnLayout {
        id: appContent
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

                const typeToSearchAllowed = inputCoordinator.canRun("typeToSearch")
                if (typeof inputRoutingLogEnabled !== "undefined" && inputRoutingLogEnabled) {
                    root.inputRoutingLog("keys-pressed", "key=" + event.key + " text=" + event.text + " typeAllowed=" + typeToSearchAllowed)
                    inputCoordinator.traceDecision("typeToSearch")
                }
                if (typeToSearchAllowed && mainToolbar.focusSearch(event.text)) {
                    root.inputRoutingLog("type-to-search-focused", event.text)
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

            AmbientPanelBackground {
                anchors.fill: parent
                strength: 0.95
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
                externalScrollActive: sidebar.sidebarScrollActive
                externalScrollOptimizationEnabled: true
                workspaceController: root.workspaceService
                propertiesController: root.propertiesService
                quickLookPopup: quickLookPopup
                quickLookController: root.quickLookService
                onPanelVisualStateChanged: root.scheduleWorkspaceStateSave()
                onInitialFocusReady: root.scheduleInitialPanelFocus("workspace-ready")
            }

            Item {
                id: previewPane
                SplitView.preferredWidth: root.previewPanePreferredWidth
                SplitView.minimumWidth: root.previewPaneVisible ? 280 : 0
                SplitView.fillWidth: false
                visible: root.previewPaneVisible || width > 0
                opacity: root.previewPaneVisible ? 1.0 : 0.0
                property bool previewPaneLoaded: false

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

                Loader {
                    id: previewPaneLoader
                    anchors.fill: parent
                    active: root.previewPaneVisible || previewPane.previewPaneLoaded
                    sourceComponent: PreviewPane {
                        liveResizeActive: root.anyLiveResize || root.previewPaneTransitionActive || fileWorkspace.previewScrollActive
                        scrollPauseActive: fileWorkspace.previewScrollActive && !root.anyLiveResize && !root.previewPaneTransitionActive
                        previewPending: previewCoordinator.previewPending
                        pendingPreviewPath: previewCoordinator.pendingPreviewPath
                    }
                    onLoaded: {
                        previewPane.previewPaneLoaded = true
                    }
                }
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

            Rectangle {
                id: transientInfoBanner
                visible: root.transientInfoMessage.length > 0
                readonly property var targetPanel: fileWorkspace ? fileWorkspace.leftPanelView : null
                readonly property real panelLeft: fileWorkspace ? fileWorkspace.x : 0
                readonly property real panelTop: fileWorkspace ? fileWorkspace.y : 0
                readonly property real drawerLeft: fileWorkspace && fileWorkspace.operationsDrawerVisible
                                                   ? fileWorkspace.x + fileWorkspace.operationsDrawerX
                                                   : -1
                readonly property real drawerAvoidWidth: drawerLeft > 0
                                                         ? drawerLeft - panelLeft - 24
                                                         : 100000
                readonly property bool drawerWouldOverlap: fileWorkspace
                                                           && fileWorkspace.operationsDrawerVisible
                                                           && drawerAvoidWidth < Math.min(targetPanel ? targetPanel.width - 24 : parent.width - 40, 520)
                readonly property real bottomInset: (targetPanel ? targetPanel.bottomChromeHeight : 0)
                                                    + (targetPanel && targetPanel.errorBannerVisible
                                                       ? targetPanel.errorBannerHeight + 20
                                                       : 12)
                                                    + (drawerWouldOverlap
                                                       ? fileWorkspace.operationsDrawerHeight + 12
                                                       : 0)

                x: Math.round(panelLeft + 12)
                y: Math.round(panelTop + (targetPanel ? targetPanel.height : parent.height) - bottomInset - height)
                width: Math.max(280,
                                Math.min(targetPanel ? targetPanel.width - 24 : parent.width - 40,
                                         drawerAvoidWidth,
                                         520))
                height: Math.max(44, infoBannerLabel.implicitHeight + 18)
                radius: Theme.radiusSm
                readonly property color bannerSurface: Theme.opaque(Theme.panelSurfaceStrong)
                readonly property color bannerAccent: Theme.activeAccent

                color: bannerSurface
                border.width: 2
                border.color: Theme.withAlpha(bannerAccent, themeController.isDark ? 0.90 : 0.76)
                opacity: visible ? 1 : 0
                z: 1000

                layer.enabled: visible
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: 0.30
                    shadowVerticalOffset: 5
                    shadowOpacity: themeController.isDark ? 0.18 : 0.12
                    shadowColor: Theme.shadow
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Math.max(0, parent.radius - 1)
                    visible: Theme.useGradientColors
                    opacity: themeController.isDark ? 0.62 : 0.72
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.chromeGradientStart }
                        GradientStop { position: 0.48; color: Theme.chromeGradientMid }
                        GradientStop { position: 1.0; color: Theme.chromeGradientEnd }
                    }
                    border.color: "transparent"
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Math.max(0, parent.radius - 1)
                    visible: !Theme.useGradientColors
                    color: Theme.withAlpha(parent.bannerAccent, themeController.isDark ? 0.20 : 0.16)
                    border.color: "transparent"
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: Math.max(0, parent.radius - 3)
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.withAlpha(parent.bannerAccent, themeController.isDark ? 0.32 : 0.24)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 9
                    anchors.bottomMargin: 9
                    anchors.leftMargin: 9
                    width: 3
                    radius: 1.5
                    color: parent.bannerAccent
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 140
                        easing.type: Easing.OutQuad
                    }
                }

                Label {
                    id: infoBannerLabel
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 14
                    anchors.topMargin: 9
                    anchors.bottomMargin: 9
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    text: root.transientInfoMessage
                    color: Theme.readableOn(transientInfoBanner.bannerSurface, Theme.textPrimary)
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.Medium
                }
            }
        }
    }

    Popup {
        id: adminSafetyDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(480, root.width - 48)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        padding: 0

        background: DialogShell {
            accentColor: Theme.warning
            shellBorderColor: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.36 : 0.26)
        }

        contentItem: ColumnLayout {
            spacing: 14
            width: adminSafetyDialog.width

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 18
                text: "Administrator mode"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                implicitHeight: warningText.implicitHeight + 20
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.13 : 0.08)
                border.color: Theme.withAlpha(Theme.warning, 0.34)

                Label {
                    id: warningText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "Administrator actions can modify protected system files. FM will keep running as your normal user, but confirmed file operations can change root-owned paths. Review each target carefully."
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WordWrap
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                text: "You can lock administrator mode at any time. FM does not store the root password."
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeLabel
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 18
                spacing: 10

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    text: "Cancel"
                    onClicked: {
                        root.pendingAdminAction = null
                        adminSafetyDialog.close()
                    }
                }

                DialogActionButton {
                    text: "Continue"
                    highlighted: true
                    primaryColor: Theme.warning
                    primaryHoverColor: Theme.withAlpha(Theme.warning, 0.86)
                    primaryPressedColor: Theme.withAlpha(Theme.warning, 0.72)
                    textColor: Theme.panelSurface
                    onClicked: root.confirmAdminSafetyAndUnlock()
                }
            }
        }
    }

    QtObject {
        id: quickLookPopup
        property string previewPath: ""
        readonly property bool opened: !!root.quickLookPopupItem && root.quickLookPopupItem.opened
        readonly property bool visible: !!root.quickLookPopupItem && root.quickLookPopupItem.visible

        function open() {
            root.openQuickLookPath(quickLookPopup.previewPath)
        }

        function close() {
            if (root.quickLookPopupItem) {
                root.quickLookPopupItem.close()
            }
        }
    }

    Component {
        id: quickLookPopupComponent
        QuickLook {}
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
        createFolderInActivePanelAsAdministrator: root.createFolderInActivePanelAsAdministrator
        createFileInActivePanelAsAdministrator: root.createFileInActivePanelAsAdministrator
        renameActiveSelection: root.renameActiveSelection
        copyActiveSelection: root.copyActiveSelection
        copyActiveSelectionToOpposite: root.copyActiveSelectionToOpposite
        moveActiveSelectionToOpposite: root.moveActiveSelectionToOpposite
        duplicateActiveSelection: root.duplicateActiveSelection
        compressActiveSelection: root.compressActiveSelection
        cutActiveSelection: root.cutActiveSelection
        pasteClipboardToActivePanel: root.pasteClipboardToActivePanel
        pasteClipboardToActivePanelAsAdministrator: root.pasteClipboardToActivePanelAsAdministrator
        addSelectionToFavorites: root.addSelectionToFavorites
        requestDeleteActiveSelection: root.requestDeleteActiveSelection
        requestDeleteActiveSelectionAsAdministrator: root.requestDeleteActiveSelectionAsAdministrator
        showActiveProperties: root.showActiveProperties
        showActiveChecksums: root.showActiveChecksums
        quickLookActiveTarget: root.quickLookActiveTarget
        openHelpDialog: root.openHelpDialog
        openSettingsDialog: root.openSettingsDialog
        openTextColorOverridesOverlay: root.openTextColorOverridesOverlay
        resetTextColorOverrides: root.resetTextColorOverrides
        openPluginManagerDialog: root.openPluginManagerDialog
        openThemeEditorDialog: workspaceOverlays.openThemeEditorDialog
        openSettingsImportDialog: root.openSettingsImportDialog
        openSettingsExportDialog: root.openSettingsExportDialog
        openSettingsDataFolder: root.openSettingsDataFolder
        openDiskUsage: root.openDiskUsage
        openFileSearch: root.openFileSearch
        resetSavedWorkspaceState: root.resetSavedWorkspaceState
        resetCommandUsageStats: root.resetCommandUsageStats
        relaunchAsAdmin: root.relaunchAsAdmin
        unlockAdminMode: root.unlockAdminMode
        lockAdminMode: root.lockAdminMode
        showAdminModeStatus: root.showAdminModeStatus
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
                           || root.operationPreviewSuppressed
                           || root.renamePreviewSuppressed
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
        function onDeviceEjectStarted(rootPath, displayName) {
            root.releasePreviewForVolumeRoot(rootPath)
        }
        function onDeviceRemoved(rootPath, displayName) {
            root.showTransientInfo("Device was removed")
        }
        function onDeviceEjectSucceeded(rootPath, displayName) {
            root.showTransientInfo("Device ejected safely")
        }
        function onDeviceEjectFailed(rootPath, displayName, message) {
            root.showTransientInfo(message && message.length > 0 ? message : "Cannot eject device.")
        }
    }

    Connections {
        target: root.workspaceService.operationQueue
        function onOperationFinished(type, sources, destination) {
            if (!root.deletePreviewReleaseActive) {
                return
            }
            const isDeleteLike = !destination || String(destination).length === 0
            if (isDeleteLike && root.samePathList(sources, root.deletePreviewReleasePaths)) {
                root.finishOperationPreviewSuppression()
            }
        }
    }

    Connections {
        target: root.workspaceService.volumeMonitor
        function onVolumeRemoved(rootPath, displayName) {
            root.releasePreviewForVolumeRoot(rootPath)
        }
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
            return
        }
        if (!root.forceQuitRequested) {
            root.quitApplication()
        }
    }

    Component.onCompleted: {
        root.inputRoutingObjectsReady = true
        root.inputRoutingLog("component-completed", "")
        if (root.shellFirstQmlRestoreEnabled) {
            root.startupWorkspaceRestoreDeferred = true
        } else {
            restoreWorkspaceState()
        }
    }
}
