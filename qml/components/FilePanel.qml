import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"
import "common"

Pane {
    id: root

    required property var controller
    property bool active: false
    readonly property bool showActiveHighlight: root.active && workspaceController.splitEnabled
    readonly property int viewMode: root.controller.viewMode
    property int gridIconSize: 48
    readonly property int gridIconMinSize: 32
    readonly property int gridIconMaxSize: 96
    readonly property int gridCellWidth: Math.max(96, gridIconSize + 52)
    readonly property int gridCellHeight: Math.max(112, gridIconSize + 72)
    property int briefColumnWidth: 240
    property int briefRowHeight: 28
    readonly property int briefRowMinHeight: 22
    readonly property int briefRowMaxHeight: 64
    readonly property bool statusRailVisible: root.statusMessage.length > 0 || root.showLoadingRail
    readonly property real horizontalScrollX: horizontalFlick ? horizontalFlick.contentX : 0
    readonly property bool horizontalScrollActive: root.viewMode === 0 && horizontalFlick && horizontalFlick.contentWidth > horizontalFlick.width
    property bool showLoadingRail: false
    readonly property bool isCurrentPathArchive: root.controller.currentPath ? root.controller.currentPath.toLowerCase().startsWith("archive://") : false

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
    property var scrollPositions: ({})
    property string pendingScrollRestorePath: ""
    property real pendingScrollRestoreY: -1
    property bool pendingScrollRestoreEnabled: false
    property string targetSelectPath: ""
    property bool disableSelectionOnCurrentIndexChanged: false
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

    function updateNameColumnWidth() {
        if (nameColumnManuallyResized) {
            colWidthName = Math.max(180, preferredColWidthName)
        } else {
            let space = (contentArea ? contentArea.width : 500) - 24 - totalOtherColumnsWidth
            colWidthName = Math.max(180, space)
        }
    }

    onPreferredColWidthNameChanged: updateNameColumnWidth()
    onNameColumnManuallyResizedChanged: updateNameColumnWidth()
    onColShowSizeChanged: updateNameColumnWidth()
    onColShowTypeChanged: updateNameColumnWidth()
    onColShowDateChanged: updateNameColumnWidth()
    onColShowDateCreatedChanged: updateNameColumnWidth()
    onColShowExtensionChanged: updateNameColumnWidth()
    onColShowAttributesChanged: updateNameColumnWidth()
    onColShowResolutionChanged: updateNameColumnWidth()
    onColShowDurationChanged: updateNameColumnWidth()
    onColShowArtistChanged: updateNameColumnWidth()
    onColShowAlbumChanged: updateNameColumnWidth()
    onColShowBitrateChanged: updateNameColumnWidth()
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
                    root.showLoadingRail = true
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
    }

    function updateScrollingState() {
        if (root.controller.directoryModel.loading) return
        const moving   = root.viewMode === 2 ? briefView.moving   : root.viewMode === 0 ? listView.moving   : gridView.moving
        const flicking = root.viewMode === 2 ? briefView.flicking : root.viewMode === 0 ? listView.flicking : gridView.flicking
        const isScrolling = moving || flicking

        if (isScrolling) {
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
        if (!root.scrolling) {
            root.scrolling = true
            root.controller.scrolling = true
            // Clear hover on scroll start
            root.controller.hoveredPath = ""
        }
        scrollStopTimer.restart()
    }

    function activeView() {
        if (root.viewMode === 2) return briefView
        if (root.viewMode === 0) return listView
        return gridView
    }

    function setViewCurrentIndexWithoutSelection(view, index) {
        root.disableSelectionOnCurrentIndexChanged = true
        view.currentIndex = index
        root.updateCurrentItemPath(index)
        Qt.callLater(() => {
            root.disableSelectionOnCurrentIndexChanged = false
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
        if (p !== "devices://" && !p.endsWith("/")) {
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
        if (!path || path === "devices://") {
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
        if (!path || path === "devices://") {
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
        target: workspaceController.operationQueue
        function onStatusMessageChanged() {
            root.statusMessage = workspaceController.operationQueue.statusMessage
            statusTimer.restart()
        }
        function onBusyChanged() {
            if (!workspaceController.operationQueue.busy) {
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
        target: workspaceController
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
        
        layer.enabled: root.showActiveHighlight
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
                    NumberAnimation { duration: Theme.motionFast }
                }
            }

            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
            Behavior on border.width { NumberAnimation { duration: Theme.motionFast } }
        }
    }

    function contextRow() {
        if (root.viewMode === 2) return briefView.currentIndex
        if (root.viewMode === 0) return listView.currentIndex
        return gridView.currentIndex
    }

    function startRename() {
        if (root.isCurrentPathArchive) return
        let idx = contextRow()
        if (idx < 0) return
        
        if (root.controller.directoryModel.selectedCount > 1) {
            root.Window.window.showBatchRename(root.controller.selectedPaths())
            return
        }

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

    function handleItemRightClick(index, path) {
        root.activated()
        if (root.viewMode === 2)      briefView.currentIndex = index
        else if (root.viewMode === 0) listView.currentIndex = index
        else                          gridView.currentIndex = index
        root.updateCurrentItemPath(index)

        if (!root.controller.directoryModel.selectedCount || !root.controller.directoryModel.selectedPaths().includes(path)) {
            root.controller.directoryModel.selectOnly(index)
        }
        filePanelContextMenu.popupContextMenu()
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
        workspaceController: workspaceController
        windowObject: root.Window.window
        contextRowProvider: root.contextRow
        isCurrentPathArchive: root.isCurrentPathArchive
        onRenameRequested: root.startRename()
    }

    FilePanelEmptyMenu {
        id: filePanelEmptyMenu
        controller: root.controller
        workspaceController: workspaceController
        propertiesController: propertiesController
        isCurrentPathArchive: root.isCurrentPathArchive
    }

    FilePanelDropOverlay {
        anchors.fill: parent
        workspaceController: workspaceController
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
                    color: root.showActiveHighlight ? Theme.withAlpha(Theme.accent, 0.55)
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

                PathBar {
                    id: panelPathBar
                    Layout.fillWidth: true
                    controller: root.controller
                    path: root.controller.currentPath
                    onActiveFocusChanged: if (activeFocus) root.activated()
                }

                FilePanelViewMenu {
                    controller: root.controller
                }

                FilePanelSelectionBadge {
                    controller: root.controller
                    active: root.active
                }
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
                    width: listView.width
                    controller: root.controller
                    currentItem: ListView.isCurrentItem
                    panelActive: root.active
                    scrolling: root.scrolling
                    onClicked: (mouse) => root.handleItemClick(index, mouse)
                    onRightClicked: root.handleItemRightClick(index, path)
                    onDoubleClicked: root.controller.openItem(index)
                }
            }

            Component {
                id: detailsDelegate
                FileTableDelegate {
                    width: listView.width
                    controller: root.controller
                    panel: root
                    currentItem: ListView.isCurrentItem
                    panelActive: root.active
                    scrolling: root.scrolling
                    onClicked: (mouse) => root.handleItemClick(index, mouse)
                    onRightClicked: root.handleItemRightClick(index, path)
                    onEmptySpaceRightClicked: filePanelEmptyMenu.popupEmptyMenu()
                    onDoubleClicked: root.controller.openItem(index)
                }
            }

            Flickable {
                id: horizontalFlick
                anchors.fill: parent
                visible: !root.controller.isDeviceRoot
                enabled: visible
                contentWidth: root.viewMode === 0 ? Math.max(width, root.totalColumnsWidth) : width
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                interactive: root.viewMode === 0

                ScrollBar.horizontal: ScrollBar {
                    id: hScrollBar
                    parent: contentArea
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.statusRailVisible ? (root.controller.directoryModel.loading ? 52 : 36) : 0
                    policy: ScrollBar.AlwaysOn
                    visible: root.viewMode === 0 && root.horizontalScrollActive
                    z: 10
                }

                Column {
                    width: horizontalFlick.contentWidth
                    height: horizontalFlick.height

                    FilePanelHeader {
                        id: tableHeader
                        width: parent.width
                        visible: root.viewMode === 0
                        controller: root.controller
                        panel: root
                    }

                    ListView {
                        id: listView
                        width: parent.width
                        height: root.viewMode === 0 ? parent.height - tableHeader.height : parent.height
                        visible: root.viewMode === 0
                        enabled: visible
                        clip: true
                        boundsBehavior: Flickable.DragAndOvershootBounds
                        pixelAligned: false
                        flickableDirection: Flickable.VerticalFlick
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
                                positionViewAtIndex(currentIndex, ListView.Contain)
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
                                positionViewAtIndex(idx, ListView.Contain)
                            }
                        }
                        cacheBuffer: Math.max(0, height * 2)
                        reuseItems: true
                        onMovingChanged: root.updateScrollingState()
                        onFlickingChanged: root.updateScrollingState()
                        onContentYChanged: root.handleScrollActivity()
                        onContentXChanged: root.handleScrollActivity()
                        bottomMargin: (root.statusRailVisible ? (root.controller.directoryModel.loading ? 52 : 36) : 0) + (root.horizontalScrollActive ? 12 : 0)
                        
                        highlight: null
                        highlightFollowsCurrentItem: false

                        add: root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? listAddTransition : null
                        remove: root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? listRemoveTransition : null

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
                                workspaceController.focusActivePanel()
                                event.accepted = true
                            }
                        }

                        delegate: root.viewMode === 0 ? detailsDelegate : listDelegate

                        ScrollBar.vertical: ScrollBar {
                            parent: contentArea
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: root.viewMode === 0 ? tableHeader.height : 0
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: (root.statusRailVisible ? (root.controller.directoryModel.loading ? 52 : 36) : 0) + (root.horizontalScrollActive ? 12 : 0)
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
                visible: !root.controller.isDeviceRoot
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
                visible: root.viewMode === 2 && !root.controller.isDeviceRoot
                enabled: visible
                clip: true
                flow: GridView.FlowLeftToRight
                cellWidth: Math.floor(width / 2)
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
                        positionViewAtIndex(currentIndex, GridView.Contain)
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
                        positionViewAtIndex(idx, GridView.Contain)
                    }
                }
                cacheBuffer: Math.max(0, height * 2)
                reuseItems: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                onMovingChanged:  root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: root.handleScrollActivity()
                onContentXChanged: root.handleScrollActivity()
                bottomMargin: root.statusRailVisible ? (root.controller.directoryModel.loading ? 52 : 36) : 0

                add:    root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? briefAddTransition : null
                remove: root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? briefRemoveTransition : null

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
                        workspaceController.focusActivePanel()
                        event.accepted = true
                    }
                }

                delegate: Component {
                    FileBriefDelegate {
                        width: briefView.cellWidth
                        height: briefView.cellHeight
                        controller: root.controller
                        currentItem: GridView.isCurrentItem
                        panelActive: root.active
                        scrolling: root.scrolling

                        onClicked: (mouse) => root.handleItemClick(index, mouse)
                        onRightClicked: root.handleItemRightClick(index, path)
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
                    anchors.bottomMargin: root.statusRailVisible ? (root.controller.directoryModel.loading ? 52 : 36) : 0
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
                anchors.bottomMargin: root.viewMode === 1 ? (root.showLoadingRail ? 92 : (root.statusMessage.length > 0 ? 92 : 56)) : 10
                visible: root.viewMode === 1 && !root.controller.isDeviceRoot
                enabled: visible
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                pixelAligned: false
                flickableDirection: Flickable.VerticalFlick
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
                        positionViewAtIndex(currentIndex, GridView.Contain)
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
                        positionViewAtIndex(idx, GridView.Contain)
                    }
                }
                cacheBuffer: Math.max(0, height * 1.5)
                reuseItems: true
                onMovingChanged: root.updateScrollingState()
                onFlickingChanged: root.updateScrollingState()
                onContentYChanged: root.handleScrollActivity()
                onContentXChanged: root.handleScrollActivity()
                
                highlight: null
                highlightFollowsCurrentItem: false

                add: root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? gridAddTransition : null
                remove: root.controller.directoryModel.count < 500 && !root.controller.directoryModel.loading ? gridRemoveTransition : null

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
                        workspaceController.focusActivePanel()
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

                    property bool isRenaming: false
                    property bool currentItem: GridView.isCurrentItem
                    property bool panelActive: root.active
                    property real visualOffsetY: 0

                    opacity: isHidden ? 0.55 : 1.0

                    onPathChanged: {
                        isRenaming = false
                        visualOffsetY = 0
                        Qt.callLater(() => {
                            if (hoverGrid) {
                                hoverGrid.enabled = false
                                hoverGrid.enabled = true
                            }
                        })
                    }

                    Component.onCompleted: {
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
                        if (root.controller.hoveredPath === path) {
                            root.controller.hoveredPath = ""
                        }
                    }

                    GridView.onReused: {
                        isRenaming = false
                        visualOffsetY = 0
                        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
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

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
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
                        enabled: true
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
                            if (!root.scrolling) {
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
                        target: root.controller ? root.controller.directoryModel : null
                        ignoreUnknownSignals: true
                        function onLoadingChanged() {
                            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading) {
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
                        anchors.topMargin: root.gridIconSize + 18
                        width: Math.min(Math.max(parent.width + 72, 220), Math.max(parent.width, gridView.width - 24))
                        height: Math.min(74, Math.max(52, parent.height - root.gridIconSize - 34))
                        x: Math.round((parent.width - width) / 2)
                        active: isRenaming
                        visible: isRenaming
                        sourceComponent: TextArea {
                            id: gridRenameInput
                            text: name
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 14
                            color: Theme.textPrimary
                            selectByMouse: true
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 6
                            bottomPadding: 6
                            selectionColor: Theme.accent
                            selectedTextColor: "white"
                            wrapMode: TextEdit.Wrap
                            clip: true
                            background: Rectangle {
                                color: Theme.panelSurfaceStrong
                                radius: Theme.radiusSm
                                border.color: Theme.focusRing
                                border.width: 1.5

                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    shadowEnabled: true
                                    shadowColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.35 : 0.20)
                                    shadowBlur: 12
                                    shadowVerticalOffset: 2
                                }
                            }

                            function commitRename() {
                                if (index >= 0) {
                                    const idx = index
                                    const txt = text.replace(/[\r\n]+/g, " ").trim()
                                    const ctrl = root.controller
                                    Qt.callLater(function() {
                                        if (ctrl.rename(idx, txt)) {
                                            isRenaming = false
                                        } else {
                                            if (gridRenameLoader.item) {
                                                gridRenameLoader.item.forceActiveFocus()
                                                gridRenameLoader.item.selectAll()
                                            }
                                        }
                                    })
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
                                event.accepted = true
                            }
                            onActiveFocusChanged: if (!activeFocus) isRenaming = false
                            
                            Component.onCompleted: {
                                forceActiveFocus()
                                let lastDot = name.lastIndexOf(".")
                                if (!isDirectory && lastDot > 0) {
                                    select(0, lastDot)
                                } else {
                                    selectAll()
                                }
                            }
                        }
                    }

                    ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6
                    transform: Translate { y: gridDelegate.visualOffsetY }

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.gridIconSize
                        Layout.preferredHeight: root.gridIconSize

                        Image {
                            anchors.centerIn: parent
                            width: Math.max(28, Math.round(root.gridIconSize * 0.8))
                            height: width
                            source: isImage ? "../assets/icons/image.svg" : "image://icon/" + encodeURIComponent(path) + (isDirectory ? "?directory=true" : "")
                            sourceSize: Qt.size(width, height)
                            visible: !isImage && (thumbnail.status !== Image.Ready || !hasThumbnail)
                            opacity: isImage ? 0.72 : 1.0
                            smooth: true
                            mipmap: false
                        }

                        Image {
                            anchors.centerIn: parent
                            width: Math.round(root.gridIconSize * 0.9)
                            height: width
                            source: "../assets/icons/image.svg"
                            sourceSize: Qt.size(width, height)
                            visible: isImage && (thumbnail.status !== Image.Ready)
                            opacity: 0.74
                            smooth: true
                            mipmap: false
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            visible: !isDirectory && hasThumbnail && thumbnail.status !== Image.Ready
                            color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.18 : 0.12)
                        }

                        Image {
                            id: thumbnail
                            anchors.fill: parent
                            source: !isDirectory && hasThumbnail ? "image://thumbnail/" + encodeURIComponent(path) : ""
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
                        
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) root.handleItemRightClick(index, path)
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

                ScrollBar.vertical: ScrollBar {}
            }

            Rectangle {
                id: gridDensityBar
                visible: root.viewMode === 1 && !root.controller.isDeviceRoot
                z: 5
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12
                anchors.bottomMargin: root.showLoadingRail || root.statusMessage.length > 0 ? 44 : 12
                width: 292
                height: 42
                radius: Theme.controlRadius
                color: Theme.panelSurfaceSoft
                border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.95 : 0.85)
                border.width: 1
                layer.enabled: false

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        text: "Icon size"
                        Layout.preferredWidth: 58
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Slider {
                        id: gridIconSlider
                        Layout.fillWidth: true
                        from: root.gridIconMinSize
                        to: root.gridIconMaxSize
                        stepSize: 4
                        snapMode: Slider.SnapAlways
                        focusPolicy: Qt.StrongFocus
                        value: root.gridIconSize

                        onMoved: {
                            const snapped = Math.round(value / stepSize) * stepSize
                            if (snapped !== root.gridIconSize) {
                                root.gridIconSize = snapped
                            }
                        }

                        background: Item {
                            anchors.fill: parent
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 4
                                radius: 2
                                color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.65 : 0.5)
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: gridIconSlider.visualPosition * parent.width
                                height: 4
                                radius: 2
                                color: Theme.accent
                            }
                        }

                        handle: Rectangle {
                            width: 0
                            height: 0
                            opacity: 0
                        }
                    }

                    Label {
                        text: root.gridIconSize + " px"
                        Layout.preferredWidth: 34
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Rectangle {
                id: briefDensityBar
                visible: root.viewMode === 2 && !root.controller.isDeviceRoot
                z: 5
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12
                anchors.bottomMargin: root.showLoadingRail || root.statusMessage.length > 0 ? 44 : 12
                width: 292
                height: 42
                radius: Theme.controlRadius
                color: Theme.panelSurfaceSoft
                border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.95 : 0.85)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        text: "Density"
                        Layout.preferredWidth: 58
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Slider {
                        id: briefDensitySlider
                        Layout.fillWidth: true
                        from: root.briefRowMinHeight
                        to: root.briefRowMaxHeight
                        stepSize: 2
                        snapMode: Slider.SnapAlways
                        value: root.briefRowHeight

                        onMoved: {
                            root.briefRowHeight = Math.round(value / stepSize) * stepSize
                        }

                        background: Item {
                            anchors.fill: parent
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 4
                                radius: 2
                                color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, themeController.isDark ? 0.65 : 0.5)
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: briefDensitySlider.visualPosition * parent.width
                                height: 4
                                radius: 2
                                color: Theme.accent
                            }
                        }

                        handle: Rectangle { width: 0; height: 0; opacity: 0 }
                    }

                    Label {
                        text: root.briefRowHeight + " px"
                        Layout.preferredWidth: 34
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            // ── Storage View (This PC / devices://) ──────────────────────────
            StorageView {
                id: storageView
                anchors.fill: parent
                controller: root.controller
                panel: root
                visible: root.controller.isDeviceRoot
                enabled: visible
                focus: root.active && root.controller.isDeviceRoot
            }

            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.RightButton
                enabled: !root.controller.isDeviceRoot
                onClicked: (mouse) => {
                    root.activated()
                    filePanelEmptyMenu.popupEmptyMenu()
                }
            }

            // ── Modern Integrated Status Bar ────────────────────────────────
            FilePanelStatusBar {
                id: statusRail
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                active: root.active
                showLoadingRail: root.showLoadingRail
                statusMessage: root.statusMessage
                isCurrentPathArchive: root.isCurrentPathArchive
                loadingFolderNameProvider: root.loadingFolderName
            }
        }
    }
}

}


