import QtQuick

QtObject {
    id: root

    property var panel: null
    property var createRenameTimerRef: null
    property var renameFocusTimerRef: null
    property var currentIndexEnsureTimerRef: null
    property var windowProvider: null
    property var viewRegistry: null
    property var renameSelectionSnapshot: []
    property string committedRenameOldPath: ""
    property string committedRenameNewPath: ""

    function windowObject() {
        return root.windowProvider ? root.windowProvider() : null
    }

    function trace(stage, detail) {
        if (root.panel && root.panel.traceRenameFocus) {
            root.panel.traceRenameFocus(stage, detail || "")
        }
    }

    function stopTimer(timer) {
        if (timer) {
            timer.stop()
        }
    }

    function restartTimer(timer) {
        if (timer) {
            timer.restart()
        }
    }

    function restorePreviewAfterRenameEdit() {
        const window = root.windowObject()
        if (window && window.finishRenamePreviewSuppression) {
            window.finishRenamePreviewSuppression(true)
        } else if (window && window.previewPaneVisible) {
            window.syncPreviewFromActivePanel(true)
        }
    }

    function captureRenameSelection() {
        const panel = root.panel
        root.renameSelectionSnapshot = panel ? Array.from(panel.controller.selectedPaths()) : []
    }

    function clearRenameIdentity() {
        root.renameSelectionSnapshot = []
        root.committedRenameOldPath = ""
        root.committedRenameNewPath = ""
    }

    function clearPendingInlineRenameFocus() {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("clearPendingInlineRenameFocus")
        panel.pendingRenameFocusPath = ""
        panel.pendingRenameFocusAttempts = 0
        panel.pendingRenameFocusSelectText = false
        root.stopTimer(root.renameFocusTimerRef)
    }

    function queueInlineRenameFocus(path, selectText) {
        const panel = root.panel
        if (!panel || !path || path.length === 0) {
            return
        }

        root.trace("queueInlineRenameFocus", "path=" + path + " select=" + (selectText === true))
        panel.pendingRenameFocusPath = path
        panel.pendingRenameFocusAttempts = 0
        panel.pendingRenameFocusSelectText = selectText === true
        root.stopTimer(root.currentIndexEnsureTimerRef)
        root.restartTimer(root.renameFocusTimerRef)
    }

    function retryPendingInlineRenameFocus() {
        const panel = root.panel
        if (!panel) {
            return
        }

        if (panel.pendingRenameFocusAttempts === 0 || panel.pendingRenameFocusAttempts % 10 === 0) {
            root.trace("retryPendingInlineRenameFocus", "nextAttempt=" + (panel.pendingRenameFocusAttempts + 1))
        }
        if (++panel.pendingRenameFocusAttempts <= 120) {
            root.restartTimer(root.renameFocusTimerRef)
        } else {
            root.clearPendingInlineRenameFocus()
        }
    }

    function tryFocusPendingInlineRename() {
        const panel = root.panel
        if (!panel) {
            return
        }

        const path = panel.pendingRenameFocusPath
        if (path.length === 0) {
            return
        }

        if (panel.pendingRenameFocusAttempts === 0 || panel.pendingRenameFocusAttempts % 10 === 0) {
            root.trace("tryFocusPendingInlineRename", "path=" + path + " attempt=" + panel.pendingRenameFocusAttempts)
        }

        if (!panel.isRenaming
                || panel.pendingInlineRenamePath.length === 0
                || !panel.samePanelPath(panel.pendingInlineRenamePath, path)) {
            root.trace("tryFocusPendingInlineRename-clear-stale", "path=" + path)
            root.clearPendingInlineRenameFocus()
            return
        }

        const idx = panel.controller.directoryModel.indexOfPath(path)
        const view = panel.activeView()
        if (!view || idx < 0 || idx >= panel.controller.directoryModel.count) {
            root.trace("tryFocusPendingInlineRename-wait-view", "path=" + path + " idx=" + idx)
            root.retryPendingInlineRenameFocus()
            return
        }

        if (view.currentIndex !== idx) {
            panel.setViewCurrentIndexWithoutSelection(view, idx)
        }

        if (!view.currentItem || !view.currentItem.focusRenameEditor) {
            root.trace("tryFocusPendingInlineRename-wait-item", "path=" + path)
            root.retryPendingInlineRenameFocus()
            return
        }

        if (view.currentItem.focusRenameEditor(panel.pendingRenameFocusSelectText)
                || (view.currentItem.renameEditorHasFocus && view.currentItem.renameEditorHasFocus())) {
            root.trace("tryFocusPendingInlineRename-success", "path=" + path)
            root.clearPendingInlineRenameFocus()
            return
        }

        root.trace("tryFocusPendingInlineRename-focus-failed", "path=" + path)
        root.retryPendingInlineRenameFocus()
    }

    function cancelInlineRename() {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("cancelInlineRename")
        panel.isRenaming = false
        panel.pendingInlineRenamePath = ""
        root.clearPendingInlineRenameFocus()
        root.cancelCreateRenameSession()
        root.restorePreviewAfterRenameEdit()
        root.clearRenameIdentity()
    }

    function cancelActiveInlineRename() {
        const panel = root.panel
        if (!panel || !root.inlineRenameFocusActive()) {
            return false
        }

        root.trace("cancelActiveInlineRename")
        const view = panel.activeView()
        if (view && view.currentItem && view.currentItem.cancelRename) {
            view.currentItem.cancelRename()
        }
        root.cancelInlineRename()
        return true
    }

    function cancelForPathChange() {
        const panel = root.panel
        if (!panel) {
            return
        }

        panel.isRenaming = false
        panel.pendingInlineRenamePath = ""
        root.clearPendingInlineRenameFocus()
        root.cancelCreateRenameSession()
        const window = root.windowObject()
        if (window && window.finishRenamePreviewSuppression) {
            window.finishRenamePreviewSuppression(false)
        }
    }

    function beginCreateRenameSession(path) {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("beginCreateRenameSession", "path=" + (path || ""))
        panel.createRenameSessionId += 1
        panel.createRenamePath = path || ""
        panel.createRenameAttempts = 0
        panel.createRenameRevealReady = false
        panel.createRenameStarted = false
        root.stopTimer(root.createRenameTimerRef)
        root.clearStaleInlineRenameState()
    }

    function cancelCreateRenameSession() {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("cancelCreateRenameSession")
        panel.createRenameSessionId += 1
        panel.createRenamePath = ""
        panel.createRenameAttempts = 0
        panel.createRenameRevealReady = false
        panel.createRenameStarted = false
        root.stopTimer(root.createRenameTimerRef)
    }

    function finishCreateRenameSession() {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("finishCreateRenameSession")
        panel.createRenamePath = ""
        panel.createRenameAttempts = 0
        panel.createRenameRevealReady = false
        panel.createRenameStarted = false
        root.stopTimer(root.createRenameTimerRef)
    }

    function createRenameSessionActive() {
        const panel = root.panel
        return Boolean(panel && panel.createRenamePath.length > 0 && !panel.createRenameStarted)
    }

    function inlineRenameFocusActive() {
        const panel = root.panel
        return Boolean(panel
                       && (panel.isRenaming
                           || panel.pendingInlineRenamePath.length > 0
                           || root.createRenameSessionActive()))
    }

    function recoverInlineRenameFocus(reason) {
        const panel = root.panel
        if (!panel) {
            return false
        }

        root.trace("recoverInlineRenameFocus-request", reason || "")
        if (!panel.active) {
            root.trace("recoverInlineRenameFocus-skip", "reason=panel-inactive " + (reason || ""))
            return false
        }
        if (!root.inlineRenameFocusActive()) {
            root.trace("recoverInlineRenameFocus-skip", "reason=inactive " + (reason || ""))
            return false
        }

        const window = root.windowObject()
        if (!window || !window.active) {
            root.trace("recoverInlineRenameFocus-skip", "reason=window-inactive " + (reason || ""))
            return false
        }
        if (panel.panelKeysBlockedByOverlay()) {
            root.trace("recoverInlineRenameFocus-skip", "reason=overlay " + (reason || ""))
            return false
        }
        if (panel.pendingInlineRenamePath.length === 0) {
            root.trace("recoverInlineRenameFocus-skip", "reason=no-path " + (reason || ""))
            return false
        }

        root.queueInlineRenameFocus(panel.pendingInlineRenamePath, false)
        return true
    }

    function clearStaleInlineRenameState() {
        const panel = root.panel
        if (!panel || (!panel.isRenaming && panel.pendingInlineRenamePath.length === 0)) {
            return
        }

        root.trace("clearStaleInlineRenameState-check")
        const view = panel.activeView()
        const idx = view ? view.currentIndex : -1
        const hasActiveEditor = Boolean(view && view.currentItem && view.currentItem.isRenaming)
        if (hasActiveEditor
                && panel.pendingInlineRenamePath.length > 0
                && idx >= 0
                && idx < panel.controller.directoryModel.count
                && panel.samePanelPath(panel.controller.directoryModel.pathAt(idx), panel.pendingInlineRenamePath)) {
            return
        }

        root.trace("clearStaleInlineRenameState-clear")
        panel.isRenaming = false
        panel.pendingInlineRenamePath = ""
        root.clearPendingInlineRenameFocus()
        root.restorePreviewAfterRenameEdit()
    }

    function queueCreateRenameAttempt() {
        const panel = root.panel
        if (!panel || panel.createRenamePath.length === 0 || panel.createRenameStarted) {
            return
        }

        root.trace("queueCreateRenameAttempt", "path=" + panel.createRenamePath + " attempts=" + panel.createRenameAttempts)
        root.restartTimer(root.createRenameTimerRef)
    }

    function startRenameForPath(path) {
        const panel = root.panel
        if (!panel) {
            return false
        }

        root.trace("startRenameForPath-begin", "path=" + (path || ""))
        if (!path || path.length === 0 || panel.isCurrentPathReadOnlyContainer) {
            root.trace("startRenameForPath-reject", "reason=empty-or-readonly path=" + (path || ""))
            return false
        }

        const idx = panel.controller.directoryModel.indexOfPath(path)
        if (idx < 0) {
            root.trace("startRenameForPath-reject", "reason=missing-index path=" + path)
            return false
        }

        const view = panel.activeView()
        if (!view || view.count <= idx) {
            root.trace("startRenameForPath-reject", "reason=bad-view path=" + path + " idx=" + idx)
            return false
        }

        panel.setViewCurrentIndexWithoutSelection(view, idx)
        panel.controller.directoryModel.selectOnly(idx)
        if (!panel.resizeOptimized) {
            if (view.forceLayout) {
                view.forceLayout()
            }
            view.positionViewAtIndex(idx, panel.viewMode === 0 ? ListView.Contain : GridView.Contain)
            if (view.forceLayout) {
                view.forceLayout()
            }
        }
        if (view.currentIndex !== idx) {
            root.trace("startRenameForPath-reject", "reason=current-index-mismatch path=" + path + " idx=" + idx + " current=" + view.currentIndex)
            return false
        }

        const currentPath = panel.controller.directoryModel.pathAt(view.currentIndex)
        if (!panel.samePanelPath(currentPath, path)) {
            root.trace("startRenameForPath-reject", "reason=current-path-mismatch path=" + path + " currentPath=" + currentPath)
            return false
        }

        if (!view.currentItem) {
            root.trace("startRenameForPath-reject", "reason=no-current-item path=" + path)
            return false
        }

        panel.pendingInlineRenamePath = path
        root.captureRenameSelection()
        root.windowObject().beginRenamePreviewSuppression([path], function() {
            const renameIndex = panel.controller.directoryModel.indexOfPath(path)
            const renameView = panel.activeView()
            if (panel.pendingInlineRenamePath !== path
                    || renameIndex < 0
                    || !renameView
                    || renameIndex >= renameView.count) {
                root.trace("startRenameForPath-cancelled-after-release", "path=" + path)
                panel.pendingInlineRenamePath = ""
                root.restorePreviewAfterRenameEdit()
                return
            }

            panel.setViewCurrentIndexWithoutSelection(renameView, renameIndex)
            if (!renameView.currentItem || !renameView.currentItem.startRename) {
                root.trace("startRenameForPath-no-item-after-release", "path=" + path)
                panel.pendingInlineRenamePath = ""
                root.restorePreviewAfterRenameEdit()
                return
            }

            renameView.currentItem.startRename()
            panel.isRenaming = true
            root.queueInlineRenameFocus(path, true)
            root.trace("startRenameForPath-started", "path=" + path)
        })
        root.trace("startRenameForPath-awaiting-release", "path=" + path)
        return true
    }

    function startManualRename(index) {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("manual-startRename-begin")
        if (panel.isCurrentPathReadOnlyContainer) return
        let idx = index
        if (idx < 0) return

        if (panel.controller.directoryModel.selectedCount > 1) {
            const selectedPaths = panel.controller.selectedPaths()
            root.windowObject().showBatchRename(selectedPaths)
            return
        }

        panel.pendingInlineRenamePath = panel.controller.directoryModel.pathAt(idx)
        root.captureRenameSelection()
        const renamePath = panel.pendingInlineRenamePath
        root.windowObject().beginRenamePreviewSuppression([renamePath], function() {
            const renameIndex = panel.controller.directoryModel.indexOfPath(renamePath)
            const renameView = panel.activeView()
            if (panel.pendingInlineRenamePath !== renamePath
                    || renameIndex < 0
                    || !renameView
                    || renameIndex >= renameView.count) {
                panel.pendingInlineRenamePath = ""
                root.restorePreviewAfterRenameEdit()
                root.trace("manual-startRename-cancelled-after-release", "path=" + renamePath)
                return
            }

            panel.setViewCurrentIndexWithoutSelection(renameView, renameIndex)
            const started = root.viewRegistry.startCurrentItemRename()
            if (started) {
                panel.isRenaming = true
                root.queueInlineRenameFocus(renamePath, true)
                root.trace("manual-startRename-started", "path=" + renamePath)
            } else {
                panel.pendingInlineRenamePath = ""
                root.restorePreviewAfterRenameEdit()
                root.trace("manual-startRename-failed")
            }
        })
        root.trace("manual-startRename-awaiting-release", "path=" + renamePath)
    }

    function tryStartCreateRename() {
        const panel = root.panel
        if (!panel || panel.createRenamePath.length === 0 || panel.createRenameStarted) {
            return
        }

        root.trace("tryStartCreateRename", "path=" + panel.createRenamePath + " attempts=" + panel.createRenameAttempts)
        const sessionId = panel.createRenameSessionId
        root.clearStaleInlineRenameState()
        if (panel.navigationCommitPending()
                || panel.controller.directoryModel.loading
                || panel.pendingRevealPath.length > 0
                || !panel.createRenameRevealReady) {
            root.trace("tryStartCreateRename-wait",
                       "navPending=" + panel.navigationCommitPending()
                       + " loading=" + panel.controller.directoryModel.loading
                       + " pendingReveal=" + panel.pendingRevealPath
                       + " revealReady=" + panel.createRenameRevealReady)
            root.queueCreateRenameAttempt()
            return
        }

        if (root.startRenameForPath(panel.createRenamePath)) {
            if (sessionId !== panel.createRenameSessionId) {
                root.trace("tryStartCreateRename-drop-session", "path=" + panel.createRenamePath)
                return
            }
            root.trace("tryStartCreateRename-started", "path=" + panel.createRenamePath)
            panel.createRenameStarted = true
            panel.createRenamePath = ""
            panel.createRenameAttempts = 0
            return
        }

        if (sessionId !== panel.createRenameSessionId) {
            root.trace("tryStartCreateRename-drop-session-after-fail")
            return
        }
        if (++panel.createRenameAttempts <= 180) {
            root.queueCreateRenameAttempt()
        } else {
            root.trace("tryStartCreateRename-timeout")
            root.cancelCreateRenameSession()
        }
    }

    function focusRenamedPath(path) {
        const panel = root.panel
        if (!panel) {
            return
        }

        if (!path || path.length === 0) {
            root.restorePreviewAfterRenameEdit()
            return
        }

        panel.requestRevealPath(path, true, "rename")
    }

    function handleEntryRenamed(oldPath, newPath) {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("controller-entryRenamed", "old=" + oldPath + " new=" + newPath)
        if (panel.pendingInlineRenamePath.length > 0
                && panel.samePanelPath(oldPath, panel.pendingInlineRenamePath)) {
            panel.isRenaming = false
            panel.pendingInlineRenamePath = ""
            root.clearPendingInlineRenameFocus()
            root.finishCreateRenameSession()
            root.committedRenameOldPath = oldPath
            root.committedRenameNewPath = newPath
            root.focusRenamedPath(newPath)
        }
    }

    function handleEntryCreated(path) {
        root.trace("controller-entryCreated", "path=" + path)
        root.beginCreateRenameSession(path)
        const panel = root.panel
        if (panel) {
            panel.requestRevealPath(path, true, "create")
        }
    }

    function handlePathRevealRequested(path) {
        const panel = root.panel
        if (!panel) {
            return
        }

        root.trace("controller-pathRevealRequested", "path=" + path)
        panel.requestRevealPath(path, true, "navigation")
    }

    function handleRevealFinished(path, purpose, success) {
        const panel = root.panel
        if (!panel) {
            return
        }
        root.trace("reveal-finished", "path=" + path + " purpose=" + purpose + " success=" + success)
        if (purpose === "rename") {
            if (success && panel.samePanelPath(path, root.committedRenameNewPath)) {
                const mappedPaths = root.renameSelectionSnapshot.map(function(selectedPath) {
                    return panel.samePanelPath(selectedPath, root.committedRenameOldPath)
                            ? root.committedRenameNewPath : selectedPath
                })
                const rows = mappedPaths.map(function(selectedPath) {
                    return panel.controller.directoryModel.indexOfPath(selectedPath)
                }).filter(function(row) { return row >= 0 })
                panel.controller.directoryModel.selectRows(rows)
            }
            root.restorePreviewAfterRenameEdit()
            root.clearRenameIdentity()
            return
        }
        if (purpose !== "create"
                || panel.createRenamePath.length === 0
                || !panel.samePanelPath(panel.createRenamePath, path)) {
            return
        }
        if (!success) {
            root.cancelCreateRenameSession()
            return
        }
        panel.createRenamePath = path
        panel.createRenameRevealReady = true
        root.queueCreateRenameAttempt()
    }
}
