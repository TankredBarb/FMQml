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
    property bool previewOpenSyncPending: false

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
        if (selected.length > 0 && modelContainsPath(controller, selected[0])) {
            return selected[0]
        }

        if (controller.currentItemPath
                && controller.currentItemPath.length > 0
                && modelContainsPath(controller, controller.currentItemPath)) {
            return controller.currentItemPath
        }

        if (controller.isDeviceRoot) {
            return "devices://"
        }

        return controller.currentPath || ""
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
            quickLookController.preview(root.pendingPreviewPath)
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
            root.pendingPreviewPath = targetPath
            quickLookController.preview(targetPath)
            return
        }

        root.pendingPreviewPath = targetPath
        previewSyncTimer.restart()
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
        previewOpenSyncTimer.stop()
        root.previewOpenSyncPending = false
    }

    Connections {
        target: root.workspaceController ? root.workspaceController : null
        function onActivePanelChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible) {
                syncPreviewFromActivePanel(true)
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
                syncPreviewFromActivePanel(!root.workspaceController.leftPanel.scrolling)
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
                syncPreviewFromActivePanel(!root.workspaceController.rightPanel.scrolling)
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
        function onSelectionChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0) {
                syncPreviewFromActivePanel(!root.workspaceController.leftPanel.scrolling)
            }
        }
    }

    Connections {
        target: root.workspaceController ? root.workspaceController.rightPanel.directoryModel : null
        function onSelectionChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1) {
                syncPreviewFromActivePanel(!root.workspaceController.rightPanel.scrolling)
            }
        }
    }
}
