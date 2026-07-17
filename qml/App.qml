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

    function openDeleteConfirm(paths, label, items) {
        workspaceOverlays.openDeleteConfirm(paths, label, items)
    }

    function ensureQuickLookPopup() {
        if (!root.quickLookPopupItem) {
            root.quickLookPopupItem = quickLookPopupComponent.createObject(root)
        }
        return root.quickLookPopupItem
    }

    function openQuickLookPath(targetPath, loadTarget) {
        const popup = root.ensureQuickLookPopup()
        popup.restorePreviewOnClose = false
        popup.previewPath = targetPath
        if (loadTarget !== false && targetPath && root.quickLookService && root.quickLookService.preview) {
            root.quickLookService.preview(targetPath)
        }
        popup.open()
    }

    function openTransientQuickLookPath(targetPath) {
        const popup = root.ensureQuickLookPopup()
        const previousPath = root.quickLookService ? (root.quickLookService.path || "") : ""
        const controller = activePanelController()
        popup.restorePreviewOnClose = true
        popup.restorePreviewPath = previousPath
        popup.restorePreviewSelection = previousPath === "selection://" && controller
                                        ? controller.selectedPaths() : []
        popup.previewPath = targetPath
        if (targetPath && root.quickLookService && root.quickLookService.preview)
            root.quickLookService.preview(targetPath)
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

    function pathCanBeFavorited(path) {
        const ctrl = activePanelController()
        return Boolean(ctrl && ctrl.pathCanBeFavorited(path))
    }

    function isProviderPath(path) {
        const ctrl = activePanelController()
        return Boolean(ctrl && ctrl.pathIsProvider(path))
    }

    function pathCanShowProperties(path) {
        const ctrl = activePanelController()
        return Boolean(ctrl && ctrl.pathCanShowProperties(path))
    }

    function pathsCanShowProperties(paths) {
        const ctrl = activePanelController()
        return Boolean(ctrl && ctrl.pathsCanShowProperties(paths || []))
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
        workspaceStateCoordinator.stopPersistenceTimers()
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
    readonly property bool operationPreviewSuppressed: previewCoordinator.operationSuppressed
    readonly property bool renamePreviewSuppressed: previewCoordinator.renameSuppressed
    readonly property bool deletePreviewReleaseActive: previewCoordinator.deleteReleaseActive
    readonly property var deletePreviewReleasePaths: previewCoordinator.deleteReleasePaths
    property string transientInfoMessage: ""
    property real sidebarStoredWidth: 200
    property real previewPaneStoredWidth: 340
    property real sidebarPreferredWidth: 200
    property real previewPanePreferredWidth: 0
    property var previewPanePendingWorkspaceSplitState: null
    property var quickLookPopupItem: null
    readonly property bool anyLiveResize: root.mainSplitResizing || fileWorkspace.splitResizing
    readonly property var workspaceService: workspaceController
    readonly property var quickLookService: quickLookController
    readonly property var propertiesService: propertiesController
    readonly property var appSettingsService: typeof appSettings !== "undefined" ? appSettings : null
    readonly property var adminService: typeof adminController !== "undefined" ? adminController : null
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
        workspaceStateCoordinator.scheduleSave()
    }

    function saveWorkspaceStateNow(includePanelLayout) {
        workspaceStateCoordinator.saveNow(includePanelLayout)
    }

    function stopWorkspaceStatePersistenceTimers() {
        workspaceStateCoordinator.stopPersistenceTimers()
    }

    function stopWorkspaceStateAuxiliaryTimers() {
        sidebarWidthCommitTimer.stop()
        previewPaneWidthCommitTimer.stop()
        previewPaneTransitionTimer.stop()
    }

    function restoreWorkspaceState() {
        workspaceStateCoordinator.restore()
    }

    function startupShellReady() {
        workspaceStateCoordinator.startupShellReady()
    }

    function restoreWorkspaceStateFrom(state) {
        workspaceStateCoordinator.restoreFrom(state)
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

    function invalidateThumbnailsForPaths(paths) {
        const list = paths || []
        if (list.length === 0 || !workspaceController) {
            return
        }
        const leftModel = workspaceController.leftPanel && workspaceController.leftPanel.directoryModel
                          ? workspaceController.leftPanel.directoryModel : null
        const rightModel = workspaceController.rightPanel && workspaceController.rightPanel.directoryModel
                           ? workspaceController.rightPanel.directoryModel : null
        if (leftModel && leftModel.invalidateThumbnails) {
            leftModel.invalidateThumbnails(list)
        }
        if (rightModel && rightModel !== leftModel && rightModel.invalidateThumbnails) {
            rightModel.invalidateThumbnails(list)
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
            propertiesDialog.accessOwnershipAdminEditMode = false
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
            showActiveProperties(4)
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
        root.openQuickLookPath(targetPath, false)
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

    function openOpenWith(controller, paths) {
        workspaceOverlays.openOpenWith(controller, paths)
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
        const ctrl = activePanelController()
        const target = path && path.length > 0
                     ? path
                     : (ctrl ? ctrl.currentPath : "")
        if (!ctrl || !ctrl.pathCanUseLocalShellAction(target)
                || (workspaceController && workspaceController.isInsideManagedIsoMount(target))) {
            showTransientInfo("Open a regular folder before analyzing disk usage.")
            return
        }
        workspaceOverlays.openDiskUsage(target)
    }

    function openFileSearch() {
        const ctrl = activePanelController()
        const path = ctrl && ctrl.currentPath ? String(ctrl.currentPath) : ""
        if (!ctrl || path.length === 0 || ctrl.isVirtualRoot
                || !ctrl.pathCanUseLocalShellAction(path)) {
            showTransientInfo("Open a regular folder before searching.")
            return
        }
        workspaceOverlays.openFileSearch(path, true)
    }

    function openFolderCompare() {
        const left = workspaceController ? workspaceController.leftPanel : null
        const right = workspaceController ? workspaceController.rightPanel : null
        if (!workspaceController || !workspaceController.splitEnabled || !left || !right
                || !folderCompareController.canCompare(left.currentPath, right.currentPath)) {
            showTransientInfo("Open two regular local folders in split view before comparing.")
            return
        }
        workspaceOverlays.openFolderCompare(left.currentPath, right.currentPath)
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
        return previewCoordinator.samePathList(left, right)
    }

    function finishOperationPreviewSuppression(paths) {
        previewCoordinator.finishOperationSuppression(paths)
    }

    function clearPreviewForPaths(paths, forceRelease) {
        previewCoordinator.clearPreviewForPaths(paths, forceRelease)
    }

    function releasePreviewForPaths(paths, forceRelease) {
        previewCoordinator.releasePreviewForPaths(paths, forceRelease)
    }

    function captureDeleteAttention(paths) {
        const active = fileWorkspace ? fileWorkspace.activePanelView() : null
        if (active && active.captureDeleteSnapshot(paths)) return
        const left = fileWorkspace ? fileWorkspace.leftPanelView : null
        const right = fileWorkspace ? fileWorkspace.rightPanelView : null
        if (left && left !== active && left.captureDeleteSnapshot(paths)) return
        if (right && right !== active) right.captureDeleteSnapshot(paths)
    }

    function cancelDeleteAttention(paths) {
        const left = fileWorkspace ? fileWorkspace.leftPanelView : null
        const right = fileWorkspace ? fileWorkspace.rightPanelView : null
        if (left && left.cancelDeleteSnapshot(paths)) return
        if (right) right.cancelDeleteSnapshot(paths)
    }

    function beginRenamePreviewSuppression(paths, releasedCallback) {
        return previewCoordinator.beginRenameSuppression(paths, releasedCallback)
    }

    function finishRenamePreviewSuppression(restorePreview) {
        previewCoordinator.finishRenameSuppression(restorePreview)
    }

    function previewPathBelongsToVolumeRoot(path, rootPath) {
        return previewCoordinator.pathBelongsToVolumeRoot(path, rootPath)
    }

    function releasePreviewForVolumeRoot(rootPath) {
        previewCoordinator.releasePreviewForVolumeRoot(rootPath)
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
        return adminModeCoordinator.relaunchAsAdmin()
    }

    function unlockAdminMode() {
        return adminModeCoordinator.unlockAdminMode()
    }

    function adminModeActive() {
        return adminModeCoordinator.adminModeActive()
    }

    function confirmAdminSafetyAndUnlock() {
        return adminModeCoordinator.confirmSafetyAndUnlock()
    }

    function lockAdminMode() {
        adminModeCoordinator.lockAdminMode()
    }

    function showAdminModeStatus() {
        adminModeCoordinator.showStatus()
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
            diskUsageReturnVisible: workspaceOverlays.diskUsageReturnAvailable && !root.anyOverlayOpen
            onPreviewToggleRequested: (visible) => {
                root.setPreviewPaneVisible(visible)
            }
            onSearchReturnRequested: workspaceOverlays.reopenFileSearchResults()
            onDiskUsageReturnRequested: workspaceOverlays.reopenDiskUsageResults()
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
                        releaseActive: root.operationPreviewSuppressed || root.renamePreviewSuppressed
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
                    onClicked: adminSafetyDialog.close()
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
        openFolderCompare: root.openFolderCompare
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

    WorkspaceStateCoordinator {
        id: workspaceStateCoordinator
        appRoot: root
        appSettings: root.appSettingsService
        workspaceController: root.workspaceService
        fileWorkspace: fileWorkspace
        previewCoordinator: previewCoordinator
    }

    AdminModeCoordinator {
        id: adminModeCoordinator
        appRoot: root
        adminController: root.adminService
        safetyDialog: adminSafetyDialog
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
            root.showTransientInfo("Device disconnected safely")
        }
        function onDeviceEjectFailed(rootPath, displayName, message) {
            root.showTransientInfo(message && message.length > 0 ? message : "Cannot eject device.")
        }
    }

    Connections {
        target: root.workspaceService.operationQueue
        function onOperationCompleted(result) {
            if (!result || !result.sources || !root.deletePreviewReleaseActive) return
            if (!previewCoordinator.hasDeleteReleaseToken(result.sources)) return
            const finish = function() { root.finishOperationPreviewSuppression(result.sources) }
            const active = fileWorkspace ? fileWorkspace.activePanelView() : null
            if (active && active.acceptDeleteCompletion(result, finish)) return
            const left = fileWorkspace ? fileWorkspace.leftPanelView : null
            const right = fileWorkspace ? fileWorkspace.rightPanelView : null
            if (left && left !== active && left.acceptDeleteCompletion(result, finish)) return
            if (right && right !== active && right.acceptDeleteCompletion(result, finish)) return
            finish()
        }
    }

    Connections {
        target: root.workspaceService.operationQueue
        function onOperationStartedDetailed(operationId, type, sources, destination) {
            if (typeof panelInteractionTraceEnabled !== "undefined" && panelInteractionTraceEnabled) {
                console.info("[FM_INTERACTION][result-route-start] operationId=" + operationId
                             + " type=" + type + " destination=" + destination)
            }
            if (type !== OperationQueue.Copy && type !== OperationQueue.Move
                    && type !== OperationQueue.Duplicate && type !== OperationQueue.Extract
                    && type !== OperationQueue.Compress) return
            const left = fileWorkspace ? fileWorkspace.leftPanelView : null
            const right = fileWorkspace ? fileWorkspace.rightPanelView : null
            const resultDirectory = type === OperationQueue.Compress
                    ? root.activePanelController().parentPathForPath(destination)
                    : destination
            if (left) left.captureResultAttention(operationId, resultDirectory)
            if (right) right.captureResultAttention(operationId, resultDirectory)
            if (type === OperationQueue.Move) {
                if (left) left.captureMoveSourceAttention(operationId, sources, destination)
                if (right) right.captureMoveSourceAttention(operationId, sources, destination)
            }
        }
        function onOperationCompleted(result) {
            if (typeof panelInteractionTraceEnabled !== "undefined" && panelInteractionTraceEnabled) {
                console.info("[FM_INTERACTION][result-route-complete] operationId="
                             + (result ? result.operationId : "") + " type=" + (result ? result.type : "")
                             + " resultPaths=" + (result ? Array.from(result.resultPaths || []).join("|") : ""))
            }
            if (!result || result.type === OperationQueue.Delete) return
            const left = fileWorkspace ? fileWorkspace.leftPanelView : null
            const right = fileWorkspace ? fileWorkspace.rightPanelView : null
            if (left) left.acceptResultAttention(result)
            if (right) right.acceptResultAttention(result)
            if (result.type === OperationQueue.Move) {
                if (left) left.acceptMoveSourceCompletion(result)
                if (right) right.acceptMoveSourceCompletion(result)
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
        target: typeof thumbnailController !== "undefined" ? thumbnailController : null
        function onThumbnailReady(path, identity, width, height, revision) {
            root.invalidateThumbnailsForPaths([path])
        }
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
        workspaceStateCoordinator.stopPersistenceTimers()
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
        root.startupWorkspaceRestoreDeferred = true
    }
}
