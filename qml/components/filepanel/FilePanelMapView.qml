import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../../style"
import "../framework"

Item {
    id: root
    required property var controller
    required property var panel
    property bool gpuAvailable: false
    property bool gpuReady: false
    property bool locationSupported: true
    readonly property bool mapUsable: gpuReady && gpuAvailable && locationSupported
    property int currentIndex: -1
    readonly property int count: controller.directoryModel.count
    readonly property bool moving: map.moving
    readonly property bool flicking: false
    // The shared panel registry uses these for list scroll restoration. Map has its own camera.
    property real contentX: 0
    property real contentY: 0
    readonly property real contentHeight: 0
    readonly property real originY: 0
    readonly property real cellWidth: 160
    readonly property real cellHeight: 120
    readonly property var currentItem: null
    property bool spacePressed: false
    property string pendingBreadcrumbPath: ""
    property bool ownsNestedSelection: false
    readonly property bool hasCurrentEntry: controller.currentItemPath.length > 0

    function forceLayout() {}
    function entryForPath(path) { return map.entryForPath(path) }
    function itemAtIndex(index) { return null }
    function positionViewAtIndex(index, mode) {} // Selection restoration must not move the camera.

    function prepareBreadcrumbNavigation(path) {
        pendingBreadcrumbPath = path === controller.currentPath ? "" : path
        if (path === controller.currentPath) map.fitCurrent()
    }

    function focusEntry(entry) {
        currentIndex = controller.directoryModel.indexOfPath(entry.path)
        controller.currentItemPath = entry.path
        panel.completeCurrentIndexInit()
    }

    function selectEntry(entry, modifiers) {
        if (!entry.path) return
        panel.noteUserInteraction("item-click")
        panel.cancelActiveInlineRename()
        panel.invertSelectionActive = false
        const model = controller.directoryModel
        if (modifiers & Qt.ShiftModifier) {
            const paths = map.selectionContext(entry.path).paths || []
            model.selectMapRange(paths, entry.path)
        } else {
            model.selectMapPath(entry.path, (modifiers & Qt.ControlModifier) !== 0)
        }
        focusEntry(entry)
    }

    function clearNestedSelection() {
        if (ownsNestedSelection) controller.directoryModel.clearSelection()
        if (controller.directoryModel.indexOfPath(controller.currentItemPath) < 0)
            controller.currentItemPath = ""
    }

    function showContextMenu(entry) {
        panel.noteUserInteraction("item-right-click")
        if (!controller.directoryModel.selectedPaths().includes(entry.path))
            selectEntry(entry, Qt.NoModifier)
        focusEntry(entry)
        const row = controller.directoryModel.indexOfPath(entry.path)
        if (row < 0) panel.popupFilePanelContextMenu(-1, entry.path,
            !panel.isCurrentPathArchive && !panel.isCurrentPathRemote && entry.isArchiveFile === true && entry.isIsoImageFile !== true,
            !panel.isCurrentPathArchive && !panel.isCurrentPathRemote && entry.isIsoImageFile === true, entry.directory)
        else panel.handleItemRightClick(row, entry.path, entry.isArchiveFile, entry.isIsoImageFile)
    }

    function handleSelectionKey(event) {
        const path = controller.currentItemPath
        if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
            if (hasCurrentEntry) controller.directoryModel.selectMapPath(path, true)
            event.accepted = true
            return true
        }
        if (![Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down,
              Qt.Key_Home, Qt.Key_End, Qt.Key_PageUp, Qt.Key_PageDown].includes(event.key)) return false
        const context = map.selectionContext(hasCurrentEntry ? path : "")
        const paths = context.paths || []
        if (paths.length === 0) { event.accepted = true; return true }
        let index = paths.indexOf(path)
        const columns = context.columns || 1
        if (event.key === Qt.Key_Home) index = 0
        else if (event.key === Qt.Key_End) index = paths.length - 1
        else if (index < 0) index = event.key === Qt.Key_Up ? paths.length - 1 : 0
        else {
            const step = event.key === Qt.Key_Up || event.key === Qt.Key_Down
                         || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown ? columns : 1
            const backwards = event.key === Qt.Key_Left || event.key === Qt.Key_Up || event.key === Qt.Key_PageUp
            index = Math.max(0, Math.min(paths.length - 1, index + (backwards ? -step : step)))
        }
        const entry = map.entryForPath(paths[index])
        if ((event.modifiers & Qt.ControlModifier) && !(event.modifiers & Qt.ShiftModifier)) focusEntry(entry)
        else selectEntry(entry, event.modifiers)
        event.accepted = true
        return true
    }

    onCurrentIndexChanged: if (visible && currentIndex >= 0) panel.updateCurrentItemPath(currentIndex)
    onVisibleChanged: if (!visible) clearNestedSelection()

    Connections {
        target: root.controller
        function onCurrentPathChanged() { root.clearNestedSelection() }
        function onEntryRenamed(oldPath, newPath) {
            if (!root.visible || !root.mapUsable) return
            const remap = path => path === oldPath || path.startsWith(oldPath + "/")
                                 ? newPath + path.substring(oldPath.length) : path
            const paths = root.controller.directoryModel.selectedPaths().map(remap)
            const focused = remap(root.controller.currentItemPath)
            map.remapEntry(oldPath, newPath)
            root.controller.directoryModel.selectMapPaths(paths)
            root.controller.currentItemPath = focused
        }
    }

    Connections {
        target: root.controller.directoryModel
        function onSelectionChanged() {
            const paths = root.controller.directoryModel.selectedPaths()
            root.ownsNestedSelection = paths.some(path => root.controller.directoryModel.indexOfPath(path) < 0)
            map.selectedPaths = paths
        }
        function onModelReset() { root.currentIndex = root.controller.directoryModel.indexOfPath(root.controller.currentItemPath) }
        function onRowsInserted() { root.currentIndex = root.controller.directoryModel.indexOfPath(root.controller.currentItemPath) }
        function onRowsRemoved() { root.currentIndex = root.controller.directoryModel.indexOfPath(root.controller.currentItemPath) }
        function onLayoutChanged() {
            root.currentIndex = root.controller.directoryModel.indexOfPath(root.controller.currentItemPath)
            map.selectedPaths = root.controller.directoryModel.selectedPaths()
        }
    }

    FolderMapItem {
        id: map
        objectName: "folderMapGpuItem"
        anchors.fill: parent
        gpuEnabled: root.mapUsable && root.visible
        visible: root.mapUsable
        currentPath: root.controller.currentPath
        directoryModel: root.controller.directoryModel
        showHidden: root.controller.directoryModel.showHidden
        useNativeIcons: root.panel.useNativeIcons
        showThumbnails: root.panel.showThumbnails
        onEntriesChanged: {
            const model = root.controller.directoryModel
            const paths = model.selectedPaths()
            const retained = paths.filter(path => model.indexOfPath(path) >= 0 || map.entryForPath(path).path !== undefined)
            if (retained.length !== paths.length) model.selectMapPaths(retained)
            const current = root.controller.currentItemPath
            if (current && model.indexOfPath(current) < 0 && !map.entryForPath(current).path)
                root.controller.currentItemPath = ""
        }
        onCurrentPathChanged: {
            const target = root.pendingBreadcrumbPath
            root.pendingBreadcrumbPath = ""
            if (target === currentPath) Qt.callLater(() => {
                if (map.currentPath === target) map.fitCurrent()
            })
        }
        folderColor: themeController.isDark ? Theme.panelSurfaceSoft
                       : Theme.mixColors(Theme.opaque(Theme.panelSurfaceSoft), Theme.textPrimary, 0.035)
        fileColor: themeController.isDark ? Theme.surface : Theme.panelSurface
        textColor: Theme.textPrimary
        accentColor: Theme.activeAccent
        borderColor: themeController.isDark ? Theme.panelBorder
                     : Theme.mixColors(Theme.opaque(Theme.panelSurfaceSoft), Theme.textPrimary, 0.14)
        headerColor: themeController.isDark ? Theme.panelSurfaceStrong
                     : Theme.mixColors(Theme.opaque(Theme.panelSurfaceSoft), Theme.textPrimary, 0.085)
        gradientStartColor: Theme.opaque(Theme.chromeGradientMid)
        gradientEndColor: Theme.opaque(Theme.chromeGradientEnd)
        gradientStrength: Theme.useGradientColors ? (themeController.isDark ? 0.04 : 0.10) : 0
    }

    MouseArea {
        id: input
        anchors.fill: parent
        enabled: root.mapUsable
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        property point lastPosition
        property point pressPosition
        property bool dragged: false
        property bool panning: false
        cursorShape: panning ? Qt.ClosedHandCursor : root.spacePressed ? Qt.OpenHandCursor : Qt.ArrowCursor
        onPressed: mouse => {
            root.panel.activated()
            root.forceActiveFocus()
            lastPosition = Qt.point(mouse.x, mouse.y)
            pressPosition = lastPosition
            dragged = false
            panning = mouse.button === Qt.MiddleButton || mouse.button === Qt.RightButton || root.spacePressed
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            if (Math.abs(mouse.x - pressPosition.x) + Math.abs(mouse.y - pressPosition.y) > 5) dragged = true
            if (panning || dragged) map.panBy(mouse.x - lastPosition.x, mouse.y - lastPosition.y)
            lastPosition = Qt.point(mouse.x, mouse.y)
        }
        onReleased: panning = false
        onCanceled: panning = false
        onClicked: mouse => {
            if (dragged || root.spacePressed || mouse.button === Qt.MiddleButton) return
            const entry = map.entryAt(mouse.x, mouse.y)
            if (!entry.path || entry.path === root.controller.currentPath) {
                if (mouse.button === Qt.RightButton) root.panel.popupFilePanelEmptyMenu()
                return
            }
            if (mouse.button === Qt.RightButton) {
                root.showContextMenu(entry)
            } else root.selectEntry(entry, mouse.modifiers)
        }
        onDoubleClicked: mouse => {
            if (mouse.button !== Qt.LeftButton || root.spacePressed) return
            const entry = map.entryAt(mouse.x, mouse.y)
            if (!entry.path) return
            if (entry.directory) root.controller.openPath(entry.path)
            else root.controller.openFilePath(entry.path)
        }
        onWheel: wheel => {
            // Wayland mouse wheels can also carry pixel deltas; use their rotation.
            const mouseWheel = wheel.phase === Qt.NoScrollPhase && wheel.angleDelta.y !== 0
            const steps = mouseWheel ? wheel.angleDelta.y / 120
                                    : wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y / 80 : wheel.angleDelta.y / 120
            map.zoomAt(wheel.x, wheel.y, steps)
            wheel.accepted = true
        }
    }

    Keys.onPressed: event => {
        if (root.panel.panelKeysBlockedByOverlay()) {
            event.accepted = true
            return
        }
        if (!root.mapUsable) {
            event.accepted = false
            if (event.key === Qt.Key_Backspace && event.modifiers === Qt.NoModifier) {
                controller.goUp()
                event.accepted = true
            }
            return
        }
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (currentIndex >= 0) controller.openItem(currentIndex)
            else {
                const entry = map.entryForPath(controller.currentItemPath)
                if (entry.directory) controller.openPath(entry.path)
                else if (entry.path) controller.openFilePath(entry.path)
            }
            event.accepted = true
            return
        }
        event.accepted = false
        if (root.handleSelectionKey(event)) return
        root.panel.handleViewKeyPressed(root, event)
        if (event.accepted) return
        if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier) {
            spacePressed = true
            event.accepted = true
        } else if (event.modifiers === Qt.NoModifier && (event.key === Qt.Key_F || event.key === Qt.Key_0)) {
            map.fitCurrent()
            event.accepted = true
        }
    }
    Keys.onReleased: event => {
        if (event.key === Qt.Key_Space) {
            spacePressed = false
            event.accepted = true
        }
    }
    onActiveFocusChanged: if (!activeFocus) spacePressed = false

    Label {
        anchors.left: parent.left
        anchors.right: helpButton.left
        anchors.bottom: parent.bottom
        anchors.margins: 12
        visible: root.mapUsable && map.status.length > 0
        text: map.status
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
        elide: Text.ElideRight
    }

    FmIconButton {
        id: helpButton
        visible: root.mapUsable
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 28
        height: 28
        iconSource: "../../assets/icons-classic/info.svg"
        iconTone: "info"
        opacity: hovered ? 1 : 0.65
        ToolTip.visible: hovered
        ToolTip.delay: 350
        ToolTip.text: "Wheel — zoom\nDrag — pan\nDouble-click — open\nF or 0 — fit folder\nRight-click — file actions"
    }

    Rectangle {
        objectName: "mapUnavailablePlaceholder"
        anchors.fill: parent
        anchors.margins: 12
        visible: !root.mapUsable
        color: Theme.panelSurfaceSoft
        border.color: Theme.panelBorder
        border.width: 1
        gradient: Theme.useGradientColors ? placeholderGradient : null
        Gradient {
            id: placeholderGradient
            GradientStop { position: 0; color: Theme.mixColors(Theme.panelSurfaceSoft, Theme.chromeGradientMid, 0.12) }
            GradientStop { position: 1; color: Theme.mixColors(Theme.panelSurfaceSoft, Theme.chromeGradientEnd, 0.12) }
        }

        Label {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 12
            text: "Map"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
        }

        ColumnLayout {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: (root.panel.bottomChromeHeight - root.panel.topChromeHeight) / 2
            width: Math.min(parent.width - 32, 380)
            spacing: 12
            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 180
                Layout.preferredHeight: 135
                source: "../../assets/icons-classic/map-unavailable.svg"
                fillMode: Image.PreserveAspectFit
            }
            Label {
                Layout.fillWidth: true
                text: !root.locationSupported ? "Map is not available here"
                      : !root.gpuReady ? "Preparing Map…" : "Map needs a GPU"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel + 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: !root.locationSupported ? "Map supports local folders and local archives."
                      : !root.gpuReady ? "Checking graphics support…"
                      : "A hardware graphics renderer is required to show the folder map."
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: parent.width
                spacing: 8
                Repeater {
                    model: ["Details", "Grid", "Brief"]
                    FmButton {
                        required property int index
                        required property string modelData
                        objectName: "mapPlaceholderViewButton" + index
                        text: modelData
                        onClicked: {
                            root.panel.activated()
                            root.controller.viewMode = index
                            Qt.callLater(() => root.panel.focusContent())
                        }
                    }
                }
            }
        }
    }
}
