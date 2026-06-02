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
    readonly property int gridCellBaseWidth: Math.max(88, gridIconSize + 40)
    readonly property int gridCellWidth: {
        const availableWidth = gridView && gridView.width > 0
            ? gridView.width
            : Math.max(1, root.width - 20)
        const columns = Math.max(1, Math.floor(availableWidth / root.gridCellBaseWidth))
        return Math.max(root.gridCellBaseWidth, Math.floor(availableWidth / columns))
    }
    readonly property int gridCellHeight: Math.max(112, gridIconSize + 72)
    property int briefColumnWidth: 240
    property int briefRowHeight: 28
    readonly property int briefRowMinHeight: 22
    readonly property int briefRowMaxHeight: 64
    readonly property int footerHeight: 32
    property bool showActionBar: true
    property bool isRenaming: false
    readonly property int selectionActionsHeight: 44
    property bool selectionActionsVisible: false
    readonly property int selectionActionsReservedHeight: root.selectionActionsVisible ? root.selectionActionsHeight : 0
    readonly property int bottomChromeHeight: root.footerHeight + root.selectionActionsReservedHeight
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useHighQualitySystemIcons : true
    readonly property bool showThumbnails: typeof appSettings !== "undefined" && appSettings ? appSettings.showThumbnails : true
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings ? appSettings.ultraLightMode : false
    readonly property bool effectiveShowThumbnails: root.showThumbnails && !root.ultraLightMode
    readonly property bool loadingDirectory: Boolean(root.controller
                                                     && ((root.controller.navigationPending === true)
                                                         || (root.controller.directoryModel
                                                             && root.controller.directoryModel.loading)))
    readonly property real horizontalScrollX: horizontalFlick ? horizontalFlick.contentX : 0
    readonly property bool horizontalScrollActive: root.viewMode === 0 && horizontalFlick && horizontalFlick.contentWidth > horizontalFlick.width
    property bool loadingRailReady: false
    readonly property bool showLoadingRail: root.loadingDirectory && root.loadingRailReady
    readonly property bool isCurrentPathArchive: root.controller.currentPath ? root.controller.currentPath.toLowerCase().startsWith("archive://") : false
    readonly property bool isCurrentPathManagedIsoMount: root.workspaceController && root.controller.currentPath
        ? root.workspaceController.isInsideManagedIsoMount(root.controller.currentPath)
        : false
    readonly property bool isCurrentPathReadOnlyContainer: root.isCurrentPathArchive || root.isCurrentPathManagedIsoMount
    readonly property bool canInvertSelection: Boolean(root.controller
                                                       && root.controller.directoryModel
                                                       && root.controller.currentPath.length > 0
                                                       && !root.virtualRootMode)
    readonly property var panelErrorInfo: root.controller.lastError
        && root.controller.lastError.code
        && root.controller.lastError.code !== "none"
            ? root.controller.lastError
            : (root.controller.directoryModel.lastError || ({}))

    onIsCurrentPathArchiveChanged: {
        if (root.loadingDirectory) {
            if (isCurrentPathArchive) {
                loadingRailTimer.stop()
                root.loadingRailReady = root.controller.directoryModel.count === 0
            } else {
                root.loadingRailReady = false
                loadingRailTimer.start()
            }
        }
    }
    onCanInvertSelectionChanged: if (!root.canInvertSelection) root.invertSelectionActive = false
    onShowActionBarChanged: updateSelectionActionsVisible()
    onActiveChanged: {
        updateSelectionActionsVisible()
        if (!root.active) {
            root.disableFileViewsReuse("inactive")
            root.cancelRubberBand(false)
        } else {
            root.queueCurrentIndexEnsure()
        }
    }
    onVirtualRootModeChanged: {
        if (root.virtualRootMode) {
            root.invertSelectionActive = false
            root.disableFileViewsReuse("virtual-root")
            root.fileViewsModelEnabled = false
        }
        updateSelectionActionsVisible()
    }
    onIsRenamingChanged: {
        updateSelectionActionsVisible()
        root.disableFileViewsReuse(root.isRenaming ? "rename-start" : "rename-end")
        if (root.isRenaming) {
            root.cancelRubberBand(false)
        }
    }
    onInvertSelectionActiveChanged: updateSelectionActionsVisible()

    function updateDirectoryLoadingState() {
        root.disableFileViewsReuse()
        if (root.loadingDirectory) {
            if (root.isCurrentPathArchive) {
                loadingRailTimer.stop()
                root.loadingRailReady = root.controller.directoryModel.count === 0
            } else {
                if (!root.loadingRailReady && !loadingRailTimer.running) {
                    loadingRailTimer.start()
                }
            }
            root.scrolling = true
            root.controller.scrolling = true
            root.suppressHoverBriefly()
            scrollStopTimer.restart()
        } else {
            loadingRailTimer.stop()
            root.loadingRailReady = false

            const restoringScroll = root.pendingScrollRestorePath.length > 0

            if (root.active && !restoringScroll) {
                root.focusContentAndQueueCurrentIndexEnsure()
            }

            if (restoringScroll) {
                root.queuePendingScrollRestore()
            }
            scrollStopTimer.restart()
        }
    }

    property bool scrolling: false
    property bool hoverSuppressed: false
    property bool previewScrollActive: false
    property bool rubberBandPressed: false
    property bool rubberBandActive: false
    property bool rubberBandMoved: false
    property bool invertSelectionActive: false
    property real rubberBandStartX: 0
    property real rubberBandStartY: 0
    property real rubberBandCurrentX: 0
    property real rubberBandCurrentY: 0
    property var rubberBandView: null
    property var rubberBandPressView: null
    property int rubberBandPressIndex: -1
    readonly property real rubberBandLeft: Math.min(rubberBandStartX, rubberBandCurrentX)
    readonly property real rubberBandTop: Math.min(rubberBandStartY, rubberBandCurrentY)
    readonly property real rubberBandRight: Math.max(rubberBandStartX, rubberBandCurrentX)
    readonly property real rubberBandBottom: Math.max(rubberBandStartY, rubberBandCurrentY)
    readonly property real rubberBandWidth: Math.max(0, rubberBandRight - rubberBandLeft)
    readonly property real rubberBandHeight: Math.max(0, rubberBandBottom - rubberBandTop)
    readonly property real rubberBandViewportLeft: rubberBandView && contentArea ? rubberBandView.mapToItem(contentArea, 0, 0).x + rubberBandLeft - rubberBandView.contentX : rubberBandLeft
    readonly property real rubberBandViewportTop: rubberBandView && contentArea ? rubberBandView.mapToItem(contentArea, 0, 0).y + rubberBandTop - rubberBandView.contentY : rubberBandTop
    readonly property real rubberBandViewportRight: rubberBandView && contentArea ? rubberBandView.mapToItem(contentArea, 0, 0).x + rubberBandRight - rubberBandView.contentX : rubberBandRight
    readonly property real rubberBandViewportBottom: rubberBandView && contentArea ? rubberBandView.mapToItem(contentArea, 0, 0).y + rubberBandBottom - rubberBandView.contentY : rubberBandBottom
    readonly property real rubberBandOverlayLeft: Math.max(0, Math.min(contentArea ? contentArea.width : 0, rubberBandViewportLeft))
    readonly property real rubberBandOverlayTop: Math.max(0, Math.min(contentArea ? contentArea.height : 0, rubberBandViewportTop))
    readonly property real rubberBandOverlayRight: Math.max(0, Math.min(contentArea ? contentArea.width : 0, rubberBandViewportRight))
    readonly property real rubberBandOverlayBottom: Math.max(0, Math.min(contentArea ? contentArea.height : 0, rubberBandViewportBottom))
    readonly property real rubberBandOverlayWidth: Math.max(0, rubberBandOverlayRight - rubberBandOverlayLeft)
    readonly property real rubberBandOverlayHeight: Math.max(0, rubberBandOverlayBottom - rubberBandOverlayTop)
    property var scrollPositions: ({})
    property string pendingScrollRestorePath: ""
    property real pendingScrollRestoreY: -1
    property int pendingScrollRestoreAttempts: 0
    property bool pendingScrollRestoreEnabled: false
    property string pendingRevealPath: ""
    property int pendingRevealAttempts: 0
    property string targetSelectPath: ""
    property bool pendingCurrentIndexInit: false
    property bool fileViewsModelEnabled: true
    property bool fileViewsReuseEnabled: false
    property bool fileViewsReuseArmedByUserScroll: false
    property var fileViewsReuseArmedView: null
    property string fileViewsReuseArmReason: ""
    property bool fileViewsReuseScrollbarPressed: false
    property int fileViewsNavigationGeneration: 0
    property int currentIndexEnsureAttempts: 0
    property string pendingInlineRenamePath: ""
    property bool disableSelectionOnCurrentIndexChanged: false
    property bool pendingAutoNameColumnWidthUpdate: false
    property real resizeFrozenListWidth: 0
    property real resizeFrozenBriefCellWidth: 0
    property real resizeFrozenGridCellWidth: 0
    property bool showPanelBreadcrumbs: true
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property bool effectsReduced: root.resizeOptimized || root.ultraLightMode
    readonly property bool lightweightDelegates: root.resizeOptimized || root.ultraLightMode
    readonly property int activeViewCacheBuffer: root.effectsReduced || root.loadingDirectory ? 0 : 1600
    onResizeOptimizedChanged: {
        if (root.resizeOptimized) {
            root.resizeFrozenListWidth = horizontalFlick ? horizontalFlick.width : root.width
            root.resizeFrozenBriefCellWidth = briefView ? briefView.cellWidth : 0
            root.resizeFrozenGridCellWidth = gridView ? gridView.cellWidth : 0
        } else {
            root.resizeFrozenListWidth = 0
            root.resizeFrozenBriefCellWidth = 0
            root.resizeFrozenGridCellWidth = 0
        }
    }
    focus: root.active
    property bool showZebraStriping: true
    property bool showGridlines: true

    // Column widths for Details View (viewMode = 2)
    property real preferredColWidthName: 220
    property bool nameColumnManuallyResized: false
    property bool columnsManuallyResized: false
    readonly property real colMinWidthName: 180
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

    function columnPreferredWidth(column) {
        if (column === "Size") return 90
        if (column === "Type") return 130
        if (column === "Date") return 150
        if (column === "DateCreated") return 150
        if (column === "Extension") return 70
        if (column === "Attributes") return 70
        if (column === "Resolution") return 100
        if (column === "Duration") return 80
        if (column === "Artist") return 140
        if (column === "Album") return 140
        if (column === "Bitrate") return 80
        return 80
    }

    function columnMinWidth(column) {
        if (column === "Size") return 72
        if (column === "Type") return 92
        if (column === "Date") return 118
        if (column === "DateCreated") return 118
        if (column === "Extension") return 54
        if (column === "Attributes") return 56
        if (column === "Resolution") return 76
        if (column === "Duration") return 62
        if (column === "Artist") return 96
        if (column === "Album") return 96
        if (column === "Bitrate") return 62
        return 60
    }

    function visibleDetailColumns() {
        const columns = []
        if (colShowSize) columns.push("Size")
        if (colShowType) columns.push("Type")
        if (colShowDate) columns.push("Date")
        if (colShowDateCreated) columns.push("DateCreated")
        if (colShowExtension) columns.push("Extension")
        if (colShowAttributes) columns.push("Attributes")
        if (colShowResolution) columns.push("Resolution")
        if (colShowDuration) columns.push("Duration")
        if (colShowArtist) columns.push("Artist")
        if (colShowAlbum) columns.push("Album")
        if (colShowBitrate) columns.push("Bitrate")
        return columns
    }

    function setDetailColumnWidth(column, width) {
        if (column === "Size") colWidthSize = width
        else if (column === "Type") colWidthType = width
        else if (column === "Date") colWidthDate = width
        else if (column === "DateCreated") colWidthDateCreated = width
        else if (column === "Extension") colWidthExtension = width
        else if (column === "Attributes") colWidthAttributes = width
        else if (column === "Resolution") colWidthResolution = width
        else if (column === "Duration") colWidthDuration = width
        else if (column === "Artist") colWidthArtist = width
        else if (column === "Album") colWidthAlbum = width
        else if (column === "Bitrate") colWidthBitrate = width
    }

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
        columnsManuallyResized = false
        showZebraStriping = true
        showGridlines = true
        updateNameColumnWidth()
    }

    function boolValue(value, fallback) {
        return value === undefined || value === null ? fallback : !!value
    }

    function numberValue(value, fallback) {
        return value === undefined || value === null || isNaN(Number(value)) ? fallback : Number(value)
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
            nameColumnManuallyResized: nameColumnManuallyResized,
            columnsManuallyResized: columnsManuallyResized,
            preferredColWidthName: preferredColWidthName,
            colWidthSize: colWidthSize,
            colWidthType: colWidthType,
            colWidthDate: colWidthDate,
            colWidthDateCreated: colWidthDateCreated,
            colWidthExtension: colWidthExtension,
            colWidthAttributes: colWidthAttributes,
            colWidthResolution: colWidthResolution,
            colWidthDuration: colWidthDuration,
            colWidthArtist: colWidthArtist,
            colWidthAlbum: colWidthAlbum,
            colWidthBitrate: colWidthBitrate,
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
        nameColumnManuallyResized = boolValue(state.nameColumnManuallyResized, false)
        columnsManuallyResized = boolValue(state.columnsManuallyResized, false)
        preferredColWidthName = numberValue(state.preferredColWidthName, preferredColWidthName)
        if (columnsManuallyResized) {
            colWidthSize = numberValue(state.colWidthSize, colWidthSize)
            colWidthType = numberValue(state.colWidthType, colWidthType)
            colWidthDate = numberValue(state.colWidthDate, colWidthDate)
            colWidthDateCreated = numberValue(state.colWidthDateCreated, colWidthDateCreated)
            colWidthExtension = numberValue(state.colWidthExtension, colWidthExtension)
            colWidthAttributes = numberValue(state.colWidthAttributes, colWidthAttributes)
            colWidthResolution = numberValue(state.colWidthResolution, colWidthResolution)
            colWidthDuration = numberValue(state.colWidthDuration, colWidthDuration)
            colWidthArtist = numberValue(state.colWidthArtist, colWidthArtist)
            colWidthAlbum = numberValue(state.colWidthAlbum, colWidthAlbum)
            colWidthBitrate = numberValue(state.colWidthBitrate, colWidthBitrate)
        }
        showZebraStriping = boolValue(state.showZebraStriping, true)
        showGridlines = boolValue(state.showGridlines, true)
        updateNameColumnWidth()
    }

    function updateSelectionActionsVisible() {
        const next = Boolean(root.showActionBar
                  && root.active
                  && !root.virtualRootMode
                  && !root.isRenaming
                  && root.controller
                  && root.controller.directoryModel
                  && (root.controller.directoryModel.selectedCount > 0 || root.invertSelectionActive))
        if (root.selectionActionsVisible !== next) {
            root.selectionActionsVisible = next
        }
    }

    function updateNameColumnWidth(force) {
        if (force !== true && root.resizeOptimized) {
            root.pendingAutoNameColumnWidthUpdate = true
            return
        }

        root.pendingAutoNameColumnWidthUpdate = false
        const available = Math.max(0, (contentArea ? contentArea.width : 500) - 24)

        if (!columnsManuallyResized) {
            const columns = visibleDetailColumns()
            let preferredOther = 0
            let minOther = 0
            for (let i = 0; i < columns.length; ++i) {
                preferredOther += columnPreferredWidth(columns[i])
                minOther += columnMinWidth(columns[i])
            }

            const desiredNameWidth = nameColumnManuallyResized
                                   ? Math.max(colMinWidthName, preferredColWidthName)
                                   : preferredColWidthName
            const targetOther = Math.max(minOther, Math.min(preferredOther, available - desiredNameWidth))
            const shrinkRange = Math.max(1, preferredOther - minOther)
            const shrinkRatio = preferredOther <= targetOther ? 0 : (preferredOther - targetOther) / shrinkRange

            for (let j = 0; j < columns.length; ++j) {
                const column = columns[j]
                const preferred = columnPreferredWidth(column)
                const minimum = columnMinWidth(column)
                const width = Math.round(preferred - (preferred - minimum) * shrinkRatio)
                setDetailColumnWidth(column, Math.max(minimum, width))
            }
        }

        if (nameColumnManuallyResized) {
            colWidthName = Math.max(colMinWidthName, preferredColWidthName)
        } else {
            const space = available - totalOtherColumnsWidth
            colWidthName = Math.max(colMinWidthName, space)
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

    onPreferredColWidthNameChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onNameColumnManuallyResizedChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
    onColumnsManuallyResizedChanged: { updateNameColumnWidth(); detailsVisualStateChanged() }
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
    onViewModeChanged: {
        root.cancelRubberBand(false)
        root.disableFileViewsReuse()
        updateNameColumnWidth()
    }

    Component.onCompleted: {
        updateNameColumnWidth()
        updateSelectionActionsVisible()
    }

    Timer {
        id: scrollStopTimer
        interval: 50
        onTriggered: {
            if (root.viewMotionActive()) {
                scrollStopTimer.restart()
                return
            }
            root.scrolling = false
            root.controller.scrolling = false
            root.scheduleFileViewsReuseDisable("scroll-stop")
        }
    }

    Timer {
        id: hoverSuppressTimer
        interval: 50
        repeat: false
        onTriggered: {
            if (root.viewMotionActive()) {
                root.hoverSuppressed = true
                hoverSuppressTimer.restart()
                return
            }
            root.hoverSuppressed = false
        }
    }

    Timer {
        id: fileViewsReuseGraceTimer
        interval: 120
        repeat: false
        onTriggered: root.disableFileViewsReuse("reuse-grace-expired")
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
            if (root.loadingDirectory) {
                root.loadingRailReady = true
            }
        }
    }

    Timer {
        id: scrollRestoreTimer
        interval: 0
        repeat: false
        onTriggered: root.restorePendingScrollPosition()
    }

    Timer {
        id: currentIndexEnsureTimer
        interval: 0
        repeat: false
        onTriggered: root.ensureCurrentIndexWithoutSelection()
    }

    Timer {
        id: pendingRevealTimer
        interval: 16
        repeat: false
        onTriggered: {
            if (root.pendingRevealPath.length === 0) {
                return
            }

            if (root.revealCreatedPath(root.pendingRevealPath)) {
                root.pendingRevealPath = ""
                root.pendingRevealAttempts = 0
                return
            }

            if (++root.pendingRevealAttempts <= 120) {
                pendingRevealTimer.restart()
            } else {
                root.pendingRevealPath = ""
                root.pendingRevealAttempts = 0
            }
        }
    }

    Connections {
        target: root.controller.directoryModel
        function onVisualStructureAboutToChange() {
            root.disableFileViewsReuse()
        }
        function onLoadingChanged() {
            root.disableFileViewsReuse()
            root.updateDirectoryLoadingState()
        }
        function onCountChanged() {
            root.disableFileViewsReuse()
            root.queuePendingScrollRestore()
            root.queuePendingReveal()
        }
        function onSelectionChanged() {
            root.disableFileViewsReuse()
            root.updateSelectionActionsVisible()
        }
    }

    Connections {
        target: root.controller
        function onNavigationPendingChanged() {
            root.updateDirectoryLoadingState()
        }

        function onPathAboutToChange(from, to, preserveScroll) {
            root.fileViewsNavigationGeneration += 1
            root.saveScrollPositionForPath(from)
            root.disableFileViewsReuse("path-about-to-change")
            root.fileViewsModelEnabled = false
            root.cancelRubberBand(false)
            root.invertSelectionActive = false
            root.isRenaming = false
            root.pendingInlineRenamePath = ""
            root.pendingCurrentIndexInit = true
            root.currentIndexEnsureAttempts = 0
            root.pendingScrollRestoreEnabled = preserveScroll
            if (!preserveScroll) {
                root.pendingScrollRestorePath = ""
                root.pendingScrollRestoreY = -1
                root.targetSelectPath = ""
            } else {
                root.targetSelectPath = root.findDirectChildPath(to, from)
                const state = scrollPositions[scrollKeyForPath(to)]
                if (root.targetSelectPath.length === 0 && state && state.focusedPath) {
                    root.targetSelectPath = state.focusedPath
                }
            }
            root.scrolling = true
            root.controller.scrolling = true
            root.suppressHoverBriefly()
            scrollStopTimer.restart()
        }
        function onPathNavigated(path) {
            root.restoreFileViewsForGeneration(root.fileViewsNavigationGeneration)
            if (root.active) {
                Qt.callLater(() => {
                    let isSidebarFocused = typeof sidebar !== "undefined" && sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
                    if (!isSidebarFocused) {
                        root.focusContent()
                    }
                    
                    root.queueCurrentIndexEnsure()
                })
            }
            if (root.pendingScrollRestoreEnabled) {
                root.queueScrollRestoreForPath(path)
                root.pendingScrollRestoreEnabled = false
            }
            root.scrolling = true
            root.controller.scrolling = true
            root.suppressHoverBriefly()
            scrollStopTimer.restart()
        }
        function onPathNavigationFailed(path) {
            root.restoreFileViewsForGeneration(root.fileViewsNavigationGeneration)
            root.pendingCurrentIndexInit = true
            root.currentIndexEnsureAttempts = 0
            root.targetSelectPath = path
            root.pendingScrollRestoreEnabled = false
            root.pendingScrollRestorePath = ""
            root.pendingScrollRestoreY = -1
            root.pendingScrollRestoreAttempts = 0
            scrollStopTimer.restart()
        }
        function onEntryRenamed(oldPath, newPath) {
            if (root.pendingInlineRenamePath.length > 0 && oldPath === root.pendingInlineRenamePath) {
                root.isRenaming = false
                root.pendingInlineRenamePath = ""
                root.focusRenamedPath(newPath)
            }
        }
        function onCreatedEntryRevealRequested(path) {
            root.requestRevealPath(path)
        }
    }

    function restoreFileViewsForGeneration(generation) {
        Qt.callLater(() => {
            if (generation !== root.fileViewsNavigationGeneration) {
                return
            }
            root.fileViewsModelEnabled = !root.virtualRootMode
            root.disableFileViewsReuse("navigation-restore")
        })
    }

    function disableFileViewsReuse(reason) {
        fileViewsReuseGraceTimer.stop()
        root.fileViewsReuseArmedByUserScroll = false
        root.fileViewsReuseArmedView = null
        root.fileViewsReuseArmReason = ""
        root.fileViewsReuseScrollbarPressed = false
        if (root.fileViewsReuseEnabled) {
            root.fileViewsReuseEnabled = false
        }
    }

    function scheduleFileViewsReuseDisable(reason) {
        if (!root.fileViewsReuseArmedByUserScroll && !root.fileViewsReuseEnabled) {
            return
        }
        fileViewsReuseGraceTimer.restart()
    }

    function armFileViewsReuseForUserScroll(view, reason) {
        if (!root.canArmFileViewsReuseFromUserScroll(view, reason)) {
            root.disableFileViewsReuse("arm-rejected")
            return
        }
        fileViewsReuseGraceTimer.stop()
        root.fileViewsReuseArmedByUserScroll = true
        root.fileViewsReuseArmedView = view
        root.fileViewsReuseArmReason = reason || "user-scroll"
        root.fileViewsReuseScrollbarPressed = reason === "scrollbar-press"
        root.updateFileViewsReuseForMotion()
    }

    function handleScrollbarPressedChanged(view, pressed) {
        if (pressed) {
            root.armFileViewsReuseForUserScroll(view, "scrollbar-press")
            return
        }
        root.fileViewsReuseScrollbarPressed = false
        root.updateFileViewsReuseForMotion()
        root.scheduleFileViewsReuseDisable("scrollbar-release")
    }

    function canArmFileViewsReuseFromUserScroll(view, reason) {
        if (reason !== "movement-start" && reason !== "flick-start" && reason !== "scrollbar-press") return false
        if (!view || view !== root.activeView()) return false
        if (root.virtualRootMode || !root.fileViewsModelEnabled) return false
        if (root.loadingDirectory || root.resizeOptimized) return false
        if (root.isRenaming || root.rubberBandPressed || root.rubberBandActive) return false
        if (root.pendingCurrentIndexInit || root.pendingScrollRestoreEnabled || root.pendingScrollRestorePath.length > 0) return false
        if (root.panelKeysBlockedByOverlay()) return false
        if (!root.controller || !root.controller.directoryModel) return false
        if (root.controller.directoryModel.loading || root.controller.directoryModel.count <= 0) return false
        return view.count > 0
    }

    // !!! DANGER: reuseItems is a poisoned performance switch.
    // !!! It exists only to smooth active user scrolling in huge folders.
    // !!! Do not enable it for navigation, delete, refresh, selection, restore,
    // !!! keyboard jumps, context menus, rubber banding, rename, or model changes.
    // !!! Allowed user-scroll sources: movement-start, flick-start, scrollbar-press.
    // !!! Every future enable path must satisfy this single gate.
    function canEnableFileViewsReuse(view) {
        if (!view || view !== root.activeView()) return false
        if (!root.fileViewsReuseArmedByUserScroll || root.fileViewsReuseArmedView !== view) return false
        if (root.virtualRootMode || !root.fileViewsModelEnabled) return false
        if (root.loadingDirectory || root.resizeOptimized) return false
        if (root.isRenaming || root.rubberBandPressed || root.rubberBandActive) return false
        if (root.pendingCurrentIndexInit || root.pendingScrollRestoreEnabled || root.pendingScrollRestorePath.length > 0) return false
        if (root.panelKeysBlockedByOverlay()) return false
        if (!root.controller || !root.controller.directoryModel) return false
        if (root.controller.directoryModel.loading || root.controller.directoryModel.count <= 0) return false
        const userScrollActive = view.moving || view.flicking || root.fileViewsReuseScrollbarPressed
        return view.count > 0 && userScrollActive
    }

    function updateFileViewsReuseForMotion() {
        const next = root.canEnableFileViewsReuse(root.fileViewsReuseArmedView)
        if (root.fileViewsReuseEnabled !== next) {
            root.fileViewsReuseEnabled = next
        }
    }

    function updateScrollingState() {
        const isScrolling = root.viewMotionActive()

        if (isScrolling) {
            root.updateFileViewsReuseForMotion()
            root.markPreviewScrollActive()
            scrollStopTimer.stop()
            hoverSuppressTimer.stop()
            root.hoverSuppressed = true
            if (!root.scrolling) {
                root.scrolling = true
                root.controller.scrolling = true
                // Clear hover on scroll start
                root.controller.hoveredPath = ""
            }
        } else {
            root.scheduleFileViewsReuseDisable("motion-stop")
            if (root.hoverSuppressed && !hoverSuppressTimer.running) {
                hoverSuppressTimer.start()
            }
            if (root.scrolling && !scrollStopTimer.running) {
                scrollStopTimer.start()
            }
        }
    }

    function viewMotionActive() {
        const view = root.viewMode === 2 ? briefView : root.viewMode === 0 ? listView : gridView
        return Boolean(view && (view.moving || view.flicking))
    }

    function suppressHoverBriefly() {
        root.hoverSuppressed = true
        hoverSuppressTimer.restart()
    }

    function queuePendingScrollRestore() {
        if (root.pendingScrollRestorePath.length === 0) {
            return false
        }
        scrollRestoreTimer.restart()
        return true
    }

    function emptyAreaInputEnabled() {
        const model = root.controller ? root.controller.directoryModel : null
        return !model || !model.loading || model.count > 0
    }

    function handleScrollActivity() {
        root.markPreviewScrollActive()
        root.suppressHoverBriefly()
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

    function focusContentAndQueueCurrentIndexEnsure() {
        Qt.callLater(() => {
            let isSidebarFocused = typeof sidebar !== "undefined" && sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
            if (!isSidebarFocused) {
                root.focusContent()
            }
            root.queueCurrentIndexEnsure()
        })
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
        if (root.pendingCurrentIndexInit && view.currentIndex === index) {
            view.currentIndex = -1
        }
        view.currentIndex = index
        root.updateCurrentItemPath(index)
        Qt.callLater(() => {
            root.disableSelectionOnCurrentIndexChanged = false
        })
    }

    function viewCurrentIndexInvalid(view, count) {
        return !view || view.currentIndex < 0 || view.currentIndex >= count
    }

    function queueCurrentIndexEnsure() {
        if (!root.active || root.virtualRootMode || root.controller.directoryModel.count <= 0
                || root.rubberBandPressed || root.rubberBandActive) {
            return
        }
        currentIndexEnsureTimer.restart()
    }

    function targetPathPendingDuringLoad() {
        return root.pendingCurrentIndexInit
                && root.targetSelectPath !== ""
                && root.controller.directoryModel.loading
                && root.controller.directoryModel.indexOfPath(root.targetSelectPath) < 0
    }

    function desiredInitialCurrentIndex() {
        let idx = 0
        if (root.targetSelectPath !== "") {
            let pathIdx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
            if (pathIdx >= 0) {
                idx = pathIdx
            } else {
                idx = -1
            }
        }
        return idx
    }

    function completeCurrentIndexInit() {
        root.pendingCurrentIndexInit = false
        root.currentIndexEnsureAttempts = 0
        root.targetSelectPath = ""
    }

    function shouldAutoPositionCurrentIndex() {
        return !root.resizeOptimized
                && root.pendingScrollRestorePath.length === 0
                && !root.pendingScrollRestoreEnabled
    }

    function revealTargetSelectPath() {
        if (root.targetSelectPath === "") {
            return false
        }
        if (root.pendingScrollRestorePath.length > 0 || root.pendingScrollRestoreEnabled) {
            return false
        }

        const idx = root.controller.directoryModel.indexOfPath(root.targetSelectPath)
        if (idx < 0) {
            return false
        }

        const view = root.activeView()
        if (!view || view.count <= idx) {
            return false
        }

        root.setViewCurrentIndexWithoutSelection(view, idx)
        if (root.shouldAutoPositionCurrentIndex()) {
            if (view.forceLayout) {
                view.forceLayout()
            }
            view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
        }
        root.completeCurrentIndexInit()
        return true
    }

    function verifyCurrentIndexInitialized() {
        if (!root.pendingCurrentIndexInit) {
            return
        }
        if (root.pendingScrollRestorePath.length > 0 || root.pendingScrollRestoreEnabled) {
            return
        }
        if (!root.active || root.virtualRootMode || root.controller.directoryModel.count <= 0) {
            return
        }

        const view = root.activeView()
        const idx = root.desiredInitialCurrentIndex()
        if (view && view.currentIndex === idx && view.currentItem && !root.targetPathPendingDuringLoad()) {
            root.completeCurrentIndexInit()
        } else {
            root.queueCurrentIndexEnsure()
        }
    }

    function ensureCurrentIndexWithoutSelection() {
        if (!root.active || root.virtualRootMode || root.controller.directoryModel.count <= 0) {
            return
        }

        const view = root.activeView()
        if (!view || (!root.pendingCurrentIndexInit && !root.viewCurrentIndexInvalid(view, root.controller.directoryModel.count))) {
            return
        }

        if (root.pendingScrollRestorePath.length > 0 || root.pendingScrollRestoreEnabled) {
            if (!root.controller.directoryModel.loading) {
                root.queuePendingScrollRestore()
            }
            return
        }

        if (root.targetPathPendingDuringLoad()
                && !root.viewCurrentIndexInvalid(view, root.controller.directoryModel.count)
                && view.currentItem) {
            return
        }

        if (root.revealTargetSelectPath()) {
            return
        }

        const idx = root.desiredInitialCurrentIndex()
        if (idx < 0) {
            if (root.controller.directoryModel.loading) {
                return
            }

            if (++root.currentIndexEnsureAttempts <= 8) {
                currentIndexEnsureTimer.restart()
                return
            }

            root.completeCurrentIndexInit()
            root.queueCurrentIndexEnsure()
            return
        }

        if (++root.currentIndexEnsureAttempts > 8
                && root.targetSelectPath === ""
                && !root.targetPathPendingDuringLoad()
                && root.pendingScrollRestorePath.length === 0
                && !root.pendingScrollRestoreEnabled) {
            root.completeCurrentIndexInit()
            return
        }

        root.setViewCurrentIndexWithoutSelection(view, idx)
        if (root.shouldAutoPositionCurrentIndex()) {
            view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
        }
        Qt.callLater(root.verifyCurrentIndexInitialized)
    }

    function restorePreviewAfterRenameEdit() {
        if (root.Window.window && root.Window.window.previewPaneVisible) {
            root.Window.window.syncPreviewFromActivePanel(true)
        }
    }

    function cancelInlineRename() {
        root.isRenaming = false
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
            return false
        }

        const idx = root.controller.directoryModel.indexOfPath(path)
        if (idx < 0) {
            return false
        }

        const view = root.activeView()
        if (!view || view.count <= idx) {
            return false
        }

        root.setViewCurrentIndexWithoutSelection(view, idx)
        root.controller.directoryModel.selectOnly(idx)
        if (!root.resizeOptimized) {
            if (view.forceLayout) {
                view.forceLayout()
            }
            view.positionViewAtIndex(idx, root.viewMode === 0 ? ListView.Contain : GridView.Contain)
        }
        root.pendingCurrentIndexInit = false
        root.currentIndexEnsureAttempts = 0
        root.targetSelectPath = ""
        return true
    }

    function requestRevealPath(path) {
        if (!path || path.length === 0) {
            return
        }
        root.pendingRevealPath = path
        root.pendingRevealAttempts = 0
        root.queuePendingReveal()
    }

    function queuePendingReveal() {
        if (root.pendingRevealPath.length === 0) {
            return false
        }
        pendingRevealTimer.restart()
        return true
    }

    function updateCurrentItemPath(index) {
        if (!root.controller || !root.controller.directoryModel) {
            return
        }
        root.controller.currentItemPath = index >= 0 && index < root.controller.directoryModel.count
                                        ? root.controller.directoryModel.pathAt(index)
                                        : ""
    }

    function normalizedPanelPath(path) {
        let value = String(path || "").replace(/\\/g, "/")
        if (value === "devices://" || value === "favorites://") {
            return value
        }
        while (value.length > 1
               && value.endsWith("/")
               && !/^[A-Za-z]:\/$/.test(value)
               && !value.endsWith("|/")) {
            value = value.slice(0, -1)
        }
        return Qt.platform.os === "windows" ? value.toLowerCase() : value
    }

    function samePanelPath(left, right) {
        return root.normalizedPanelPath(left) === root.normalizedPanelPath(right)
    }

    function scrollKeyForPath(path) {
        return root.normalizedPanelPath(path) + "|" + root.viewMode
    }

    function findDirectChildPath(parentPath, childPath) {
        if (!parentPath || !childPath) return "";
        let p = parentPath.replace(/\\/g, "/");
        let c = childPath.replace(/\\/g, "/");
        if (c.startsWith("archive://") && !p.startsWith("archive://")) {
            let archiveFile = c.substring(10).split("|")[0];
            if (archiveFile.startsWith(p.endsWith("/") ? p : p + "/")) {
                return archiveFile;
            }
        }
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

        scrollPositions[scrollKeyForPath(path)] = {
            y: view.contentY,
            x: view.contentX,
            focusedPath: root.controller.currentItemPath
        }
    }

    function queueScrollRestoreForPath(path) {
        if (!path || path === "devices://" || path === "favorites://") {
            pendingScrollRestorePath = ""
            pendingScrollRestoreY = -1
            pendingScrollRestoreAttempts = 0
            return
        }

        const state = scrollPositions[scrollKeyForPath(path)]
        if (!state) {
            pendingScrollRestorePath = ""
            pendingScrollRestoreY = -1
            pendingScrollRestoreAttempts = 0
            return
        }

        pendingScrollRestorePath = path
        pendingScrollRestoreY = state.y
        pendingScrollRestoreAttempts = 0

        if (!root.controller.directoryModel.loading) {
            root.queuePendingScrollRestore()
        }
    }

    function restorePendingScrollPosition() {
        if (!pendingScrollRestorePath) {
            return
        }

        if (!root.samePanelPath(root.controller.currentPath, pendingScrollRestorePath)) {
            pendingScrollRestorePath = ""
            pendingScrollRestoreY = -1
            pendingScrollRestoreAttempts = 0
            root.currentIndexEnsureAttempts = 0
            if (root.active) {
                root.focusContentAndQueueCurrentIndexEnsure()
            }
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

        if (view.forceLayout) {
            view.forceLayout()
        }

        const maxY = Math.max(0, view.contentHeight - view.height)
        const restoredY = Math.min(Math.max(0, pendingScrollRestoreY), maxY)
        if (pendingScrollRestoreY > 0 && restoredY === 0
                && root.controller.directoryModel.count > 0
                && pendingScrollRestoreAttempts < 6) {
            pendingScrollRestoreAttempts += 1
            scrollRestoreTimer.restart()
            return
        }

        view.contentY = restoredY
        pendingScrollRestorePath = ""
        pendingScrollRestoreY = -1
        pendingScrollRestoreAttempts = 0
        root.currentIndexEnsureAttempts = 0
        root.revealTargetSelectPath()
        if (root.active) {
            root.focusContentAndQueueCurrentIndexEnsure()
        }
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
            root.selectAll()
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
        let started = false
        if (root.viewMode === 2) {
            if (briefView.currentItem) {
                briefView.currentItem.startRename()
                started = true
            }
        } else if (root.viewMode === 0) {
            if (listView.currentItem) {
                listView.currentItem.startRename()
                started = true
            }
        } else {
            if (gridView.currentItem) {
                gridView.currentItem.startRename()
                started = true
            }
        }
        if (started) {
            root.isRenaming = true
        } else {
            root.pendingInlineRenamePath = ""
            root.restorePreviewAfterRenameEdit()
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

    function viewPointToViewContentPoint(view, x, y) {
        return Qt.point(x + (view.contentX || 0), y + (view.contentY || 0))
    }

    function contentAreaPointToViewContentPoint(view, x, y) {
        const point = contentArea.mapToItem(view, x, y)
        return root.viewPointToViewContentPoint(view, point.x, point.y)
    }

    function beginRubberBand(view, mouse) {
        if (root.virtualRootMode || root.isRenaming || root.panelKeysBlockedByOverlay() || mouse.button !== Qt.LeftButton) {
            return
        }
        const point = root.viewPointToViewContentPoint(view, mouse.x, mouse.y)
        root.beginRubberBandAtContentPoint(view, mouse, point.x, point.y)
    }

    function beginRubberBandAtContentPoint(view, mouse, contentX, contentY) {
        if (root.virtualRootMode || root.isRenaming || root.panelKeysBlockedByOverlay() || mouse.button !== Qt.LeftButton) {
            return
        }
        root.activated()
        root.rubberBandView = view
        root.rubberBandStartX = contentX
        root.rubberBandStartY = contentY
        root.rubberBandCurrentX = contentX
        root.rubberBandCurrentY = contentY
        root.rubberBandPressed = true
        root.rubberBandActive = false
        root.rubberBandMoved = false
    }

    function beginRubberBandPress(view, mouse) {
        if (mouse.button === Qt.RightButton) {
            if (root.viewItemAtPoint(view, mouse.x, mouse.y)) {
                mouse.accepted = false
            }
            return
        }
        if (mouse.button !== Qt.LeftButton
                || root.virtualRootMode
                || root.isRenaming
                || root.panelKeysBlockedByOverlay()) {
            mouse.accepted = false
            return
        }

        const item = root.viewItemAtPoint(view, mouse.x, mouse.y)
        root.rubberBandPressView = view
        root.rubberBandPressIndex = item ? item.index : -1
        root.beginRubberBand(view, mouse)
    }

    function updateRubberBand(view, mouse) {
        const point = root.viewPointToViewContentPoint(view, mouse.x, mouse.y)
        root.updateRubberBandAtContentPoint(view, point.x, point.y)
    }

    function updateRubberBandAtContentPoint(view, contentX, contentY) {
        if (!root.rubberBandPressed || root.rubberBandView !== view) {
            return
        }
        root.rubberBandCurrentX = contentX
        root.rubberBandCurrentY = contentY
        const dx = contentX - root.rubberBandStartX
        const dy = contentY - root.rubberBandStartY
        if (!root.rubberBandActive && Math.sqrt(dx * dx + dy * dy) >= 5) {
            root.rubberBandActive = true
            root.rubberBandMoved = true
            root.clearSelection()
            view.currentIndex = -1
            rubberBandAutoScroll.restart()
        }
    }

    function finishRubberBand(view, mouse) {
        if (!root.rubberBandPressed || root.rubberBandView !== view) {
            return false
        }
        if (!mouse) {
            return root.finishRubberBandAtContentPoint(view, root.rubberBandCurrentX, root.rubberBandCurrentY)
        }
        const point = root.viewPointToViewContentPoint(view, mouse.x, mouse.y)
        return root.finishRubberBandAtContentPoint(view, point.x, point.y)
    }

    function finishRubberBandAtContentPoint(view, contentX, contentY) {
        if (!root.rubberBandPressed || root.rubberBandView !== view) {
            return false
        }
        if (root.rubberBandActive) {
            root.rubberBandCurrentX = contentX
            root.rubberBandCurrentY = contentY
            root.commitRubberBandSelection()
        }

        const usedRubberBand = root.rubberBandActive
        root.rubberBandPressed = false
        root.rubberBandActive = false
        root.rubberBandMoved = false
        root.rubberBandView = null
        rubberBandAutoScroll.stop()
        if (usedRubberBand) {
            root.queueCurrentIndexEnsure()
        }
        return usedRubberBand
    }

    function finishRubberBandPress(view, mouse) {
        if (mouse.button !== Qt.LeftButton) {
            return
        }

        const index = root.rubberBandPressIndex
        const usedRubberBand = root.finishRubberBand(view, mouse)
        if (!usedRubberBand) {
            if (index >= 0 && root.rubberBandPressView === view) {
                root.handleItemClick(index, mouse)
            } else {
                root.handleEmptyViewClick(view, mouse)
            }
        }
        root.clearRubberBandPress()
    }

    function beginRubberBandContentPress(view, mouse, contentX, contentY) {
        if (mouse.button !== Qt.LeftButton
                || root.virtualRootMode
                || root.isRenaming
                || root.panelKeysBlockedByOverlay()) {
            mouse.accepted = false
            return
        }

        root.rubberBandPressView = view
        root.rubberBandPressIndex = -1
        const point = root.contentAreaPointToViewContentPoint(view, contentX, contentY)
        root.beginRubberBandAtContentPoint(view, mouse, point.x, point.y)
    }

    function finishRubberBandContentPress(view, mouse, contentX, contentY) {
        if (mouse.button !== Qt.LeftButton) {
            return
        }

        const point = root.contentAreaPointToViewContentPoint(view, contentX, contentY)
        const usedRubberBand = root.finishRubberBandAtContentPoint(view, point.x, point.y)
        if (!usedRubberBand) {
            root.handleEmptyViewClick(view, mouse)
        }
        root.clearRubberBandPress()
    }

    function updateRubberBandContentPress(view, contentX, contentY) {
        const point = root.contentAreaPointToViewContentPoint(view, contentX, contentY)
        root.updateRubberBandAtContentPoint(view, point.x, point.y)
    }

    function cancelRubberBand(clearSelection) {
        root.rubberBandPressed = false
        root.rubberBandActive = false
        root.rubberBandMoved = false
        root.rubberBandView = null
        root.clearRubberBandPress()
        rubberBandAutoScroll.stop()
        if (clearSelection) {
            root.clearSelection()
        }
    }

    function clearRubberBandPress() {
        root.rubberBandPressView = null
        root.rubberBandPressIndex = -1
    }

    function toggleInvertSelection() {
        if (!root.canInvertSelection || root.isRenaming || root.panelKeysBlockedByOverlay()) {
            return
        }

        root.controller.directoryModel.invertSelection()
        root.invertSelectionActive = !root.invertSelectionActive
    }

    function clearSelection() {
        root.invertSelectionActive = false
        if (root.controller && root.controller.directoryModel) {
            root.controller.directoryModel.clearSelection()
        }
        root.queueCurrentIndexEnsure()
    }

    function selectAll() {
        root.invertSelectionActive = false
        if (root.controller && root.controller.directoryModel) {
            root.controller.directoryModel.selectAll()
        }
        root.queueCurrentIndexEnsure()
    }

    function handleRubberBandDoubleClick(view, mouse) {
        const item = root.viewItemAtPoint(view, mouse.x, mouse.y)
        if (!item) {
            return
        }
        root.cancelRubberBand(false)
        root.controller.openItem(item.index)
    }

    function handleEmptyViewClick(view, mouse) {
        root.activated()
        if (mouse.button === Qt.RightButton) {
            filePanelEmptyMenu.popupEmptyMenu()
            return
        }
        if (!root.rubberBandMoved) {
            root.clearSelection()
            if (root.controller.directoryModel.count <= 0) {
                view.currentIndex = -1
            } else {
                root.queueCurrentIndexEnsure()
            }
        }
    }

    function viewItemAtPoint(view, x, y) {
        const rows = root.viewCandidateRows(view)
        for (let i = 0; i < rows.length; ++i) {
            const item = view.itemAtIndex(rows[i])
            if (!item || !item.visible) {
                continue
            }
            const point = item.mapToItem(view, 0, 0)
            if (x >= point.x && x <= point.x + item.width
                    && y >= point.y && y <= point.y + item.height) {
                return item
            }
        }
        return null
    }

    function viewCandidateRows(view) {
        const count = root.controller && root.controller.directoryModel ? root.controller.directoryModel.count : 0
        if (count <= 0 || !view) {
            return []
        }

        if (view === listView) {
            let first = view.indexAt(Math.max(1, view.contentX + 1), view.contentY + 1)
            if (first < 0) {
                first = 0
            }
            first = Math.max(0, first - 8)
            const rows = []
            for (let i = first; i < count; ++i) {
                const item = view.itemAtIndex(i)
                if (!item) {
                    if (rows.length > 0) {
                        break
                    }
                    continue
                }
                const point = item.mapToItem(contentArea, 0, 0)
                if (point.y > contentArea.height) {
                    break
                }
                if (point.y + item.height >= 0) {
                    rows.push(i)
                }
            }
            return rows
        }

        const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
        const startRow = Math.max(0, Math.floor(view.contentY / Math.max(1, view.cellHeight)) - 1)
        const endRow = Math.ceil((view.contentY + view.height) / Math.max(1, view.cellHeight)) + 1
        const start = Math.max(0, startRow * columns)
        const end = Math.min(count - 1, ((endRow + 1) * columns) - 1)
        const rows = []
        for (let i = start; i <= end; ++i) {
            rows.push(i)
        }
        return rows
    }

    function rubberBandCandidateRows(view) {
        const count = root.controller && root.controller.directoryModel ? root.controller.directoryModel.count : 0
        if (count <= 0 || !view) {
            return []
        }

        if (view === listView) {
            const rowHeight = Math.max(1, Theme.rowHeight)
            const start = Math.max(0, Math.floor(root.rubberBandTop / rowHeight) - 1)
            const end = Math.min(count - 1, Math.ceil(root.rubberBandBottom / rowHeight) + 1)
            const rows = []
            for (let i = start; i <= end; ++i) {
                rows.push(i)
            }
            return rows
        }

        const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
        const startRow = Math.max(0, Math.floor(root.rubberBandTop / Math.max(1, view.cellHeight)) - 1)
        const endRow = Math.ceil(root.rubberBandBottom / Math.max(1, view.cellHeight)) + 1
        const start = Math.max(0, startRow * columns)
        const end = Math.min(count - 1, ((endRow + 1) * columns) - 1)
        const rows = []
        for (let i = start; i <= end; ++i) {
            rows.push(i)
        }
        return rows
    }

    function rubberBandItemRect(view, row) {
        if (view === listView) {
            const rowHeight = Math.max(1, Theme.rowHeight)
            return { x: 0, y: row * rowHeight, width: view.width, height: rowHeight }
        }

        const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
        return {
            x: (row % columns) * view.cellWidth,
            y: Math.floor(row / columns) * view.cellHeight,
            width: view.cellWidth,
            height: view.cellHeight
        }
    }

    function rubberBandSelectsItem(view, itemX, itemY, itemWidth, itemHeight) {
        let targetX = itemX
        let targetY = itemY
        let targetWidth = itemWidth
        let targetHeight = itemHeight

        if (view === listView && root.viewMode === 0) {
            targetX = itemX + 12
            targetY = itemY + 4
            targetWidth = Math.max(0, root.colWidthName - 20)
            targetHeight = Math.max(0, itemHeight - 8)
        } else if (view === gridView) {
            const visualWidth = Math.min(itemWidth - 18, Math.max(root.gridIconSize + 34, 72))
            const visualHeight = Math.min(itemHeight - 12, root.gridIconSize + 54)
            targetX = itemX + Math.max(8, (itemWidth - visualWidth) / 2)
            targetY = itemY + 6
            targetWidth = visualWidth
            targetHeight = visualHeight
        } else if (view === briefView) {
            targetX = itemX + 10
            targetY = itemY + 3
            targetWidth = Math.max(0, itemWidth - 20)
            targetHeight = Math.max(0, itemHeight - 6)
            const overlapLeft = Math.max(targetX, root.rubberBandLeft)
            const overlapTop = Math.max(targetY, root.rubberBandTop)
            const overlapRight = Math.min(targetX + targetWidth, root.rubberBandRight)
            const overlapBottom = Math.min(targetY + targetHeight, root.rubberBandBottom)
            const overlapWidth = Math.max(0, overlapRight - overlapLeft)
            const overlapHeight = Math.max(0, overlapBottom - overlapTop)
            const overlapArea = overlapWidth * overlapHeight
            const targetArea = targetWidth * targetHeight
            return targetArea > 0 && overlapArea >= Math.min(targetArea * 0.18, 24 * targetHeight)
        } else {
            targetX = itemX + 16
            targetY = itemY + 4
            targetWidth = Math.max(0, itemWidth - 32)
            targetHeight = Math.max(0, itemHeight - 8)
        }

        const centerX = targetX + targetWidth / 2
        const centerY = targetY + targetHeight / 2
        return targetWidth > 0
                && targetHeight > 0
                && centerX >= root.rubberBandLeft
                && centerX <= root.rubberBandRight
                && centerY >= root.rubberBandTop
                && centerY <= root.rubberBandBottom
    }

    function commitRubberBandSelection() {
        if (!root.rubberBandActive || !root.rubberBandView || root.rubberBandWidth <= 0 || root.rubberBandHeight <= 0) {
            return
        }

        const selectedRows = []
        const rows = root.rubberBandCandidateRows(root.rubberBandView)
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            const rect = root.rubberBandItemRect(root.rubberBandView, row)
            if (!rect) {
                continue
            }
            if (root.rubberBandSelectsItem(root.rubberBandView, rect.x, rect.y, rect.width, rect.height)) {
                selectedRows.push(row)
            }
        }
        root.controller.directoryModel.selectRows(selectedRows)
        if (selectedRows.length > 0) {
            root.setViewCurrentIndexWithoutSelection(root.rubberBandView, selectedRows[0])
        }
    }

    function updateRubberBandAutoScroll() {
        if (!root.rubberBandActive || !root.rubberBandView) {
            rubberBandAutoScroll.stop()
            return
        }
        const view = root.rubberBandView
        const pointY = root.rubberBandCurrentY - (view.contentY || 0)
        const edge = 36
        let delta = 0
        if (pointY < edge) {
            delta = -Math.ceil((edge - pointY) / 4)
        } else if (pointY > view.height - edge) {
            delta = Math.ceil((pointY - (view.height - edge)) / 4)
        }
        if (delta === 0) {
            return
        }
        const maxY = Math.max(0, view.contentHeight - view.height + (view.bottomMargin || 0))
        const oldY = view.contentY
        const nextY = Math.max(0, Math.min(maxY, view.contentY + delta))
        if (nextY !== view.contentY) {
            view.contentY = nextY
            root.rubberBandCurrentY += view.contentY - oldY
            root.markPreviewScrollActive()
            root.suppressHoverBriefly()
            if (!root.scrolling) {
                root.scrolling = true
                root.controller.scrolling = true
                root.controller.hoveredPath = ""
            }
            scrollStopTimer.restart()
        }
    }

    function handleItemClick(index, mouse) {
        root.activated()
        root.invertSelectionActive = false
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
        let path = root.controller.navigationPending && root.controller.pendingNavigationPath.length > 0
            ? root.controller.pendingNavigationPath
            : root.controller.currentPath
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
        windowObject: root.Window.window
        isCurrentPathArchive: root.isCurrentPathArchive
        isCurrentPathReadOnlyContainer: root.isCurrentPathReadOnlyContainer
        onSelectAllRequested: root.selectAll()
    }

    FilePanelDropOverlay {
        anchors.fill: parent
        workspaceController: root.workspaceController
        currentPath: root.controller.currentPath
    }

    Timer {
        id: rubberBandAutoScroll
        interval: 30
        repeat: true
        onTriggered: root.updateRubberBandAutoScroll()
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
                    showActionBar: root.showActionBar
                    onActionBarVisibilityRequested: (visible) => root.showActionBar = visible
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
            onWidthChanged: {
                root.updateNameColumnWidth()
            }

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
                    scrolling: root.hoverSuppressed
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
                    scrolling: root.hoverSuppressed
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
                    anchors.bottomMargin: root.bottomChromeHeight
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
                        model: root.fileViewsModelEnabled && root.viewMode === 0 && !root.virtualRootMode ? root.controller.directoryModel : null
                        currentIndex: -1
                        focus: root.active && root.viewMode === 0
                        
                        onActiveFocusChanged: {
                            if (activeFocus && count > 0) {
                                root.queueCurrentIndexEnsure()
                            }
                        }
                        onCurrentIndexChanged: {
                            if (root.pendingCurrentIndexInit && root.viewCurrentIndexInvalid(listView, count)) {
                                root.queueCurrentIndexEnsure()
                                return
                            }
                            root.updateCurrentItemPath(currentIndex)
                            if (activeFocus && currentIndex >= 0 && currentIndex < count) {
                                if (!root.disableSelectionOnCurrentIndexChanged) {
                                    root.controller.directoryModel.selectOnly(currentIndex)
                                }
                                if (root.shouldAutoPositionCurrentIndex()) {
                                    positionViewAtIndex(currentIndex, ListView.Contain)
                                }
                            }
                        }
                        onCountChanged: {
                            if (count > 0 && (root.viewCurrentIndexInvalid(listView, count)
                                              || root.pendingCurrentIndexInit)) {
                                root.queueCurrentIndexEnsure()
                            }
                        }
                        cacheBuffer: root.activeViewCacheBuffer
                        reuseItems: root.fileViewsReuseEnabled && root.canEnableFileViewsReuse(listView)
                        onMovementStarted: root.armFileViewsReuseForUserScroll(listView, "movement-start")
                        onFlickStarted: root.armFileViewsReuseForUserScroll(listView, "flick-start")
                        onMovementEnded: root.scheduleFileViewsReuseDisable("movement-end")
                        onFlickEnded: root.scheduleFileViewsReuseDisable("flick-end")
                        onMovingChanged: root.updateScrollingState()
                        onFlickingChanged: root.updateScrollingState()
                        onContentYChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                        onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                        bottomMargin: root.bottomChromeHeight + (root.horizontalScrollActive ? 12 : 0)
                        
                        highlight: null
                        highlightFollowsCurrentItem: false

                        add: null
                        remove: null
                        Keys.onPressed: (event) => {
                            if (root.panelKeysBlockedByOverlay()) {
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
                                if (currentIndex >= 0 && currentIndex < count) {
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
                            if (currentIndex === -1 && count > 0 &&
                                (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                                currentIndex = (event.key === Qt.Key_Up) ? count - 1 : 0
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
                                root.cancelRubberBand(true)
                                root.workspaceController.focusActivePanel()
            event.accepted = true
        }
    }
                        delegate: root.viewMode === 0 ? detailsDelegate : listDelegate

                        MouseArea {
                            anchors.fill: parent
                            z: 8
                            enabled: root.emptyAreaInputEnabled()
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            preventStealing: true
                            scrollGestureEnabled: false
                            onPressed: (mouse) => root.beginRubberBandPress(listView, mouse)
                            onPositionChanged: (mouse) => root.updateRubberBand(listView, mouse)
                            onReleased: (mouse) => root.finishRubberBandPress(listView, mouse)
                            onCanceled: root.cancelRubberBand(false)
                            onWheel: (wheel) => { wheel.accepted = false }
                            onDoubleClicked: (mouse) => root.handleRubberBandDoubleClick(listView, mouse)
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) {
                                    root.handleEmptyViewClick(listView, mouse)
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            parent: contentArea
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: root.viewMode === 0 && tableHeader.visible ? tableHeader.height : 0
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: root.bottomChromeHeight + (root.horizontalScrollActive ? 12 : 0)
                            visible: listView.visible
                            active: listView.moving || listView.flicking || scrollHover.hovered
                            policy: ScrollBar.AsNeeded
                            z: 10
                            onPressedChanged: root.handleScrollbarPressedChanged(listView, pressed)
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
                           : Math.max(160, Math.floor(width / 2))
                cellHeight: root.briefRowHeight
                model: root.fileViewsModelEnabled && root.viewMode === 2 && !root.virtualRootMode ? root.controller.directoryModel : null
                currentIndex: -1
                focus: root.active && root.viewMode === 2

                onActiveFocusChanged: {
                    if (activeFocus && count > 0) {
                        root.queueCurrentIndexEnsure()
                    }
                }
                onCurrentIndexChanged: {
                    if (root.pendingCurrentIndexInit && root.viewCurrentIndexInvalid(briefView, count)) {
                        root.queueCurrentIndexEnsure()
                        return
                    }
                    root.updateCurrentItemPath(currentIndex)
                    if (activeFocus && currentIndex >= 0 && currentIndex < count) {
                        if (!root.disableSelectionOnCurrentIndexChanged) {
                            root.controller.directoryModel.selectOnly(currentIndex)
                        }
                        if (root.shouldAutoPositionCurrentIndex()) {
                            positionViewAtIndex(currentIndex, GridView.Contain)
                        }
                    }
                }
                onCountChanged: {
                    if (count > 0 && (root.viewCurrentIndexInvalid(briefView, count)
                                      || root.pendingCurrentIndexInit)) {
                        root.queueCurrentIndexEnsure()
                    }
                }
                cacheBuffer: root.activeViewCacheBuffer
                reuseItems: root.fileViewsReuseEnabled && root.canEnableFileViewsReuse(briefView)
                onMovementStarted: root.armFileViewsReuseForUserScroll(briefView, "movement-start")
                onFlickStarted: root.armFileViewsReuseForUserScroll(briefView, "flick-start")
                onMovementEnded: root.scheduleFileViewsReuseDisable("movement-end")
                onFlickEnded: root.scheduleFileViewsReuseDisable("flick-end")
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                interactive: !root.resizeOptimized
                onMovingChanged:  root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                bottomMargin: root.bottomChromeHeight

                add: null
                remove: null

                Keys.onPressed: (event) => {
                    if (root.panelKeysBlockedByOverlay()) {
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
                        if (currentIndex >= 0 && currentIndex < count) {
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
                    if (currentIndex === -1 && count > 0 &&
                        (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                        currentIndex = (event.key === Qt.Key_Up) ? count - 1 : 0
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
                        root.cancelRubberBand(true)
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
                        scrolling: root.hoverSuppressed
                        resizeOptimized: root.lightweightDelegates

                        onClicked: (mouse) => root.handleItemClick(index, mouse)
                        onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                        onDoubleClicked: root.controller.openItem(index)
                    }
                }

                // Empty area handling
                MouseArea {
                    anchors.fill: parent
                    z: 8
                    enabled: root.emptyAreaInputEnabled()
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    scrollGestureEnabled: false
                    onPressed: (mouse) => root.beginRubberBandPress(briefView, mouse)
                    onPositionChanged: (mouse) => root.updateRubberBand(briefView, mouse)
                    onReleased: (mouse) => root.finishRubberBandPress(briefView, mouse)
                    onCanceled: root.cancelRubberBand(false)
                    onWheel: (wheel) => { wheel.accepted = false }
                    onDoubleClicked: (mouse) => root.handleRubberBandDoubleClick(briefView, mouse)
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            root.handleEmptyViewClick(briefView, mouse)
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    parent: contentArea
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.bottomChromeHeight
                    visible: briefView.visible
                    active: briefView.moving || briefView.flicking || briefScrollHover.hovered
                    policy: ScrollBar.AsNeeded
                    z: 10
                    onPressedChanged: root.handleScrollbarPressedChanged(briefView, pressed)
                    HoverHandler { id: briefScrollHover }
                }
            }

            GridView {
                id: gridView
                anchors.fill: parent
                anchors.margins: 10
                anchors.bottomMargin: root.viewMode === 1 ? root.bottomChromeHeight + 12 : 10
                visible: root.viewMode === 1 && !root.virtualRootMode
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
                interactive: !root.resizeOptimized
                cellWidth: root.resizeOptimized && root.resizeFrozenGridCellWidth > 0
                           ? root.resizeFrozenGridCellWidth
                           : root.gridCellWidth
                cellHeight: root.gridCellHeight
                model: root.fileViewsModelEnabled && root.viewMode === 1 && !root.virtualRootMode ? root.controller.directoryModel : null
                currentIndex: -1
                focus: root.active && root.viewMode === 1

                onActiveFocusChanged: {
                    if (activeFocus && count > 0) {
                        root.queueCurrentIndexEnsure()
                    }
                }
                onCurrentIndexChanged: {
                    if (root.pendingCurrentIndexInit && root.viewCurrentIndexInvalid(gridView, count)) {
                        root.queueCurrentIndexEnsure()
                        return
                    }
                    root.updateCurrentItemPath(currentIndex)
                    if (activeFocus && currentIndex >= 0 && currentIndex < count) {
                        if (!root.disableSelectionOnCurrentIndexChanged) {
                            root.controller.directoryModel.selectOnly(currentIndex)
                        }
                        if (root.shouldAutoPositionCurrentIndex()) {
                            positionViewAtIndex(currentIndex, GridView.Contain)
                        }
                    }
                }
                onCountChanged: {
                    if (count > 0 && (root.viewCurrentIndexInvalid(gridView, count)
                                      || root.pendingCurrentIndexInit)) {
                        root.queueCurrentIndexEnsure()
                    }
                }
                cacheBuffer: root.activeViewCacheBuffer
                reuseItems: root.fileViewsReuseEnabled && root.canEnableFileViewsReuse(gridView)
                onMovementStarted: root.armFileViewsReuseForUserScroll(gridView, "movement-start")
                onFlickStarted: root.armFileViewsReuseForUserScroll(gridView, "flick-start")
                onMovementEnded: root.scheduleFileViewsReuseDisable("movement-end")
                onFlickEnded: root.scheduleFileViewsReuseDisable("flick-end")
                onMovingChanged: root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                
                highlight: null
                highlightFollowsCurrentItem: false

                add: null
                remove: null

                Keys.onPressed: (event) => {
                    if (root.panelKeysBlockedByOverlay()) {
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
                        if (currentIndex >= 0 && currentIndex < count) {
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
                    if (currentIndex === -1 && count > 0 &&
                        (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                        currentIndex = (event.key === Qt.Key_Up) ? count - 1 : 0
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
                        root.cancelRubberBand(true)
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
                               : ((hoverGrid.hovered && !root.hoverSuppressed) ? Theme.itemHoverFill : "transparent")
                        border.color: isSelected
                                      ? (root.active ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                                      : (currentItem ? Theme.withAlpha(Theme.focusRing, root.active ? 0.82 : 0.38) : "transparent")
                        border.width: isSelected || currentItem ? 1 : 0
                        transform: Translate { y: gridDelegate.visualOffsetY }
                    }

                    HoverHandler { 
                        id: hoverGrid 
                        enabled: !gridDelegate.lightweightActive
                        onHoveredChanged: {
                            if (root.hoverSuppressed) return
                            if (hovered) {
                                root.controller.hoveredPath = path
                            } else if (root.controller.hoveredPath === path) {
                                root.controller.hoveredPath = ""
                            }
                        }
                    }

                    Connections {
                        target: root
                        function onHoverSuppressedChanged() {
                            if (!root.hoverSuppressed && !gridDelegate.lightweightActive) {
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
                            visible: (!gridDelegate.thumbnailRequestActive || thumbnail.status !== Image.Ready)
                                     && (!root.useNativeIcons || gridNativeIcon.status !== Image.Ready)
                            opacity: isImage ? 0.72 : 1.0
                            smooth: true
                            mipmap: false
                            asynchronous: false
                        }

                        Image {
                            id: gridNativeIcon
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

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "#ffffff"
                            visible: gridDelegate.thumbnailRequestActive
                                     && String(suffix || "").toLowerCase() === "pdf"
                                     && thumbnail.status === Image.Ready
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
                        scrollGestureEnabled: false
                        onWheel: (wheel) => { wheel.accepted = false }
                        
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
                    z: 8
                    enabled: root.emptyAreaInputEnabled()
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    scrollGestureEnabled: false
                    onPressed: (mouse) => root.beginRubberBandPress(gridView, mouse)
                    onPositionChanged: (mouse) => root.updateRubberBand(gridView, mouse)
                    onReleased: (mouse) => root.finishRubberBandPress(gridView, mouse)
                    onCanceled: root.cancelRubberBand(false)
                    onWheel: (wheel) => { wheel.accepted = false }
                    onDoubleClicked: (mouse) => root.handleRubberBandDoubleClick(gridView, mouse)
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            root.handleEmptyViewClick(gridView, mouse)
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    parent: contentArea
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.bottomChromeHeight
                    visible: gridView.visible
                    active: gridView.moving || gridView.flicking || gridScrollHover.hovered
                    policy: ScrollBar.AsNeeded
                    z: 10
                    onPressedChanged: root.handleScrollbarPressedChanged(gridView, pressed)
                    HoverHandler { id: gridScrollHover }
                }
            }

            // ── Storage View (This PC / devices://) ──────────────────────────
            Item {
                id: gridEdgeRubberBandAreas
                anchors.fill: parent
                z: 7
                visible: root.viewMode === 1 && !root.virtualRootMode
                enabled: visible && !root.controller.directoryModel.loading

                function contentPoint(area, mouse) {
                    return area.mapToItem(contentArea, mouse.x, mouse.y)
                }

                function begin(area, mouse) {
                    if (mouse.button === Qt.RightButton) {
                        return
                    }
                    const point = contentPoint(area, mouse)
                    root.beginRubberBandContentPress(gridView, mouse, point.x, point.y)
                }

                function update(area, mouse) {
                    const point = contentPoint(area, mouse)
                    root.updateRubberBandContentPress(gridView, point.x, point.y)
                }

                function finish(area, mouse) {
                    const point = contentPoint(area, mouse)
                    root.finishRubberBandContentPress(gridView, mouse, point.x, point.y)
                }

                function popupEmptyMenu(area, mouse) {
                    const point = contentPoint(area, mouse)
                    if (point.y < contentArea.height - root.bottomChromeHeight) {
                        root.handleEmptyViewClick(gridView, mouse)
                    }
                }

                MouseArea {
                    id: gridTopRubberBandEdge
                    x: 0
                    y: 0
                    width: parent.width
                    height: Math.max(0, gridView.y)
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    scrollGestureEnabled: false
                    onPressed: (mouse) => gridEdgeRubberBandAreas.begin(gridTopRubberBandEdge, mouse)
                    onPositionChanged: (mouse) => gridEdgeRubberBandAreas.update(gridTopRubberBandEdge, mouse)
                    onReleased: (mouse) => gridEdgeRubberBandAreas.finish(gridTopRubberBandEdge, mouse)
                    onCanceled: root.cancelRubberBand(false)
                    onWheel: (wheel) => { wheel.accepted = false }
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            gridEdgeRubberBandAreas.popupEmptyMenu(gridTopRubberBandEdge, mouse)
                        }
                    }
                }

                MouseArea {
                    id: gridLeftRubberBandEdge
                    x: 0
                    y: gridView.y
                    width: Math.max(0, gridView.x)
                    height: gridView.height
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    scrollGestureEnabled: false
                    onPressed: (mouse) => gridEdgeRubberBandAreas.begin(gridLeftRubberBandEdge, mouse)
                    onPositionChanged: (mouse) => gridEdgeRubberBandAreas.update(gridLeftRubberBandEdge, mouse)
                    onReleased: (mouse) => gridEdgeRubberBandAreas.finish(gridLeftRubberBandEdge, mouse)
                    onCanceled: root.cancelRubberBand(false)
                    onWheel: (wheel) => { wheel.accepted = false }
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            gridEdgeRubberBandAreas.popupEmptyMenu(gridLeftRubberBandEdge, mouse)
                        }
                    }
                }

                MouseArea {
                    id: gridRightRubberBandEdge
                    x: gridView.x + gridView.width
                    y: gridView.y
                    width: Math.max(0, parent.width - x)
                    height: gridView.height
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    scrollGestureEnabled: false
                    onPressed: (mouse) => gridEdgeRubberBandAreas.begin(gridRightRubberBandEdge, mouse)
                    onPositionChanged: (mouse) => gridEdgeRubberBandAreas.update(gridRightRubberBandEdge, mouse)
                    onReleased: (mouse) => gridEdgeRubberBandAreas.finish(gridRightRubberBandEdge, mouse)
                    onCanceled: root.cancelRubberBand(false)
                    onWheel: (wheel) => { wheel.accepted = false }
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            gridEdgeRubberBandAreas.popupEmptyMenu(gridRightRubberBandEdge, mouse)
                        }
                    }
                }

                MouseArea {
                    id: gridBottomRubberBandEdge
                    x: 0
                    y: gridView.y + gridView.height
                    width: parent.width
                    height: Math.max(0, contentArea.height - root.bottomChromeHeight - y)
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    scrollGestureEnabled: false
                    onPressed: (mouse) => gridEdgeRubberBandAreas.begin(gridBottomRubberBandEdge, mouse)
                    onPositionChanged: (mouse) => gridEdgeRubberBandAreas.update(gridBottomRubberBandEdge, mouse)
                    onReleased: (mouse) => gridEdgeRubberBandAreas.finish(gridBottomRubberBandEdge, mouse)
                    onCanceled: root.cancelRubberBand(false)
                    onWheel: (wheel) => { wheel.accepted = false }
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            gridEdgeRubberBandAreas.popupEmptyMenu(gridBottomRubberBandEdge, mouse)
                        }
                    }
                }
            }

            Rectangle {
                id: rubberBandOverlay
                x: root.rubberBandOverlayLeft
                y: root.rubberBandOverlayTop
                width: root.rubberBandOverlayWidth
                height: root.rubberBandOverlayHeight
                z: 9
                visible: root.rubberBandActive
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.18 : 0.12)
                border.color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.72 : 0.58)
                border.width: 1
            }

            StorageView {
                id: storageView
                anchors.fill: parent
                anchors.bottomMargin: root.bottomChromeHeight
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
                anchors.bottomMargin: root.bottomChromeHeight
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
                anchors.bottomMargin: root.bottomChromeHeight + 10
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

            FilePanelSelectionActions {
                id: selectionActionsBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: root.footerHeight
                height: root.selectionActionsHeight
                visible: root.selectionActionsVisible
                z: 17
                controller: root.controller
                workspaceController: root.workspaceController
                favoritesController: root.favoritesBackend
                active: root.active
                invertSelectionActive: root.invertSelectionActive
                canInvertSelection: root.canInvertSelection
                resizeOptimized: root.resizeOptimized
                ultraLightMode: root.ultraLightMode
                onCopyRequested: {
                    root.activated()
                    if (root.workspaceController) {
                        root.workspaceController.copyToClipboard()
                    }
                }
                onCopyToOtherPanelRequested: {
                    root.activated()
                    if (root.workspaceController) {
                        root.workspaceController.copyActiveSelectionToOpposite()
                    }
                }
                onMoveRequested: {
                    root.activated()
                    if (root.workspaceController) {
                        root.workspaceController.moveActiveSelectionToOpposite()
                    }
                }
                onRenameRequested: {
                    root.activated()
                    root.startRename()
                }
                onDeleteRequested: {
                    root.activated()
                    if (root.workspaceController) {
                        root.workspaceController.requestDelete(root.controller.selectedPaths(), root.controller.currentPath)
                    }
                }
                onPinToggleRequested: (paths, allPinned) => {
                    root.activated()
                    if (!root.favoritesBackend || !paths || paths.length === 0) {
                        return
                    }
                    const changed = allPinned
                                  ? root.favoritesBackend.unpinPaths(paths)
                                  : root.favoritesBackend.pinPaths(paths)
                    const window = root.Window.window
                    if (window && window.showTransientInfo) {
                        if (changed > 0) {
                            window.showTransientInfo(changed + (changed === 1
                                ? (allPinned ? " item unpinned from Favorites" : " item pinned to Favorites")
                                : (allPinned ? " items unpinned from Favorites" : " items pinned to Favorites")))
                        } else {
                            window.showTransientInfo(allPinned
                                ? "Selection was not pinned to Favorites"
                                : "Selection is already pinned to Favorites")
                        }
                    }
                }
                onPropertiesRequested: {
                    root.activated()
                    const window = root.Window.window
                    if (window && window.showActiveProperties) {
                        window.showActiveProperties()
                    }
                }
                onClearSelectionRequested: {
                    root.clearSelection()
                }
                onInvertSelectionRequested: {
                    root.activated()
                    root.toggleInvertSelection()
                }
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
                root.loadingRailReady = false
                if (root.scrolling) {
                    scrollStopTimer.restart()
                }
            }
        }
    }
}

}


