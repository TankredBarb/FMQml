import QtQuick
import QtQml

Item {
    id: root

    property var appRoot
    property var workspaceController
    property var quickLookController
    property var quickLookPopup
    property var propertiesController
    property bool previewSuppressed: false
    property bool operationSuppressed: false
    property bool renameSuppressed: false
    property bool deleteReleaseActive: false
    property var deleteReleasePaths: []
    property int nextReleaseTokenId: 1
    property var releaseTokens: ({})
    property var renameReleaseTokenIds: []

    property string pendingPreviewPath: ""
    property string pendingPreviewRefreshPath: ""
    property bool previewOpenSyncPending: false
    property bool previewPending: false
    readonly property int selectionPreviewDelay: 90
    readonly property bool interactionTraceEnabled: typeof panelInteractionTraceEnabled !== "undefined"
                                                    && panelInteractionTraceEnabled

    function interactionTrace(stage, detail) {
        if (!root.interactionTraceEnabled) {
            return
        }
        const controller = activePanelController()
        console.log("[FM_INTERACTION][preview]",
                    stage,
                    detail || "",
                    "panel=" + (root.workspaceController ? root.workspaceController.activePanel : -1),
                    "path=" + (controller ? controller.currentPath : ""),
                    "current=" + (controller ? controller.currentItemPath : ""),
                    "target=" + (root.quickLookController ? root.quickLookController.path : ""),
                    "renameSuppressed=" + root.renameSuppressed,
                    "operationSuppressed=" + root.operationSuppressed)
    }

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
        return root.previewSuppressed || (controller ? controller.scrolling : false)
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

        root.interactionTrace("preview-sync", "requestedTarget=" + targetPath)
        const selected = selectedPathsFor(controller)
        if (selected.length > 1 && targetPath === "selection://") {
            quickLookController.previewSelection(selected)
            root.previewPending = false
            return
        }

        quickLookController.preview(targetPath)
        root.previewPending = false
    }

    function setPendingPreviewPath(targetPath, pending) {
        const quickLookController = quickLook()
        root.pendingPreviewPath = targetPath
        root.previewPending = pending
                              && targetPath.length > 0
                              && (targetPath === "selection://" || !quickLookController || quickLookController.path !== targetPath)
    }

    onPreviewSuppressedChanged: {
        if (root.previewSuppressed) {
            return
        }
        const appRoot = app()
        if (!appRoot || !appRoot.previewPaneVisible) {
            return
        }
        if (root.pendingPreviewRefreshPath.length > 0) {
            previewRefreshTimer.restart()
        }
        root.syncPreviewFromActivePanel(false)
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
            root.setPendingPreviewPath(targetPath, true)
            return
        }

        if (immediate === true) {
            previewSyncTimer.stop()
            previewSelectionSyncTimer.stop()
            root.setPendingPreviewPath(targetPath, false)
            root.syncQuickLookPreview(controller, targetPath)
            return
        }

        previewSelectionSyncTimer.stop()
        root.setPendingPreviewPath(targetPath, true)
        previewSyncTimer.restart()
    }

    function schedulePreviewFromSelection() {
        const appRoot = app()
        const quickLookController = quickLook()
        if (!appRoot || !quickLookController || !appRoot.previewPaneVisible) {
            return
        }

        root.setPendingPreviewPath(previewTargetFor(activePanelController()), true)
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
        root.previewPending = false
        root.pendingPreviewRefreshPath = ""
    }

    function samePathList(left, right) {
        if (!left || !right || left.length !== right.length) return false
        for (let i = 0; i < left.length; ++i) {
            if (String(left[i] || "") !== String(right[i] || "")) return false
        }
        return true
    }

    function refreshReleaseState() {
        let operationActive = false
        let renameActive = false
        const deletePaths = []
        const tokens = root.releaseTokens || ({})
        const ids = Object.keys(tokens)
        for (let i = 0; i < ids.length; ++i) {
            const token = tokens[ids[i]]
            if (!token) continue
            if (token.reason === "Rename") {
                renameActive = true
            } else {
                operationActive = true
            }
            if (token.reason === "Delete") {
                const paths = token.paths || []
                for (let pathIndex = 0; pathIndex < paths.length; ++pathIndex) {
                    if (deletePaths.indexOf(paths[pathIndex]) < 0) {
                        deletePaths.push(paths[pathIndex])
                    }
                }
            }
        }
        root.operationSuppressed = operationActive
        root.renameSuppressed = renameActive
        root.deleteReleaseActive = deletePaths.length > 0
        root.deleteReleasePaths = deletePaths
    }

    function acquireReleaseToken(reason, paths, restorePreview) {
        const id = root.nextReleaseTokenId++
        const next = Object.assign({}, root.releaseTokens)
        next[id] = {
            "id": id,
            "reason": reason,
            "paths": paths ? Array.from(paths) : [],
            "restorePreview": restorePreview === true
        }
        root.releaseTokens = next
        root.refreshReleaseState()
        operationSuppressionTimer.restart()
        root.interactionTrace("preview-token-acquired",
                              "token=" + id + " reason=" + reason
                              + " paths=" + next[id].paths.join("|"))
        return id
    }

    function releaseToken(id, restorePreview) {
        if (!id || !root.releaseTokens[id]) {
            return false
        }
        const token = root.releaseTokens[id]
        const next = Object.assign({}, root.releaseTokens)
        delete next[id]
        root.releaseTokens = next
        root.refreshReleaseState()
        root.interactionTrace("preview-token-released", "token=" + id + " reason=" + token.reason)
        if (Object.keys(next).length === 0) {
            operationSuppressionTimer.stop()
            const shouldRestore = restorePreview === undefined
                    ? token.restorePreview === true
                    : restorePreview === true
            if (shouldRestore) {
                root.syncPreviewFromActivePanel(true)
            }
        }
        return true
    }

    function releaseTokensForReason(reason, restorePreview) {
        const tokens = root.releaseTokens || ({})
        const ids = Object.keys(tokens)
        let released = false
        for (let i = 0; i < ids.length; ++i) {
            const token = tokens[ids[i]]
            if (token && token.reason === reason) {
                released = root.releaseToken(Number(ids[i]), false) || released
            }
        }
        if (released && restorePreview === true && Object.keys(root.releaseTokens).length === 0) {
            root.syncPreviewFromActivePanel(true)
        }
        return released
    }

    function finishOperationSuppression(paths) {
        const tokens = root.releaseTokens || ({})
        const ids = Object.keys(tokens)
        for (let i = 0; i < ids.length; ++i) {
            const token = tokens[ids[i]]
            if (token && token.reason === "Delete"
                    && (!paths || root.samePathList(token.paths || [], paths))) {
                root.releaseToken(Number(ids[i]), true)
                return true
            }
        }
        return false
    }

    function hasDeleteReleaseToken(paths) {
        const tokens = root.releaseTokens || ({})
        const ids = Object.keys(tokens)
        for (let i = 0; i < ids.length; ++i) {
            const token = tokens[ids[i]]
            if (token && token.reason === "Delete"
                    && root.samePathList(token.paths || [], paths || [])) {
                return true
            }
        }
        return false
    }

    function clearPreviewForPaths(paths, forceRelease) {
        const quickLookController = quickLook()
        if (!quickLookController || !paths || paths.length === 0) return
        if (forceRelease === true) {
            root.clearPreviewTimers()
            if (root.quickLookPopup) {
                root.quickLookPopup.close()
                root.quickLookPopup.previewPath = ""
            }
            quickLookController.preview("")
            return
        }

        const previewPath = quickLookController.path || ""
        const previewAbsolutePath = quickLookController.absolutePath || ""
        for (let i = 0; i < paths.length; ++i) {
            const path = paths[i] || ""
            if (path.length === 0) continue
            const normalizedPath = path.toLowerCase()
            if (previewPath === path || previewAbsolutePath === path
                    || previewPath.toLowerCase() === normalizedPath
                    || previewAbsolutePath.toLowerCase() === normalizedPath) {
                root.clearPreviewTimers()
                if (root.quickLookPopup) {
                    root.quickLookPopup.close()
                    root.quickLookPopup.previewPath = ""
                }
                quickLookController.preview("")
                return
            }
        }
    }

    function releasePreviewForPaths(paths, forceRelease) {
        root.interactionTrace("preview-release", "paths=" + (paths ? paths.length : 0) + " force=" + (forceRelease === true))
        if (forceRelease === true) {
            const tokenId = root.acquireReleaseToken("Delete", paths, true)
            root.clearPreviewForPaths(paths, true)
            return tokenId
        }
        root.clearPreviewForPaths(paths, forceRelease === true)
        return 0
    }

    function beginRenameSuppression(paths, releasedCallback) {
        root.interactionTrace("rename-suppression-begin", "paths=" + (paths ? paths.length : 0))
        const tokenId = root.acquireReleaseToken("Rename", paths, true)
        const nextTokens = Array.from(root.renameReleaseTokenIds)
        nextTokens.push(tokenId)
        root.renameReleaseTokenIds = nextTokens
        root.clearPreviewForPaths(paths, true)
        if (releasedCallback) {
            Qt.callLater(function() {
                Qt.callLater(function() {
                    if (!root.releaseTokens[tokenId]) {
                        return
                    }
                    root.interactionTrace("preview-release-ready", "token=" + tokenId + " reason=Rename")
                    releasedCallback(tokenId)
                })
            })
        }
        return tokenId
    }

    function finishRenameSuppression(restorePreview) {
        root.interactionTrace("rename-suppression-finish", "restore=" + (restorePreview === true))
        if (root.renameReleaseTokenIds.length === 0) {
            return
        }
        const nextTokens = Array.from(root.renameReleaseTokenIds)
        const tokenId = nextTokens.pop()
        root.renameReleaseTokenIds = nextTokens
        root.releaseToken(tokenId, restorePreview === true)
    }

    function pathBelongsToVolumeRoot(path, rootPath) {
        return path && path.length > 0
            && root.workspaceController
            && root.workspaceController.pathBelongsToVolumeRoot
            && root.workspaceController.pathBelongsToVolumeRoot(path, rootPath)
    }

    function releasePreviewForVolumeRoot(rootPath) {
        if (!rootPath || rootPath.length === 0) return
        const quickLookController = quickLook()
        const previewPath = quickLookController ? (quickLookController.path || "") : ""
        const previewAbsolutePath = quickLookController ? (quickLookController.absolutePath || "") : ""
        const popupPath = root.quickLookPopup ? (root.quickLookPopup.previewPath || "") : ""
        const previewMatches = pathBelongsToVolumeRoot(previewPath, rootPath)
            || pathBelongsToVolumeRoot(previewAbsolutePath, rootPath)
        const popupMatches = pathBelongsToVolumeRoot(popupPath, rootPath)
        if (previewMatches && quickLookController) {
            root.clearPreviewTimers()
            quickLookController.preview("devices://")
        }
        if (popupMatches && root.quickLookPopup) {
            root.quickLookPopup.close()
            root.quickLookPopup.previewPath = ""
        }
    }

    Timer {
        id: operationSuppressionTimer
        interval: 10000
        repeat: false
        onTriggered: {
            const count = Object.keys(root.releaseTokens || ({})).length
            if (count > 0) {
                const tokens = root.releaseTokens || ({})
                const details = Object.keys(tokens).map(function(id) {
                    const token = tokens[id]
                    return id + ":" + token.reason + ":" + (token.paths || []).join("|")
                }).join(",")
                console.warn("[FM_INTERACTION][preview-token-watchdog] activeTokens="
                             + count + " tokens=" + details)
                restart()
            }
        }
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
