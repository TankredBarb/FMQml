import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import "common"
import "framework"
import "../style"
import "sidebar"

Pane {
    id: root

    padding: 0

    property alias placesList: placesList
    property alias recentList: recentList
    property alias foldersTree: foldersTree
    readonly property var panelCatalog: ["places", "recent", "folders"]
    property var panelOrder: ["places", "recent", "folders"]
    property var hiddenPanelIds: ["recent"]
    property var collapsedPanelIds: []
    property var panelWeights: ({ "places": 1.0, "recent": 1.0, "folders": 1.0 })
    readonly property var effectivePanelOrder: root.normalizedPanelOrder(root.panelOrder)
    readonly property var effectiveHiddenPanelIds: root.normalizedPanelSubset(root.hiddenPanelIds)
    readonly property var effectiveCollapsedPanelIds: root.normalizedPanelSubset(root.collapsedPanelIds)
    property string lastFocusedPanelId: "places"
    property bool trapTabNavigation: false
    property int selectedPlaceIndex: -2
    property string selectedPlacePath: ""
    property bool liveResizeActive: false
    property bool sectionResizeActive: false
    property bool placesScrollActive: false
    property bool recentScrollActive: false
    property bool treeScrollActive: false
    property string pendingScrollPreviewPath: ""
    property int treeSyncRequestId: 0
    property string treeSyncTargetPath: ""
    property var activePanelViewProvider: null
    readonly property bool activePathIsProvider: {
        const panel = root.activePanelController()
        return Boolean(panel && panel.pathIsProvider(panel.currentPath || ""))
    }
    readonly property bool sidebarScrollActive: root.placesScrollActive || root.recentScrollActive || root.treeScrollActive
    readonly property bool placesTraceEnabled: Qt.application.arguments.indexOf("--places-trace") >= 0
    property real lastPlacesTraceContentY: -1000000
    property double lastPlacesTraceTime: 0
    property real lastPlacesContentYValue: 0
    property double lastPlacesContentYTime: 0
    readonly property int placePathRole: Qt.UserRole + 2
    readonly property int placeSectionHeaderHeight: Math.max(18, Theme.fontSizeMicro + 8)
    readonly property int placeCompactRowHeight: Math.max(38, Theme.fontSizeLabel + 24)
    readonly property int placeExpandedRowHeight: Math.max(45, Theme.fontSizeLabel + Theme.fontSizeCaption + 24)
    readonly property int placeIconSize: 21
    readonly property int placePrimaryFontSize: Theme.fontSizeBody
    readonly property int placeSecondaryFontSize: Theme.fontSizeCaption
    readonly property int placeHorizontalPadding: 9
    readonly property int placeRowSpacing: 9
    readonly property int placeSecondaryVerticalMargin: 4
    readonly property int placeUsageBottomMargin: 4
    readonly property int placeUsageBarHeight: 3
    readonly property bool effectsReduced: root.liveResizeActive || root.sectionResizeActive
    readonly property bool interactionEffectsReduced: root.effectsReduced || root.sidebarScrollActive
    readonly property bool containsActiveFocus: placesList.activeFocus || recentList.activeFocus || foldersTree.activeFocus
    readonly property bool hasEnabledPanels: root.effectivePanelOrder.length > root.effectiveHiddenPanelIds.length
    readonly property bool compactMode: {
        const enabled = root.enabledPanelOrder()
        if (enabled.length === 0) return false
        for (let i = 0; i < enabled.length; ++i) {
            if (!root.panelCollapsed(enabled[i])) return false
        }
        return true
    }
    readonly property color sidebarSelectedFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark ? 0.34 : 0.28)
    readonly property color sidebarCurrentFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark ? 0.18 : 0.14)

    signal configurationChanged()
    signal panelEnabledPreferenceChanged(string panelId, bool enabled)
    signal panelCollapsedPreferenceChanged(string panelId, bool collapsed)

    function sidebarStateFill(selected, current, hovered, pressed) {
        if (selected) {
            return root.sidebarSelectedFill
        }
        if (current) {
            return root.sidebarCurrentFill
        }
        if (pressed) {
            return Theme.surfaceActive
        }
        if (hovered) {
            return Theme.itemNeutralHoverFill
        }
        return "transparent"
    }

    function sidebarStateFillTop(selected, current, hovered, pressed) {
        if (selected || current) {
            return Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.35 : 0.28)
        }
        if (pressed) {
            return Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.18 : 0.13)
        }
        if (hovered) {
            return Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.16 : 0.11)
        }
        return "transparent"
    }

    function sidebarStateFillBottom(selected, current, hovered, pressed) {
        if (selected || current) {
            return Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.19 : 0.16)
        }
        if (pressed) {
            return Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.34 : 0.24)
        }
        if (hovered) {
            return Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.25 : 0.17)
        }
        return "transparent"
    }

    function normalizedPanelOrder(order) {
        const normalized = []
        const requested = order !== undefined && order !== null ? order : []
        const requestedLength = typeof requested.length === "number" ? requested.length : 0
        for (let i = 0; i < requestedLength; ++i) {
            const panelId = String(requested[i])
            if (root.panelCatalog.indexOf(panelId) >= 0 && normalized.indexOf(panelId) < 0) {
                normalized.push(panelId)
            }
        }
        for (let j = 0; j < root.panelCatalog.length; ++j) {
            const fallbackId = root.panelCatalog[j]
            if (normalized.indexOf(fallbackId) < 0) {
                normalized.push(fallbackId)
            }
        }
        return normalized
    }

    function panelStackHeight(panelId) {
        if (!root.panelEnabled(panelId)) return 0
        if (root.panelCollapsed(panelId)) return placesCard.headerHeight

        return root.expandedPanelHeights()[panelId] || placesCard.headerHeight
    }

    function expandedPanelHeights() {
        const enabledOrder = root.enabledPanelOrder()
        let collapsedCount = 0
        const expandedIds = []
        for (let i = 0; i < enabledOrder.length; ++i) {
            if (root.panelCollapsed(enabledOrder[i])) ++collapsedCount
            else expandedIds.push(enabledOrder[i])
        }
        const gapsHeight = Math.max(0, enabledOrder.length - 1) * sidebarStack.spacing
        const expandedSpace = Math.max(0, sidebarStack.height
                - gapsHeight
                - collapsedCount * placesCard.headerHeight)
        const heights = {}
        if (expandedIds.length === 0) return heights

        const minimum = Math.min(placesCard.headerHeight + 44,
                                 expandedSpace / expandedIds.length)
        let remainingIds = expandedIds.slice()
        let remainingSpace = expandedSpace
        while (remainingIds.length > 0) {
            let remainingWeight = 0
            for (let j = 0; j < remainingIds.length; ++j) {
                remainingWeight += root.panelWeight(remainingIds[j])
            }
            let constrainedId = ""
            for (let k = 0; k < remainingIds.length; ++k) {
                const candidateId = remainingIds[k]
                const candidateHeight = remainingSpace * root.panelWeight(candidateId) / remainingWeight
                if (candidateHeight < minimum) {
                    constrainedId = candidateId
                    break
                }
            }
            if (!constrainedId) {
                for (let m = 0; m < remainingIds.length; ++m) {
                    const finalId = remainingIds[m]
                    heights[finalId] = remainingSpace * root.panelWeight(finalId) / remainingWeight
                }
                break
            }
            heights[constrainedId] = minimum
            remainingSpace -= minimum
            remainingIds.splice(remainingIds.indexOf(constrainedId), 1)
        }
        return heights
    }

    function panelStackY(panelId) {
        const enabledOrder = root.enabledPanelOrder()
        let y = 0
        for (let i = 0; i < enabledOrder.length; ++i) {
            if (enabledOrder[i] === panelId) return y
            y += root.panelStackHeight(enabledOrder[i]) + sidebarStack.spacing
        }
        return y
    }

    function normalizedPanelSubset(panelIds) {
        const normalized = []
        const requested = panelIds !== undefined && panelIds !== null ? panelIds : []
        const requestedLength = typeof requested.length === "number" ? requested.length : 0
        for (let i = 0; i < requestedLength; ++i) {
            const panelId = String(requested[i])
            if (root.panelCatalog.indexOf(panelId) >= 0 && normalized.indexOf(panelId) < 0) {
                normalized.push(panelId)
            }
        }
        return normalized
    }

    function normalizedPanelWeights(weights) {
        const source = weights || {}
        const normalized = {}
        for (let i = 0; i < root.panelCatalog.length; ++i) {
            const panelId = root.panelCatalog[i]
            const candidate = Number(source[panelId])
            normalized[panelId] = isFinite(candidate) && candidate > 0
                    ? Math.max(0.05, Math.min(candidate, 20.0)) : 1.0
        }
        return normalized
    }

    function panelWeight(panelId) {
        const candidate = Number(root.panelWeights[panelId])
        return isFinite(candidate) && candidate > 0 ? candidate : 1.0
    }

    function resizeDividerPairs() {
        const enabled = root.enabledPanelOrder()
        const pairs = []
        for (let i = 0; i < enabled.length - 1; ++i) {
            pairs.push({ beforeId: enabled[i], afterId: enabled[i + 1] })
        }
        return pairs
    }

    function resizePanelPair(beforeId, afterId, beforeHeight, afterHeight, delta) {
        const pairHeight = beforeHeight + afterHeight
        if (pairHeight <= 0) return
        const minimum = Math.min(placesCard.headerHeight + 44, pairHeight / 2)
        const resizedBefore = Math.max(minimum, Math.min(beforeHeight + delta, pairHeight - minimum))
        const pairWeight = root.panelWeight(beforeId) + root.panelWeight(afterId)
        const weights = root.normalizedPanelWeights(root.panelWeights)
        weights[beforeId] = pairWeight * resizedBefore / pairHeight
        weights[afterId] = pairWeight - weights[beforeId]
        root.panelWeights = weights
    }

    function panelEnabled(panelId) {
        return root.effectiveHiddenPanelIds.indexOf(panelId) < 0
    }

    function panelCollapsed(panelId) {
        return root.effectiveCollapsedPanelIds.indexOf(panelId) >= 0
    }

    function panelContainsActiveFocus(panelId) {
        if (panelId === "places") return placesList.activeFocus
        if (panelId === "recent") return recentList.activeFocus
        if (panelId === "folders") return foldersTree.activeFocus
        return false
    }

    function enabledPanelOrder() {
        return root.effectivePanelOrder.filter(function(panelId) {
            return root.panelEnabled(panelId)
        })
    }

    function focusablePanelOrder(excludedPanelId) {
        return root.effectivePanelOrder.filter(function(panelId) {
            return panelId !== excludedPanelId
                    && root.panelEnabled(panelId)
                    && !root.panelCollapsed(panelId)
                    && (panelId !== "folders" || !root.activePathIsProvider)
        })
    }

    function resolveFocusBeforeUnavailable(panelId) {
        if (!root.panelContainsActiveFocus(panelId)) return
        const candidates = root.focusablePanelOrder(panelId)
        if (candidates.length > 0) {
            root.focusPanelById(candidates[0])
        } else {
            workspaceController.focusActivePanel()
        }
    }

    function focusPanelById(panelId) {
        if (!root.panelEnabled(panelId) || root.panelCollapsed(panelId)) return false
        if (panelId === "folders") {
            if (root.activePathIsProvider) return false
            foldersTree.forceActiveFocus()
            return true
        }
        if (panelId === "places") {
            placesList.forceActiveFocus()
            return true
        }
        if (panelId === "recent") {
            recentList.forceActiveFocus()
            return true
        }
        return false
    }

    function focusAdjacentPanel(panelId, direction) {
        const order = root.focusablePanelOrder("")
        const currentIndex = order.indexOf(panelId)
        if (currentIndex < 0 || order.length === 0) return false
        const nextIndex = (currentIndex + direction + order.length) % order.length
        return root.focusPanelById(order[nextIndex])
    }

    function setPanelOrder(order) {
        const normalized = root.normalizedPanelOrder(order)
        if (JSON.stringify(root.effectivePanelOrder) === JSON.stringify(normalized)) return
        root.panelOrder = normalized
        root.configurationChanged()
    }

    function setPanelEnabled(panelId, enabled) {
        if (root.panelCatalog.indexOf(panelId) < 0) return false
        const hidden = root.effectiveHiddenPanelIds.slice()
        const index = hidden.indexOf(panelId)
        if (enabled && index >= 0) {
            hidden.splice(index, 1)
        } else if (!enabled && index < 0) {
            root.resolveFocusBeforeUnavailable(panelId)
            hidden.push(panelId)
        }
        root.hiddenPanelIds = hidden
        root.panelEnabledPreferenceChanged(panelId, enabled)
        root.configurationChanged()
        return true
    }

    function setPanelCollapsed(panelId, collapsed) {
        if (root.panelCatalog.indexOf(panelId) < 0) return false
        const collapsedIds = root.effectiveCollapsedPanelIds.slice()
        const index = collapsedIds.indexOf(panelId)
        if (collapsed && index < 0) {
            root.resolveFocusBeforeUnavailable(panelId)
            collapsedIds.push(panelId)
        } else if (!collapsed && index >= 0) {
            collapsedIds.splice(index, 1)
        }
        root.collapsedPanelIds = collapsedIds
        if (panelId === "folders" && !collapsed && index >= 0) {
            Qt.callLater(root.syncTreeToActivePath)
        }
        root.panelCollapsedPreferenceChanged(panelId, collapsed)
        root.configurationChanged()
        return true
    }

    function focusSidebar(trapTab) {
        trapTabNavigation = trapTab === true
        if (!root.focusPanelById(lastFocusedPanelId)) {
            root.focusPanelById(root.effectivePanelOrder[0])
        }
    }

    function clearPlaceSelection() {
        root.selectedPlaceIndex = -2
        root.selectedPlacePath = ""
    }

    function placePathForIndex(index) {
        if (index === -1) {
            return "devices://"
        }
        if (index < 0 || index >= placesList.count) {
            return ""
        }
        const modelIndex = workspaceController.placesModel.index(index, 0)
        return workspaceController.placesModel.data(modelIndex, root.placePathRole) || ""
    }

    function activePanelMatchesSelectedPlace() {
        if (root.selectedPlaceIndex === -2 || root.selectedPlacePath.length === 0) {
            return true
        }

        const panel = root.activePanelController()
        if (!panel) {
            return false
        }

        if (root.selectedPlacePath === "devices://") {
            return panel.isDeviceRoot
        }

        return !panel.isDeviceRoot && root.pathsEqual(panel.currentPath, root.selectedPlacePath)
    }

    function updatePlaceSelectionForActivePath() {
        if (!root.activePanelMatchesSelectedPlace()) {
            root.clearPlaceSelection()
        }
    }

    function setSelectedPlaceIndex(index) {
        const path = root.placePathForIndex(index)
        if (path.length === 0) {
            root.clearPlaceSelection()
            return false
        }

        root.selectedPlaceIndex = index
        root.selectedPlacePath = path
        return true
    }

    function syncTreeToActivePath() {
        syncTimer.restart()
    }

    function placesScrollInputActive() {
        return placesList.activeFocus
            || placesListHover.hovered
            || placesListVerticalScrollBar.pressed
    }

    function markPlacesScrollActivity() {
        if (!root.placesScrollInputActive()) {
            return
        }
        root.placesScrollActive = true
        root.tracePlacesScroll("activity")
        placesScrollStopTimer.restart()
    }

    function tracePlaces(message) {
        if (root.placesTraceEnabled) {
            console.log("[PlacesTrace][Sidebar] t=" + Date.now() + " " + message)
        }
    }

    function tracePlacesScroll(reason) {
        if (!root.placesTraceEnabled) {
            return
        }
        const now = Date.now()
        if (Math.abs(placesList.contentY - root.lastPlacesTraceContentY) < 64
                && now - root.lastPlacesTraceTime < 250) {
            return
        }
        root.lastPlacesTraceContentY = placesList.contentY
        root.lastPlacesTraceTime = now
        root.tracePlaces(reason
                         + " contentY=" + Math.round(placesList.contentY)
                         + " count=" + placesList.count
                         + " visible=" + placesList.visibleArea.heightRatio.toFixed(3)
                         + " moving=" + placesList.moving
                         + " dragging=" + placesList.dragging
                         + " scrollActive=" + root.placesScrollActive)
    }

    function tracePlacesContentYFrame() {
        if (!root.placesTraceEnabled) {
            return
        }
        const now = Date.now()
        if (root.lastPlacesContentYTime > 0) {
            const dt = now - root.lastPlacesContentYTime
            const dy = placesList.contentY - root.lastPlacesContentYValue
            if (placesList.moving && dt > 80) {
                root.tracePlaces("scrollGap dt=" + dt
                                 + " dy=" + Math.round(dy)
                                 + " contentY=" + Math.round(placesList.contentY)
                                 + " count=" + placesList.count
                                 + " scrollActive=" + root.placesScrollActive)
            }
        }
        root.lastPlacesContentYTime = now
        root.lastPlacesContentYValue = placesList.contentY
    }

    function treeScrollInputActive() {
        return foldersTree.activeFocus
            || foldersTreeHover.hovered
            || foldersTreeVerticalScrollBar.pressed
    }

    function markTreeScrollActivity() {
        if (!root.treeScrollInputActive()) {
            return
        }
        root.treeScrollActive = true
        treeScrollStopTimer.restart()
    }

    function clearTreeSelection() {
        if (foldersTree.selectionModel) {
            foldersTree.selectionModel.clear()
        }
    }

    function selectTreeIndex(index) {
        if (!index || !index.valid) return false

        // QML TreeView doesn't have expandToIndex. We must expand ancestors top-down.
        let current = index
        let ancestors = []
        while (current && current.valid) {
            let p = workspaceController.treeModel.parentIndex(current)
            if (p && p.valid) {
                ancestors.unshift(p)
                current = p
            } else {
                break
            }
        }

        for (let i = 0; i < ancestors.length; ++i) {
            let r = foldersTree.rowAtIndex(ancestors[i])
            if (r >= 0) {
                if (!foldersTree.isExpanded(r)) {
                    foldersTree.expand(r)
                    foldersTree.forceLayout()
                }
            }
        }

        let finalRow = foldersTree.rowAtIndex(index)
        if (finalRow < 0) {
            foldersTree.forceLayout()
            finalRow = foldersTree.rowAtIndex(index)
        }

        if (finalRow < 0) return false

        if (foldersTree.selectionModel) {
            foldersTree.selectionModel.setCurrentIndex(index,
                ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
        }
        foldersTree.positionViewAtRow(finalRow, TableView.Contain)
        return true
    }

    function activePanelController() {
        return workspaceController.activePanel === 0
            ? workspaceController.leftPanel
            : workspaceController.rightPanel
    }

    function activePanelView() {
        return root.activePanelViewProvider ? root.activePanelViewProvider() : null
    }

    function openPathInActivePanel(path) {
        if (!path) return
        const panelView = root.activePanelView()
        if (panelView && panelView.openPath) {
            panelView.openPath(path)
            return
        }
        const panel = root.activePanelController()
        if (panel) panel.openPath(path)
    }

    function prepareNavigation(reason) {
        const panelView = root.activePanelView()
        if (panelView && panelView.cancelInlineRenameForNavigation) {
            panelView.cancelInlineRenameForNavigation(reason)
        }
    }

    function previewPath(path) {
        if (!path || typeof quickLookController === "undefined" || !quickLookController) return
        if (root.sidebarScrollActive) {
            root.pendingScrollPreviewPath = path
            return
        }
        quickLookController.preview(path)
    }

    function flushPendingScrollPreview() {
        if (root.sidebarScrollActive || root.pendingScrollPreviewPath.length === 0) {
            return
        }
        const path = root.pendingScrollPreviewPath
        root.pendingScrollPreviewPath = ""
        if (typeof quickLookController !== "undefined" && quickLookController) {
            quickLookController.preview(path)
        }
    }

    function previewCurrentPlace() {
        if (!placesList.activeFocus) return

        if (placesList.currentIndex === -1) {
            root.previewPath("devices://")
            return
        }

        if (placesList.currentIndex < 0 || placesList.currentIndex >= placesList.count) return

        const modelIndex = workspaceController.placesModel.index(placesList.currentIndex, 0)
        const path = workspaceController.placesModel.data(modelIndex, root.placePathRole)
        if (workspaceController.placesModel.data(modelIndex, Qt.UserRole + 4) && path) {
            quickLookController.previewDrive({
                rootPath: path,
                name: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 1),
                totalBytes: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 5),
                freeBytes: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 6),
                fileSystem: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 9),
                driveType: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 10),
                critical: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 12),
                deviceDescription: workspaceController.placesModel.data(modelIndex, Qt.UserRole + 25),
                blockDevice: ""
            })
        } else {
            root.previewPath(path)
        }
    }

    function setPlaceCurrentIndex(index) {
        const validIndex = index === -1 || (index >= 0 && index < placesList.count)
        placesList.currentIndex = validIndex ? index : -1
        if (index === -1) {
            placesList.positionViewAtBeginning()
        } else if (index >= 0 && index < placesList.count) {
            placesList.positionViewAtIndex(index, ListView.Contain)
        }
    }

    function previewCurrentFolderTreeItem() {
        if (!foldersTree.activeFocus || !foldersTree.selectionModel) return

        const idx = foldersTree.selectionModel.currentIndex
        if (idx === undefined || idx === null || !idx.valid) return

        root.previewPath(workspaceController.treeModel.pathForIndex(idx))
    }

    function previewRecentPath(path, exists) {
        if (exists && path) root.previewPath(path)
    }

    function setRecentCurrentIndex(index) {
        if (recentList.count <= 0) {
            recentList.currentIndex = -1
            return
        }
        const bounded = Math.max(0, Math.min(index, recentList.count - 1))
        recentList.currentIndex = bounded
        recentList.positionViewAtIndex(bounded, ListView.Contain)
    }

    function previewCurrentRecent() {
        if (!recentList.activeFocus) return
        const item = recentList.currentItem
        if (item) root.previewRecentPath(item.itemTargetPath, item.itemExists)
    }

    function openCurrentRecent() {
        const item = recentList.currentItem
        if (item && item.itemExists) root.openPathInActivePanel(item.itemTargetPath)
    }

    function selectPlace(index) {
        root.trapTabNavigation = false
        placesList.forceActiveFocus()
        root.setPlaceCurrentIndex(index)
        if (!root.setSelectedPlaceIndex(index)) {
            return
        }
        root.previewCurrentPlace()
    }

    function openSelectedPlace() {
        const index = placesList.currentIndex
        if (index < -1 || index >= placesList.count) {
            return
        }

        if (!root.setSelectedPlaceIndex(index)) {
            return
        }

        if (index === -1) {
            root.openPathInActivePanel("devices://")
            return
        }

        const modelIndex = workspaceController.placesModel.index(index, 0)
        const path = workspaceController.placesModel.data(modelIndex, root.placePathRole)
        root.openPathInActivePanel(path)
    }

    function resetPlaceDriveMenu() {
        placeDriveContextMenu.reset()
    }

    function closePlaceDriveMenuForPath(path) {
        if (root.pathsEqual(placeDriveContextMenu.drivePath, path)) {
            placeDriveContextMenu.close()
            root.resetPlaceDriveMenu()
        }
    }

    function openPlaceDriveMenu(index, path, driveType, canEject, canUnmount, canSafelyRemove, canMount, mountId, actionPending, isDrive) {
        if (!isDrive || !path) return

        placeDriveContextMenu.driveIndex = index
        placeDriveContextMenu.drivePath = path
        placeDriveContextMenu.driveType = driveType || ""
        placeDriveContextMenu.canEject = canEject === true
        placeDriveContextMenu.canUnmount = canUnmount === true
        placeDriveContextMenu.canSafelyRemove = canSafelyRemove === true
        placeDriveContextMenu.canMount = canMount === true
        placeDriveContextMenu.mountId = mountId || ""
        placeDriveContextMenu.actionPending = actionPending === true
        placeDriveContextMenu.managedIsoMount = workspaceController.isManagedIsoMountRoot(path)
        placeDriveContextMenu.popup()
    }

    Timer {
        id: syncTimer
        interval: 120
        repeat: false
        onTriggered: {
            let panel = workspaceController.activePanel === 0
                ? workspaceController.leftPanel
                : workspaceController.rightPanel
            
            if (!panel) return;

            root.treeSyncRequestId += 1
            root.treeSyncTargetPath = panel.currentPath || ""

            if (root.activePathIsProvider) {
                root.clearTreeSelection()
                return
            }

            // Handle virtual root (This PC)
            if (panel.isDeviceRoot) {
                root.clearTreeSelection()
                return
            }

            let targetPath = root.treeSyncTargetPath

            // Keep panel navigation independent from folder enumeration.
            let index = workspaceController.treeModel.nearestLoadedIndexForPath(targetPath, 0)
            
            if (!index || !index.valid) {
                root.clearTreeSelection()
                return
            }

            root.selectTreeIndex(index)
            workspaceController.treeModel.revealPathAsync(targetPath, root.treeSyncRequestId)
        }
    }

    Timer {
        id: treeScrollStopTimer
        interval: 160
        repeat: false
        onTriggered: {
            root.treeScrollActive = false
            root.flushPendingScrollPreview()
        }
    }

    Timer {
        id: recentScrollStopTimer
        interval: 160
        repeat: false
        onTriggered: {
            root.recentScrollActive = false
            root.flushPendingScrollPreview()
        }
    }

    Timer {
        id: placesScrollStopTimer
        interval: 650
        repeat: false
        onTriggered: {
            root.placesScrollActive = false
            root.flushPendingScrollPreview()
        }
    }

    function pathsEqual(lhs, rhs) {
        if (!lhs || !rhs) return false;
        
        // Strip trailing slashes
        let cleanLhs = lhs.replace(/[/\\]$/, "")
        let cleanRhs = rhs.replace(/[/\\]$/, "")

        if (Qt.platform.os === "windows") {
            let eq = cleanLhs.toLowerCase() === cleanRhs.toLowerCase()
            // console.log("[Sidebar] pathsEqual(win) lhs:", lhs, "rhs:", rhs, "eq:", eq) // too noisy, let's keep quiet for now or only log if close
            return eq
        }
        return cleanLhs === cleanRhs
    }

    function placePathAt(index) {
        if (index < 0 || index >= placesList.count) {
            return ""
        }
        const m = workspaceController.placesModel
        return String(m.data(m.index(index, 0), root.placePathRole) || "")
    }

    function placeSectionLabel(sectionKey) {
        switch (String(sectionKey || "")) {
        case "system": return "System"
        case "pinned": return "Pinned"
        case "cloud": return "Cloud"
        case "folders": return "Folders"
        case "drives": return "Drives"
        case "portable": return "Portable media"
        default: return "Places"
        }
    }

    function placeSectionTone(sectionKey) {
        switch (String(sectionKey || "")) {
        case "system": return Theme.actionIconColor("system")
        case "pinned": return Theme.actionIconColor("favorite")
        case "cloud": return Theme.actionIconColor("navigation")
        case "folders": return Theme.actionIconColor("folder")
        case "drives": return Theme.actionIconColor("drive")
        case "portable": return Theme.actionIconColor("media")
        default: return Theme.accent
        }
    }

    function formatBytes(bytes) {
        const value = Number(bytes || 0)
        if (value <= 0) return ""
        const kb = 1024
        const mb = kb * 1024
        const gb = mb * 1024
        const tb = gb * 1024
        if (value >= tb) return (value / tb).toFixed(2) + " TB"
        if (value >= gb) return (value / gb).toFixed(1) + " GB"
        if (value >= mb) return Math.round(value / mb) + " MB"
        if (value >= kb) return Math.round(value / kb) + " KB"
        return Math.round(value) + " B"
    }

    function driveTypeLabel(driveType) {
        switch (String(driveType || "")) {
        case "ssd": return "SSD"
        case "hdd": return "HDD"
        case "usb": return "USB"
        case "optical": return "Optical"
        case "network": return "Network"
        case "iso": return "ISO"
        case "camera": return "Camera"
        case "portable": return "MTP"
        default: return ""
        }
    }

    function placeSecondaryText(sectionKey, path, subtitle, isDrive, isReady, totalSpace, freeSpace, fileSystem, driveType) {
        const subtitleText = String(subtitle || "")
        if (subtitleText.length > 0) {
            if (isDrive && isReady === true && Number(totalSpace || 0) > 0) {
                return subtitleText + " • " + root.formatBytes(freeSpace) + " free of " + root.formatBytes(totalSpace)
            }
            return subtitleText
        }
        if (String(path || "") === "mega:///") {
            return ""
        }
        if (isDrive) {
            if (isReady !== true) {
                return "Not ready"
            }
            let parts = []
            const typeLabel = root.driveTypeLabel(driveType)
            if (typeLabel.length > 0) parts.push(typeLabel)
            const fs = String(fileSystem || "")
            if (fs.length > 0) parts.push(fs)
            const total = Number(totalSpace || 0)
            if (total > 0) {
                parts.push(root.formatBytes(freeSpace) + " free")
            }
            return parts.join(" - ")
        }
        if (sectionKey === "cloud") {
            return "Cloud storage"
        }
        return ""
    }

    function usageColor(usagePercent, critical) {
        if (critical) {
            return Theme.danger
        }
        return Theme.accent
    }

    function iconSourceFor(name) {
        const iconName = String(name || "")
        if (iconName.length === 0) {
            return ""
        }
        if (iconName === "drive") {
            return "../assets/icons-classic/hard-drive.svg"
        }
        if (iconName === "gdrive") {
            return "../assets/filetypes-next/gdrive.svg"
        }
        if (iconName === "mega") {
            return "../assets/filetypes-next/mega.svg"
        }
        if (iconName === "telegram") {
            return "../assets/filetypes-next/telegram.svg"
        }
        if (iconName === "home") {
            return "../assets/icons-classic/home.svg"
        }
        if (["computer", "desktop", "document", "download", "folder", "hard-drive",
             "image", "music", "star", "video"].indexOf(iconName) !== -1) {
            return "../assets/icons-classic/" + iconName + ".svg"
        }
        return "../assets/icons/" + iconName + ".svg"
    }

    function resolvedIconSourceFor(name) {
        const source = root.iconSourceFor(name)
        return source.length > 0 ? Qt.resolvedUrl(source) : ""
    }

    function iconToneFor(name, active, hovered) {
        let role = "default"
        switch (String(name)) {
        case "computer": role = "system"; break
        case "home":
        case "folder":
        case "file-manager": role = "folder"; break
        case "desktop": role = "navigation"; break
        case "download": role = "action"; break
        case "document": role = "document"; break
        case "image": role = "image"; break
        case "music":
        case "video": role = "media"; break
        case "drive":
        case "hard-drive": role = "drive"; break
        case "gdrive":
        case "mega": role = "navigation"; break
        case "star": role = "favorite"; break
        }
        const base = Theme.actionIconColor(role)
        if (active) {
            return Qt.lighter(base, themeController.isDark ? 1.12 : 1.05)
        }
        if (hovered) {
            return Qt.lighter(base, themeController.isDark ? 1.08 : 1.03)
        }
        return base
    }

    background: AmbientPanelBackground {
        cornerRadius: Theme.panelRadius
        topLeftCornerRadius: 0
        bottomLeftCornerRadius: 0
        baseColor: themeController.isDark
                   ? Theme.mixColors(Theme.bg, Theme.panelSurface, 0.28)
                   : Theme.panelSurface
        endColor: themeController.isDark
                  ? Theme.withAlpha(Theme.bg, 0.94)
                  : Theme.withAlpha(Theme.panelSurface, 0.82)
        strength: 0.70

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.panelRadius
            anchors.bottomMargin: Theme.panelRadius
            width: 1
            color: themeController.isDark
                ? Theme.withAlpha(Theme.accent, 0.10)
                : Theme.panelStrokeStrong
        }

        border.color: themeController.isDark
            ? Theme.withAlpha(Theme.accent, 0.12)
            : Theme.panelStroke
        border.width: 1
    }

    Item {
        id: sidebarStack

        readonly property int spacing: 10

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 8

        SidebarSectionCard {
            id: placesCard
            visible: root.panelEnabled("places")
            x: 0
            y: root.panelStackY("places")
            width: parent.width
            height: root.panelStackHeight("places")
            title: "Places"
            iconSource: "../assets/icons-classic/home.svg"
            iconColor: Theme.actionIconColor("navigation")
            collapsed: root.panelCollapsed("places")
            compact: root.compactMode
            onCollapseRequested: function(collapsed) {
                root.setPanelCollapsed("places", collapsed)
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                ListView {
                    id: placesList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1
                    model: workspaceController.placesModel
                    clip: true
                    interactive: contentHeight > height
                    focus: true
                    focusPolicy: Qt.StrongFocus
                    currentIndex: -1

                    Component.onCompleted: positionViewAtBeginning()

                    onContentYChanged: {
                        root.tracePlacesContentYFrame()
                        root.markPlacesScrollActivity()
                        root.tracePlacesScroll("contentY")
                    }
                    onContentXChanged: root.markPlacesScrollActivity()

                    HoverHandler {
                        id: placesListHover
                    }

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            root.lastFocusedPanelId = "places"
                            root.previewCurrentPlace()
                        }
                    }

                    onCurrentIndexChanged: root.previewCurrentPlace()

                    Keys.onTabPressed: function(event) {
                        if (root.trapTabNavigation) {
                            event.accepted = root.focusAdjacentPanel("places", 1)
                        }
                    }

                    Keys.onBacktabPressed: function(event) {
                        if (root.trapTabNavigation) {
                            event.accepted = root.focusAdjacentPanel("places", -1)
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Up) {
                            if (placesList.currentIndex > 0) {
                                root.setPlaceCurrentIndex(placesList.currentIndex - 1)
                            } else if (placesList.currentIndex === 0) {
                                root.setPlaceCurrentIndex(-1) // Focus "This PC" (header)
                            } else {
                                root.setPlaceCurrentIndex(placesList.count - 1) // Wrap around
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            if (placesList.currentIndex === -1) {
                                root.setPlaceCurrentIndex(0)
                            } else if (placesList.currentIndex < placesList.count - 1) {
                                root.setPlaceCurrentIndex(placesList.currentIndex + 1)
                            } else {
                                root.setPlaceCurrentIndex(-1) // Wrap to "This PC"
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.openSelectedPlace()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            workspaceController.focusActivePanel()
                            event.accepted = true
                        }
                    }

                    header: Item {
                        id: thisPcHeader

                        width: placesList.width
                        height: root.placeSectionHeaderHeight + root.placeCompactRowHeight

                        readonly property bool isActive: {
                            return root.selectedPlaceIndex === -1
                        }

                        readonly property bool hasKeyboardCurrent: placesList.activeFocus && placesList.currentIndex === -1

                        Column {
                            anchors.fill: parent
                            spacing: 0

                            SidebarPlacesSectionHeader {
                                sidebar: root
                                width: parent.width
                                label: root.placeSectionLabel("system")
                                tone: root.placeSectionTone("system")
                            }

                            Item {
                                width: parent.width
                                height: root.placeCompactRowHeight

                                Rectangle {
                                    id: thisPcBg
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    radius: Theme.radiusMd

                                    color: "transparent"
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0
                                            color: root.sidebarStateFillTop(thisPcHeader.isActive,
                                                                            thisPcHeader.hasKeyboardCurrent,
                                                                            thisPcMouse.containsMouse,
                                                                            thisPcMouse.containsPress)
                                        }
                                        GradientStop {
                                            position: 1
                                            color: root.sidebarStateFillBottom(thisPcHeader.isActive,
                                                                               thisPcHeader.hasKeyboardCurrent,
                                                                               thisPcMouse.containsMouse,
                                                                               thisPcMouse.containsPress)
                                        }
                                    }
                                    border.color: "transparent"
                                    border.width: 0

                                    Behavior on color {
                                        enabled: !root.interactionEffectsReduced
                                        ColorAnimation { duration: Theme.motionFast }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: root.placeHorizontalPadding
                                        anchors.rightMargin: root.placeHorizontalPadding
                                        spacing: root.placeRowSpacing

                                        RecolorSvgIcon {
                                            Layout.preferredWidth: root.placeIconSize
                                            Layout.preferredHeight: root.placeIconSize
                                            Layout.minimumWidth: root.placeIconSize
                                            Layout.minimumHeight: root.placeIconSize
                                            Layout.maximumWidth: root.placeIconSize
                                            Layout.maximumHeight: root.placeIconSize
                                            sourcePath: "../assets/icons-classic/computer.svg"
                                            recolorColor: root.iconToneFor("computer", thisPcHeader.isActive || thisPcHeader.hasKeyboardCurrent, false)
                                            cacheKey: "sidebar"
                                            sourceSize: Qt.size(root.placeIconSize * 2, root.placeIconSize * 2)
                                            asynchronous: true
                                            cache: true
                                            opacity: thisPcHeader.isActive || thisPcHeader.hasKeyboardCurrent || thisPcMouse.containsMouse ? 1 : 0.86
                                        }

                                        Label {
                                            text: "This PC"
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            font.family: Theme.fontFamily
                                            font.pixelSize: root.placePrimaryFontSize
                                            font.weight: thisPcHeader.isActive || thisPcHeader.hasKeyboardCurrent ? Font.Medium : Font.Normal
                                            color: TextColors.sidebarText
                                            opacity: thisPcHeader.isActive || thisPcHeader.hasKeyboardCurrent ? 1.0 : 0.92
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        id: thisPcMouse
                                        anchors.fill: parent
                                        hoverEnabled: !root.interactionEffectsReduced
                                        acceptedButtons: Qt.LeftButton
                                        cursorShape: Qt.PointingHandCursor
                                        onPressed: function(mouse) {
                                            root.selectPlace(-1)
                                        }
                                        onClicked: function(mouse) {
                                            mouse.accepted = true
                                        }
                                        onDoubleClicked: function(mouse) {
                                            root.openPathInActivePanel("devices://")
                                            mouse.accepted = true
                                        }
                                    }
                                }
                            }
                        }
                    }

                    delegate: SidebarPlaceDelegate {
                        sidebar: root
                        listView: placesList
                        workspace: workspaceController
                        theme: themeController
                    }

                    ScrollBar.vertical: FmScrollBar {
                        id: placesListVerticalScrollBar
                        policy: ScrollBar.AsNeeded
                        flat: true
                    }
                }
            }
        }

        SidebarSectionCard {
            id: recentCard
            visible: root.panelEnabled("recent")
            x: 0
            y: root.panelStackY("recent")
            width: parent.width
            height: root.panelStackHeight("recent")
            title: "Recent folders"
            iconSource: "../assets/icons-classic/calendar-clock.svg"
            iconColor: Theme.categoryUtility
            collapsed: root.panelCollapsed("recent")
            compact: root.compactMode
            onCollapseRequested: function(collapsed) {
                root.setPanelCollapsed("recent", collapsed)
            }

            ListView {
                id: recentList
                anchors.fill: parent
                model: typeof favoritesController !== "undefined" && favoritesController
                       ? favoritesController.frequentModel : null
                clip: true
                interactive: contentHeight > height
                focus: true
                focusPolicy: Qt.StrongFocus
                currentIndex: count > 0 ? 0 : -1

                onMovementStarted: {
                    root.recentScrollActive = true
                    recentScrollStopTimer.restart()
                }
                onMovingChanged: {
                    if (moving) {
                        root.recentScrollActive = true
                        recentScrollStopTimer.restart()
                    }
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        root.lastFocusedPanelId = "recent"
                        if (currentIndex < 0 && count > 0) root.setRecentCurrentIndex(0)
                        root.previewCurrentRecent()
                    }
                }
                onCurrentIndexChanged: root.previewCurrentRecent()

                Keys.onTabPressed: function(event) {
                    if (root.trapTabNavigation) event.accepted = root.focusAdjacentPanel("recent", 1)
                }
                Keys.onBacktabPressed: function(event) {
                    if (root.trapTabNavigation) event.accepted = root.focusAdjacentPanel("recent", -1)
                }
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Up) {
                        root.setRecentCurrentIndex(currentIndex > 0 ? currentIndex - 1 : count - 1)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Down) {
                        root.setRecentCurrentIndex(currentIndex < count - 1 ? currentIndex + 1 : 0)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.openCurrentRecent()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        workspaceController.focusActivePanel()
                        event.accepted = true
                    }
                }

                delegate: SidebarRecentDelegate {
                    sidebar: root
                    listView: recentList
                }

                ScrollBar.vertical: FmScrollBar {
                    policy: ScrollBar.AsNeeded
                    flat: true
                }

                Label {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - 24)
                    visible: recentList.count === 0
                    text: "Open folders and they will appear here."
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        SidebarSectionCard {
            id: foldersCard
            visible: root.panelEnabled("folders")
            x: 0
            y: root.panelStackY("folders")
            width: parent.width
            height: root.panelStackHeight("folders")
            title: "Folders"
            iconSource: "../assets/icons-classic/folder-open.svg"
            iconColor: Theme.actionIconColor("folder")
            collapsed: root.panelCollapsed("folders")
            compact: root.compactMode
            onCollapseRequested: function(collapsed) {
                root.setPanelCollapsed("folders", collapsed)
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                TreeView {
                    id: foldersTree
                    visible: !root.activePathIsProvider
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1
            model: workspaceController.treeModel
            selectionModel: ItemSelectionModel {
                model: workspaceController.treeModel
            }
            clip: true
            focus: true
            focusPolicy: Qt.StrongFocus

            onContentYChanged: root.markTreeScrollActivity()
            onContentXChanged: root.markTreeScrollActivity()

            HoverHandler {
                id: foldersTreeHover
            }

            onActiveFocusChanged: {
                if (activeFocus) {
                    root.lastFocusedPanelId = "folders"
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx === undefined || idx === null || !idx.valid) {
                        let firstIdx = workspaceController.treeModel.index(0, 0)
                        if (firstIdx && firstIdx.valid) {
                            if (foldersTree.selectionModel) {
                                foldersTree.selectionModel.setCurrentIndex(firstIdx, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
                            }
                        }
                    }
                    root.previewCurrentFolderTreeItem()
                }
            }

            Connections {
                target: foldersTree.selectionModel
                function onCurrentChanged(current, previous) {
                    root.previewCurrentFolderTreeItem()
                }
            }

            Keys.onTabPressed: function(event) {
                if (root.trapTabNavigation) {
                    event.accepted = root.focusAdjacentPanel("folders", 1)
                }
            }

            Keys.onBacktabPressed: function(event) {
                if (root.trapTabNavigation) {
                    event.accepted = root.focusAdjacentPanel("folders", -1)
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let path = workspaceController.treeModel.pathForIndex(idx)
                        if (path) {
                            root.openPathInActivePanel(path)
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Space) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let row = foldersTree.rowAtIndex(idx)
                        if (row >= 0) {
                            foldersTree.toggleExpanded(row)
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Right) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let row = foldersTree.rowAtIndex(idx)
                        if (row >= 0) {
                            if (!foldersTree.isExpanded(row)) {
                                foldersTree.expand(row)
                            }
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Left) {
                    let idx = foldersTree.selectionModel ? foldersTree.selectionModel.currentIndex : null
                    if (idx !== undefined && idx !== null && idx.valid) {
                        let row = foldersTree.rowAtIndex(idx)
                        if (row >= 0) {
                            if (foldersTree.isExpanded(row)) {
                                foldersTree.collapse(row)
                            } else {
                                let parentIdx = workspaceController.treeModel.parentIndex(idx)
                                if (parentIdx && parentIdx.valid) {
                                    if (foldersTree.selectionModel) {
                                        foldersTree.selectionModel.setCurrentIndex(parentIdx, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows | ItemSelectionModel.Current)
                                    }
                                }
                            }
                        }
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    workspaceController.focusActivePanel()
                    event.accepted = true
                }
            }

            delegate: SidebarFolderDelegate {
                sidebar: root
                workspace: workspaceController
                folderIcon: model.icon
            }

                    ScrollBar.vertical: FmScrollBar {
                        id: foldersTreeVerticalScrollBar
                        policy: ScrollBar.AsNeeded
                        flat: true
                    }
                }

                ColumnLayout {
                    visible: root.activePathIsProvider
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    spacing: 8

                    Item { Layout.fillHeight: true }

                    RecolorSvgIcon {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
                        recolorColor: Theme.textSecondary
                        sourceSize: Qt.size(48, 48)
                        cacheKey: "sidebar-remote-tree-placeholder"
                        asynchronous: true
                        cache: true
                        opacity: 0.72
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "Folder tree is available for local locations."
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "Browse this provider in the file panel."
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }

        Repeater {
            model: root.resizeDividerPairs()

            Item {
                required property var modelData

                x: 0
                y: root.panelStackY(modelData.afterId) - sidebarStack.spacing
                width: sidebarStack.width
                height: sidebarStack.spacing
                z: 20

                property real initialBeforeHeight: 0
                property real initialAfterHeight: 0
                property real expansionTranslation: 0

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 12
                    height: dividerHover.hovered || dividerDrag.active ? 3 : 1
                    radius: height / 2
                    color: dividerDrag.active
                           ? Theme.activeAccent
                           : (dividerHover.hovered
                              ? Theme.panelStrokeStrong
                              : Theme.panelStrokeSubtle)
                }

                HoverHandler {
                    id: dividerHover
                    cursorShape: Qt.SplitVCursor
                }

                DragHandler {
                    id: dividerDrag
                    target: null
                    xAxis.enabled: false
                    cursorShape: Qt.SplitVCursor
                    onActiveChanged: {
                        if (active) {
                            parent.initialBeforeHeight = root.panelStackHeight(parent.modelData.beforeId)
                            parent.initialAfterHeight = root.panelStackHeight(parent.modelData.afterId)
                            parent.expansionTranslation = 0
                            root.sectionResizeActive = true
                        } else {
                            root.sectionResizeActive = false
                            root.configurationChanged()
                        }
                    }
                    onTranslationChanged: {
                        if (active) {
                            if (root.panelCollapsed(parent.modelData.beforeId)
                                    && translation.y > 4) {
                                root.setPanelCollapsed(parent.modelData.beforeId, false)
                                parent.expansionTranslation = translation.y
                                parent.initialBeforeHeight = root.panelStackHeight(parent.modelData.beforeId)
                                parent.initialAfterHeight = root.panelStackHeight(parent.modelData.afterId)
                            } else if (root.panelCollapsed(parent.modelData.afterId)
                                       && translation.y < -4) {
                                root.setPanelCollapsed(parent.modelData.afterId, false)
                                parent.expansionTranslation = translation.y
                                parent.initialBeforeHeight = root.panelStackHeight(parent.modelData.beforeId)
                                parent.initialAfterHeight = root.panelStackHeight(parent.modelData.afterId)
                            }
                            if (!root.panelCollapsed(parent.modelData.beforeId)
                                    && !root.panelCollapsed(parent.modelData.afterId)) {
                                root.resizePanelPair(parent.modelData.beforeId,
                                                     parent.modelData.afterId,
                                                     parent.initialBeforeHeight,
                                                     parent.initialAfterHeight,
                                                     translation.y - parent.expansionTranslation)
                            }
                        }
                    }
                }
            }
        }
    }

    DriveContextMenu {
        id: placeDriveContextMenu

        onOpenRequested: function(path) {
            root.openPathInActivePanel(path)
        }

        onAnalyzeRequested: function(path) {
            if (root.Window.window && root.Window.window.openDiskUsage) {
                root.Window.window.openDiskUsage(path)
            }
        }

        onEjectRequested: function(path, managedIsoMount) {
            if (managedIsoMount) {
                workspaceController.unmountIsoRoot(path)
            } else {
                workspaceController.requestEjectVolume(path)
            }
        }

        onMountRequested: function(mountId) {
            workspaceController.requestMountVolume(mountId)
        }

        onPropertiesRequested: function(path) {
            propertiesController.load(path)
        }
    }

    Connections {
        target: workspaceController
        function onActivePanelChanged() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.treeModel
        function onPathRevealReady(requestId, index, exact) {
            if (requestId !== root.treeSyncRequestId) return
            if (!index || !index.valid) {
                root.clearTreeSelection()
                return
            }
            root.selectTreeIndex(index)
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onCurrentPathChanged() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.leftPanel
        function onPathNavigated() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onCurrentPathChanged() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.rightPanel
        function onPathNavigated() {
            root.updatePlaceSelectionForActivePath()
            root.syncTreeToActivePath()
        }
    }

    Connections {
        target: workspaceController.isoMountManager
        function onUnmountStarted(rootPath) {
            root.closePlaceDriveMenuForPath(rootPath)
        }

        function onUnmountFinished(rootPath, success, error) {
            root.closePlaceDriveMenuForPath(rootPath)
        }
    }

    Connections {
        target: workspaceController.volumeMonitor
        function onVolumeRemoved(rootPath, displayName) {
            root.closePlaceDriveMenuForPath(rootPath)
            if (root.pathsEqual(root.selectedPlacePath, rootPath)) {
                root.clearPlaceSelection()
            }
        }
    }

    Connections {
        target: workspaceController.placesModel
        function onModelReset() {
            root.tracePlaces("modelReset contentY=" + Math.round(placesList.contentY)
                             + " count=" + placesList.count
                             + " scrollActive=" + root.placesScrollActive
                             + " moving=" + placesList.moving)
            root.clearPlaceSelection()
            placeDriveContextMenu.close()
            root.resetPlaceDriveMenu()
        }

        function onRowsRemoved(removedParent, first, last) {
            root.tracePlaces("rowsRemoved first=" + first
                             + " last=" + last
                             + " contentY=" + Math.round(placesList.contentY)
                             + " count=" + placesList.count)
            root.clearPlaceSelection()
            placeDriveContextMenu.close()
            root.resetPlaceDriveMenu()
        }
    }

    Component.onCompleted: syncTreeToActivePath()
}
