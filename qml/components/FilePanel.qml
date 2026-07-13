import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window
import FM
import "../style"
import "common"
import "filepanel"

Pane {
    id: root

    required property var controller
    required property var workspaceController
    property int panelSide: -1
    property var dragCoordinator: null
    property var oppositePanelItem: null
    property bool limitedDragNDropEnabled: false
    property var propertiesController
    property var quickLookPopup
    property var quickLookController
    property bool active: false
    property bool liveResizeActive: false
    property bool externalScrollActive: false
    property int externalScrollSuppressFileCountThreshold: 5
    property bool externalScrollOptimizationEnabled: false
    property int externalScrollFileCountThreshold: 96
    signal detailsVisualStateChanged()
    readonly property bool showActiveHighlight: root.active && root.workspaceController.splitEnabled
    readonly property bool internalDragEnabled: root.limitedDragNDropEnabled
                                                && Boolean(root.dragCoordinator)
                                                && root.workspaceController
                                                && root.workspaceController.splitEnabled
    property int hoverDragCursorShape: Qt.ArrowCursor
    readonly property real activePanelFillAlpha: themeController.isDark ? 0.075 : 0.105
    readonly property real activePanelStrokeOpacity: themeController.isDark ? 0.88 : 0.96
    readonly property real activePanelIndicatorOpacity: themeController.isDark ? 0.86 : 0.94
    readonly property int viewMode: root.controller.viewMode
    readonly property bool virtualRootMode: root.controller.isDeviceRoot || root.controller.isFavoritesRoot
    readonly property var favoritesBackend: typeof favoritesController !== "undefined" ? favoritesController : null
    readonly property bool containsActiveFocus: root.activeFocus
                                              || listView.activeFocus
                                              || gridView.activeFocus
                                              || briefView.activeFocus
                                              || storageView.activeFocus
                                              || favoritesView.activeFocus
                                              || root.isRenaming
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
    property int briefRowHeight: Math.max(Theme.controlHeight - 10, Theme.fontSizeLabel + 16)
    readonly property int briefRowMinHeight: Math.max(22, Theme.fontSizeLabel + 14)
    readonly property int briefRowMaxHeight: 64
    readonly property int footerHeight: Math.max(34, Theme.controlHeight - 2)
    readonly property int panelToolbarHeight: Math.max(42, Theme.controlHeight + 4)
    readonly property int panelToolbarDividerHeight: 1
    readonly property int topChromeHeight: root.panelToolbarHeight + root.panelToolbarDividerHeight
    property bool showActionBar: true
    property bool showSelectionBadges: true
    property bool showHoverPreviews: false
    property rect hoverPreviewAnchorRect: Qt.rect(width - 24, root.topChromeHeight + 12, 1, 1)
    property bool isRenaming: false
    readonly property int selectionActionsHeight: Math.max(44, Theme.controlHeight + 6)
    property bool selectionActionsVisible: false
    property bool rubberBandReserveSelectionActions: false
    readonly property int selectionActionsReservedHeight: (root.selectionActionsVisible || root.rubberBandReserveSelectionActions) ? root.selectionActionsHeight : 0
    readonly property int bottomChromeHeight: root.footerHeight + root.selectionActionsReservedHeight
    readonly property bool errorBannerVisible: errorBanner.visible
    readonly property int errorBannerHeight: errorBanner.visible ? errorBanner.height : 0
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useHighQualitySystemIcons : true
    readonly property bool showThumbnails: typeof appSettings !== "undefined" && appSettings ? appSettings.showThumbnails : true
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings ? appSettings.ultraLightMode : false
    readonly property string currentPathKind: root.controller && root.controller.currentPath
                                              ? root.controller.pathKindFor(root.controller.currentPath)
                                              : "local"
    readonly property bool isCurrentPathRemote: root.currentPathKind === "remote"
                                                || root.currentPathKind === "ftp"
                                                || root.currentPathKind === "gdrive"
                                                || root.currentPathKind === "mega"
                                                || root.currentPathKind === "portable"
    readonly property bool effectiveUseNativeIcons: root.useNativeIcons
    readonly property bool effectiveShowThumbnails: root.showThumbnails && !root.ultraLightMode
    readonly property bool loadingDirectory: Boolean(root.controller
                                                     && ((root.controller.navigationPending === true)
                                                         || (root.controller.directoryModel
                                                             && root.controller.directoryModel.loading)))
    readonly property bool archiveProgressLoading: Boolean(root.isCurrentPathArchive
                                                           && root.controller
                                                           && root.controller.directoryModel
                                                           && root.controller.directoryModel.loading
                                                           && root.controller.directoryModel.scanProgress >= 0)
    readonly property real horizontalScrollX: horizontalFlick ? horizontalFlick.contentX : 0
    readonly property bool horizontalScrollActive: root.viewMode === 0 && horizontalFlick && horizontalFlick.contentWidth > horizontalFlick.width
    property bool loadingRailReady: false
    readonly property bool showLoadingRail: root.loadingDirectory && (root.loadingRailReady || root.archiveProgressLoading)
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
        filePanelLoadingPolicy.handleArchiveModeChanged()
    }
    onCanInvertSelectionChanged: if (!root.canInvertSelection) root.invertSelectionActive = false
    onShowActionBarChanged: updateSelectionActionsVisible()
    onActiveChanged: {
        root.traceRenameFocus("panel-active-changed", "value=" + root.active)
        updateSelectionActionsVisible()
        if (!root.active) {
            root.disableFileViewsReuse("inactive")
            root.cancelRubberBand(false)
            root.cancelActiveInlineRename()
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
        root.traceRenameFocus("panel-isRenaming-changed", "value=" + root.isRenaming)
        updateSelectionActionsVisible()
        root.disableFileViewsReuse(root.isRenaming ? "rename-start" : "rename-end")
        if (root.isRenaming) {
            root.cancelRubberBand(false)
        }
    }
    onInvertSelectionActiveChanged: updateSelectionActionsVisible()

    function updateDirectoryLoadingState() {
        filePanelLoadingPolicy.updateDirectoryLoadingState()
    }

    property alias scrolling: filePanelScrollCoordinator.scrolling
    property alias hoverSuppressed: filePanelHoverCoordinator.suppressed
    property bool fileViewPreviewScrollActive: false
    readonly property bool previewScrollActive: root.fileViewPreviewScrollActive
                                                || (favoritesView && favoritesView.previewScrollActive)
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
    property alias pendingScrollRestorePath: filePanelScrollCoordinator.pendingRestorePath
    property alias pendingScrollRestoreY: filePanelScrollCoordinator.pendingRestoreY
    property alias pendingScrollRestoreAttempts: filePanelScrollCoordinator.pendingRestoreAttempts
    property alias pendingScrollRestoreEnabled: filePanelScrollCoordinator.pendingRestoreEnabled
    property alias pendingRevealPath: filePanelCurrentIndexCoordinator.pendingRevealPath
    property alias pendingRevealAttempts: filePanelCurrentIndexCoordinator.pendingRevealAttempts
    property string targetSelectPath: ""
    property string pendingNavigationCommitPath: ""
    property alias createRenameSessionId: filePanelRenameCoordinator.createSessionId
    property alias createRenamePath: filePanelRenameCoordinator.createPath
    property alias createRenameAttempts: filePanelRenameCoordinator.createAttempts
    property alias createRenameRevealReady: filePanelRenameCoordinator.createRevealReady
    property alias createRenameStarted: filePanelRenameCoordinator.createStarted
    property alias pendingRenameFocusPath: filePanelRenameCoordinator.pendingFocusPath
    property alias pendingRenameFocusAttempts: filePanelRenameCoordinator.pendingFocusAttempts
    property alias pendingRenameFocusSelectText: filePanelRenameCoordinator.pendingFocusSelectText
    property alias pendingCurrentIndexInit: filePanelCurrentIndexCoordinator.pendingInit
    property bool fileViewsModelEnabled: true
    property bool fileViewsReuseEnabled: false
    property bool fileViewsReuseArmedByUserScroll: false
    property var fileViewsReuseArmedView: null
    property string fileViewsReuseArmReason: ""
    property bool fileViewsReuseScrollbarPressed: false
    property bool keyboardNavigationActive: false
    property int fileViewsNavigationGeneration: 0
    property int lastViewMode: -1
    property alias currentIndexEnsureAttempts: filePanelCurrentIndexCoordinator.ensureAttempts
    property alias pendingInlineRenamePath: filePanelRenameCoordinator.pendingInlinePath
    property alias disableSelectionOnCurrentIndexChanged: filePanelCurrentIndexCoordinator.disableSelectionOnChange
    property alias suppressCurrentIndexAutoPosition: filePanelCurrentIndexCoordinator.suppressAutoPosition
    property bool pendingAutoNameColumnWidthUpdate: false
    property real resizeFrozenListWidth: 0
    property real resizeFrozenBriefCellWidth: 0
    property real resizeFrozenGridCellWidth: 0
    property bool showPanelBreadcrumbs: true
    readonly property int selectionDragThreshold: 10
    readonly property bool startupLazyPanelMenus: true
    property var filePanelContextMenuItem: null
    property var filePanelEmptyMenuItem: null
    property alias contextMenuOpen: filePanelContextMenuCoordinator.open
    property alias pendingContextMenuRequest: filePanelContextMenuCoordinator.pendingRequest
    property alias pendingHoverClearPath: filePanelHoverCoordinator.pendingClearPath
    property alias scrollStopTimer: filePanelScrollCoordinator.stopTimer
    property alias scrollRestoreTimer: filePanelScrollCoordinator.restoreTimer
    property alias hoverSuppressTimer: filePanelHoverCoordinator.suppressTimer
    property alias pendingHoverClearTimer: filePanelHoverCoordinator.clearTimer
    property alias currentIndexEnsureTimer: filePanelCurrentIndexCoordinator.ensureTimer
    property alias pendingRevealTimer: filePanelCurrentIndexCoordinator.revealTimer
    property alias createRenameTimer: filePanelRenameCoordinator.createTimer
    property alias renameFocusTimer: filePanelRenameCoordinator.focusTimer
    property alias contextMenuPopupDelayTimer: filePanelContextMenuCoordinator.popupDelayTimer
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property int externalScrollFileCount: root.controller && root.controller.directoryModel
                                                   ? root.controller.directoryModel.count
                                                   : 0
    readonly property bool externalScrollSuppressActive: root.externalScrollActive
                                                         && root.active
                                                         && root.externalScrollFileCount >= root.externalScrollSuppressFileCountThreshold
    readonly property bool externalScrollOptimizationActive: root.externalScrollOptimizationEnabled
                                                             && root.externalScrollActive
                                                             && root.active
    readonly property bool externalScrollAnySuppressionActive: root.externalScrollSuppressActive || root.externalScrollOptimizationActive
    readonly property bool placesTraceEnabled: Qt.application.arguments.indexOf("--places-trace") >= 0
    readonly property bool scrollTraceEnabled: Qt.application.arguments.indexOf("--scroll-trace") >= 0
    readonly property bool rubberTraceEnabled: Qt.application.arguments.indexOf("--rubber-trace") >= 0
    readonly property bool panelScrollActive: root.scrolling && root.active
    readonly property bool thumbnailSchedulingPaused: root.panelScrollActive || (root.externalScrollActive && root.active)
    readonly property bool thumbnailLoadingPaused: root.resizeOptimized || root.panelScrollActive || (root.externalScrollActive && root.active) || root.ultraLightMode
    readonly property bool effectsReduced: root.resizeOptimized || root.ultraLightMode
    readonly property bool lightweightDelegates: root.resizeOptimized || root.ultraLightMode
    readonly property int activeViewCacheBuffer: root.effectsReduced || root.externalScrollAnySuppressionActive || root.loadingDirectory ? 0 : 1600
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
    onExternalScrollAnySuppressionActiveChanged: {
        if (root.placesTraceEnabled && root.externalScrollOptimizationEnabled && root.externalScrollActive) {
            console.log("[PlacesTrace][FilePanel] externalScrollSuppression active="
                        + root.externalScrollAnySuppressionActive
                        + " optimization=" + root.externalScrollOptimizationActive
                        + " thumbnailPause=" + root.thumbnailLoadingPaused
                        + " enabled=" + root.externalScrollOptimizationEnabled
                        + " activePanel=" + root.active
                        + " external=" + root.externalScrollActive
                        + " count=" + root.externalScrollFileCount
                        + " panel=" + root.panelSide)
        }
        if (root.externalScrollAnySuppressionActive) {
            hoverSuppressTimer.stop()
            root.hoverSuppressed = true
            root.clearHoveredItem()
        } else if (!hoverSuppressTimer.running) {
            hoverSuppressTimer.restart()
        }
    }
    onThumbnailLoadingPausedChanged: {
        if (root.placesTraceEnabled && root.active && (root.externalScrollActive || root.panelScrollActive)) {
            console.log("[PlacesTrace][FilePanel] thumbnailLoadingPaused="
                        + root.thumbnailLoadingPaused
                        + " panelScroll=" + root.panelScrollActive
                        + " external=" + root.externalScrollActive
                        + " activePanel=" + root.active
                        + " count=" + root.externalScrollFileCount
                        + " panel=" + root.panelSide)
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

    readonly property real detailsAvailableWidth: Math.max(0, (contentArea ? contentArea.width : 500) - 24)
    readonly property var detailsEffectiveLayout: filePanelDetailsPolicy.fitDetailsColumns(root.detailsAvailableWidth)
    readonly property real effectiveColWidthName: effectiveDetailColumnWidth("Name")
    readonly property real effectiveColWidthSize: effectiveDetailColumnWidth("Size")
    readonly property real effectiveColWidthType: effectiveDetailColumnWidth("Type")
    readonly property real effectiveColWidthDate: effectiveDetailColumnWidth("Date")
    readonly property real effectiveColWidthDateCreated: effectiveDetailColumnWidth("DateCreated")
    readonly property real effectiveColWidthExtension: effectiveDetailColumnWidth("Extension")
    readonly property real effectiveColWidthAttributes: effectiveDetailColumnWidth("Attributes")
    readonly property real effectiveColWidthResolution: effectiveDetailColumnWidth("Resolution")
    readonly property real effectiveColWidthDuration: effectiveDetailColumnWidth("Duration")
    readonly property real effectiveColWidthArtist: effectiveDetailColumnWidth("Artist")
    readonly property real effectiveColWidthAlbum: effectiveDetailColumnWidth("Album")
    readonly property real effectiveColWidthBitrate: effectiveDetailColumnWidth("Bitrate")
    readonly property bool effectiveColShowSize: effectiveDetailColumnVisible("Size")
    readonly property bool effectiveColShowType: effectiveDetailColumnVisible("Type")
    readonly property bool effectiveColShowDate: effectiveDetailColumnVisible("Date")
    readonly property bool effectiveColShowDateCreated: effectiveDetailColumnVisible("DateCreated")
    readonly property bool effectiveColShowExtension: effectiveDetailColumnVisible("Extension")
    readonly property bool effectiveColShowAttributes: effectiveDetailColumnVisible("Attributes")
    readonly property bool effectiveColShowResolution: effectiveDetailColumnVisible("Resolution")
    readonly property bool effectiveColShowDuration: effectiveDetailColumnVisible("Duration")
    readonly property bool effectiveColShowArtist: effectiveDetailColumnVisible("Artist")
    readonly property bool effectiveColShowAlbum: effectiveDetailColumnVisible("Album")
    readonly property bool effectiveColShowBitrate: effectiveDetailColumnVisible("Bitrate")

    function effectiveDetailColumnWidth(column) {
        const layout = root.detailsEffectiveLayout
        if (!layout || !layout.widths || layout.widths[column] === undefined) {
            return 0
        }
        return layout.widths[column]
    }

    function effectiveDetailColumnVisible(column) {
        if (column === "Name") {
            return true
        }
        const layout = root.detailsEffectiveLayout
        return Boolean(layout && layout.visible && layout.visible[column])
    }

    function captureEffectiveDetailColumnWidths() {
        colWidthName = effectiveColWidthName > 0 ? effectiveColWidthName : colWidthName
        preferredColWidthName = colWidthName
        colWidthSize = effectiveColWidthSize > 0 ? effectiveColWidthSize : colWidthSize
        colWidthType = effectiveColWidthType > 0 ? effectiveColWidthType : colWidthType
        colWidthDate = effectiveColWidthDate > 0 ? effectiveColWidthDate : colWidthDate
        colWidthDateCreated = effectiveColWidthDateCreated > 0 ? effectiveColWidthDateCreated : colWidthDateCreated
        colWidthExtension = effectiveColWidthExtension > 0 ? effectiveColWidthExtension : colWidthExtension
        colWidthAttributes = effectiveColWidthAttributes > 0 ? effectiveColWidthAttributes : colWidthAttributes
        colWidthResolution = effectiveColWidthResolution > 0 ? effectiveColWidthResolution : colWidthResolution
        colWidthDuration = effectiveColWidthDuration > 0 ? effectiveColWidthDuration : colWidthDuration
        colWidthArtist = effectiveColWidthArtist > 0 ? effectiveColWidthArtist : colWidthArtist
        colWidthAlbum = effectiveColWidthAlbum > 0 ? effectiveColWidthAlbum : colWidthAlbum
        colWidthBitrate = effectiveColWidthBitrate > 0 ? effectiveColWidthBitrate : colWidthBitrate
    }

    function columnPreferredWidth(column) {
        return filePanelDetailsPolicy.columnPreferredWidth(column)
    }

    function columnMinWidth(column) {
        return filePanelDetailsPolicy.columnMinWidth(column)
    }

    function visibleDetailColumns() {
        return filePanelDetailsPolicy.visibleDetailColumns()
    }

    function setDetailColumnWidth(column, width) {
        filePanelDetailsPolicy.setDetailColumnWidth(column, width)
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
        let w = effectiveColWidthName
        if (effectiveColShowSize) w += effectiveColWidthSize
        if (effectiveColShowType) w += effectiveColWidthType
        if (effectiveColShowDate) w += effectiveColWidthDate
        if (effectiveColShowDateCreated) w += effectiveColWidthDateCreated
        if (effectiveColShowExtension) w += effectiveColWidthExtension
        if (effectiveColShowAttributes) w += effectiveColWidthAttributes
        if (effectiveColShowResolution) w += effectiveColWidthResolution
        if (effectiveColShowDuration) w += effectiveColWidthDuration
        if (effectiveColShowArtist) w += effectiveColWidthArtist
        if (effectiveColShowAlbum) w += effectiveColWidthAlbum
        if (effectiveColShowBitrate) w += effectiveColWidthBitrate
        return w + 24 // 12+12 side margins
    }

    function resetColumnsToDefaults() {
        filePanelDetailsPolicy.resetColumnsToDefaults()
    }

    function boolValue(value, fallback) {
        return filePanelDetailsPolicy.boolValue(value, fallback)
    }

    function numberValue(value, fallback) {
        return filePanelDetailsPolicy.numberValue(value, fallback)
    }

    function detailsVisualState() {
        return filePanelDetailsPolicy.detailsVisualState()
    }

    function restoreDetailsVisualState(state) {
        filePanelDetailsPolicy.restoreDetailsVisualState(state)
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
        const available = Math.max(0, (contentArea ? contentArea.width : 500) - 24)
        filePanelDetailsPolicy.updateNameColumnWidth(available, force)
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
        const oldMode = root.lastViewMode
        if (oldMode >= 0 && oldMode !== root.viewMode) {
            root.saveScrollPositionForPathAndMode(root.controller.currentPath, oldMode)
            root.prepareViewModeRestore(oldMode, root.viewMode)
        }
        root.lastViewMode = root.viewMode
        root.cancelRubberBand(false)
        root.disableFileViewsReuse()
        updateNameColumnWidth()
    }

    Component.onCompleted: {
        root.lastViewMode = root.viewMode
        updateNameColumnWidth()
        updateSelectionActionsVisible()
    }

    Timer {
        id: keyboardNavigationTimer
        interval: 120
        repeat: false
        onTriggered: root.keyboardNavigationActive = false
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
        onTriggered: root.fileViewPreviewScrollActive = false
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
        id: fileViewsLayoutSyncTimer
        interval: 0
        repeat: false
        onTriggered: root.syncActiveFileViewLayout()
    }

    FilePanelCurrentIndexCoordinator {
        id: filePanelCurrentIndexCoordinator
        panel: root
    }

    FilePanelScrollCoordinator {
        id: filePanelScrollCoordinator
        panel: root
    }

    FilePanelRenameCoordinator {
        id: filePanelRenameCoordinator
        panel: root
    }

    FilePanelHoverCoordinator {
        id: filePanelHoverCoordinator
        panel: root
    }

    FilePanelContextMenuCoordinator {
        id: filePanelContextMenuCoordinator
        panel: root
    }

    FileViewsReusePolicy {
        id: fileViewsReusePolicy
        activeViewProvider: function() { return root.activeView() }
        overlayBlockedProvider: function() { return root.panelKeysBlockedByOverlay() }
        directoryModel: root.controller ? root.controller.directoryModel : null
        virtualRootMode: root.virtualRootMode
        fileViewsModelEnabled: root.fileViewsModelEnabled
        loadingDirectory: root.loadingDirectory
        resizeOptimized: root.resizeOptimized
        isRenaming: root.isRenaming
        rubberBandPressed: root.rubberBandPressed
        rubberBandActive: root.rubberBandActive
        pendingCurrentIndexInit: root.pendingCurrentIndexInit
        pendingScrollRestoreEnabled: root.pendingScrollRestoreEnabled
        pendingScrollRestorePath: root.pendingScrollRestorePath
        reuseArmedByUserScroll: root.fileViewsReuseArmedByUserScroll
        reuseArmedView: root.fileViewsReuseArmedView
        reuseScrollbarPressed: root.fileViewsReuseScrollbarPressed
    }

    FilePanelPathPolicy {
        id: filePanelPathPolicy
    }

    FilePanelDetailsPolicy {
        id: filePanelDetailsPolicy
        panel: root
    }

    FilePanelSelectionPolicy {
        id: filePanelSelectionPolicy
        directoryModel: root.controller ? root.controller.directoryModel : null
    }

    FilePanelRubberBandPolicy {
        id: filePanelRubberBandPolicy
        listRowHeight: Theme.rowHeight
    }

    FilePanelInlineRenamePolicy {
        id: filePanelInlineRenamePolicy
        panel: root
        createRenameTimerRef: createRenameTimer
        renameFocusTimerRef: renameFocusTimer
        currentIndexEnsureTimerRef: currentIndexEnsureTimer
        windowProvider: function() { return root.Window.window }
        viewRegistry: filePanelViewRegistry
    }

    FilePanelIconPolicy {
        id: filePanelIconPolicy
        useNativeIcons: root.effectiveUseNativeIcons
        useHighQualitySystemIcons: root.useHighQualitySystemIcons
    }

    FilePanelKeyboardPolicy {
        id: filePanelKeyboardPolicy
        panel: root
        directoryModel: root.controller ? root.controller.directoryModel : null
    }

    FilePanelLoadingPolicy {
        id: filePanelLoadingPolicy
        panel: root
        loadingRailTimerRef: loadingRailTimer
        scrollStopTimerRef: scrollStopTimer
    }

    FilePanelNavigationPolicy {
        id: filePanelNavigationPolicy
        pendingNavigationCommitPath: root.pendingNavigationCommitPath
        currentPath: root.controller ? root.controller.currentPath : ""
        pendingScrollRestorePath: root.pendingScrollRestorePath
        pendingScrollRestoreEnabled: root.pendingScrollRestoreEnabled
        resizeOptimized: root.resizeOptimized
        samePanelPathProvider: function(a, b) { return root.samePanelPath(a, b) }
    }

    FilePanelScrollState {
        id: filePanelScrollState
        currentMode: root.viewMode
        normalizePathProvider: function(path) { return root.normalizedPanelPath(path) }
    }

    FilePanelScrollRestorePolicy {
        id: filePanelScrollRestorePolicy
        directoryModel: root.controller ? root.controller.directoryModel : null
        targetSelectPath: root.targetSelectPath
        pendingScrollRestoreY: root.pendingScrollRestoreY
    }

    FilePanelStatusMessagePolicy {
        id: filePanelStatusMessagePolicy
        panel: root
        operationQueue: root.workspaceController ? root.workspaceController.operationQueue : null
        controller: root.controller
    }

    FilePanelViewAnchorPolicy {
        id: filePanelViewAnchorPolicy
        directoryModel: root.controller ? root.controller.directoryModel : null
        currentItemPath: root.controller ? root.controller.currentItemPath : ""
        listView: listView
        listRowHeight: Theme.rowHeight
        itemRectProvider: function(view, index) { return root.rubberBandItemRect(view, index) }
    }

    FilePanelViewRegistry {
        id: filePanelViewRegistry
        currentMode: root.viewMode
        listView: listView
        gridView: gridView
        briefView: briefView
    }

    Connections {
        target: root.controller.directoryModel
        function onVisualStructureAboutToChange() {
            root.disableFileViewsReuse()
            root.queueActiveFileViewLayoutSync()
        }
        function onLoadingChanged() {
            root.disableFileViewsReuse()
            root.updateDirectoryLoadingState()
            root.scrollTrace("model-loading-changed")
            if (!root.controller.directoryModel.loading) {
                root.queueActiveFileViewLayoutSync()
            }
            if (!root.controller.directoryModel.loading) {
                root.queuePendingScrollRestore()
            }
        }
        function onCountChanged() {
            root.disableFileViewsReuse()
            root.queueActiveFileViewLayoutSync()
            root.scrollTrace("model-count-changed")
            root.queuePendingScrollRestore()
            root.queuePendingReveal()
        }
        function onSelectionChanged() {
            root.disableFileViewsReuse()
            root.updateSelectionActionsVisible()
            root.rememberScrollPositionForView(root.activeView(), root.viewMode)
        }
    }

    Connections {
        target: root.controller
        function onNavigationPendingChanged() {
            root.updateDirectoryLoadingState()
        }

        function onPathAboutToChange(from, to, preserveScroll) {
            root.traceRenameFocus("controller-pathAboutToChange", "from=" + from + " to=" + to + " preserveScroll=" + preserveScroll)
            root.scrollTrace("path-about-to-change-before", "from=" + from + " to=" + to + " preserve=" + preserveScroll + " loadMore=" + root.isProviderLoadMorePathForCurrent(from, to))
            root.fileViewsNavigationGeneration += 1
            root.saveScrollPositionForPath(from)
            root.disableFileViewsReuse("path-about-to-change")
            root.pendingNavigationCommitPath = to
            root.cancelRubberBand(false)
            root.invertSelectionActive = false
            filePanelInlineRenamePolicy.cancelForPathChange()
            root.pendingCurrentIndexInit = true
            root.currentIndexEnsureAttempts = 0
            root.pendingScrollRestoreEnabled = preserveScroll
            if (!preserveScroll) {
                root.clearPendingScrollRestore()
                root.targetSelectPath = ""
            } else {
                root.prepareScrollRestoreForPath(to)
                root.targetSelectPath = root.findDirectChildPath(to, from)
                const state = filePanelScrollState.state(to)
                if (root.targetSelectPath.length === 0 && state && state.focusedPath) {
                    root.targetSelectPath = state.focusedPath
                }
            }
            root.scrollTrace("path-about-to-change-after", "from=" + from + " to=" + to + " preserve=" + preserveScroll + " loadMore=" + root.isProviderLoadMorePathForCurrent(from, to))
            root.scrolling = true
            root.controller.scrolling = true
            root.suppressHoverBriefly()
            scrollStopTimer.restart()
        }
        function onPathNavigated(path) {
            root.traceRenameFocus("controller-pathNavigated", "path=" + path)
            root.scrollTrace("path-navigated-before", "path=" + path)
            root.restoreFileViewsForGeneration(root.fileViewsNavigationGeneration)
            if (root.active) {
                Qt.callLater(() => {
                    let isSidebarFocused = typeof sidebar !== "undefined" && sidebar && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
                    if (!isSidebarFocused && !root.inlineRenameFocusActive()) {
                        root.focusContent()
                    }

                    if (!root.inlineRenameFocusActive()) {
                        root.queueCurrentIndexEnsure()
                    }
                })
            }
            if (root.pendingScrollRestoreEnabled) {
                root.queueScrollRestoreForPath(path)
                root.pendingScrollRestoreEnabled = false
            }
            root.scrollTrace("path-navigated-after", "path=" + path)
            root.scrolling = true
            root.controller.scrolling = true
            root.suppressHoverBriefly()
            scrollStopTimer.restart()
        }
        function onPathNavigationFailed(path) {
            root.traceRenameFocus("controller-pathNavigationFailed", "path=" + path)
            root.pendingNavigationCommitPath = ""
            root.restoreFileViewsForGeneration(root.fileViewsNavigationGeneration)
            root.pendingCurrentIndexInit = false
            root.currentIndexEnsureAttempts = 0
            root.targetSelectPath = ""
            root.clearPendingScrollRestore()
            scrollStopTimer.restart()
        }
        function onEntryRenamed(oldPath, newPath) {
            filePanelInlineRenamePolicy.handleEntryRenamed(oldPath, newPath)
        }
        function onEntryCreated(path) {
            filePanelInlineRenamePolicy.handleEntryCreated(path)
        }
        function onCreatedEntryRevealRequested(path) {
            filePanelInlineRenamePolicy.handleCreatedEntryRevealRequested(path)
        }

        function onCurrentPathChanged() {
            root.traceRenameFocus("controller-currentPathChanged")
            root.clearNavigationCommitIfArrived()
            root.queuePendingScrollRestore()
            root.queuePendingReveal()
            if (root.active) {
                root.queueCurrentIndexEnsure()
            }
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
        return fileViewsReusePolicy.canArm(view, reason)
    }

    // !!! DANGER: reuseItems is a poisoned performance switch.
    // !!! It exists only to smooth active user scrolling in huge folders.
    // !!! Do not enable it for navigation, delete, refresh, selection, restore,
    // !!! keyboard jumps, context menus, rubber banding, rename, or model changes.
    // !!! Allowed user-scroll sources: movement-start, flick-start, scrollbar-press.
    // !!! Every future enable path must satisfy this single gate.
    function canEnableFileViewsReuse(view) {
        return fileViewsReusePolicy.canEnable(view)
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
                root.clearHoveredItem()
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
        return filePanelViewRegistry.motionActive(root.fileViewsReuseScrollbarPressed)
    }

    function suppressHoverBriefly() {
        root.hoverSuppressed = true
        root.hoverDragCursorShape = Qt.ArrowCursor
        hoverSuppressTimer.restart()
    }

    function queuePendingScrollRestore() {
        if (root.pendingScrollRestorePath.length === 0) {
            return false
        }
        if (root.restorePendingScrollPosition()) {
            return true
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
            root.clearHoveredItem()
            root.hoverDragCursorShape = Qt.ArrowCursor
        }
        scrollStopTimer.restart()
    }

    function setHoveredItem(item, path, localPoint) {
        if (!root.controller || !item || !path || root.hoverSuppressed) {
            return
        }

        if (root.controller.hoveredPath !== path || !filePanelOverlayHost.hoverPreview.visible) {
            const point = localPoint || Qt.point(item.width / 2, item.height / 2)
            const target = filePanelOverlayHost.hoverPreview && filePanelOverlayHost.hoverPreview.parent ? filePanelOverlayHost.hoverPreview.parent : root
            const mapped = item.mapToItem(target, point.x, point.y)
            root.hoverPreviewAnchorRect = Qt.rect(mapped.x, mapped.y, 1, 1)
        }
        root.controller.hoveredPath = path
    }

    function thumbnailSourceFor(path, revision) {
        const cleanPath = String(path || "")
        if (cleanPath.length === 0) {
            return ""
        }
        const rev = Math.max(0, Number(revision || 0))
        return "image://thumbnail/" + encodeURIComponent(cleanPath + "::thumbrev=" + rev)
    }

    function clearHoveredItem(path) {
        if (!root.controller) {
            return
        }
        if (!path) {
            pendingHoverClearTimer.stop()
            root.pendingHoverClearPath = ""
            root.controller.hoveredPath = ""
            return
        }
        if (root.controller.hoveredPath !== path) {
            return
        }
        root.pendingHoverClearPath = path
        pendingHoverClearTimer.restart()
    }

    function openHoverPreviewQuickLook(path) {
        if (!path || !root.quickLookPopup) {
            return
        }
        if (root.quickLookController && root.quickLookController.preview) {
            root.quickLookController.preview(path)
        }
        root.quickLookPopup.previewPath = path
        root.quickLookPopup.open()
        root.clearHoveredItem()
    }

    function openHoverPreviewPath(path) {
        if (!path || !root.controller || !root.controller.directoryModel) {
            return
        }
        const row = root.controller.directoryModel.indexOfPath(path)
        if (row >= 0) {
            root.openItem(row)
            root.clearHoveredItem()
        }
    }

    function openHoverPreviewProperties(path) {
        if (!path || !root.controller || !root.controller.showPropertiesForPath) {
            return
        }
        root.controller.showPropertiesForPath(path)
        root.clearHoveredItem()
    }

    function setHoverPreviewWallpaper(path) {
        if (!path || !root.controller || !root.controller.setPathAsWallpaper) {
            return
        }
        root.controller.setPathAsWallpaper(path)
        root.clearHoveredItem()
    }

    function markKeyboardNavigationActivity() {
        root.keyboardNavigationActive = true
        keyboardNavigationTimer.restart()
    }

    function markPreviewScrollActive() {
        if (root.resizeOptimized || root.virtualRootMode) {
            return
        }
        root.fileViewPreviewScrollActive = true
        previewScrollStopTimer.restart()
    }

    function activeView() {
        return filePanelViewRegistry.activeView()
    }

    function viewForMode(mode) {
        return filePanelViewRegistry.viewForMode(mode)
    }

    function traceRenameFocus(stage, detail) {
    }

    function activeViewName() {
        if (root.viewMode === 0) return "list"
        if (root.viewMode === 1) return "grid"
        if (root.viewMode === 2) return "brief"
        return "unknown"
    }

    function scrollTrace(stage, detail) {
        if (!root.scrollTraceEnabled) {
            return
        }
        const view = root.activeView()
        const model = root.controller && root.controller.directoryModel ? root.controller.directoryModel : null
        console.log("[FM_SCROLL]",
                    stage,
                    detail || "",
                    "path=" + (root.controller ? root.controller.currentPath : ""),
                    "view=" + root.activeViewName(),
                    "y=" + (view ? view.contentY : -1),
                    "originY=" + (view ? (view.originY || 0) : 0),
                    "h=" + (view ? view.contentHeight : -1),
                    "vh=" + (view ? view.height : -1),
                    "viewCount=" + (view ? view.count : -1),
                    "currentIndex=" + (view ? view.currentIndex : -1),
                    "count=" + (model ? model.count : -1),
                    "selected=" + (model ? model.selectedCount : -1),
                    "loading=" + (model ? model.loading : false),
                    "pendingPath=" + root.pendingScrollRestorePath,
                    "pendingY=" + root.pendingScrollRestoreY,
                    "pendingEnabled=" + root.pendingScrollRestoreEnabled,
                    "navCommit=" + root.pendingNavigationCommitPath,
                    "target=" + root.targetSelectPath,
                    "pendingIndex=" + root.pendingCurrentIndexInit)
    }

    function isProviderLoadMorePathForCurrent(currentPath, targetPath) {
        const current = String(currentPath || "").replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase()
        const target = String(targetPath || "").replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase()
        if (current.length === 0 || !(current.startsWith("telegram://") || current.startsWith("instagram://"))) {
            return false
        }
        const suffix = "/__load_more__"
        if (!target.endsWith(suffix)) {
            return false
        }
        return target.substring(0, target.length - suffix.length) === current
    }

    function rubberTrace(stage, detail, view, rows) {
        if (!root.rubberTraceEnabled) {
            return
        }
        const targetView = view || root.rubberBandView || root.activeView()
        const model = root.controller && root.controller.directoryModel ? root.controller.directoryModel : null
        const rowSummary = rows ? rows.slice(0, 12).join(",") + (rows.length > 12 ? ".../" + rows.length : "/" + rows.length) : ""
        let sample = ""
        if (targetView && rows && rows.length > 0) {
            const sampleRows = [rows[0], rows[Math.floor(rows.length / 2)], rows[rows.length - 1]]
            const parts = []
            for (let i = 0; i < sampleRows.length; ++i) {
                const row = sampleRows[i]
                const rect = root.rubberBandItemRect(targetView, row)
                const item = targetView.itemAtIndex ? targetView.itemAtIndex(row) : null
                parts.push(row + ":" + (rect ? Math.round(rect.x) + "," + Math.round(rect.y) + "," + Math.round(rect.width) + "x" + Math.round(rect.height) : "no-rect") + ":item=" + Boolean(item))
            }
            sample = parts.join("|")
        }
        console.log("[FM_RUBBER]",
                    stage,
                    detail || "",
                    "path=" + (root.controller ? root.controller.currentPath : ""),
                    "view=" + root.rubberBandViewKind(targetView),
                    "viewCount=" + (targetView ? targetView.count : -1),
                    "modelCount=" + (model ? model.count : -1),
                    "selected=" + (model ? model.selectedCount : -1),
                    "loading=" + (model ? model.loading : false),
                    "contentY=" + (targetView ? Math.round(targetView.contentY) : -1),
                    "originY=" + (targetView ? Math.round(targetView.originY || 0) : 0),
                    "contentHeight=" + (targetView ? Math.round(targetView.contentHeight) : -1),
                    "height=" + (targetView ? Math.round(targetView.height) : -1),
                    "bottomMargin=" + (targetView ? Math.round(targetView.bottomMargin || 0) : -1),
                    "cell=" + (targetView && targetView.cellWidth ? Math.round(targetView.cellWidth) : 0) + "x" + (targetView && targetView.cellHeight ? Math.round(targetView.cellHeight) : 0),
                    "band=" + Math.round(root.rubberBandLeft) + "," + Math.round(root.rubberBandTop) + "-" + Math.round(root.rubberBandRight) + "," + Math.round(root.rubberBandBottom),
                    "rows=" + rowSummary,
                    "sample=" + sample,
                    "currentIndex=" + (targetView ? targetView.currentIndex : -1))
    }

    function queueActiveFileViewLayoutSync() {
        fileViewsLayoutSyncTimer.restart()
    }

    function syncActiveFileViewLayout() {
        if (root.virtualRootMode || !root.fileViewsModelEnabled) {
            return
        }
        const view = root.activeView()
        if (view && view.forceLayout) {
            view.forceLayout()
        }
    }

    function ensureFilePanelContextMenu() {
        if (root.startupLazyPanelMenus) {
            if (!root.filePanelContextMenuItem) {
                root.filePanelContextMenuItem = filePanelMenuHost.createContextMenu(root)
            }
            return root.filePanelContextMenuItem
        }
        return filePanelMenuHost.contextMenu
    }

    function ensureFilePanelEmptyMenu() {
        if (root.startupLazyPanelMenus) {
            if (!root.filePanelEmptyMenuItem) {
                root.filePanelEmptyMenuItem = filePanelMenuHost.createEmptyMenu(root)
            }
            return root.filePanelEmptyMenuItem
        }
        return filePanelMenuHost.emptyMenu
    }

    function hoverPreviewVisibleForMenuTransition() {
        return filePanelOverlayHost.hoverPreview
                && filePanelOverlayHost.hoverPreview.visible
                && filePanelOverlayHost.hoverPreview.opacity > 0.05
    }

    function hoverPreviewPointerInside() {
        return filePanelOverlayHost.hoverPreview
                && filePanelOverlayHost.hoverPreview.pointerInside
    }

    function popupContextMenuRequest(request) {
        if (!request) {
            root.contextMenuOpen = false
            return
        }

        if (request.kind === "item") {
            const menu = root.ensureFilePanelContextMenu()
            if (menu) {
                menu.popupContextMenu(request.index, request.path, request.canExtractArchive, request.canMountIso)
            } else {
                root.contextMenuOpen = false
            }
            return
        }

        if (request.kind === "empty") {
            const menu = root.ensureFilePanelEmptyMenu()
            if (menu) {
                menu.popupEmptyMenu()
            } else {
                root.contextMenuOpen = false
            }
        }
    }

    function scheduleContextMenuRequest(request) {
        if (contextMenuPopupDelayTimer.running || (!root.contextMenuOpen && root.hoverPreviewVisibleForMenuTransition())) {
            root.pendingContextMenuRequest = request
            root.contextMenuOpen = true
            contextMenuPopupDelayTimer.restart()
            return
        }

        root.popupContextMenuRequest(request)
    }

    function popupFilePanelContextMenu(index, path, canExtractArchive, canMountIso) {
        root.scheduleContextMenuRequest({
            kind: "item",
            index: index,
            path: path,
            canExtractArchive: canExtractArchive,
            canMountIso: canMountIso
        })
    }

    function popupFilePanelEmptyMenu() {
        root.scheduleContextMenuRequest({ kind: "empty" })
    }

    function sidebarHasActiveFocus() {
        return typeof sidebar !== "undefined"
                && sidebar
                && (sidebar.placesList.activeFocus || sidebar.foldersTree.activeFocus)
    }

    function focusContentAfterViewModeRestore() {
        if (!root.active || root.inlineRenameFocusActive() || root.sidebarHasActiveFocus()) {
            return
        }
        root.focusContent()
    }

    function focusContentAfterPanelViewMenu() {
        Qt.callLater(() => {
            Qt.callLater(root.focusContentAfterViewModeRestore)
        })
    }

    function focusContentAndQueueCurrentIndexEnsure() {
        Qt.callLater(() => {
            if (root.inlineRenameFocusActive()) {
                root.traceRenameFocus("focusContentAndQueue-skip-inline-rename")
                return
            }
            if (!root.sidebarHasActiveFocus()) {
                root.traceRenameFocus("focusContentAndQueue-focus-content")
                root.focusContent()
            }
            root.traceRenameFocus("focusContentAndQueue-current-index")
            root.queueCurrentIndexEnsure()
        })
    }

    function bundledIconForSuffix(isDirectory, suffix) {
        return filePanelIconPolicy.bundledIconForSuffix(isDirectory, suffix)
    }

    function bundledIconForPath(path, isDirectory, suffix, iconName) {
        return filePanelIconPolicy.bundledIconForPath(path, isDirectory, suffix, iconName)
    }

    function panelIconSource(path, isDirectory, suffix, iconName, mimeType, name) {
        return filePanelIconPolicy.panelIconSource(path, isDirectory, suffix, iconName, mimeType, name)
    }

    function providerFolderOverlaySource(path, iconName) {
        return filePanelIconPolicy.providerFolderOverlaySource(path, iconName)
    }

    function shouldUseNativeFolderOverlay(path, isDirectory, iconName) {
        return filePanelIconPolicy.shouldUseNativeFolderOverlay(path, isDirectory, iconName)
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
        if (root.navigationCommitPending()) {
            return
        }
        if (root.pendingRevealPath.length > 0) {
            return
        }
        if (root.inlineRenameFocusActive()) {
            return
        }
        if (!root.active || root.virtualRootMode || root.controller.directoryModel.count <= 0
                || root.rubberBandPressed || root.rubberBandActive) {
            return
        }
        if (filePanelNavigationPolicy.scrollRestorePending()) {
            if (root.pendingScrollRestorePath.length > 0 && !root.controller.directoryModel.loading) {
                root.queuePendingScrollRestore()
            }
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

    function navigationCommitPending() {
        return filePanelNavigationPolicy.navigationCommitPending()
    }

    function clearNavigationCommitIfArrived() {
        if (filePanelNavigationPolicy.navigationCommitArrived()) {
            root.pendingNavigationCommitPath = ""
        }
    }

    function clearPendingScrollRestore() {
        root.scrollTrace("clear-pending-scroll-restore")
        scrollRestoreTimer.stop()
        root.pendingScrollRestoreEnabled = false
        root.pendingScrollRestorePath = ""
        root.pendingScrollRestoreY = -1
        root.pendingScrollRestoreAttempts = 0
    }

    function shouldAutoPositionCurrentIndex() {
        return !root.suppressCurrentIndexAutoPosition
                && filePanelNavigationPolicy.canAutoPositionCurrentIndex()
    }

    function revealTargetSelectPath(allowAutoPosition) {
        const autoPosition = allowAutoPosition !== false
        if (root.targetSelectPath === "") {
            return false
        }
        if (autoPosition && (root.pendingScrollRestorePath.length > 0 || root.pendingScrollRestoreEnabled)) {
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
        if (autoPosition && root.shouldAutoPositionCurrentIndex()) {
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
        if (root.inlineRenameFocusActive()) {
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
        if (root.inlineRenameFocusActive()) {
            return
        }
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
        filePanelInlineRenamePolicy.restorePreviewAfterRenameEdit()
    }

    function clearPendingInlineRenameFocus() {
        filePanelInlineRenamePolicy.clearPendingInlineRenameFocus()
    }

    function queueInlineRenameFocus(path, selectText) {
        filePanelInlineRenamePolicy.queueInlineRenameFocus(path, selectText)
    }

    function retryPendingInlineRenameFocus() {
        filePanelInlineRenamePolicy.retryPendingInlineRenameFocus()
    }

    function tryFocusPendingInlineRename() {
        filePanelInlineRenamePolicy.tryFocusPendingInlineRename()
    }

    function cancelInlineRename() {
        filePanelInlineRenamePolicy.cancelInlineRename()
    }

    function cancelActiveInlineRename() {
        return filePanelInlineRenamePolicy.cancelActiveInlineRename()
    }

    function beginCreateRenameSession(path) {
        filePanelInlineRenamePolicy.beginCreateRenameSession(path)
    }

    function cancelCreateRenameSession() {
        filePanelInlineRenamePolicy.cancelCreateRenameSession()
    }

    function finishCreateRenameSession() {
        filePanelInlineRenamePolicy.finishCreateRenameSession()
    }

    function createRenameSessionActive() {
        return filePanelInlineRenamePolicy.createRenameSessionActive()
    }

    function inlineRenameFocusActive() {
        return filePanelInlineRenamePolicy.inlineRenameFocusActive()
    }

    function recoverInlineRenameFocus(reason) {
        return filePanelInlineRenamePolicy.recoverInlineRenameFocus(reason)
    }

    function clearStaleInlineRenameState() {
        filePanelInlineRenamePolicy.clearStaleInlineRenameState()
    }

    function queueCreateRenameAttempt() {
        filePanelInlineRenamePolicy.queueCreateRenameAttempt()
    }

    function startRenameForPath(path) {
        return filePanelInlineRenamePolicy.startRenameForPath(path)
    }

    function tryStartCreateRename() {
        filePanelInlineRenamePolicy.tryStartCreateRename()
    }

    function focusRenamedPath(path) {
        filePanelInlineRenamePolicy.focusRenamedPath(path)
    }

    function revealPathInView(path) {
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

    function requestRevealPath(path, cancelScrollRestore) {
        if (!path || path.length === 0) {
            return
        }
        if (cancelScrollRestore === true) {
            root.clearPendingScrollRestore()
        }
        root.pendingCurrentIndexInit = false
        root.currentIndexEnsureAttempts = 0
        root.targetSelectPath = ""
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
        root.rememberScrollPositionForView(root.activeView(), root.viewMode)
    }

    function normalizedPanelPath(path) {
        return filePanelPathPolicy.normalizedPath(path)
    }

    function samePanelPath(left, right) {
        return filePanelPathPolicy.samePath(left, right)
    }

    function findDirectChildPath(parentPath, childPath) {
        return filePanelPathPolicy.directChildPath(parentPath, childPath)
    }

    function saveScrollPositionForView(path, mode, view) {
        if (!view) {
            return false
        }
        if (view.count <= 0 || view.contentHeight <= 0) {
            return false
        }

        const anchor = filePanelViewAnchorPolicy.viewAnchor(view)
        const focusedPath = anchor && anchor.setsCurrent ? anchor.path : ""
        const focusedOffsetY = anchor && anchor.setsCurrent ? anchor.offsetY : undefined
        return filePanelScrollState.save(path, mode,
                                         view.contentY - (view.originY || 0),
                                         view.contentX,
                                         focusedPath,
                                         focusedOffsetY,
                                         anchor ? anchor.path : "",
                                         anchor ? anchor.offsetY : 0,
                                         anchor ? anchor.setsCurrent : false,
                                         anchor ? anchor.source : "")
    }

    function saveScrollPositionForPathAndMode(path, mode) {
        return root.saveScrollPositionForView(path, mode, root.viewForMode(mode))
    }

    function saveScrollPositionForPath(path) {
        return root.saveScrollPositionForPathAndMode(path, root.viewMode)
    }

    function rememberScrollPositionForView(view, mode) {
        if (mode !== root.viewMode || view !== root.activeView()) {
            return
        }
        if (!root.controller || !root.controller.directoryModel || root.virtualRootMode) {
            return
        }
        if (root.navigationCommitPending()
                || root.controller.directoryModel.loading
                || root.pendingScrollRestorePath.length > 0
                || root.pendingScrollRestoreEnabled) {
            return
        }
        root.saveScrollPositionForView(root.controller.currentPath, mode, view)
    }

    function restoreViewModeAnchor(anchor) {
        const view = root.activeView()
        if (!view) {
            return false
        }

        if (!anchor || !anchor.path || anchor.path.length === 0) {
            view.contentY = 0
            root.completeCurrentIndexInit()
            return true
        }

        const idx = root.controller.directoryModel.indexOfPath(anchor.path)
        if (idx < 0 || view.count <= idx) {
            view.contentY = 0
            root.completeCurrentIndexInit()
            return true
        }

        const rect = root.rubberBandItemRect(view, idx)
        if (!rect || view.contentHeight <= 0) {
            return false
        }

        const offsetY = anchor.offsetY !== undefined && isFinite(anchor.offsetY) ? anchor.offsetY : 0
        const maxY = Math.max(0, view.contentHeight - view.height)
        const restoredY = Math.max(0, Math.min(maxY, rect.y - offsetY))

        root.suppressCurrentIndexAutoPosition = true
        if (anchor.setsCurrent || root.viewCurrentIndexInvalid(view, root.controller.directoryModel.count)) {
            root.setViewCurrentIndexWithoutSelection(view, idx)
        }
        view.contentY = restoredY
        root.saveScrollPositionForView(root.controller.currentPath, root.viewMode, view)
        root.completeCurrentIndexInit()
        Qt.callLater(() => {
            root.suppressCurrentIndexAutoPosition = false
        })
        return true
    }

    function prepareViewModeRestore(oldMode, newMode) {
        if (!root.controller
                || !root.controller.directoryModel
                || root.virtualRootMode
                || !root.controller.currentPath
                || root.controller.directoryModel.loading) {
            return
        }

        const path = root.controller.currentPath
        root.pendingCurrentIndexInit = true
        root.currentIndexEnsureAttempts = 0
        const anchor = filePanelScrollState.viewModeAnchor(path, oldMode)
        root.targetSelectPath = anchor.setsCurrent ? anchor.path : ""
        root.clearPendingScrollRestore()

        Qt.callLater(() => {
            if (root.viewMode !== newMode || !root.samePanelPath(root.controller.currentPath, path)) {
                return
            }
            if (root.restoreViewModeAnchor(anchor)) {
                root.focusContentAfterViewModeRestore()
                return
            }
            if (!root.revealTargetSelectPath()) {
                root.queueCurrentIndexEnsure()
            }
            root.focusContentAfterViewModeRestore()
        })
    }

    function prepareScrollRestoreForPath(path) {
        if (!filePanelScrollState.canStorePath(path)) {
            root.scrollTrace("prepare-scroll-restore-no-store", "path=" + path)
            root.clearPendingScrollRestore()
            return false
        }

        const state = filePanelScrollState.state(path)
        if (!state) {
            root.scrollTrace("prepare-scroll-restore-no-state", "path=" + path)
            root.clearPendingScrollRestore()
            return false
        }

        pendingScrollRestorePath = path
        pendingScrollRestoreY = state.y
        pendingScrollRestoreAttempts = 0
        root.scrollTrace("prepare-scroll-restore-ready", "path=" + path + " stateY=" + state.y)
        return true
    }

    function queueScrollRestoreForPath(path) {
        if (!root.prepareScrollRestoreForPath(path)) {
            root.scrollTrace("queue-scroll-restore-prepare-failed", "path=" + path)
            return
        }

        if (!root.controller.directoryModel.loading) {
            root.scrollTrace("queue-scroll-restore-now", "path=" + path)
            root.queuePendingScrollRestore()
        } else {
            root.scrollTrace("queue-scroll-restore-wait-loading", "path=" + path)
        }
    }

    function restorePendingScrollPosition() {
        if (!pendingScrollRestorePath || pendingScrollRestoreY < 0) {
            root.scrollTrace("restore-scroll-skip-no-pending")
            return false
        }

        const restorePath = pendingScrollRestorePath
        const restoreY = pendingScrollRestoreY
        if (root.navigationCommitPending()
                && root.samePanelPath(root.pendingNavigationCommitPath, restorePath)) {
            root.scrollTrace("restore-scroll-skip-nav-pending")
            return false
        }

        if (!root.samePanelPath(root.controller.currentPath, restorePath)) {
            root.scrollTrace("restore-scroll-path-mismatch", "current=" + root.controller.currentPath + " pending=" + restorePath)
            root.clearPendingScrollRestore()
            root.currentIndexEnsureAttempts = 0
            if (root.active) {
                root.focusContentAndQueueCurrentIndexEnsure()
            }
            return false
        }

        const view = activeView()
        const readiness = filePanelScrollRestorePolicy.readiness(view)
        if (!readiness.ready) {
            root.scrollTrace("restore-scroll-not-ready", "reason=" + readiness.reason + " minY=" + readiness.minY + " maxY=" + readiness.maxY)
            return false
        }
        if (!root.samePanelPath(pendingScrollRestorePath, restorePath) || pendingScrollRestoreY !== restoreY) {
            root.scrollTrace("restore-scroll-stale-after-layout", "path=" + restorePath + " y=" + restoreY)
            return pendingScrollRestorePath.length === 0
        }

        const minY = readiness.minY
        const maxY = readiness.maxY
        const targetY = minY + restoreY
        const restoredY = Math.min(Math.max(minY, targetY), maxY)
        if (restoreY > 0 && restoredY === minY
                && root.controller.directoryModel.count > 0
                && pendingScrollRestoreAttempts < 6) {
            pendingScrollRestoreAttempts += 1
            root.scrollTrace("restore-scroll-delay-min-bound", "targetY=" + targetY + " minY=" + minY + " maxY=" + maxY + " attempt=" + pendingScrollRestoreAttempts)
            return false
        }

        root.scrollTrace("restore-scroll-apply", "targetY=" + targetY + " restoredY=" + restoredY + " minY=" + minY + " maxY=" + maxY)
        const targetSelectPathAfterRestore = root.targetSelectPath
        root.clearPendingScrollRestore()
        root.suppressCurrentIndexAutoPosition = true
        view.contentY = restoredY
        root.targetSelectPath = targetSelectPathAfterRestore
        if (!root.revealTargetSelectPath(false)) {
            root.completeCurrentIndexInit()
        }
        Qt.callLater(() => {
            root.suppressCurrentIndexAutoPosition = false
        })
        if (root.active) root.focusContent()
        return true
    }

    property string statusMessage: ""

    function showStatusMessage(message) {
        filePanelStatusMessagePolicy.showMessage(message)
    }

    function dropMenuOpen() {
        const menu = filePanelMenuHost.oppositeDropMenu
        return Boolean(menu && menu.menuOpen)
    }

    function cancelDropMenu() {
        const menu = filePanelMenuHost.oppositeDropMenu
        return Boolean(menu && menu.cancelDropMenu())
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

        AmbientPanelBackground {
            id: panelBg
            anchors.fill: parent
            cornerRadius: Theme.panelRadius
            strength: 0.68
            border.color: Theme.panelStrokeSubtle
            border.width: 1

            Rectangle {
                anchors.fill: parent
                radius: Theme.innerRadius(panelBg.radius, 1)
                color: root.showActiveHighlight 
                       ? Theme.withAlpha(Theme.activeAccent, root.activePanelFillAlpha)
                       : Theme.withAlpha(Theme.accent, themeController.isDark ? 0.010 : 0.016)
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: root.topChromeHeight
                anchors.bottomMargin: root.bottomChromeHeight
                radius: 0
                topLeftRadius: 0
                topRightRadius: 0
                bottomLeftRadius: 0
                bottomRightRadius: 0
                color: "transparent"
                border.width: 1
                border.color: Theme.activePanelStroke
                opacity: root.showActiveHighlight ? root.activePanelStrokeOpacity : 0.0
                antialiasing: true
            }

            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: root.topChromeHeight + 2
                anchors.horizontalCenter: parent.horizontalCenter
                width: 84
                height: 3
                radius: 1.5
                color: Theme.activeAccent
                opacity: root.showActiveHighlight ? root.activePanelIndicatorOpacity : 0.0
                antialiasing: true
                
                Behavior on opacity {
                    enabled: !root.effectsReduced
                    NumberAnimation { duration: Theme.motionFast }
                }
            }

            Behavior on border.color { enabled: !root.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
        }
    }

    function contextRow() {
        return filePanelViewRegistry.currentIndex()
    }

    function cancelInlineRenameForNavigation(reason) {
        if (!root.inlineRenameFocusActive()) {
            return false
        }
        root.traceRenameFocus("navigation-cancel-inline-rename", reason || "")
        return root.cancelActiveInlineRename()
    }

    function openPath(path) {
        if (!root.controller || !path || String(path).trim().length === 0) {
            return false
        }
        if (root.inlineRenameFocusActive()
                && root.controller.canOpenPath
                && !root.controller.canOpenPath(path)) {
            return false
        }
        root.cancelInlineRenameForNavigation("openPath")
        return root.controller.openPath(path)
    }

    function openItem(index) {
        if (!root.controller) {
            return
        }
        root.cancelInlineRenameForNavigation("openItem")
        root.controller.openItem(index)
    }

    function goBack() {
        if (!root.controller || !root.controller.canGoBack) {
            return
        }
        root.cancelInlineRenameForNavigation("goBack")
        root.controller.goBack()
    }

    function goForward() {
        if (!root.controller || !root.controller.canGoForward) {
            return
        }
        root.cancelInlineRenameForNavigation("goForward")
        root.controller.goForward()
    }

    function goUp() {
        if (!root.controller || root.virtualRootMode) {
            return
        }
        root.cancelInlineRenameForNavigation("goUp")
        root.controller.goUp()
    }

    function startRename() {
        filePanelInlineRenamePolicy.startManualRename(contextRow())
    }

    function focusContent() {
        if (root.inlineRenameFocusActive()) {
            root.traceRenameFocus("focusContent-skip-inline-rename")
            return false
        }
        root.traceRenameFocus("focusContent-apply")
        if (root.controller.isDeviceRoot) {
            storageView.forceActiveFocus()
        } else if (root.controller.isFavoritesRoot) {
            favoritesView.forceActiveFocus()
        } else {
            filePanelViewRegistry.forceActiveFocus()
        }
        return true
    }

    function quickLookCurrentFavorite() {
        if (!root.controller.isFavoritesRoot || !favoritesView) {
            return false
        }
        return favoritesView.openQuickLookForCurrentFavorite()
    }

    function panelKeysBlockedByOverlay() {
        return root.Window.window && root.Window.window.anyOverlayOpen
    }

    function viewPointToViewContentPoint(view, x, y) {
        return Qt.point(x + (view.contentX || 0), y + (view.contentY || 0))
    }

    function contentAreaPointToViewContentPoint(view, x, y) {
        return filePanelRubberBandPolicy.contentPoint(view, contentArea, x, y)
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
        root.cancelSelectionDragForRubberBand()
        root.activated()
        root.rubberBandView = view
        root.rubberBandStartX = contentX
        root.rubberBandStartY = contentY
        root.rubberBandCurrentX = contentX
        root.rubberBandCurrentY = contentY
        root.rubberBandPressed = true
        root.rubberBandActive = false
        root.rubberBandMoved = false
        root.rubberTrace("begin", "x=" + Math.round(contentX) + " y=" + Math.round(contentY), view)
    }

    function beginRubberBandPress(view, mouse) {
        if (mouse.button === Qt.RightButton) {
            if (root.viewItemAtPoint(view, mouse.x, mouse.y)) {
                mouse.accepted = false
            }
            return
        }
        if (root.inlineRenameFocusActive()) {
            root.cancelActiveInlineRename()
        }
        if (mouse.button !== Qt.LeftButton
                || root.virtualRootMode
                || root.isRenaming
                || root.panelKeysBlockedByOverlay()) {
            mouse.accepted = false
            return
        }

        const item = root.viewItemAtPoint(view, mouse.x, mouse.y)
        if (item && typeof item.isPointOnBadge === "function") {
            const mapped = item.mapFromItem(view, mouse.x, mouse.y)
            if (item.isPointOnBadge(mapped.x, mapped.y)) {
                mouse.accepted = false
                return
            }
        }
        if (item && root.internalDragEnabled) {
            const mapped = item.mapFromItem(view, mouse.x, mouse.y)
            const selectedItemDrag = item.isSelected === true
                    && root.dragCoordinator
                    && root.dragCoordinator.canStartDrag(root.panelSide, item.path)
            const itemOwnsDrag = selectedItemDrag
                    || typeof item.isPointOnDragSurface !== "function"
                    || item.isPointOnDragSurface(mapped.x, mapped.y)
            if (!itemOwnsDrag) {
                root.rubberBandPressView = view
                root.rubberBandPressIndex = item.index
                root.beginRubberBand(view, mouse)
                return
            }
            mouse.accepted = false
            return
        }
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
        if (!root.rubberBandActive
                && (dx * dx + dy * dy) >= root.selectionDragThreshold * root.selectionDragThreshold) {
            root.rubberBandActive = true
            root.rubberBandMoved = true
            root.rubberBandReserveSelectionActions = root.selectionActionsVisible
            root.clearSelection()
            root.rubberTrace("active", "dx=" + Math.round(dx) + " dy=" + Math.round(dy), view)
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
        let selectedCount = 0
        if (root.rubberBandActive) {
            root.rubberBandCurrentX = contentX
            root.rubberBandCurrentY = contentY
            root.rubberTrace("finish-before-commit", "x=" + Math.round(contentX) + " y=" + Math.round(contentY), view)
            selectedCount = root.commitRubberBandSelection()
        }

        const usedRubberBand = root.rubberBandActive
        root.rubberBandPressed = false
        root.rubberBandActive = false
        root.rubberBandMoved = false
        root.rubberBandView = null
        root.rubberBandReserveSelectionActions = false
        rubberBandAutoScroll.stop()
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
        if (root.inlineRenameFocusActive()) {
            root.cancelActiveInlineRename()
        }
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
        root.rubberBandReserveSelectionActions = false
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
        root.openItem(item.index)
    }

    function handleEmptyViewClick(view, mouse) {
        root.activated()
        const cancelledRename = root.cancelActiveInlineRename()
        if (cancelledRename) {
            root.focusContent()
        }
        if (mouse.button === Qt.RightButton) {
            root.popupFilePanelEmptyMenu()
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
        if (view && view.indexAt) {
            const directIndex = view.indexAt(x + (view.contentX || 0), y + (view.contentY || 0))
            if (directIndex >= 0) {
                const directItem = view.itemAtIndex(directIndex)
                if (directItem && directItem.visible) {
                    return directItem
                }
            }
        }
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
        return filePanelRubberBandPolicy.visibleRows(view, count, contentArea, listView)
    }

    function rubberBandCandidateRows(view) {
        const count = root.controller && root.controller.directoryModel ? root.controller.directoryModel.count : 0
        return filePanelRubberBandPolicy.selectionRows(view, count, listView,
                                                       root.rubberBandLeft,
                                                       root.rubberBandTop,
                                                       root.rubberBandRight,
                                                       root.rubberBandBottom)
    }

    function rubberBandItemRect(view, row) {
        if (view === listView) {
            return filePanelRubberBandPolicy.itemRect(view, row, listView)
        }
        if (view && view.itemAtIndex) {
            const item = view.itemAtIndex(row)
            if (item && item.visible) {
                const point = item.mapToItem(view, 0, 0)
                return {
                    x: point.x + (view.contentX || 0),
                    y: point.y + (view.contentY || 0),
                    width: item.width,
                    height: item.height
                }
            }
            return null
        }
        return filePanelRubberBandPolicy.itemRect(view, row, listView)
    }

    function rubberBandViewKind(view) {
        if (view === listView && root.viewMode === 0) return "list"
        if (view === gridView) return "grid"
        if (view === briefView) return "brief"
        return "other"
    }

    function rubberBandSelectsItem(view, itemX, itemY, itemWidth, itemHeight) {
        return filePanelRubberBandPolicy.selectsItem(root.rubberBandViewKind(view),
                                                     itemX, itemY, itemWidth, itemHeight,
                                                     root.rubberBandLeft,
                                                     root.rubberBandTop,
                                                     root.rubberBandRight,
                                                     root.rubberBandBottom,
                                                     root.effectiveColWidthName,
                                                     root.gridIconSize)
    }

    function commitRubberBandSelection() {
        if (!root.rubberBandActive || !root.rubberBandView || root.rubberBandWidth <= 0 || root.rubberBandHeight <= 0) {
            return 0
        }

        const selectedRows = []
        const rows = root.rubberBandCandidateRows(root.rubberBandView)
        root.rubberTrace("commit-candidates", "", root.rubberBandView, rows)
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
        root.rubberTrace("commit-selected", "selectedRows=" + selectedRows.join(","), root.rubberBandView, selectedRows)
        root.controller.directoryModel.selectRows(selectedRows)
        if (selectedRows.length > 0) {
            root.suppressCurrentIndexAutoPosition = true
            root.setViewCurrentIndexWithoutSelection(root.rubberBandView, selectedRows[0])
            Qt.callLater(() => {
                root.suppressCurrentIndexAutoPosition = false
            })
        }
        return selectedRows.length
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
        const minY = view.originY || 0
        const maxY = Math.max(minY, minY + view.contentHeight - view.height + (view.bottomMargin || 0))
        const oldY = view.contentY
        const nextY = Math.max(minY, Math.min(maxY, view.contentY + delta))
        if (nextY !== view.contentY) {
            view.contentY = nextY
            root.rubberBandCurrentY += view.contentY - oldY
            root.rubberTrace("autoscroll", "oldY=" + Math.round(oldY) + " nextY=" + Math.round(nextY) + " delta=" + Math.round(delta), view)
            root.markPreviewScrollActive()
            root.suppressHoverBriefly()
            if (!root.scrolling) {
                root.scrolling = true
                root.controller.scrolling = true
                root.clearHoveredItem()
            }
            scrollStopTimer.restart()
        }
    }

    function handleItemClick(index, mouse) {
        root.activated()
        const cancelledRename = root.cancelActiveInlineRename()
        if (cancelledRename) {
            root.focusContent()
        }
        root.invertSelectionActive = false
        root.disableSelectionOnCurrentIndexChanged = true
        let prevIdx = filePanelViewRegistry.setCurrentIndex(index)
        root.updateCurrentItemPath(index)
        root.disableSelectionOnCurrentIndexChanged = false

        filePanelSelectionPolicy.selectClickedRow(index, mouse.modifiers, prevIdx)
    }

    function handleItemRightClick(index, path, isArchiveFile, isIsoImageFile) {
        root.activated()
        const cancelledRename = root.cancelActiveInlineRename()
        if (cancelledRename) {
            root.focusContent()
        }
        root.disableSelectionOnCurrentIndexChanged = true
        filePanelViewRegistry.setCurrentIndex(index)
        root.disableSelectionOnCurrentIndexChanged = false
        root.updateCurrentItemPath(index)

        filePanelSelectionPolicy.selectRightClickedRow(index, path)
        root.popupFilePanelContextMenu(
            index,
            path,
            !root.isCurrentPathArchive && !root.isCurrentPathRemote && isArchiveFile === true && isIsoImageFile !== true,
            !root.isCurrentPathArchive && !root.isCurrentPathRemote && isIsoImageFile === true)
    }

    function selectedPathsContain(path) {
        if (!root.controller || !root.controller.selectedPaths) {
            return false
        }
        const selected = root.controller.selectedPaths()
        return selected && selected.indexOf(path) >= 0
    }

    function updateSelectionDragCandidate(index, path, startX, startY, currentX, currentY, mouse) {
        if (!root.internalDragEnabled || !root.dragCoordinator || root.dragCoordinator.active || root.isRenaming) {
            return false
        }
        const dx = currentX - startX
        const dy = currentY - startY
        if ((dx * dx + dy * dy) < root.selectionDragThreshold * root.selectionDragThreshold) {
            return false
        }
        if (!root.selectedPathsContain(path)) {
            root.handleItemClick(index, mouse)
        }
        return root.dragCoordinator.startDrag(root.panelSide, path)
    }

    function cancelSelectionDragForRubberBand() {
        if (root.internalDragEnabled && root.dragCoordinator && root.dragCoordinator.active) {
            root.dragCoordinator.cancelDrag("Rubber-band selection started.")
        }
    }

    function updateSelectionDragPosition(mouse, sourceItem) {
        if (root.internalDragEnabled && root.dragCoordinator && root.dragCoordinator.active && mouse && sourceItem) {
            root.dragCoordinator.updateDragPosition(sourceItem, mouse.x, mouse.y)
        }
    }

    function selectionDragCursorShape() {
        if (!root.internalDragEnabled || !root.dragCoordinator || !root.dragCoordinator.active || !root.oppositePanelItem) {
            return Qt.ArrowCursor
        }
        if (!root.dragCoordinator.parent) {
            return Qt.ForbiddenCursor
        }
        const point = root.oppositePanelItem.mapFromItem(
                    root.dragCoordinator.parent,
                    root.dragCoordinator.pointerX,
                    root.dragCoordinator.pointerY)
        const overOppositePanel = point.x >= 0
                && point.y >= 0
                && point.x <= root.oppositePanelItem.width
                && point.y <= root.oppositePanelItem.height
        return overOppositePanel && root.dragCoordinator.canDropOn(root.oppositePanelItem.panelSide)
                ? Qt.ArrowCursor
                : Qt.ForbiddenCursor
    }

    function panelHoverCursorShape() {
        if (root.dragCoordinator && root.dragCoordinator.active) {
            return root.selectionDragCursorShape()
        }
        return root.internalDragEnabled && !root.resizeOptimized
                ? root.hoverDragCursorShape
                : Qt.ArrowCursor
    }

    function updateHoverDragCursor(item, x, y) {
        root.hoverDragCursorShape = root.itemDragAffordanceCursor(item, x, y)
    }

    function clearHoverDragCursor(item) {
        if (!item || root.hoverDragCursorShape !== Qt.ArrowCursor) {
            root.hoverDragCursorShape = Qt.ArrowCursor
        }
    }

    function itemDragAffordanceCursor(item, x, y) {
        if (!root.internalDragEnabled || !item || root.isRenaming
                || item.resizeOptimized === true || item.lightweightActive === true) {
            return Qt.ArrowCursor
        }
        if (root.dragCoordinator && root.dragCoordinator.active) {
            return root.selectionDragCursorShape()
        }
        if (item.isSelected === true) {
            return Qt.SizeAllCursor
        }
        if (typeof item.isPointOnDragSurface === "function" && item.isPointOnDragSurface(x, y)) {
            return Qt.SizeAllCursor
        }
        return Qt.ArrowCursor
    }

    function itemHoverCursorShape(item, x, y) {
        if (root.dragCoordinator && root.dragCoordinator.active) {
            return Qt.ArrowCursor
        }
        return root.itemDragAffordanceCursor(item, x, y)
    }

    function finishSelectionDrag(mouse, sourceItem) {
        if (!root.internalDragEnabled || !root.dragCoordinator || !root.dragCoordinator.active) {
            return
        }
        root.updateSelectionDragPosition(mouse, sourceItem)
        if (!mouse || !sourceItem || !root.oppositePanelItem) {
            root.dragCoordinator.cancelDrag("Drag released.")
            return
        }
        const point = root.oppositePanelItem.mapFromItem(sourceItem, mouse.x, mouse.y)
        const overOppositePanel = point.x >= 0
                && point.y >= 0
                && point.x <= root.oppositePanelItem.width
                && point.y <= root.oppositePanelItem.height
        if (!overOppositePanel || !root.dragCoordinator.canDropOn(root.oppositePanelItem.panelSide)) {
            const reason = root.dragCoordinator.deniedReasonFor(root.oppositePanelItem.panelSide)
            if (overOppositePanel && reason.length > 0) {
                root.showStatusMessage(reason)
            }
            root.dragCoordinator.cancelDrag("Drag released outside drop target.")
            return
        }
        const menuPoint = root.mapFromItem(sourceItem, mouse.x, mouse.y)
        if (!filePanelMenuHost.oppositeDropMenu
                || !filePanelMenuHost.oppositeDropMenu.popupDropMenu(root, menuPoint.x, menuPoint.y)) {
            root.dragCoordinator.cancelDrag("Drop menu failed.")
        }
    }

    function loadingFolderName() {
        return filePanelLoadingPolicy.loadingFolderName()
    }

    signal activated()
    FilePanelMenuHost {
        id: filePanelMenuHost
        anchors.fill: parent
        panelRoot: root
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
            implicitHeight: root.panelToolbarHeight
            color: Theme.panelSurfaceStrong
            radius: Theme.innerRadius(Theme.panelRadius, 1)
            bottomLeftRadius: 0
            bottomRightRadius: 0

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        root.activated()
                        root.popupFilePanelEmptyMenu()
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
                    showSelectionBadges: root.showSelectionBadges
                    showHoverPreviews: root.showHoverPreviews
                    onActionBarVisibilityRequested: (visible) => root.showActionBar = visible
                    onSelectionBadgesVisibilityRequested: (visible) => root.showSelectionBadges = visible
                    onHoverPreviewsVisibilityRequested: (visible) => root.showHoverPreviews = visible
                    onViewModeSelected: root.focusContentAfterPanelViewMenu()
                }
            }
        }

        Component {
            id: panelPathBarComponent

            PathBar {
                controller: root.controller
                path: root.controller.currentPath
                openPathHandler: function(path) { return root.openPath(path) }
                prepareNavigationHandler: function(reason) { root.cancelInlineRenameForNavigation(reason) }
                onActiveFocusChanged: if (activeFocus) root.activated()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: root.panelToolbarDividerHeight
            color: Theme.panelStrokeSubtle
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
                    onDoubleClicked: root.openItem(index)
                }
            }

            Component {
                id: detailsDelegate
                FileTableAdaptiveDelegate {
                    width: listView.width
                    controller: root.controller
                    panel: root
                    currentItem: ListView.isCurrentItem
                    panelActive: root.active
                    scrolling: root.hoverSuppressed
                    onClicked: (mouse) => root.handleItemClick(index, mouse)
                    onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                    onEmptySpaceRightClicked: root.popupFilePanelEmptyMenu()
                    onDoubleClicked: root.openItem(index)
                }
            }

            Flickable {
                id: horizontalFlick
                anchors.fill: parent
                visible: !root.virtualRootMode
                enabled: visible
                contentWidth: root.lightweightDelegates && root.viewMode === 0
                              ? width
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
                        boundsBehavior: Flickable.StopAtBounds
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
                        onContentYChanged: {
                            if (root.scrollTraceEnabled && root.viewMode === 0) {
                                root.scrollTrace("list-contentY-changed", "newY=" + contentY)
                            }
                            if (!root.resizeOptimized) root.handleScrollActivity()
                            root.rememberScrollPositionForView(listView, 0)
                        }
                        onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                        bottomMargin: root.bottomChromeHeight + (root.horizontalScrollActive ? 12 : 0)
                        
                        highlight: null
                        highlightFollowsCurrentItem: false

                        add: null
                        remove: null
                        Keys.onPressed: (event) => filePanelKeyboardPolicy.handleViewKeyPressed(listView, event)
                        delegate: root.viewMode === 0 ? detailsDelegate : listDelegate

                        MouseArea {
                            anchors.fill: parent
                            z: 8
                            enabled: root.emptyAreaInputEnabled()
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            preventStealing: true
                            cursorShape: root.panelHoverCursorShape()
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
                cellWidth: root.lightweightDelegates
                           ? Math.max(160, width)
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
                boundsBehavior: Flickable.StopAtBounds
                pixelAligned: false
                interactive: !root.resizeOptimized
                onMovingChanged:  root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: {
                    if (root.scrollTraceEnabled && root.viewMode === 2) {
                        root.scrollTrace("brief-contentY-changed", "newY=" + contentY)
                    }
                    if (!root.resizeOptimized) root.handleScrollActivity()
                    root.rememberScrollPositionForView(briefView, 2)
                }
                onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                bottomMargin: root.bottomChromeHeight

                add: null
                remove: null

                Keys.onPressed: (event) => filePanelKeyboardPolicy.handleViewKeyPressed(briefView, event)

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
                        thumbnailSchedulingPaused: root.thumbnailSchedulingPaused
                        thumbnailLoadingPaused: root.thumbnailLoadingPaused

                        onClicked: (mouse) => root.handleItemClick(index, mouse)
                        onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                        onDoubleClicked: root.openItem(index)
                    }
                }

                // Empty area handling
                MouseArea {
                    anchors.fill: parent
                    z: 8
                    enabled: root.emptyAreaInputEnabled()
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    cursorShape: root.panelHoverCursorShape()
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
                boundsBehavior: Flickable.StopAtBounds
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
                onContentYChanged: {
                    if (root.scrollTraceEnabled && root.viewMode === 1) {
                        root.scrollTrace("grid-contentY-changed", "newY=" + contentY)
                    }
                    if (!root.resizeOptimized) root.handleScrollActivity()
                    root.rememberScrollPositionForView(gridView, 1)
                }
                onContentXChanged: if (!root.resizeOptimized) root.handleScrollActivity()
                
                highlight: null
                highlightFollowsCurrentItem: false

                add: null
                remove: null

                Keys.onPressed: (event) => filePanelKeyboardPolicy.handleViewKeyPressed(gridView, event)

                delegate: Item {
                    id: gridDelegate
                    width: gridView.cellWidth
                    height: gridView.cellHeight
                    z: isRenaming ? 100 : 0

                    required property int index
                    required property string name
                    required property string path
                    required property string iconName
                    required property string suffix
                    required property string mimeType
                    required property bool isDirectory
                    required property bool isSelected
                    required property bool isHidden
                    required property bool isImage
                    required property bool hasThumbnail
                    required property int thumbnailRevision
                    required property bool isArchiveFile
                    required property bool isIsoImageFile
                    required property string primaryBadgeKind
                    required property bool isPinned

                    property bool isRenaming: false
                    property bool currentItem: GridView.isCurrentItem
                    property bool panelActive: root.active
                    property real dragStartX: 0
                    property real dragStartY: 0
                    property bool dragCandidate: false
                    property bool dragStarted: false
                    property bool badgePressed: false
                    property bool suppressClickAfterDrag: false
                    property string thumbnailFailedPath: ""
                    property int thumbnailRetryAttempt: 0
                    property int thumbnailRetryRevision: 0
                    readonly property bool lightweightActive: root.lightweightDelegates && !isRenaming
                    readonly property color selectedStateFill: Theme.withAlpha(
                        Theme.activeAccent,
                        themeController.isDark
                            ? (root.active ? 0.34 : 0.20)
                            : (root.active ? 0.28 : 0.16))
                    readonly property color currentStateFill: Theme.withAlpha(
                        Theme.activeAccent,
                        themeController.isDark
                            ? (root.active ? 0.18 : 0.11)
                            : (root.active ? 0.14 : 0.09))
                    readonly property int contentMargin: 10
                    readonly property int contentSpacing: 6
                    readonly property int renameEditorTop: contentMargin + root.gridIconSize + contentSpacing
                    readonly property int renameEditorSideMargin: contentMargin
                    readonly property int renameEditorAvailableHeight: Math.max(30, height - renameEditorTop - contentMargin)
                    readonly property bool canLoadThumbnail: root.effectiveUseNativeIcons
                                                              && root.effectiveShowThumbnails
                                                              && !root.thumbnailLoadingPaused
                                                              && !gridDelegate.lightweightActive
                                                              && !isDirectory
                                                              && hasThumbnail
                                                              && gridDelegate.thumbnailFailedPath !== path
                    readonly property bool canScheduleThumbnail: gridDelegate.canLoadThumbnail
                                                                 && !root.thumbnailSchedulingPaused
                    property bool thumbnailLoadEnabled: false
                    readonly property bool thumbnailRequestActive: thumbnailLoadEnabled && canLoadThumbnail
                    property real visualOffsetY: 0

                    opacity: isHidden ? 0.55 : 1.0

                    onPathChanged: {
                        isRenaming = false
                        visualOffsetY = 0
                        thumbnailFailedPath = ""
                        thumbnailRetryAttempt = 0
                        thumbnailRetryRevision = 0
                        queueThumbnailLoad(true)
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

                    onThumbnailRevisionChanged: {
                        thumbnailFailedPath = ""
                        thumbnailRetryAttempt = 0
                        thumbnailRetryRevision = 0
                        if (!thumbnailLoadEnabled) {
                            queueThumbnailLoad()
                        }
                    }

                    Component.onCompleted: {
                        queueThumbnailLoad(true)
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
                        thumbnailFailedPath = ""
                        thumbnailRetryAttempt = 0
                        thumbnailRetryRevision = 0
                        // A path can still be visible in the other panel. Cancellation needs
                        // per-delegate ownership before pooled delegates may cancel shared jobs.
                        if (root.controller.hoveredPath === path) {
                            root.clearHoveredItem(path)
                        }
                    }

                    GridView.onReused: {
                        isRenaming = false
                        visualOffsetY = 0
                        thumbnailFailedPath = ""
                        thumbnailRetryAttempt = 0
                        thumbnailRetryRevision = 0
                        queueThumbnailLoad(true)
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

                    function cancelRename() {
                        isRenaming = false
                    }

                    function focusRenameEditor(selectText) {
                        if (!isRenaming || !gridRenameLoader.item) {
                            root.traceRenameFocus("grid-focusRenameEditor-reject", "path=" + path + " hasItem=" + Boolean(gridRenameLoader.item))
                            return false
                        }
                        root.traceRenameFocus("grid-focusRenameEditor-before", "path=" + path + " select=" + (selectText === true))
                        gridRenameLoader.item.forceActiveFocus()
                        if (selectText === true) {
                            gridRenameLoader.item.select(0, gridRenameLoader.item.defaultSelectionEnd())
                        }
                        root.traceRenameFocus("grid-focusRenameEditor-after", "path=" + path + " activeFocus=" + gridRenameLoader.item.activeFocus)
                        return gridRenameLoader.item.activeFocus
                    }

                    function renameEditorHasFocus() {
                        return Boolean(gridRenameLoader.item && gridRenameLoader.item.activeFocus)
                    }

                    function isPointOnBadge(x, y) {
                        function findBadge(item) {
                            if (item.objectName === "gridSelectionBadge") return item
                            for (var i = 0; i < item.children.length; i++) {
                                var result = findBadge(item.children[i])
                                if (result) return result
                            }
                            return null
                        }
                        var badge = findBadge(gridDelegate)
                        if (!badge || !badge.visible) return false
                        var mapped = badge.mapFromItem(gridMouseArea, x, y)
                        return mapped.x >= 0 && mapped.y >= 0
                            && mapped.x < badge.width && mapped.y < badge.height
                    }

                    function isPointOnDragSurface(x, y) {
                        function within(item) {
                            if (!item || !item.visible) {
                                return false
                            }
                            const mapped = item.mapFromItem(gridDelegate, x, y)
                            return mapped.x >= 0 && mapped.y >= 0
                                    && mapped.x < item.width && mapped.y < item.height
                        }
                        return within(gridIconCell)
                    }

                    function queueThumbnailLoad(clearExisting) {
                        if (clearExisting === true || !canLoadThumbnail) {
                            thumbnailLoadEnabled = false
                        }
                        if (canScheduleThumbnail && !thumbnailLoadEnabled) {
                            thumbnailDelayTimer.restart()
                        } else {
                            thumbnailDelayTimer.stop()
                        }
                    }

                    function scheduleThumbnailRetry() {
                        if (!thumbnailRequestActive || thumbnailRetryAttempt >= 3) {
                            return
                        }
                        thumbnailRetryTimer.interval = 350 * Math.pow(2, thumbnailRetryAttempt)
                        thumbnailRetryTimer.restart()
                    }

                    function itemStateFill(hovered, fallbackColor) {
                        if (gridDelegate.isSelected) {
                            return gridDelegate.selectedStateFill
                        }
                        if (gridDelegate.currentItem) {
                            return gridDelegate.currentStateFill
                        }
                        if (hovered && !root.hoverSuppressed) {
                            return Theme.itemNeutralHoverFill
                        }
                        return fallbackColor
                    }

                    Connections {
                        target: root
                        function onResizeOptimizedChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                        function onUltraLightModeChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                        function onThumbnailLoadingPausedChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                        function onThumbnailSchedulingPausedChanged() {
                            gridDelegate.queueThumbnailLoad()
                        }
                    }

                    Timer {
                        id: thumbnailDelayTimer
                        interval: 100 + (Math.max(0, index) % 16) * 28
                        repeat: false
                        onTriggered: {
                            gridDelegate.thumbnailLoadEnabled = gridDelegate.canScheduleThumbnail
                            if (gridDelegate.thumbnailLoadEnabled && typeof thumbnailController !== "undefined" && thumbnailController) {
                                thumbnailController.requestThumbnail(path, root.gridIconSize * 2, root.gridIconSize * 2, 100, "visible")
                            }
                        }
                    }

                    Timer {
                        id: thumbnailRetryTimer
                        repeat: false
                        onTriggered: {
                            if (!gridDelegate.thumbnailRequestActive) {
                                return
                            }
                            gridDelegate.thumbnailRetryAttempt += 1
                            gridDelegate.thumbnailRetryRevision += 1
                        }
                    }

                    Rectangle {
                        id: gridSelectionBackground
                        anchors.fill: parent
                        anchors.margins: 4
                        visible: !gridDelegate.lightweightActive
                        radius: Theme.radiusMd
                        color: gridDelegate.itemStateFill(hoverGrid.hovered, "transparent")
                        border.color: "transparent"
                        border.width: 0
                        transform: Translate { y: gridDelegate.visualOffsetY }
                    }

                    HoverHandler { 
                        id: hoverGrid 
                        enabled: !gridDelegate.lightweightActive && !root.externalScrollAnySuppressionActive
                        cursorShape: root.internalDragEnabled
                                     ? root.itemHoverCursorShape(gridDelegate, point.position.x, point.position.y)
                                     : Qt.ArrowCursor
                        onHoveredChanged: {
                            if (root.hoverSuppressed) return
                            if (hovered) {
                                root.setHoveredItem(gridDelegate, path, point.position)
                                if (root.internalDragEnabled) {
                                    root.updateHoverDragCursor(gridDelegate, point.position.x, point.position.y)
                                }
                            } else {
                                root.clearHoveredItem(path)
                                root.clearHoverDragCursor(gridDelegate)
                            }
                        }
                        onPointChanged: {
                            if (hovered) {
                                root.setHoveredItem(gridDelegate, path, point.position)
                            }
                            if (hovered && root.internalDragEnabled) {
                                root.updateHoverDragCursor(gridDelegate, point.position.x, point.position.y)
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
                                            root.setHoveredItem(gridDelegate, path, hoverGrid.point.position)
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
                                            root.setHoveredItem(gridDelegate, path, hoverGrid.point.position)
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
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLabel
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
                            property bool canceling: false

                            opacity: 0
                            scale: 0.97
                            Behavior on opacity { enabled: !root.effectsReduced; NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }
                            Behavior on scale { enabled: !root.effectsReduced; NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }

                            background: Rectangle {
                                color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.92 : 0.96)
                                radius: Theme.controlRadius
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
                                        const adminRenameAvailable = root.Window.window
                                                                     && root.Window.window.adminModeActive
                                                                     && root.Window.window.adminModeActive()
                                                                     && ctrl.pathKindFor(path) === "local"
                                                                     && ctrl.renameAsAdministrator
                                        const renamed = adminRenameAvailable
                                                ? ctrl.renameAsAdministrator(idx, txt)
                                                : ctrl.rename(idx, txt)
                                        if (renamed) {
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
                                        || event.key === Qt.Key_Up || event.key === Qt.Key_Down
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
                                canceling = true
                                root.traceRenameFocus("grid-escape-cancel", "path=" + path)
                                isRenaming = false
                                root.cancelInlineRename()
                                event.accepted = true
                            }
                            onActiveFocusChanged: {
                                root.traceRenameFocus("grid-textField-activeFocus-changed", "path=" + path + " value=" + activeFocus)
                                if (!activeFocus && isRenaming && !committing && !canceling) {
                                    root.recoverInlineRenameFocus("grid-editor-focus-lost")
                                }
                            }
                            Component.onCompleted: {
                                root.traceRenameFocus("grid-textField-completed-before-focus", "path=" + path)
                                opacity = 1.0
                                scale = 1.0
                                forceActiveFocus()
                                select(0, gridRenameInput.defaultSelectionEnd())
                                root.traceRenameFocus("grid-textField-completed-after-focus", "path=" + path + " activeFocus=" + activeFocus)
                            }
                        }
                    }

                    FileGridResizeDelegate {
                        anchors.fill: parent
                        visible: gridDelegate.lightweightActive
                        z: 5
                        panel: root
                        index: gridDelegate.index
                        name: gridDelegate.name
                        path: gridDelegate.path
                        iconName: gridDelegate.iconName
                        suffix: gridDelegate.suffix
                        mimeType: gridDelegate.mimeType
                        isDirectory: gridDelegate.isDirectory
                        isSelected: gridDelegate.isSelected
                        isHidden: gridDelegate.isHidden
                        isArchiveFile: gridDelegate.isArchiveFile
                        isIsoImageFile: gridDelegate.isIsoImageFile
                        primaryBadgeKind: gridDelegate.primaryBadgeKind
                        isPinned: gridDelegate.isPinned
                        currentItem: gridDelegate.currentItem
                        panelActive: gridDelegate.panelActive
                        gridIconSize: root.gridIconSize
                        onClicked: (mouse) => root.handleItemClick(index, mouse)
                        onRightClicked: root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                        onDoubleClicked: root.openItem(index)
                    }

                    ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: gridDelegate.contentMargin
                    spacing: gridDelegate.contentSpacing
                    visible: !gridDelegate.lightweightActive
                    transform: Translate { y: gridDelegate.visualOffsetY }

                    Item {
                        id: gridIconFrame
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.gridIconSize
                        Layout.preferredHeight: root.gridIconSize
                        FileIconCell {
                            id: gridIconCell
                            anchors.centerIn: parent
                            width: Math.max(28, Math.round(root.gridIconSize * 0.8))
                            height: width
                            path: gridDelegate.path
                            name: gridDelegate.name
                            iconName: gridDelegate.iconName
                            mimeType: gridDelegate.mimeType
                            isDirectory: gridDelegate.isDirectory
                            hasThumbnail: gridDelegate.hasThumbnail
                            primaryBadgeKind: gridDelegate.primaryBadgeKind
                            isPinned: gridDelegate.isPinned
                            suffix: gridDelegate.suffix
                            useNativeIcons: root.effectiveUseNativeIcons
                            thumbnailSource: gridDelegate.thumbnailRequestActive
                                             ? root.thumbnailSourceFor(gridDelegate.path, gridDelegate.thumbnailRevision + gridDelegate.thumbnailRetryRevision * 1000000)
                                             : ""
                            showThumbnail: gridDelegate.thumbnailRequestActive
                            iconSize: width
                            onThumbnailError: {
                                gridDelegate.thumbnailFailedPath = gridDelegate.path
                                gridDelegate.thumbnailLoadEnabled = false
                            }
                            onThumbnailSoftMiss: gridDelegate.scheduleThumbnailRetry()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !isRenaming
                        text: {
                            if (isDirectory || !suffix) return name
                            const extLen = suffix.length
                            if (extLen > 0 && name.endsWith("." + suffix)) {
                                const baseName = name.substring(0, name.length - extLen - 1)
                                return baseName + "<font color='" + TextColors.fileExtensionText.toString() + "'>." + suffix + "</font>"
                            }
                            return name
                        }
                        textFormat: Text.StyledText
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLabel
                        color: isDirectory ? TextColors.folderNameText : TextColors.fileNameText
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                    }
                    }
                    MouseArea {
                        id: gridMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        visible: !gridDelegate.lightweightActive
                        scrollGestureEnabled: false
                        preventStealing: gridDelegate.dragCandidate || gridDelegate.badgePressed
                        cursorShape: root.internalDragEnabled
                                     && containsMouse
                                     ? root.itemHoverCursorShape(gridDelegate, mouseX, mouseY)
                                     : Qt.ArrowCursor
                        onWheel: (wheel) => { wheel.accepted = false }
                        onPressed: (mouse) => {
                            root.cancelInlineRenameForNavigation("grid-item-press")
                            gridDelegate.badgePressed = mouse.button === Qt.LeftButton
                                                         && gridDelegate.isPointOnBadge(mouse.x, mouse.y)
                            gridDelegate.dragCandidate = root.internalDragEnabled
                                                         && mouse.button === Qt.LeftButton
                                                         && !gridDelegate.isRenaming
                                                         && !gridDelegate.badgePressed
                                                         && (gridDelegate.isSelected
                                                             || gridDelegate.isPointOnDragSurface(mouse.x, mouse.y))
                            gridDelegate.dragStarted = false
                            gridDelegate.dragStartX = mouse.x
                            gridDelegate.dragStartY = mouse.y
                        }

                        onPositionChanged: (mouse) => {
                            if (gridDelegate.badgePressed) {
                                return
                            }
                            if (!gridDelegate.dragCandidate) {
                                return
                            }
                            if (gridDelegate.dragStarted) {
                                root.updateSelectionDragPosition(mouse, gridDelegate)
                            } else {
                                gridDelegate.dragStarted = root.updateSelectionDragCandidate(
                                            gridDelegate.index, gridDelegate.path,
                                            gridDelegate.dragStartX, gridDelegate.dragStartY,
                                            mouse.x, mouse.y, mouse)
                                if (gridDelegate.dragStarted) {
                                    root.updateSelectionDragPosition(mouse, gridDelegate)
                                }
                            }
                        }

                        onReleased: (mouse) => {
                            if (gridDelegate.dragStarted) {
                                root.finishSelectionDrag(mouse, gridDelegate)
                                gridDelegate.suppressClickAfterDrag = true
                                gridSuppressClickReset.restart()
                            }
                            gridDelegate.dragCandidate = false
                            gridDelegate.dragStarted = false
                            gridDelegate.badgePressed = false
                        }

                        onCanceled: {
                            if (gridDelegate.dragStarted && root.internalDragEnabled && root.dragCoordinator) {
                                root.dragCoordinator.cancelDrag("Drag canceled.")
                            }
                            gridDelegate.dragCandidate = false
                            gridDelegate.dragStarted = false
                            gridDelegate.badgePressed = false
                        }

                        onClicked: (mouse) => {
                            if (gridDelegate.suppressClickAfterDrag) {
                                gridDelegate.suppressClickAfterDrag = false
                                return
                            }
                            if (mouse.button === Qt.RightButton) root.handleItemRightClick(index, path, isArchiveFile, isIsoImageFile)
                            else if (gridDelegate.isPointOnBadge(mouse.x, mouse.y)) root.controller.directoryModel.toggleSelected(gridDelegate.index)
                            else root.handleItemClick(index, mouse)
                        }
                        onDoubleClicked: (mouse) => {
                            root.openItem(index)
                        }
                    }

                    Timer {
                        id: gridSuppressClickReset
                        interval: 0
                        repeat: false
                        onTriggered: gridDelegate.suppressClickAfterDrag = false
                    }

                    SelectionToggleBadge {
                        objectName: "gridSelectionBadge"
                        x: 8
                        y: 8 + gridDelegate.visualOffsetY
                        z: 30
                        badgeSize: Math.max(16, Math.min(21, Math.round(root.gridIconSize * 0.24)))
                        markSize: Math.max(7, Math.round(badgeSize * 0.4))
                        markStroke: badgeSize >= 22 ? 1.2 : 1
                        available: root.showSelectionBadges && !gridDelegate.lightweightActive
                        controller: root.controller
                        panel: root
                        index: gridDelegate.index
                        selected: gridDelegate.isSelected
                        hovered: hoverGrid.hovered
                        currentItem: gridDelegate.currentItem
                        scrolling: root.hoverSuppressed || gridDelegate.isRenaming
                    }
                }

                // Empty area for GridView
                MouseArea {
                    anchors.fill: parent
                    z: 8
                    enabled: root.emptyAreaInputEnabled()
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    preventStealing: true
                    cursorShape: root.panelHoverCursorShape()
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
                    cursorShape: root.internalDragEnabled && root.dragCoordinator && root.dragCoordinator.active
                                 ? root.selectionDragCursorShape()
                                 : Qt.ArrowCursor
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
                    cursorShape: root.internalDragEnabled && root.dragCoordinator && root.dragCoordinator.active
                                 ? root.selectionDragCursorShape()
                                 : Qt.ArrowCursor
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
                    cursorShape: root.internalDragEnabled && root.dragCoordinator && root.dragCoordinator.active
                                 ? root.selectionDragCursorShape()
                                 : Qt.ArrowCursor
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
                    cursorShape: root.internalDragEnabled && root.dragCoordinator && root.dragCoordinator.active
                                 ? root.selectionDragCursorShape()
                                 : Qt.ArrowCursor
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

                Canvas {
                    id: rubberBandBorder
                    anchors.fill: parent
                    readonly property color strokeColor: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.78 : 0.64)

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        if (width <= 1 || height <= 1) {
                            return
                        }

                        const inset = 0.75
                        const w = width - inset * 2
                        const h = height - inset * 2
                        const r = Math.min(Theme.radiusSm, w / 2, h / 2)
                        ctx.save()
                        ctx.lineWidth = 1.25
                        ctx.strokeStyle = strokeColor
                        if (ctx.setLineDash) {
                            ctx.setLineDash([5, 6])
                        }
                        ctx.beginPath()
                        ctx.moveTo(inset + r, inset)
                        ctx.lineTo(inset + w - r, inset)
                        ctx.quadraticCurveTo(inset + w, inset, inset + w, inset + r)
                        ctx.lineTo(inset + w, inset + h - r)
                        ctx.quadraticCurveTo(inset + w, inset + h, inset + w - r, inset + h)
                        ctx.lineTo(inset + r, inset + h)
                        ctx.quadraticCurveTo(inset, inset + h, inset, inset + h - r)
                        ctx.lineTo(inset, inset + r)
                        ctx.quadraticCurveTo(inset, inset, inset + r, inset)
                        ctx.stroke()
                        ctx.restore()
                    }

                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onStrokeColorChanged: requestPaint()
                }
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
                quickLookPopup: root.quickLookPopup
                liveResizeActive: root.liveResizeActive
                visible: root.controller.isFavoritesRoot
                enabled: visible
                focus: root.active && root.controller.isFavoritesRoot
                onActivated: root.activated()
            }

            FilePanelOverlayHost {
                id: filePanelOverlayHost
                anchors.fill: parent
                z: 17
                panelRoot: root
            }

            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.RightButton
                enabled: !root.virtualRootMode
                onClicked: (mouse) => {
                    root.activated()
                    root.popupFilePanelEmptyMenu()
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
                        root.showStatusMessage("Path copied")
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
                useNativeIcons: root.effectiveUseNativeIcons
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
                        root.workspaceController.requestDelete(root.controller.selectedPaths(), root.controller.currentPath,
                                                               root.controller.selectedItems ? root.controller.selectedItems() : [])
                    }
                }
                onPinToggleRequested: (paths, allPinned) => {
                    root.activated()
                    if (!root.favoritesBackend || !paths || paths.length === 0) {
                        return
                    }
                    for (let i = 0; i < paths.length; ++i) {
                        if (root.controller.pathKindFor(paths[i]) !== "local") {
                            const window = root.Window.window
                            if (window && window.showTransientInfo) {
                                window.showTransientInfo("This location cannot be pinned to Favorites")
                            }
                            return
                        }
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
                isCurrentPathManagedIsoMount: root.isCurrentPathManagedIsoMount
                loadingProgress: root.controller.directoryModel ? root.controller.directoryModel.scanProgress : -1
                loadingProgressText: root.controller.directoryModel ? root.controller.directoryModel.scanProgressText : ""
                loadingCancelable: root.isCurrentPathArchive
                                   && root.controller.directoryModel
                                   && root.controller.directoryModel.loading
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
                onCancelLoadingRequested: root.controller.cancelCurrentLoad()
            }
        }

        function onCountChanged() {
            filePanelLoadingPolicy.handleDirectoryCountChanged()
        }
    }
}

}
