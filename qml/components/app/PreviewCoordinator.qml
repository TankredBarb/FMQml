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
    property int hoverPreviewDelayMs: 250

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

    function previewTargetFor(controller) {
        if (!controller) {
            return ""
        }

        const appRoot = app()
        if (appRoot && appRoot.previewOnHover && controller.hoveredPath && controller.hoveredPath.length > 0) {
            return controller.hoveredPath
        }

        const selected = selectedPathsFor(controller)
        if (selected.length > 0) {
            return selected[0]
        }

        if (controller.currentItemPath && controller.currentItemPath.length > 0) {
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
        id: hoverPreviewTimer
        interval: root.hoverPreviewDelayMs
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
                hoverPreviewTimer.restart()
                return
            }
            quickLookController.preview(root.pendingPreviewPath)
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
            hoverPreviewTimer.stop()
            root.pendingPreviewPath = targetPath
            quickLookController.preview(targetPath)
            return
        }

        root.pendingPreviewPath = targetPath
        previewSyncTimer.restart()
    }

    function scheduleHoverPreview(path) {
        const appRoot = app()
        if (!appRoot || !quickLook()) {
            return
        }

        if (!appRoot.previewPaneVisible) {
            return
        }

        if (activePanelScrolling()) {
            root.pendingPreviewPath = path
            return
        }

        if (root.hoverPreviewDelayMs <= 0) {
            hoverPreviewTimer.stop()
            return
        }

        root.pendingPreviewPath = path
        hoverPreviewTimer.restart()
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
            syncPreviewFromActivePanel(true)
        } else {
            quickLookController.preview("")
        }
    }

    function togglePreviewPane() {
        const appRoot = app()
        setPreviewPaneVisible(!(appRoot && appRoot.previewPaneVisible))
    }

    function clearPreviewTimers() {
        previewSyncTimer.stop()
        hoverPreviewTimer.stop()
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
        function onHoveredPathChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 0 && appRoot.previewOnHover) {
                if (root.workspaceController.leftPanel.hoveredPath.length > 0) {
                    scheduleHoverPreview(root.workspaceController.leftPanel.hoveredPath)
                } else {
                    hoverPreviewTimer.stop()
                }
            }
        }
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
        function onHoveredPathChanged() {
            const appRoot = app()
            if (appRoot && appRoot.previewPaneVisible && root.workspaceController.activePanel === 1 && appRoot.previewOnHover) {
                if (root.workspaceController.rightPanel.hoveredPath.length > 0) {
                    scheduleHoverPreview(root.workspaceController.rightPanel.hoveredPath)
                } else {
                    hoverPreviewTimer.stop()
                }
            }
        }
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
