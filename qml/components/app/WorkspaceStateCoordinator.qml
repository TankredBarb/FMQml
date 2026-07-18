import QtQuick
import QtQuick.Window

QtObject {
    id: coordinator

    required property var appRoot
    required property var appSettings
    required property var workspaceController
    required property var fileWorkspace
    required property var previewCoordinator

    property Timer saveTimer: Timer {
        interval: 350
        repeat: false
        onTriggered: coordinator.saveNow(false)
    }

    function scheduleSave() {
        const app = coordinator.appRoot
        if (app.workspaceStateRestored && !app.workspaceStateSavePaused) saveTimer.restart()
    }

    function saveNow(includePanelLayout) {
        const app = coordinator.appRoot
        const settings = coordinator.appSettings
        const workspace = coordinator.workspaceController
        const views = coordinator.fileWorkspace
        if (!settings || !app.workspaceStateRestored || app.workspaceStateSavePaused) return

        const activeCtrl = workspace.activePanel === 0 ? workspace.leftPanel : workspace.rightPanel
        const state = {
            windowX: app.x, windowY: app.y, windowWidth: app.width, windowHeight: app.height,
            windowMaximized: app.visibility === Window.Maximized,
            splitEnabled: workspace.splitEnabled, activePanel: workspace.activePanel,
            previewPaneVisible: app.previewPaneVisible,
            leftPath: workspace.leftPanel.currentPath, rightPath: workspace.rightPanel.currentPath,
            leftViewMode: workspace.leftPanel.viewMode, rightViewMode: workspace.rightPanel.viewMode,
            leftGridIconSize: views.leftPanelView.gridIconSize, rightGridIconSize: views.rightPanelView.gridIconSize,
            leftBriefRowHeight: views.leftPanelView.briefRowHeight, rightBriefRowHeight: views.rightPanelView.briefRowHeight,
            leftShowActionBar: views.leftPanelView.showActionBar, rightShowActionBar: views.rightPanelView.showActionBar,
            leftShowSelectionBadges: views.leftPanelView.showSelectionBadges,
            rightShowSelectionBadges: views.rightPanelView.showSelectionBadges,
            leftShowHoverPreviews: views.leftPanelView.showHoverPreviews,
            rightShowHoverPreviews: views.rightPanelView.showHoverPreviews,
            leftDetailsVisualState: views.leftPanelView.detailsVisualState(),
            rightDetailsVisualState: views.rightPanelView.detailsVisualState(),
            leftSortRole: workspace.leftPanel.panelSortRole, rightSortRole: workspace.rightPanel.panelSortRole,
            leftSortOrder: workspace.leftPanel.panelSortOrder, rightSortOrder: workspace.rightPanel.panelSortOrder,
            leftMixFilesAndFolders: workspace.leftPanel.directoryModel.mixFilesAndFolders,
            rightMixFilesAndFolders: workspace.rightPanel.directoryModel.mixFilesAndFolders,
            showHidden: activeCtrl ? activeCtrl.directoryModel.showHidden : workspace.leftPanel.directoryModel.showHidden
        }
        if (includePanelLayout) {
            state.sidebarWidth = Math.round(Math.max(140, app.sidebarStoredWidth))
            state.previewPaneWidth = Math.round(Math.max(280, app.previewPaneStoredWidth))
            state.fileWorkspaceSplitState = views.saveSplitState()
        }
        settings.saveWorkspaceState(state)
    }

    function stopPersistenceTimers() {
        saveTimer.stop()
        coordinator.appRoot.stopWorkspaceStateAuxiliaryTimers()
    }

    function restore() {
        if (!coordinator.appSettings) {
            coordinator.appRoot.workspaceStateRestored = true
            return
        }
        restoreFrom(coordinator.appSettings.workspaceState())
    }

    function startupShellReady() {
        const app = coordinator.appRoot
        if (!app.startupWorkspaceRestoreDeferred || app.workspaceStateRestoreActive || app.workspaceStateRestored) return
        app.startupWorkspaceRestoreDeferred = false
        app.startupShellFirstRestoreActive = true
        Qt.callLater(() => coordinator.restore())
    }

    function restoreFrom(state) {
        const app = coordinator.appRoot
        const settings = coordinator.appSettings
        const workspace = coordinator.workspaceController
        const views = coordinator.fileWorkspace
        if (!state) {
            app.workspaceStateRestored = true
            app.startupShellFirstRestoreActive = false
            return
        }

        const restoreGeneration = ++app.workspaceStateRestoreGeneration
        const showHidden = !!state.showHidden
        let leftOpenRequested = false
        let rightOpenRequested = false
        function applyPanelState() {
            if (!leftOpenRequested && workspace.leftPanel.currentPath !== state.leftPath) {
                leftOpenRequested = app.startupShellFirstRestoreActive || workspace.leftPanel.openPath(state.leftPath)
            }
            if (!rightOpenRequested && workspace.rightPanel.currentPath !== state.rightPath) {
                rightOpenRequested = app.startupShellFirstRestoreActive || workspace.rightPanel.openPath(state.rightPath)
            }
            workspace.leftPanel.viewMode = state.leftViewMode
            workspace.rightPanel.viewMode = state.rightViewMode
            workspace.leftPanel.setPanelSortPolicy(state.leftSortRole, state.leftSortOrder)
            workspace.rightPanel.setPanelSortPolicy(state.rightSortRole, state.rightSortOrder)
            workspace.leftPanel.directoryModel.mixFilesAndFolders = state.leftMixFilesAndFolders === true
            workspace.rightPanel.directoryModel.mixFilesAndFolders = state.rightMixFilesAndFolders === true
        }

        stopPersistenceTimers()
        app.workspaceStateRestoreActive = true
        app.workspaceStateSavePaused = true
        app.workspaceStateRestored = false
        app.previewPaneTransitionActive = false
        coordinator.previewCoordinator.clearPreviewTimers()

        const restoreWindowState = !app.startupShellFirstRestoreActive
        if (restoreWindowState) {
            const geometry = settings.sanitizedWindowGeometry(state, 1120, 720)
            if (geometry.valid) {
                if (app.visibility === Window.Maximized) app.visibility = Window.Windowed
                app.x = geometry.x; app.y = geometry.y
                app.width = geometry.width; app.height = geometry.height
            }
        }

        app.sidebarStoredWidth = state.sidebarWidth
        app.previewPaneStoredWidth = state.previewPaneWidth
        app.sidebarPreferredWidth = app.sidebarStoredWidth
        app.previewPanePreferredWidth = !!state.previewPaneVisible ? Math.max(280, app.previewPaneStoredWidth) : 0
        workspace.leftPanel.directoryModel.showHidden = showHidden
        workspace.rightPanel.directoryModel.showHidden = showHidden
        workspace.treeModel.showHidden = showHidden
        workspace.splitEnabled = !!state.splitEnabled
        applyPanelState()

        views.leftPanelView.gridIconSize = state.leftGridIconSize
        views.rightPanelView.gridIconSize = state.rightGridIconSize
        views.leftPanelView.briefRowHeight = state.leftBriefRowHeight
        views.rightPanelView.briefRowHeight = state.rightBriefRowHeight
        views.leftPanelView.showActionBar = state.leftShowActionBar !== false
        views.rightPanelView.showActionBar = state.rightShowActionBar !== false
        views.leftPanelView.showSelectionBadges = state.leftShowSelectionBadges !== false
        views.rightPanelView.showSelectionBadges = state.rightShowSelectionBadges !== false
        views.leftPanelView.showHoverPreviews = state.leftShowHoverPreviews === true
        views.rightPanelView.showHoverPreviews = state.rightShowHoverPreviews === true
        views.leftPanelView.restoreDetailsVisualState(state.leftDetailsVisualState)
        views.rightPanelView.restoreDetailsVisualState(state.rightDetailsVisualState)
        coordinator.previewCoordinator.setPreviewPaneVisible(!!state.previewPaneVisible)
        app.applyPreviewPaneWidth()

        Qt.callLater(() => {
            if (restoreGeneration !== app.workspaceStateRestoreGeneration) return
            app.sidebarStoredWidth = state.sidebarWidth
            app.previewPaneStoredWidth = state.previewPaneWidth
            app.sidebarPreferredWidth = app.sidebarStoredWidth
            applyPanelState()
            app.applyPreviewPaneWidth()
            if (workspace.splitEnabled) views.restoreSplitState(state.fileWorkspaceSplitState)
            else views.expandSinglePanel()
            Qt.callLater(() => {
                if (restoreGeneration !== app.workspaceStateRestoreGeneration) return
                applyPanelState()
                workspace.activePanel = workspace.splitEnabled ? state.activePanel : 0
                app.applyPreviewPaneWidth()
                if (workspace.splitEnabled) views.restoreSplitState(state.fileWorkspaceSplitState)
                Qt.callLater(() => {
                    if (restoreGeneration !== app.workspaceStateRestoreGeneration) return
                    if (app.visible && restoreWindowState) {
                        if (state.windowMaximized) app.visibility = Window.Maximized
                        else if (app.visibility === Window.Maximized) app.visibility = Window.Windowed
                    }
                    coordinator.previewCoordinator.syncPreviewFromActivePanel(true)
                    app.startupShellFirstRestoreActive = false
                    app.workspaceStateRestoreActive = false
                    app.workspaceStateSavePaused = false
                    app.workspaceStateRestored = true
                })
            })
        })
    }
}
