import QtQuick
import QtQml

Item {
    id: root

    property var appRoot
    property var workspaceController
    property var quickLookController
    property var quickLookPopup
    property var propertiesController

    property string pendingPreviewPath: ""
    property string pendingPreviewRefreshPath: ""
    property bool previewOpenSyncPending: false
    readonly property int selectionPreviewDelay: 90

    function activePanelController() {
        if (!root.workspaceController) {
            return null
        }
        return root.workspaceController.activePanel === 0
            ? root.workspaceController.leftPanel
            : root.workspaceController.rightPanel
    }

    function activePanelScrolling() {
        const controller = activePanelController()
        return controller ? controller.scrolling : false
    }

    function quickLook() {
        return root.quickLookController ? root.quickLookController : null
    }

    function app() {
        return root.appRoot ? root.appRoot : null
    }

    function selectedPathsFor(controller) {
        if (!controller) {
            return []
        }
        if (controller.selectedPaths) {
            return controller.selectedPaths()
        }
        if (controller.directoryModel && controller.directoryModel.selectedPaths) {
            return controller.directoryModel.selectedPaths()
        }
        return []
    }

    function modelContainsPath(controller, path) {
        if (!controller || !controller.directoryModel || !path || path.length === 0) {
            return false
        }
        if (!controller.directoryModel.indexOfPath) {
            return true
        }
        return controller.directoryModel.indexOfPath(path) >= 0
    }

    function previewTargetFor(controller) {
        if (!controller) {
            return ""
        }

        const selected = selectedPathsFor(controller)
        if (selected.length > 1) {
            return "selection://"
        }

        if (controller.currentItemPath
                && controller.currentItemPath.length > 0
                && modelContainsPath(controller, controller.currentItemPath)) {
            return controller.currentItemPath
        }

        if (selected.length > 0 && modelContainsPath(controller, selected[0])) {
            return selected[0]
        }

        if (controller.isDeviceRoot) {
            return "devices://"
        }

        if (controller.isFavoritesRoot) {
            return "favorites://"
        }

        return controller.currentPath || ""
    }

    function syncQuickLookPreview(controller, targetPath) {
        const quickLookController = quickLook()
        if (!quickLookController) {
            return
        }

        const selected = selectedPathsFor(controller)
        if (selected.length > 1 && targetPath === "selection://") {
            quickLookController.previewSelection(selected)
            return
        }

        quickLookController.preview(targetPath)
    }

    Timer {
        id: previewSyncTimer
        interval: 250
        repeat: false
        onTriggered: {
            const appRoot = app()
            const quickLookController = quickLook()
            if (!appRoot || !quickLookController) {
                return
            }
            if (!appRoot.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                previewSyncTimer.restart()
                return
            }
            root.syncQuickLookPreview(activePanelController(), root.pendingPreviewPath)
        }
    }

    Timer {
        id: previewSelectionSyncTimer
        interval: root.selectionPreviewDelay
        repeat: false
        onTriggered: {
            const appRoot = app()
            const quickLookController = quickLook()
            if (!appRoot || !quickLookController) {
                return
            }
            if (!appRoot.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                previewSelectionSyncTimer.restart()
                return
            }
            root.syncQuickLookPreview(activePanelController(), root.pendingPreviewPath)
        }
    }

    Timer {
        id: previewOpenSyncTimer
        interval: 0
        repeat: false
        onTriggered: {
            root.previewOpenSyncPending = false
            root.syncPreviewFromActivePanel(true)
        }
    }

    Timer {
        id: previewRefreshTimer
        interval: 150
        repeat: false
        onTriggered: {
            const appRoot = app()
            const quickLookController = quickLook()
            const controller = activePanelController()
            const path = root.pendingPreviewRefreshPath
            root.pendingPreviewRefreshPath = ""
            if (!appRoot || !quickLookController || !controller || !appRoot.previewPaneVisible) {
                return
            }
            if (activePanelScrolling()) {
                root.pendingPreviewRefreshPath = path
                previewRefreshTimer.restart()
                return
            }
            if (path.length > 0 && quickLookController.path === path && modelContainsPath(controller, path)) {
                quickLookController.refresh()
            }
        }
    }

    function schedulePreviewRefreshForModelChange(controller, topLeft, bottomRight, roles) {
        const appRoot = app()
        const quickLookController = quickLook()
        if (!appRoot || !quickLookController || !controller || !appRoot.previewPaneVisible) {
            return
        }
        if (roles && roles.length > 0) {
            return
        }

        const path = quickLookController.path || ""
        if (path.length === 0) {
            return
        }

        const firstRow = topLeft ? topLeft.row : -1
        const lastRow = bottomRight ? bottomRight.row : firstRow
        if (firstRow < 0 || lastRow < firstRow) {
            return
        }

        for (let row = firstRow; row <= lastRow; ++row) {
            if (controller.directoryModel.pathAt(row) === path) {
                root.pendingPreviewRefreshPath = path
                previewRefreshTimer.restart()
                return
            }
        }
    }

    function syncPreviewFromActivePanel(immediate) {
        const appRoot = app()
        const quickLookController = quickLook()
        if (!appRoot || !quickLookController) {
            return
        }

        if (!appRoot.previewPaneVisible) {
            return
        }

        const controller = activePanelController()
        const targetPath = previewTargetFor(controller)

        if (immediate !== true && activePanelScrolling()) {
            root.pendingPreviewPath = targetPath
            return
        }

        if (immediate === true) {
            previewSyncTimer.stop()
            previewSelectionSyncTimer.stop()
            root.pendingPreviewPath = targetPath
            root.syncQuickLookPreview(controller, targetPath)
            return
        }

        previewSelectionSyncTimer.stop()
        root.pendingPreviewPath = targetPath
        previewSyncTimer.restart()
    }

    function schedulePreviewFromSelection() {
        const appRoot = app()
        const quickLookController = quickLook()
        if (!appRoot || !quickLookController || !appRoot.previewPaneVisible) {
            return
        }

        root.pendingPreviewPath = previewTargetFor(activePanelController())
        if (activePanelScrolling()) {
            return
        }

        previewSyncTimer.stop()
        previewSelectionSyncTimer.restart()
    }

    function setPreviewPaneVisible(visible) {
        const appRoot = app()
        const quickLookController = quickLook()
        if (!appRoot) {
            return
        }

        appRoot.previewPaneVisible = visible
        if (!quickLookController) {
            return
        }

        quickLookController.visible = visible
        if (visible) {
            root.previewOpenSyncPending = true
            previewOpenSyncTimer.restart()
        } else {
            root.previewOpenSyncPending = false
            previewOpenSyncTimer.stop()
            quickLookController.preview("")
        }
    }

    function togglePreviewPane() {
        const appRoot = app()
        setPreviewPaneVisible(!(appRoot && appRoot.previewPaneVisible))
    }

    function clearPreviewTimers() {
        previewSyncTimer.stop()
        previewSelectionSyncTimer.stop()
        previewOpenSyncTimer.stop()
        previewRefreshTimer.stop()
        root.previewOpenSyncPending = false
        root.pendingPreviewRefreshPath = ""
    }

    Connections {
        target: root.workspaceController ? root.workspaceController : null
        function onActivePanelChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible) {
                schedulePreviewFromSelection()
            }

            const controller = activePanelController()
            if (controller) {
                root.workspaceController.treeModel.showHidden = controller.directoryModel.showHidden
            }
        }
    }

    Connections {
        target: root.quickLookController ? root.quickLookController : null
        function onVisibleChanged() {
            const appRoot = app()
            const quickLookController = quickLook()
            if (!appRoot || !quickLookController) {
                return
            }
            if (appRoot.previewPaneVisible !== quickLookController.visible) {
                appRoot.previewPaneVisible = quickLookController.visible
            }
            if (!quickLookController.visible) {
                clearPreviewTimers()
                quickLookController.preview("")
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.leftPanel : null
        function onCurrentPathChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0) {
                syncPreviewFromActivePanel(!root.workspaceController.leftPanel.scrolling)
            }
        }
        function onCurrentItemPathChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0) {
                schedulePreviewFromSelection()
            }
        }
        function onScrollingChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0
                    && root.workspaceController.leftPanel.scrolling) {
                clearPreviewTimers()
            }
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0
                    && !root.workspaceController.leftPanel.scrolling) {
                syncPreviewFromActivePanel(false)
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.rightPanel : null
        function onCurrentPathChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1) {
                syncPreviewFromActivePanel(!root.workspaceController.rightPanel.scrolling)
            }
        }
        function onCurrentItemPathChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1) {
                schedulePreviewFromSelection()
            }
        }
        function onScrollingChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1
                    && root.workspaceController.rightPanel.scrolling) {
                clearPreviewTimers()
            }
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1
                    && !root.workspaceController.rightPanel.scrolling) {
                syncPreviewFromActivePanel(false)
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.leftPanel.directoryModel : null
        function onDataChanged(topLeft, bottomRight, roles) {
            if (root.workspaceController.activePanel === 0) {
                root.schedulePreviewRefreshForModelChange(root.workspaceController.leftPanel, topLeft, bottomRight, roles)
            }
        }
        function onSelectionChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0) {
                schedulePreviewFromSelection()
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.rightPanel.directoryModel : null
        function onDataChanged(topLeft, bottomRight, roles) {
            if (root.workspaceController.activePanel === 1) {
                root.schedulePreviewRefreshForModelChange(root.workspaceController.rightPanel, topLeft, bottomRight, roles)
            }
        }
        function onSelectionChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1) {
                schedulePreviewFromSelection()
            }
        }
    }
}
