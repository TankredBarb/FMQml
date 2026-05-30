import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window
import FM
import "../style"
import "common"

Pane {
    id: root

    required property var controller
    required property var workspaceController
    property var propertiesController
    property bool active: false
    property bool liveResizeActive: false
    signal detailsVisualStateChanged()
    readonly property bool showActiveHighlight: root.active && root.workspaceController.splitEnabled
    readonly property int viewMode: root.controller.viewMode
    readonly property bool virtualRootMode: root.controller.isDeviceRoot || root.controller.isFavoritesRoot
    readonly property var favoritesBackend: typeof favoritesController !== "undefined" ? favoritesController : null
    readonly property bool containsActiveFocus: root.activeFocus
                                              || listView.activeFocus
                                              || gridView.activeFocus
                                              || briefView.activeFocus
                                              || storageView.activeFocus
                                              || favoritesView.activeFocus
    property int gridIconSize: 48
    readonly property int gridIconMinSize: 32
    readonly property int gridIconMaxSize: 96
    readonly property int gridCellWidth: Math.max(96, gridIconSize + 52)
    readonly property int gridCellHeight: Math.max(112, gridIconSize + 72)
    property int briefColumnWidth: 240
    property int briefRowHeight: 28
    readonly property int briefRowMinHeight: 22
    readonly property int briefRowMaxHeight: 64
    readonly property int footerHeight: 32
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useHighQualitySystemIcons : true
    readonly property bool showThumbnails: typeof appSettings !== "undefined" && appSettings ? appSettings.showThumbnails : true
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings ? appSettings.ultraLightMode : false
    readonly property bool effectiveShowThumbnails: root.showThumbnails && !root.ultraLightMode
    readonly property real horizontalScrollX: horizontalFlick ? horizontalFlick.contentX : 0
    readonly property bool horizontalScrollActive: root.viewMode === 0 && horizontalFlick && horizontalFlick.contentWidth > horizontalFlick.width
    property bool showLoadingRail: false
    readonly property bool isCurrentPathArchive: root.controller.currentPath ? root.controller.currentPath.toLowerCase().startsWith("archive://") : false
    readonly property bool isCurrentPathManagedIsoMount: root.workspaceController && root.controller.currentPath
        ? root.workspaceController.isInsideManagedIsoMount(root.controller.currentPath)
        : false
    readonly property bool isCurrentPathReadOnlyContainer: root.isCurrentPathArchive || root.isCurrentPathManagedIsoMount
    readonly property var panelErrorInfo: root.controller.lastError
        && root.controller.lastError.code
        && root.controller.lastError.code !== "none"
            ? root.controller.lastError
            : (root.controller.directoryModel.lastError || ({}))

    onIsCurrentPathArchiveChanged: {
        if (root.controller.directoryModel.loading) {
            if (isCurrentPathArchive) {
                loadingRailTimer.stop()
                root.showLoadingRail = true
            } else {
                root.showLoadingRail = false
                loadingRailTimer.start()
            }
        }
    }
    property bool scrolling: false
    property bool previewScrollActive: false
    property var scrollPositions: ({})
    property string pendingScrollRestorePath: ""
    property real pendingScrollRestoreY: -1
    property bool pendingScrollRestoreEnabled: false
    property string targetSelectPath: ""
    property string pendingInlineRenamePath: ""
    property bool disableSelectionOnCurrentIndexChanged: false
    property bool pendingAutoNameColumnWidthUpdate: false
    property real resizeFrozenListWidth: 0
    property real resizeFrozenBriefCellWidth: 0
    property bool showPanelBreadcrumbs: true
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property bool effectsReduced: root.resizeOptimized || root.ultraLightMode
    readonly property bool lightweightDelegates: root.resizeOptimized || root.ultraLightMode
    onResizeOptimizedChanged: {
        if (root.resizeOptimized) {
            root.resizeFrozenListWidth = horizontalFlick ? horizontalFlick.width : root.width
            root.resizeFrozenBriefCellWidth = briefView ? briefView.cellWidth : 0
        } else {
            root.resizeFrozenListWidth = 0
            root.resizeFrozenBriefCellWidth = 0
        }
    }
    focus: root.active
    property bool showZebraStriping: true
    property bool showGridlines: true

    readonly property bool isRenaming: {
        if (root.viewMode === 2) {
            return briefView.currentItem ? briefView.currentItem.isRenaming : false
        } else if (root.viewMode === 0) {
            return listView.currentItem ? listView.currentItem.isRenaming : false
        } else {
            return gridView.currentItem ? gridView.currentItem.isRenaming : false
        }
    }

    // Column widths for Details View (viewMode = 2)
    property real preferredColWidthName: 220
    property bool nameColumnManuallyResized: false
    readonly property real totalOtherColumnsWidth: {
        let w = 0
        if (colShowSize) w += colWidthSize
        if (colShowType) w += colWidthType
        if (colShowDate) w += colWidthDate
        if (colShowDateCreated) w += colWidthDateCreated
        if (colShowExtension) w += colWidthExtension
        if (colShowAttributes) w += colWidthAttributes
        if (colShowResolution) w += colWidthResolution
        if (colShowDuration) w += colWidthDuration
        if (colShowArtist) w += colWidthArtist
        if (colShowAlbum) w += colWidthAlbum
        if (colShowBitrate) w += colWidthBitrate
        return w
    }
    property real colWidthName: 220
    property real colWidthSize:         90
    property real colWidthType:        130
    property real colWidthDate:        150
    property real colWidthDateCreated: 150
    property real colWidthExtension:    70
    property real colWidthAttributes:   70
    property real colWidthResolution:  100
    property real colWidthDuration:     80
    property real colWidthArtist:      140
    property real colWidthAlbum:       140
    property real colWidthBitrate:      80

    // Column visibility (Name is always visible and not togglable)
    property bool colShowSize:         true
    property bool colShowType:         true
    property bool colShowDate:         true
    property bool colShowDateCreated:  false
    property bool colShowExtension:    false
    property bool colShowAttributes:   false
    property bool colShowResolution:   false
    property bool colShowDuration:     false
    property bool colShowArtist:       false
    property bool colShowAlbum:        false
    property bool colShowBitrate:      false

    readonly property real totalColumnsWidth: {
        let w = colWidthName
        if (colShowSize) w += colWidthSize
        if (colShowType) w += colWidthType
        if (colShowDate) w += colWidthDate
        if (colShowDateCreated) w += colWidthDateCreated
        if (colShowExtension) w += colWidthExtension
        if (colShowAttributes) w += colWidthAttributes
        if (colShowResolution) w += colWidthResolution
        if (colShowDuration) w += colWidthDuration
        if (colShowArtist) w += colWidthArtist
        if (colShowAlbum) w += colWidthAlbum
        if (colShowBitrate) w += colWidthBitrate
        return w + 24 // 12+12 side margins
    }

    function resetColumnsToDefaults() {
        preferredColWidthName = 220; colWidthSize = 90; colWidthType = 130; colWidthDate = 150
        colWidthDateCreated = 150; colWidthExtension = 70; colWidthAttributes = 70
        colWidthResolution = 100; colWidthDuration = 80; colWidthArtist = 140
        colWidthAlbum = 140; colWidthBitrate = 80
        colShowSize = true; colShowType = true; colShowDate = true
        colShowDateCreated = false; colShowExtension = false; colShowAttributes = false
        colShowResolution = false; colShowDuration = false; colShowArtist = false
        colShowAlbum = false; colShowBitrate = false
        nameColumnManuallyResized = false
        showZebraStriping = true
        showGridlines = true
        updateNameColumnWidth()
    }

    function boolValue(value, fallback) {
        return value === undefined || value === null ? fallback : !!value
    }

    function detailsVisualState() {
        return {
            colShowSize: colShowSize,
            colShowType: colShowType,
            colShowDate: colShowDate,
            colShowDateCreated: colShowDateCreated,
            colShowExtension: colShowExtension,
            colShowAttributes: colShowAttributes,
            colShowResolution: colShowResolution,
            colShowDuration: colShowDuration,
            colShowArtist: colShowArtist,
            colShowAlbum: colShowAlbum,
            colShowBitrate: colShowBitrate,
            showZebraStriping: showZebraStriping,
            showGridlines: showGridlines
        }
    }

    function restoreDetailsVisualState(state) {
        if (!state) {
            return
        }

        colShowSize = boolValue(state.colShowSize, true)
        colShowType = boolValue(state.colShowType, true)
        colShowDate = boolValue(state.colShowDate, true)
        colShowDateCreated = boolValue(state.colShowDateCreated, false)
        colShowExtension = boolValue(state.colShowExtension, false)
        colShowAttributes = boolValue(state.colShowAttributes, false)
        colShowResolution = boolValue(state.colShowResolution, false)
        colShowDuration = boolValue(state.colShowDuration, false)
        colShowArtist = boolValue(state.colShowArtist, false)
        colShowAlbum = boolValue(state.colShowAlbum, false)
        colShowBitrate = boolValue(state.colShowBitrate, false)
        showZebraStriping = boolValue(state.showZebraStriping, true)
        showGridlines = boolValue(state.showGridlines, true)
        updateNameColumnWidth()
    }

    function updateNameColumnWidth(force) {
        if (force !== true && root.resizeOptimized && !nameColumnManuallyResized) {
            root.pendingAutoNameColumnWidthUpdate = true
            return
        }

        root.pendingAutoNameColumnWidthUpdate = false
        if (nameColumnManuallyResized) {
            colWidthName = Math.max(180, preferredColWidthName)
        } else {
            let space = (contentArea ? contentArea.width : 500) - 24 - totalOtherColumnsWidth
            colWidthName = Math.max(180, space)
        }
    }

    onLiveResizeActiveChanged: {
        if (!root.resizeOptimized && pendingAutoNameColumnWidthUpdate) {
            updateNameColumnWidth(true)
        }
    }

    onUltraLightModeChanged: {
        if (!root.resizeOptimized && pendingAutoNameColumnWidthUpdate) {
            updateNameColumnWidth(true)
        }
    }

    onPreferredColWidthNameChanged: updateNameColumnWidth()
    onNameColumnManuallyResizedChanged: updateNameColumnWidth()
    onColShowSizeChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowTypeChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowDateChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowDateCreatedChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowExtensionChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowAttributesChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowResolutionChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowDurationChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowArtistChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowAlbumChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColShowBitrateChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onShowZebraStripingChanged: detailsVisualStateChanged()
    onShowGridlinesChanged: detailsVisualStateChanged()
    onViewModeChanged: updateNameColumnWidth()

    Component.onCompleted: {
        updateNameColumnWidth()
    }

    Timer {
        id: scrollStopTimer
        interval: 50
        onTriggered: {
            root.scrolling = false
            root.controller.scrolling = false
        }
    }

    Timer {
        id: previewScrollStopTimer
        interval: 220
        repeat: false
        onTriggered: root.previewScrollActive = false
    }

    // Delayed loading rail: only show "Scanning folder" after 150ms of
    // sustained loading. Fast small-directory loads complete before the
    // timer fires, so the user never sees a loading state.
    Timer {
        id: loadingRailTimer
        interval: 150
        onTriggered: {
            if (root.controller.directoryModel.loading) {
                root.showLoadingRail = true
            }
        }
    }

    Timer {
        id: scrollRestoreTimer
        interval: 0
        repeat: false
        onTriggered: root.restorePendingScrollPosition()
    }

    Connections {
        target: root.controller.directoryModel
        function onLoadingChanged() {
            if (root.controller.directoryModel.loading) {
                if (root.isCurrentPathArchive) {
                    loadingRailTimer.stop()
                    root.showLoadingRail = root.controller.directoryModel.count === 0
                } else {
                    loadingRailTimer.start()
                }
                root.scrolling = true
                root.controller.scrolling = true
                scrollStopTimer.stop()
            } else {
                loadingRailTimer.stop()
                root.showLoadingRail = false
                
                if (root.active) {
                    Qt.callLater(() => {
                        let isSidebarFocused = typeof sidebar !== "undefined" && sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
                        if (!isSidebarFocused) {
                            root.focusContent()
                        }
                        if (root.controller.directoryModel.count > 0) {
                            const view = activeView()
                            if (view) {
                                let idx = 0
                                if (root.targetSelectPath !== "") {
                                    let pathIdx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
                                    if (pathIdx >= 0) {
                                        idx = pathIdx
                                    }
                                    root.targetSelectPath = ""
                                }
                                root.setViewCurrentIndexWithoutSelection(view, idx)
                                view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
                            }
                        }
                    })
                }
                
                if (root.pendingScrollRestorePath.length > 0) {
                    scrollRestoreTimer.restart()
                }
                scrollStopTimer.restart()
            }
        }
    }

    Connections {
        target: root.controller
        function onPathAboutToChange(from, to, preserveScroll) {
            root.saveScrollPositionForPath(from)
            root.pendingScrollRestoreEnabled = preserveScroll
            if (!preserveScroll) {
                root.pendingScrollRestorePath = ""
                root.pendingScrollRestoreY = -1
                root.targetSelectPath = ""
            } else {
                const state = scrollPositions[scrollKeyForPath(to)]
                if (state && state.selectedPath) {
                    root.targetSelectPath = state.selectedPath
                } else {
                    root.targetSelectPath = root.findDirectChildPath(to, from)
                }
            }
            root.scrolling = true
            root.controller.scrolling = true
            scrollStopTimer.stop()
        }
        function onPathNavigated(path) {
            if (root.active) {
                Qt.callLater(() => {
                    let isSidebarFocused = typeof sidebar !== "undefined" && sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
                    if (!isSidebarFocused) {
                        root.focusContent()
                    }
                    
                    // If loading is already complete, initialize only the keyboard cursor.
                    if (!root.controller.directoryModel.loading && root.controller.directoryModel.count > 0) {
                        const view = activeView()
                        if (view) {
                            let idx = 0
                            if (root.targetSelectPath !== "") {
                                let pathIdx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
                                if (pathIdx >= 0) {
                                    idx = pathIdx
                                }
                                root.targetSelectPath = ""
                            }
                            root.setViewCurrentIndexWithoutSelection(view, idx)
                            view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
                        }
                    }
                })
            }
            if (root.pendingScrollRestoreEnabled) {
                root.queueScrollRestoreForPath(path)
                root.pendingScrollRestoreEnabled = false
            }
            root.scrolling = true
            root.controller.scrolling = true
            if (!root.controller.directoryModel.loading) {
                scrollStopTimer.restart()
            }
        }
        function onEntryRenamed(oldPath, newPath) {
            if (root.pendingInlineRenamePath.length > 0 && oldPath === root.pendingInlineRenamePath) {
                root.pendingInlineRenamePath = ""
                root.focusRenamedPath(newPath)
            }
        }
        function onCreatedEntryRevealRequested(path) {
            root.revealCreatedPath(path)
        }
    }

    function updateScrollingState() {
        if (root.controller.directoryModel.loading) return
        const moving   = root.viewMode === 2 ? briefView.moving   : root.viewMode === 0 ? listView.moving   : gridView.moving
        const flicking = root.viewMode === 2 ? briefView.flicking : root.viewMode === 0 ? listView.flicking : gridView.flicking
        const isScrolling = moving || flicking

        if (isScrolling) {
            root.markPreviewScrollActive()
            scrollStopTimer.stop()
            if (!root.scrolling) {
                root.scrolling = true
                root.controller.scrolling = true
                // Clear hover on scroll start
                root.controller.hoveredPath = ""
            }
        } else {
            if (root.scrolling && !scrollStopTimer.running) {
                scrollStopTimer.start()
            }
        }
    }

    function handleScrollActivity() {
        if (root.controller.directoryModel.loading) return
        root.markPreviewScrollActive()
        if (!root.scrolling) {
            root.scrolling = true
            root.controller.scrolling = true
            // Clear hover on scroll start
            root.controller.hoveredPath = ""
        }
        scrollStopTimer.restart()
    }

    function markPreviewScrollActive() {
        if (root.resizeOptimized || root.virtualRootMode) {
            return
        }
        root.previewScrollActive = true
        previewScrollStopTimer.restart()
    }

    function activeView() {
        if (root.viewMode === 2) return briefView
        if (root.viewMode === 0) return listView
        return gridView
    }

    function bundledIconForSuffix(isDirectory, suffix) {
        return fileTypeIconResolver.iconForSuffix(String(suffix || ""), isDirectory)
    }

    function panelIconSource(path, isDirectory, suffix) {
        if (!root.useNativeIcons) {
            return bundledIconForSuffix(isDirectory, suffix)
        }
        const query = isDirectory
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        return "image://icon/" + encodeURIComponent(path + query)
    }

    function setViewCurrentIndexWithoutSelection(view, index) {
        root.disableSelectionOnCurrentIndexChanged = true
        view.currentIndex = index
        root.updateCurrentItemPath(index)
        Qt.callLater(() => {
            root.disableSelectionOnCurrentIndexChanged = false
        })
    }

    function restorePreviewAfterRenameEdit() {
        if (root.Window.window && root.Window.window.previewPaneVisible) {
            root.Window.window.syncPreviewFromActivePanel(true)
        }
    }

    function cancelInlineRename() {
        root.pendingInlineRenamePath = ""
        root.restorePreviewAfterRenameEdit()
    }

    function focusRenamedPath(path) {
        if (!path || path.length === 0) {
            root.restorePreviewAfterRenameEdit()
            return
        }

        Qt.callLater(() => {
            const idx = root.controller.directoryModel.indexOfPath(path)
            if (idx < 0) {
                root.restorePreviewAfterRenameEdit()
                return
            }

            const view = root.activeView()
            if (view) {
                root.setViewCurrentIndexWithoutSelection(view, idx)
                root.controller.directoryModel.selectOnly(idx)
                if (!root.resizeOptimized) {
                    view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
                }
            }
            root.restorePreviewAfterRenameEdit()
        })
    }

    function revealCreatedPath(path) {
        if (!path || path.length === 0) {
            return
        }

        Qt.callLater(() => {
            const idx = root.controller.directoryModel.indexOfPath(path)
            if (idx < 0) {
                return
            }

            const view = root.activeView()
            if (view) {
                root.setViewCurrentIndexWithoutSelection(view, idx)
                root.controller.directoryModel.selectOnly(idx)
                if (!root.resizeOptimized) {
                    view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
                }
            }
        })
    }

    function updateCurrentItemPath(index) {
        if (!root.controller || !root.controller.directoryModel) {
            return
        }
        root.controller.currentItemPath = index >= 0 && index < root.controller.directoryModel.count
                                        ? root.controller.directoryModel.pathAt(index)
                                        : ""
    }

    function scrollKeyForPath(path) {
        return path + "|" + root.viewMode
    }

    function findDirectChildPath(parentPath, childPath) {
        if (!parentPath || !childPath) return "";
        let p = parentPath.replace(/\\/g, "/");
        let c = childPath.replace(/\\/g, "/");
        if (p !== "devices://" && p !== "favorites://" && !p.endsWith("/")) {
            p = p + "/";
        }
        if (!c.startsWith(p)) {
            return "";
        }
        let sub = c.substring(p.length);
        let parts = sub.split("/");
        if (parts.length > 0 && parts[0].length > 0) {
            let slash = parentPath.endsWith("/") || parentPath.endsWith("\\") ? "" : "/";
            return parentPath + slash + parts[0];
        }
        return "";
    }

    function saveScrollPositionForPath(path) {
        if (!path || path === "devices://" || path === "favorites://") {
            return
        }

        const view = activeView()
        if (!view) {
            return
        }

        let selected = root.controller.selectedPaths()
        let selectedPath = (selected && selected.length > 0) ? selected[0] : ""

        scrollPositions[scrollKeyForPath(path)] = {
            y: view.contentY,
            x: view.contentX,
            selectedPath: selectedPath
        }
    }

    function queueScrollRestoreForPath(path) {
        if (!path || path === "devices://" || path === "favorites://") {
            pendingScrollRestorePath = ""
            pendingScrollRestoreY = -1
            return
        }

        const state = scrollPositions[scrollKeyForPath(path)]
        if (!state) {
            pendingScrollRestorePath = ""
            pendingScrollRestoreY = -1
            return
        }

        pendingScrollRestorePath = path
        pendingScrollRestoreY = state.y

        if (!root.controller.directoryModel.loading) {
            scrollRestoreTimer.restart()
        }
    }

    function restorePendingScrollPosition() {
        if (!pendingScrollRestorePath) {
            return
        }

        if (root.controller.currentPath !== pendingScrollRestorePath) {
            return
        }

        const view = activeView()
        if (!view) {
            return
        }

        if (root.controller.directoryModel.loading || view.contentHeight <= 0) {
            if (root.controller.directoryModel.count > 0) {
                scrollRestoreTimer.restart()
            }
            return
        }

        const maxY = Math.max(0, view.contentHeight - view.height)
        view.contentY = Math.min(Math.max(0, pendingScrollRestoreY), maxY)
        pendingScrollRestorePath = ""
        pendingScrollRestoreY = -1
    }

    property string statusMessage: ""
    Timer {
        id: statusTimer
        interval: 2500
        onTriggered: root.statusMessage = ""
    }

    Connections {
        target: root.workspaceController.operationQueue
        function onStatusMessageChanged() {
            root.statusMessage = root.workspaceController.operationQueue.statusMessage
            statusTimer.restart()
        }
        function onBusyChanged() {
            if (!root.workspaceController.operationQueue.busy) {
                statusTimer.restart()
            }
        }
    }

    Connections {
        target: root.controller
        function onStatusMessageChanged() {
            if (root.controller.statusMessage.length > 0) {
                root.statusMessage = root.controller.statusMessage
                statusTimer.restart()
            }
        }
    }

    Connections {
        target: root.workspaceController
        function onRenameRequested() {
            if (root.active) root.startRename()
        }
    }

    Keys.onPressed: (event) => {
        if (root.panelKeysBlockedByOverlay()) {
            event.accepted = true
            return
        }
        if (event.matches(StandardKey.SelectAll)) {
            root.controller.directoryModel.selectAll()
            event.accepted = true
        }
    }

    padding: 0
    background: Item {
        id: backgroundWrapper
        
        layer.enabled: root.showActiveHighlight && !root.effectsReduced
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.activeGlow
            shadowBlur: 12
            shadowVerticalOffset: 0
            shadowHorizontalOffset: 0
        }

        Rectangle {
            id: panelBg
            anchors.fill: parent
            radius: Theme.radiusMd
            color: Theme.panelSurface
            border.color: root.showActiveHighlight ? Theme.activeAccent : Theme.panelBorder
            border.width: root.showActiveHighlight ? 1.5 : 1
            antialiasing: true

            // Subtle overlay for the whole panel
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: root.showActiveHighlight 
                       ? Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.03 : 0.05)
                       : Theme.withAlpha(Theme.accent, themeController.isDark ? 0.015 : 0.03)
            }

            // --- ELEGANT CENTERED ACTIVE PILL ---
            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 4
                anchors.horizontalCenter: parent.horizontalCenter
                width: 48
                height: 3
                radius: 1.5
                color: Theme.activeAccent
                opacity: root.showActiveHighlight ? 1.0 : 0.0
                antialiasing: true
                
                Behavior on opacity {
                    enabled: !root.effectsReduced
                    NumberAnimation { duration: Theme.motionFast }
                }
            }

            Behavior on border.color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
            Behavior on border.width { enabled: !root.effectsReduced; NumberAnimation { duration: Theme.motionFast } }
        }
    }

    function contextRow() {
        if (root.viewMode === 2) return briefView.currentIndex
        if (root.viewMode === 0) return listView.currentIndex
        return gridView.currentIndex
    }

    function startRename() {
        if (root.isCurrentPathReadOnlyContainer) return
        let idx = contextRow()
        if (idx < 0) return
        
        if (root.controller.directoryModel.selectedCount > 1) {
            const selectedPaths = root.controller.selectedPaths()
            root.Window.window.releasePreviewForPaths(selectedPaths)
            root.Window.window.showBatchRename(selectedPaths)
            return
        }

        root.pendingInlineRenamePath = root.controller.directoryModel.pathAt(idx)
        root.Window.window.releasePreviewForPaths([root.pendingInlineRenamePath])
        if (root.viewMode === 2) {
            if (briefView.currentItem) briefView.currentItem.startRename()
        } else if (root.viewMode === 0) {
            if (listView.currentItem) listView.currentItem.startRename()
        } else {
            if (gridView.currentItem) gridView.currentItem.startRename()
        }
    }

    function focusContent() {
        if (root.controller.isDeviceRoot) {
            storageView.forceActiveFocus()
        } else if (root.controller.isFavoritesRoot) {
            favoritesView.forceActiveFocus()
        } else if (root.viewMode === 2) {
            briefView.forceActiveFocus()
        } else if (root.viewMode === 0) {
            listView.forceActiveFocus()
        } else {
            gridView.forceActiveFocus()
        }
    }

    function panelKeysBlockedByOverlay() {
        return root.Window.window && root.Window.window.anyOverlayOpen
    }

    function handleItemClick(index, mouse) {
        root.activated()
        root.disableSelectionOnCurrentIndexChanged = true
        let prevIdx = -1
        if (root.viewMode === 2) {
            prevIdx = briefView.currentIndex
            briefView.currentIndex = index
        } else if (root.viewMode === 0) {
            prevIdx = listView.currentIndex
            listView.currentIndex = index
        } else {
            prevIdx = gridView.currentIndex
            gridView.currentIndex = index
        }
        root.updateCurrentItemPath(index)
        root.disableSelectionOnCurrentIndexChanged = false

        if (mouse.modifiers & Qt.ShiftModifier) {
            if (prevIdx >= 0) {
                root.controller.directoryModel.selectRange(prevIdx, index)
            } else {
                root.controller.directoryModel.selectOnly(index)
            }
        } else if (mouse.modifiers & Qt.ControlModifier) {
            root.controller.directoryModel.toggleSelected(index)
        } else {
            root.controller.directoryModel.selectOnly(index)
        }
    }

    function handleItemRightClick(index, path, isArchiveFile, isIsoImageFile) {
        root.activated()
        root.disableSelectionOnCurrentIndexChanged = true
        if (root.viewMode === 2)      briefView.currentIndex = index
        else if (root.viewMode === 0) listView.currentIndex = index
        else                          gridView.currentIndex = index
        root.disableSelectionOnCurrentIndexChanged = false
        root.updateCurrentItemPath(index)

        if (!root.controller.directoryModel.selectedCount || !root.controller.directoryModel.selectedPaths().includes(path)) {
            root.controller.directoryModel.selectOnly(index)
        }
        filePanelContextMenu.popupContextMenu(
            index,
            path,
            !root.isCurrentPathArchive && isArchiveFile === true && isIsoImageFile !== true,
            !root.isCurrentPathArchive && isIsoImageFile === true)
    }

    function loadingFolderName() {
        let path = root.controller.currentPath
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

    signal activated()
    FilePanelContextMenu {
        id: filePanelContextMenu
        controller: root.controller
        workspaceController: root.workspaceController
        favoritesController: root.favoritesBackend
        windowObject: root.Window.window
        contextRowProvider: root.contextRow
        isCurrentPathArchive: root.isCurrentPathArchive
        isCurrentPathReadOnlyContainer: root.isCurrentPathReadOnlyContainer
        onRenameRequested: root.startRename()
    }

    FilePanelEmptyMenu {
        id: filePanelEmptyMenu
        controller: root.controller
        workspaceController: root.workspaceController
        propertiesController: root.propertiesController
        favoritesController: root.favoritesBackend
        isCurrentPathArchive: root.isCurrentPathArchive
        isCurrentPathReadOnlyContainer: root.isCurrentPathReadOnlyContainer
    }

    FilePanelDropOverlay {
        anchors.fill: parent
        workspaceController: root.workspaceController
        currentPath: root.controller.currentPath
    }

    FilePanelShell {
        anchors.fill: parent
        panelActive: root.active

        ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 42
            color: "transparent"

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: root.showActiveHighlight ? Theme.withAlpha(Theme.activeAccent, 0.34)
                                  : Theme.panelBorder
                }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        root.activated()
                        filePanelEmptyMenu.popupEmptyMenu()
                    } else {
                        root.activated()
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 12
                spacing: 8

                Loader {
                    Layout.fillWidth: true
                    active: root.showPanelBreadcrumbs
                    visible: active
                    sourceComponent: panelPathBarComponent
                }

                FilePanelViewMenu {
                    controller: root.controller
                }
            }
        }

        Component {
            id: panelPathBarComponent

            PathBar {
                controller: root.controller
                path: root.controller.currentPath
                onActiveFocusChanged: if (activeFocus) root.activated()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.panelBorder
        }

        Item {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            onWidthChanged: root.updateNameColumnWidth()

            Component {
                id: listDelegate
                FileDelegate {
                    width: root.lightweightDelegates && root.resizeFrozenListWidth > 0
                           ? root.resizeFrozenListWidth
                           : listView.width
                    controller: root.controller
                    panel: root
                    currentItem: ListView.isCurrentItem
                    panelActive: root.active
                    scrolling: root.scrolling
                    resizeOptimized: root.lightweightDelegates
                    onClicked: (mouse) => root.handleItemClick(index, mouse)
                    onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                    onDoubleClicked: root.controller.openItem(index)
                }
            }

            Component {
                id: detailsDelegate
                FileTableAdaptiveDelegate {
                    width: root.lightweightDelegates && root.resizeFrozenListWidth > 0
                           ? root.resizeFrozenListWidth
                           : listView.width
                    controller: root.controller
                    panel: root
                    currentItem: ListView.isCurrentItem
                    panelActive: root.active
                    scrolling: root.scrolling
                    onClicked: (mouse) => root.handleItemClick(index, mouse)
                    onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                    onEmptySpaceRightClicked: filePanelEmptyMenu.popupEmptyMenu()
                    onDoubleClicked: root.controller.openItem(index)
                }
            }

            Flickable {
                id: horizontalFlick
                anchors.fill: parent
                visible: !root.virtualRootMode
                enabled: visible
                contentWidth: root.lightweightDelegates && root.viewMode === 0
                              ? (root.resizeFrozenListWidth > 0 ? root.resizeFrozenListWidth : width)
                              : (root.viewMode === 0 ? Math.max(width, root.totalColumnsWidth) : width)
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                interactive: root.viewMode === 0 && !root.lightweightDelegates

                ScrollBar.horizontal: ScrollBar {
                    id: hScrollBar
                    parent: contentArea
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.footerHeight
                    policy: ScrollBar.AlwaysOn
                    visible: root.viewMode === 0 && root.horizontalScrollActive && !root.lightweightDelegates
                    z: 10
                }

                Column {
                    width: horizontalFlick.contentWidth
                    height: horizontalFlick.height

                    FilePanelHeader {
                        id: tableHeader
                        width: parent.width
                        visible: root.viewMode === 0 && !root.lightweightDelegates
                        controller: root.controller
                        panel: root
                    }

                    ListView {
                        id: listView
                        width: parent.width
                        height: root.viewMode === 0 ? parent.height - (tableHeader.visible ? tableHeader.height : 0) : parent.height
                        visible: root.viewMode === 0
                        enabled: visible
                        clip: true
                        boundsBehavior: Flickable.DragAndOvershootBounds
                        pixelAligned: false
                        flickableDirection: Flickable.VerticalFlick
                        interactive: !root.resizeOptimized
                        model: root.controller.directoryModel
                        currentIndex: -1
                        focus: root.active && root.viewMode === 0
                        
                        onActiveFocusChanged: {
                            if (activeFocus && model.count > 0) {
                                if (currentIndex === -1) {
                                    root.setViewCurrentIndexWithoutSelection(listView, 0)
                                }
                            }
                        }
                        onCurrentIndexChanged: {
                            root.updateCurrentItemPath(currentIndex)
                            if (activeFocus && currentIndex >= 0 && currentIndex < model.count) {
                                if (!root.disableSelectionOnCurrentIndexChanged) {
                                    root.controller.directoryModel.selectOnly(currentIndex)
                                }
                                if (!root.resizeOptimized) {
                                    positionViewAtIndex(currentIndex, ListView.Contain)
                                }
                            }
                        }
                        onCountChanged: {
                            if (count > 0 && currentIndex === -1) {
                                let idx = 0
                                if (root.targetSelectPath !== "") {
                                    let pathIdx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
                                    if (pathIdx >= 0) {
                                        idx = pathIdx
                                    }
                                    root.targetSelectPath = ""
                                }
                                root.setViewCurrentIndexWithoutSelection(listView, idx)
                                if (!root.resizeOptimized) {
                                    positionViewAtIndex(idx, ListView.Contain)
                                }
                            }
                        }
                        cacheBuffer: root.effectsReduced ? 0 : Math.max(0, height * 2)
                        reuseItems: true
                        onMovingChanged: root.updateScrollingState()
                        onFlickingChanged: root.updateScrollingState()
                        onContentYChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                        onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                        bottomMargin: root.footerHeight + (root.horizontalScrollActive ? 12 : 0)
                        
                        highlight: null
                        highlightFollowsCurrentItem: false

                        add: !root.effectsReduced && root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? listAddTransition : null
                        remove: !root.effectsReduced && root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? listRemoveTransition : null

                        Transition {
                            id: listAddTransition
                            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 140; easing.type: Easing.OutQuad }
                            NumberAnimation { property: "visualOffsetX"; from: -18; to: 0; duration: 160; easing.type: Easing.OutCubic }
                        }
                        Transition {
                            id: listRemoveTransition
                            NumberAnimation { property: "opacity"; to: 0.0; duration: 110; easing.type: Easing.InQuad }
                            NumberAnimation { property: "visualOffsetX"; to: -10; duration: 110; easing.type: Easing.InQuad }
                        }

                        Keys.onPressed: (event) => {
                            if (root.panelKeysBlockedByOverlay()) {
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
                                if (currentIndex >= 0 && currentIndex < model.count) {
                                    root.controller.directoryModel.toggleSelected(currentIndex)
                                    event.accepted = true
                                }
                                return
                            }
                            if (event.modifiers & Qt.ControlModifier) {
                                if (event.key === Qt.Key_Up || event.key === Qt.Key_Down ||
                                    event.key === Qt.Key_Left || event.key === Qt.Key_Right ||
                                    event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown ||
                                    event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                                    root.disableSelectionOnCurrentIndexChanged = true
                                    Qt.callLater(() => {
                                        root.disableSelectionOnCurrentIndexChanged = false
                                    })
                                }
                            }
                            if (currentIndex === -1 && model.count > 0 &&
                                (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                                currentIndex = (event.key === Qt.Key_Up) ? model.count - 1 : 0
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                                if (currentIndex >= 0 && listView.currentItem && !listView.currentItem.isRenaming)
                                    root.controller.openItem(currentIndex)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Backspace) {
                                root.controller.goUp()
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                root.controller.directoryModel.clearSelection()
                                root.workspaceController.focusActivePanel()
                                event.accepted = true
                            }
                        }

                        delegate: root.viewMode === 0 ? detailsDelegate : listDelegate

                        ScrollBar.vertical: ScrollBar {
                            parent: contentArea
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: root.viewMode === 0 && tableHeader.visible ? tableHeader.height : 0
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: root.footerHeight + (root.horizontalScrollActive ? 12 : 0)
                            visible: listView.visible
                            active: listView.moving || listView.flicking || scrollHover.hovered
                            policy: ScrollBar.AsNeeded
                            z: 10
                            HoverHandler { id: scrollHover }
                        }
                    }
                }
            }

            // ── Empty Folder Message ─────────────────────────────────────
            EmptyState {
                anchors.centerIn: parent
                visible: !root.virtualRootMode
                         && !root.controller.directoryModel.loading
                         && root.controller.directoryModel.count === 0
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder.svg"
                iconSize: 64
                iconOpacity: 0.4
                title: "This folder is empty"
                contentOpacity: 0.5
            }

            // ── Brief View (viewMode = 2) — two-column compact list ─────────
            GridView {
                id: briefView
                anchors.fill: parent
                visible: root.viewMode === 2 && !root.virtualRootMode
                enabled: visible
                clip: true
                flow: GridView.FlowLeftToRight
                cellWidth: root.lightweightDelegates && root.resizeFrozenBriefCellWidth > 0
                           ? root.resizeFrozenBriefCellWidth
                           : Math.floor(width / 2)
                cellHeight: root.briefRowHeight
                model: root.controller.directoryModel
                currentIndex: -1
                focus: root.active && root.viewMode === 2

                onActiveFocusChanged: {
                    if (activeFocus && model.count > 0) {
                        if (currentIndex === -1) {
                            root.setViewCurrentIndexWithoutSelection(briefView, 0)
                        }
                    }
                }
                onCurrentIndexChanged: {
                    root.updateCurrentItemPath(currentIndex)
                    if (activeFocus && currentIndex >= 0 && currentIndex < model.count) {
                        if (!root.disableSelectionOnCurrentIndexChanged) {
                            root.controller.directoryModel.selectOnly(currentIndex)
                        }
                        if (!root.resizeOptimized) {
                            positionViewAtIndex(currentIndex, GridView.Contain)
                        }
                    }
                }
                onCountChanged: {
                    if (count > 0 && currentIndex === -1) {
                        let idx = 0
                        if (root.targetSelectPath !== "") {
                            let pathIdx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
                            if (pathIdx >= 0) {
                                idx = pathIdx
                            }
                            root.targetSelectPath = ""
                        }
                        root.setViewCurrentIndexWithoutSelection(briefView, idx)
                        if (!root.resizeOptimized) {
                            positionViewAtIndex(idx, GridView.Contain)
                        }
                    }
                }
                cacheBuffer: root.effectsReduced ? 0 : Math.max(0, height * 2)
                reuseItems: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                interactive: !root.resizeOptimized
                onMovingChanged:  root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                bottomMargin: root.footerHeight

                add:    !root.effectsReduced && root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? briefAddTransition : null
                remove: !root.effectsReduced && root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? briefRemoveTransition : null

                Transition {
                    id: briefAddTransition
                    NumberAnimation { property: "opacity";       from: 0.0; to: 1.0; duration: 120; easing.type: Easing.OutQuad }
                    NumberAnimation { property: "visualOffsetX"; from: -12; to: 0;   duration: 140; easing.type: Easing.OutCubic }
                }
                Transition {
                    id: briefRemoveTransition
                    NumberAnimation { property: "opacity";       to: 0.0; duration: 100; easing.type: Easing.InQuad }
                    NumberAnimation { property: "visualOffsetX"; to: -8;  duration: 100; easing.type: Easing.InQuad }
                }

                Keys.onPressed: (event) => {
                    if (root.panelKeysBlockedByOverlay()) {
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
                        if (currentIndex >= 0 && currentIndex < model.count) {
                            root.controller.directoryModel.toggleSelected(currentIndex)
                            event.accepted = true
                        }
                        return
                    }
                    if (event.modifiers & Qt.ControlModifier) {
                        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down ||
                            event.key === Qt.Key_Left || event.key === Qt.Key_Right ||
                            event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown ||
                            event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                            root.disableSelectionOnCurrentIndexChanged = true
                            Qt.callLater(() => {
                                root.disableSelectionOnCurrentIndexChanged = false
                            })
                        }
                    }
                    if (currentIndex === -1 && model.count > 0 &&
                        (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                        currentIndex = (event.key === Qt.Key_Up) ? model.count - 1 : 0
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && briefView.currentItem && !briefView.currentItem.isRenaming)
                            root.controller.openItem(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.directoryModel.clearSelection()
                        root.workspaceController.focusActivePanel()
                        event.accepted = true
                    }
                }

                delegate: Component {
                    FileBriefAdaptiveDelegate {
                        width: briefView.cellWidth
                        height: briefView.cellHeight
                        controller: root.controller
                        panel: root
                        currentItem: GridView.isCurrentItem
                        panelActive: root.active
                        scrolling: root.scrolling
                        resizeOptimized: root.lightweightDelegates

                        onClicked: (mouse) => root.handleItemClick(index, mouse)
                        onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                        onDoubleClicked: root.controller.openItem(index)
                    }
                }

                // Empty area handling
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        root.activated()
                        if (mouse.button === Qt.RightButton) {
                            filePanelEmptyMenu.popupEmptyMenu()
                        } else {
                            root.controller.directoryModel.clearSelection()
                            briefView.currentIndex = -1
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    parent: contentArea
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.footerHeight
                    visible: briefView.visible
                    active: briefView.moving || briefView.flicking || briefScrollHover.hovered
                    policy: ScrollBar.AsNeeded
                    z: 10
                    HoverHandler { id: briefScrollHover }
                }
            }

            GridView {
                id: gridView
                anchors.fill: parent
                anchors.margins: 10
                anchors.bottomMargin: root.viewMode === 1 ? root.footerHeight + 12 : 10
                visible: root.viewMode === 1 && !root.virtualRootMode
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
                interactive: !root.resizeOptimized
                cellWidth: root.gridCellWidth
                cellHeight: root.gridCellHeight
                model: root.controller.directoryModel
                currentIndex: -1
                focus: root.active && root.viewMode === 1

                onActiveFocusChanged: {
                    if (activeFocus && model.count > 0) {
                        if (currentIndex === -1) {
                            root.setViewCurrentIndexWithoutSelection(gridView, 0)
                        }
                    }
                }
                onCurrentIndexChanged: {
                    root.updateCurrentItemPath(currentIndex)
                    if (activeFocus && currentIndex >= 0 && currentIndex < model.count) {
                        if (!root.disableSelectionOnCurrentIndexChanged) {
                            root.controller.directoryModel.selectOnly(currentIndex)
                        }
                        if (!root.resizeOptimized) {
                            positionViewAtIndex(currentIndex, GridView.Contain)
                        }
                    }
                }
                onCountChanged: {
                    if (count > 0 && currentIndex === -1) {
                        let idx = 0
                        if (root.targetSelectPath !== "") {
                            let pathIdx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
                            if (pathIdx >= 0) {
                                idx = pathIdx
                            }
                            root.targetSelectPath = ""
                        }
                        root.setViewCurrentIndexWithoutSelection(gridView, idx)
                        if (!root.resizeOptimized) {
                            positionViewAtIndex(idx, GridView.Contain)
                        }
                    }
                }
                cacheBuffer: root.effectsReduced ? 0 : Math.max(0, height * 1.5)
                reuseItems: true
                onMovingChanged: root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                
                highlight: null
                highlightFollowsCurrentItem: false

                add: !root.effectsReduced && root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? gridAddTransition : null
                remove: !root.effectsReduced && root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? gridRemoveTransition : null

                Transition {
                    id: gridAddTransition
                    NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutQuad }
                    NumberAnimation { property: "visualOffsetY"; from: 12; to: 0; duration: 170; easing.type: Easing.OutCubic }
                }
                Transition {
                    id: gridRemoveTransition
                    NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InQuad }
                    NumberAnimation { property: "visualOffsetY"; to: 8; duration: 120; easing.type: Easing.InQuad }
                }

                Keys.onPressed: (event) => {
                    if (root.panelKeysBlockedByOverlay()) {
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
                        if (currentIndex >= 0 && currentIndex < model.count) {
                            root.controller.directoryModel.toggleSelected(currentIndex)
                            event.accepted = true
                        }
                        return
                    }
                    if (event.modifiers & Qt.ControlModifier) {
                        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down ||
                            event.key === Qt.Key_Left || event.key === Qt.Key_Right ||
                            event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown ||
                            event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                            root.disableSelectionOnCurrentIndexChanged = true
                            Qt.callLater(() => {
                                root.disableSelectionOnCurrentIndexChanged = false
                            })
                        }
                    }
                    if (currentIndex === -1 && model.count > 0 &&
                        (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                        currentIndex = (event.key === Qt.Key_Up) ? model.count - 1 : 0
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && gridView.currentItem && !gridView.currentItem.isRenaming)
                            root.controller.openItem(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backspace) {
                        root.controller.goUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.directoryModel.clearSelection()
                        root.workspaceController.focusActivePanel()
                        event.accepted = true
                    }
                }

                delegate: Item {
                    id: gridDelegate
                    width: gridView.cellWidth
                    height: gridView.cellHeight

                    required property int index
                    required property string name
                    required property string path
                    required property string suffix
                    required property bool isDirectory
                    required property bool isSelected
                    required property bool isHidden
                    required property bool isImage
                    required property bool hasThumbnail
                    required property bool isArchiveFile
                    required property bool isIsoImageFile

                    property bool isRenaming: false
                    property bool currentItem: GridView.isCurrentItem
                    property bool panelActive: root.active
                    readonly property bool lightweightActive: root.lightweightDelegates && !isRenaming
                    readonly property int contentMargin: 10
                    readonly property int contentSpacing: 6
                    readonly property int renameEditorTop: contentMargin + root.gridIconSize + contentSpacing
                    readonly property int renameEditorSideMargin: contentMargin
                    readonly property int renameEditorAvailableHeight: Math.max(30, height - renameEditorTop - contentMargin)
                    readonly property bool canLoadThumbnail: root.useNativeIcons
                                                              && root.effectiveShowThumbnails
                                                              && !gridDelegate.lightweightActive
                                                              && !isDirectory
                                                              && hasThumbnail
                    property bool thumbnailLoadEnabled: false
                    readonly property bool thumbnailRequestActive: thumbnailLoadEnabled && canLoadThumbnail
                    property real visualOffsetY: 0

                    opacity: isHidden ? 0.55 : 1.0

                    onPathChanged: {
                        isRenaming = false
                        visualOffsetY = 0
                        queueThumbnailLoad()
                        if (gridDelegate.lightweightActive) {
                            return
                        }
                        Qt.callLater(() => {
                            if (hoverGrid) {
                                hoverGrid.enabled = false
                                hoverGrid.enabled = true
                            }
                        })
                    }

                    Component.onCompleted: {
                        queueThumbnailLoad()
                        if (gridDelegate.lightweightActive) {
                            return
                        }
                        Qt.callLater(() => {
                            if (hoverGrid) {
                                hoverGrid.enabled = false
                                hoverGrid.enabled = true
                            }
                        })
                    }

                    GridView.onPooled: {
                        isRenaming = false
                        visualOffsetY = 0
                        thumbnailLoadEnabled = false
                        if (root.controller.hoveredPath === path) {
                            root.controller.hoveredPath = ""
                        }
                    }

                    GridView.onReused: {
                        isRenaming = false
                        visualOffsetY = 0
                        queueThumbnailLoad()
                        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
                        if (gridDelegate.lightweightActive) {
                            return
                        }
                        Qt.callLater(() => {
                            if (hoverGrid) {
                                hoverGrid.enabled = false
                                hoverGrid.enabled = true
                            }
                        })
                    }

                    function startRename() {
                        isRenaming = true
                    }

                    function queueThumbnailLoad() {
                        thumbnailLoadEnabled = false
                        if (canLoadThumbnail) {
                            thumbnailDelayTimer.restart()
                        } else {
                            thumbnailDelayTimer.stop()
                        }
                    }

                    Connections {
                        target: root
                        function onResizeOptimizedChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                        function onUltraLightModeChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                    }

                    Timer {
                        id: thumbnailDelayTimer
                        interval: 100 + (Math.max(0, index) % 16) * 28
                        repeat: false
                        onTriggered: gridDelegate.thumbnailLoadEnabled = gridDelegate.canLoadThumbnail
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        visible: !gridDelegate.lightweightActive
                        radius: Theme.radiusSm
                        color: isSelected
                               ? (root.active ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
                               : (currentItem
                                  ? Theme.itemCurrentFill
                                  : ((hoverGrid.hovered && !root.scrolling) ? Theme.itemHoverFill : "transparent"))
                        border.color: isSelected
                                      ? (root.active ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                                      : (currentItem ? Theme.itemCurrentBorder : "transparent")
                        border.width: isSelected || currentItem ? 1 : 0
                        transform: Translate { y: gridDelegate.visualOffsetY }
                    }

                    HoverHandler { 
                        id: hoverGrid 
                        enabled: !gridDelegate.lightweightActive
                        onHoveredChanged: {
                            if (root.scrolling) return
                            if (hovered) {
                                root.controller.hoveredPath = path
                            } else if (root.controller.hoveredPath === path) {
                                root.controller.hoveredPath = ""
                            }
                        }
                    }

                    Connections {
                        target: root
                        function onScrollingChanged() {
                            if (!root.scrolling && !gridDelegate.lightweightActive) {
                                Qt.callLater(() => {
                                    if (hoverGrid) {
                                        hoverGrid.enabled = false
                                        hoverGrid.enabled = true
                                        if (hoverGrid.hovered) {
                                            root.controller.hoveredPath = path
                                        }
                                    }
                                })
                            }
                        }
                    }

                    Connections {
                        target: typeof appSettings !== "undefined" ? appSettings : null
                        ignoreUnknownSignals: true
                        function onUseNativeIconsChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                        function onShowThumbnailsChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                    }

                    Connections {
                        target: root.controller ? root.controller.directoryModel : null
                        ignoreUnknownSignals: true
                        function onLoadingChanged() {
                            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading && !gridDelegate.lightweightActive) {
                                Qt.callLater(() => {
                                    if (hoverGrid) {
                                        hoverGrid.enabled = false
                                        hoverGrid.enabled = true
                                        if (hoverGrid.hovered) {
                                            root.controller.hoveredPath = path
                                        }
                                    }
                                })
                            }
                        }
                    }

                    Loader {
                        id: gridRenameLoader
                        z: 20
                        anchors.top: parent.top
                        anchors.topMargin: gridDelegate.renameEditorTop
                        width: Math.max(0, parent.width - gridDelegate.renameEditorSideMargin * 2)
                        height: Math.min(36, gridDelegate.renameEditorAvailableHeight)
                        x: gridDelegate.renameEditorSideMargin
                        active: isRenaming
                        visible: active
                        sourceComponent: TextField {
                            id: gridRenameInput
                            text: name
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                            color: Theme.textPrimary
                            selectByMouse: true
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 4
                            bottomPadding: 4
                            selectionColor: Theme.withAlpha(Theme.focusRing, themeController.isDark ? 0.38 : 0.24)
                            selectedTextColor: Theme.textPrimary
                            clip: true
                            property bool committing: false

                            opacity: 0
                            scale: 0.97
                            Behavior on opacity { enabled: !root.effectsReduced; NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }
                            Behavior on scale { enabled: !root.effectsReduced; NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }

                            background: Rectangle {
                                color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.92 : 0.96)
                                radius: 10
                                border.color: gridRenameInput.activeFocus
                                              ? Theme.withAlpha(Theme.focusRing, 0.9)
                                              : Theme.withAlpha(Theme.panelBorder, 0.7)
                                border.width: gridRenameInput.activeFocus ? 1.25 : 1

                                layer.enabled: !root.effectsReduced
                                layer.effect: MultiEffect {
                                    shadowEnabled: true
                                    shadowColor: Theme.withAlpha(Theme.shadow, themeController.isDark ? 0.22 : 0.12)
                                    shadowBlur: 8
                                    shadowVerticalOffset: 1
                                }
                            }

                            function commitRename() {
                                if (index >= 0) {
                                    const idx = index
                                    const txt = text.trim()
                                    const ctrl = root.controller
                                    committing = true
                                    Qt.callLater(function() {
                                        if (ctrl.rename(idx, txt)) {
                                            isRenaming = false
                                        } else {
                                            committing = false
                                            if (gridRenameLoader.item) {
                                                gridRenameLoader.item.forceActiveFocus()
                                                gridRenameLoader.item.selectAll()
                                            }
                                        }
                                    })
                                }
                            }

                            function defaultSelectionEnd() {
                                const lastDot = name.lastIndexOf(".")
                                return !isDirectory && lastDot > 0 ? lastDot : name.length
                            }

                            Keys.priority: Keys.AfterItem
                            Keys.onPressed: (event) => {
                                if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
                                    gridRenameInput.selectAll()
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_F2) {
                                    if (gridRenameInput.selectionStart === 0
                                            && gridRenameInput.selectionEnd === gridRenameInput.defaultSelectionEnd()) {
                                        gridRenameInput.selectAll()
                                    } else {
                                        gridRenameInput.select(0, gridRenameInput.defaultSelectionEnd())
                                    }
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                        || event.key === Qt.Key_Home || event.key === Qt.Key_End
                                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                                    event.accepted = true
                                }
                            }

                            Keys.onReturnPressed: (event) => {
                                gridRenameInput.commitRename()
                                event.accepted = true
                            }
                            Keys.onEnterPressed: (event) => {
                                gridRenameInput.commitRename()
                                event.accepted = true
                            }
                            Keys.onEscapePressed: (event) => {
                                isRenaming = false
                                root.cancelInlineRename()
                                event.accepted = true
                            }
                            onActiveFocusChanged: if (!activeFocus && !committing) {
                                isRenaming = false
                                root.cancelInlineRename()
                            }
                            
                            Component.onCompleted: {
                                opacity = 1.0
                                scale = 1.0
                                forceActiveFocus()
                                select(0, gridRenameInput.defaultSelectionEnd())
                            }
                        }
                    }

                    FileGridResizeDelegate {
                        anchors.fill: parent
                        visible: gridDelegate.lightweightActive
                        z: 5
                        index: gridDelegate.index
                        name: gridDelegate.name
                        path: gridDelegate.path
                        suffix: gridDelegate.suffix
                        isDirectory: gridDelegate.isDirectory
                        isSelected: gridDelegate.isSelected
                        isHidden: gridDelegate.isHidden
                        isArchiveFile: gridDelegate.isArchiveFile
                        isIsoImageFile: gridDelegate.isIsoImageFile
                        currentItem: gridDelegate.currentItem
                        panelActive: gridDelegate.panelActive
                        gridIconSize: root.gridIconSize
                        onClicked: (mouse) => root.handleItemClick(index, mouse)
                        onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                        onDoubleClicked: root.controller.openItem(index)
                    }

                    ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: gridDelegate.contentMargin
                    spacing: gridDelegate.contentSpacing
                    visible: !gridDelegate.lightweightActive
                    transform: Translate { y: gridDelegate.visualOffsetY }

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.gridIconSize
                        Layout.preferredHeight: root.gridIconSize

                        Image {
                            id: gridFallbackIcon
                            anchors.centerIn: parent
                            width: Math.max(28, Math.round(root.gridIconSize * 0.8))
                            height: width
                            source: root.bundledIconForSuffix(isDirectory, suffix)
                            sourceSize: Qt.size(width, height)
                            visible: !gridDelegate.thumbnailRequestActive || thumbnail.status !== Image.Ready
                            opacity: isImage ? 0.72 : 1.0
                            smooth: true
                            mipmap: false
                            asynchronous: false
                        }

                        Image {
                            anchors.centerIn: parent
                            width: gridFallbackIcon.width
                            height: gridFallbackIcon.height
                            source: root.useNativeIcons ? root.panelIconSource(path, isDirectory, suffix) : ""
                            sourceSize: Qt.size(width, height)
                            visible: root.useNativeIcons
                                     && (!gridDelegate.thumbnailRequestActive || thumbnail.status !== Image.Ready)
                                     && status === Image.Ready
                            opacity: isImage ? 0.72 : 1.0
                            smooth: true
                            mipmap: false
                            asynchronous: true
                        }

                        Image {
                            anchors.centerIn: parent
                            width: Math.round(root.gridIconSize * 0.9)
                            height: width
                            source: "../assets/filetypes/image.svg"
                            sourceSize: Qt.size(width, height)
                            visible: gridDelegate.thumbnailRequestActive && isImage && (thumbnail.status !== Image.Ready)
                            opacity: 0.74
                            smooth: true
                            mipmap: false
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            visible: gridDelegate.thumbnailRequestActive && thumbnail.status !== Image.Ready
                            color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.18 : 0.12)
                        }

                        Image {
                            id: thumbnail
                            anchors.fill: parent
                            source: gridDelegate.thumbnailRequestActive ? "image://thumbnail/" + encodeURIComponent(path) : ""
                            sourceSize: Qt.size(Math.min(192, root.gridIconSize * 2), Math.min(192, root.gridIconSize * 2))
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            smooth: true
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !isRenaming
                        text: name
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    font.pixelSize: 12
                        color: Theme.textPrimary
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                    }
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        visible: !gridDelegate.lightweightActive
                        
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                            else root.handleItemClick(index, mouse)
                        }
                        onDoubleClicked: root.controller.openItem(index)
                    }
                }

                // Empty area for GridView
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        root.activated()
                        if (mouse.button === Qt.RightButton) {
                            filePanelEmptyMenu.popupEmptyMenu()
                        } else {
                            root.controller.directoryModel.clearSelection()
                            gridView.currentIndex = -1
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    parent: contentArea
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.footerHeight
                    visible: gridView.visible
                    active: gridView.moving || gridView.flicking || gridScrollHover.hovered
                    policy: ScrollBar.AsNeeded
                    z: 10
                    HoverHandler { id: gridScrollHover }
                }
            }

            // ── Storage View (This PC / devices://) ──────────────────────────
            StorageView {
                id: storageView
                anchors.fill: parent
                anchors.bottomMargin: root.footerHeight
                controller: root.controller
                panel: root
                liveResizeActive: root.liveResizeActive
                visible: root.controller.isDeviceRoot
                enabled: visible
                focus: root.active && root.controller.isDeviceRoot
            }

            FavoritesView {
                id: favoritesView
                anchors.fill: parent
                anchors.bottomMargin: root.footerHeight
                controller: root.controller
                panel: root
                favoritesBackend: root.favoritesBackend
                liveResizeActive: root.liveResizeActive
                visible: root.controller.isFavoritesRoot
                enabled: visible
                focus: root.active && root.controller.isFavoritesRoot
                onActivated: root.activated()
            }

            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.RightButton
                enabled: !root.virtualRootMode
                onClicked: (mouse) => {
                    root.activated()
                    filePanelEmptyMenu.popupEmptyMenu()
                }
            }

            // ── Error Banner and Integrated Footer ──────────────────────────
            FilePanelErrorBanner {
                id: errorBanner
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.bottomMargin: root.footerHeight + 10
                z: 18
                errorInfo: root.panelErrorInfo
                displayErrorPath: root.workspaceController
                                  ? root.workspaceController.displayPath(root.panelErrorInfo && root.panelErrorInfo.path ? String(root.panelErrorInfo.path) : "")
                                  : (root.panelErrorInfo && root.panelErrorInfo.path ? String(root.panelErrorInfo.path) : "")
                onRetryRequested: root.controller.directoryModel.refresh()
                onRefreshRequested: root.controller.directoryModel.refresh()
                onAdminRequested: {
                    const window = root.Window.window
                    if (window && window.relaunchAsAdmin) {
                        window.relaunchAsAdmin()
                    }
                }
                onCopyPathRequested: {
                    const path = errorInfo && errorInfo.path ? String(errorInfo.path) : ""
                    if (path.length > 0 && root.workspaceController) {
                        root.workspaceController.copyTextToClipboard(root.workspaceController.displayPath(path))
                        root.statusMessage = "Path copied"
                        statusTimer.restart()
                    }
                }
                onDismissRequested: root.controller.clearError()
            }

            FilePanelFooter {
                id: footerBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: root.footerHeight
                active: root.active
                controller: root.controller
                placesModel: root.workspaceController ? root.workspaceController.placesModel : null
                deviceRootMode: root.controller.isDeviceRoot
                viewMode: root.virtualRootMode ? 0 : root.viewMode
                currentPath: root.controller.currentPath
                showLoadingRail: root.showLoadingRail
                statusMessage: errorBanner.visible ? "" : root.statusMessage
                isCurrentPathArchive: root.isCurrentPathArchive
                gridIconSize: root.gridIconSize
                gridIconMinSize: root.gridIconMinSize
                gridIconMaxSize: root.gridIconMaxSize
                briefRowHeight: root.briefRowHeight
                briefRowMinHeight: root.briefRowMinHeight
                briefRowMaxHeight: root.briefRowMaxHeight
                loadingFolderNameProvider: root.loadingFolderName
                deviceRootPrimaryStatus: storageView.footerPrimaryText
                favoritesRootMode: root.controller.isFavoritesRoot
                favoritesPinnedCount: root.favoritesBackend ? root.favoritesBackend.pinnedCount : 0
                favoritesFrequentCount: root.favoritesBackend ? root.favoritesBackend.frequentCount : 0
                favoritesTagCount: root.favoritesBackend ? root.favoritesBackend.tagCount : 0
                deviceRootSecondaryStatus: storageView.footerSecondaryText
                deviceRootStorageText: storageView.footerStorageText
                deviceRootStorageTooltip: storageView.footerStorageTooltipText
                deviceRootUsagePercent: storageView.footerUsageValue
                deviceRootStorageCritical: storageView.footerStorageCritical
                onGridIconSizeRequested: (value) => root.gridIconSize = value
                onBriefRowHeightRequested: (value) => root.briefRowHeight = value
            }
        }

        function onCountChanged() {
            if (root.controller.directoryModel.loading
                    && root.isCurrentPathArchive
                    && root.controller.directoryModel.count > 0) {
                loadingRailTimer.stop()
                root.showLoadingRail = false
                if (root.scrolling) {
                    scrollStopTimer.restart()
                }
            }
        }
    }
}

}


