import QtQuick

QtObject {
    id: root

    property var panel: null
    property var loadingRailTimerRef: null
    property var scrollStopTimerRef: null

    function handleArchiveModeChanged() {
        const p = root.panel
        if (!p || !p.loadingDirectory) {
            return
        }

        if (p.isCurrentPathArchive) {
            root.loadingRailTimerRef.stop()
            p.loadingRailReady = p.controller.directoryModel.count === 0
        } else {
            p.loadingRailReady = false
            root.loadingRailTimerRef.start()
        }
    }

    function updateDirectoryLoadingState() {
        const p = root.panel
        if (!p) {
            return
        }

        p.disableFileViewsReuse()
        if (p.loadingDirectory) {
            if (p.isCurrentPathArchive) {
                root.loadingRailTimerRef.stop()
                p.loadingRailReady = p.controller.directoryModel.count === 0
            } else if (!p.loadingRailReady && !root.loadingRailTimerRef.running) {
                root.loadingRailTimerRef.start()
            }
            p.scrolling = true
            p.controller.scrolling = true
            p.suppressHoverBriefly()
            root.scrollStopTimerRef.restart()
            return
        }

        root.loadingRailTimerRef.stop()
        p.loadingRailReady = false

        const restoringScroll = p.pendingScrollRestorePath.length > 0
        if (p.active
                && !restoringScroll
                && !p.navigationCommitPending()
                && p.pendingRevealPath.length === 0) {
            p.focusContentAndQueueCurrentIndexEnsure()
        }

        if (restoringScroll) {
            p.queuePendingScrollRestore()
        }
        root.scrollStopTimerRef.restart()
    }

    function handleDirectoryCountChanged() {
        const p = root.panel
        if (!p || !p.controller || !p.controller.directoryModel) {
            return
        }

        if (p.controller.directoryModel.loading
                && p.isCurrentPathArchive
                && p.controller.directoryModel.count > 0
                && p.controller.directoryModel.scanProgress < 0) {
            root.loadingRailTimerRef.stop()
            p.loadingRailReady = false
            if (p.scrolling) {
                root.scrollStopTimerRef.restart()
            }
        }
    }

    function loadingFolderName() {
        const p = root.panel
        if (!p || !p.controller) {
            return "this folder"
        }

        let path = p.controller.navigationPending && p.controller.pendingNavigationPath.length > 0
            ? p.controller.pendingNavigationPath
            : p.controller.currentPath
        if (path.endsWith("/") || path.endsWith("\\")) {
            path = path.slice(0, -1)
        }
        const parts = path.split(/[/\\]/).filter(part => part.length > 0)
        if (parts.length === 0) {
            return "this folder"
        }
        let lastPart = parts[parts.length - 1]
        if (lastPart.endsWith("|")) {
            lastPart = lastPart.slice(0, -1)
        }
        return lastPart
    }
}
