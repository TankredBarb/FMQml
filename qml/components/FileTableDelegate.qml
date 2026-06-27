import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "filepanel"

Item {
    id: root

    required property var controller
    required property var panel // reference to FilePanel to read column widths

    // Model roles
    required property int index
    required property string name
    required property string path
    required property string iconName
    required property string mimeType
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string sizeText
    required property string modifiedText
    required property string createdText
    required property string attributesText
    required property string suffix
    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false
    readonly property bool resizeOptimized: root.panel && root.panel.resizeOptimized
    readonly property bool stateAnimationsSuppressed: root.resizeOptimized
                                                     || Boolean(root.panel && root.panel.keyboardNavigationActive)
    readonly property color selectedStateFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark
            ? (root.panelActive ? 0.34 : 0.20)
            : (root.panelActive ? 0.28 : 0.16))
    readonly property color currentStateFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark
            ? (root.panelActive ? 0.18 : 0.11)
            : (root.panelActive ? 0.14 : 0.09))
    z: root.isRenaming ? 100 : 0

    // Signals
    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()
    signal emptySpaceRightClicked()

    implicitHeight: Theme.rowHeight

    property bool isRenaming: false
    property real visualOffsetX: 0
    property real dragStartX: 0
    property real dragStartY: 0
    property bool dragCandidate: false
    property bool dragStarted: false
    property bool badgePressed: false
    property bool suppressClickAfterDrag: false

    component CellSeparator : Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
        opacity: themeController.isDark ? 0.15 : 0.35
        visible: root.panel.showGridlines
    }

    // ── Lazy media metadata ───────────────────────────────────────────────────
    property var _meta: ({})      // Loaded metadata cache: {dimensions, resolution, duration, artist, album, bitrate}
    property bool _metaRequested: false
    property bool _metaLoaded: false

    function _ensureMetaLoaded() {
        if (_metaRequested || _metaLoaded) return
        if (root.resizeOptimized) return
        if (isDirectory) return
        // Only request if any media column is visible
        if (!panel.colShowResolution && !panel.colShowDuration
            && !panel.colShowArtist && !panel.colShowAlbum && !panel.colShowBitrate) return

        _metaRequested = true
        controller.fetchMetadataAsync(root.path)
    }

    // Catch the async result from the controller
    Connections {
        target: root.controller
        function onMetadataReady(filePath, meta) {
            if (filePath === root.path) {
                root._meta = meta
                root._metaLoaded = true
            }
        }
    }

    // Trigger load when media columns become visible or item becomes ready
    onPathChanged: {
        isRenaming = false
        visualOffsetX = 0
        _meta = {}
        _metaRequested = false
        _metaLoaded = false
        _ensureMetaLoaded()
        if (root.resizeOptimized) {
            return
        }
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    Component.onCompleted: {
        _ensureMetaLoaded()
        if (root.resizeOptimized) {
            return
        }
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    ListView.onPooled: {
        isRenaming = false
        visualOffsetX = 0
        if (root.controller.hoveredPath === root.path) {
            root.controller.hoveredPath = ""
        }
    }

    ListView.onReused: {
        isRenaming = false
        visualOffsetX = 0
        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
        _ensureMetaLoaded()
        if (root.resizeOptimized) {
            return
        }
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    function startRename() {
        root.isRenaming = true
    }

    function cancelRename() {
        root.isRenaming = false
    }

    function focusRenameEditor(selectText) {
        return tableRenameEditor.forceEditorFocus(selectText)
    }

    function renameEditorHasFocus() {
        return tableRenameEditor.editorHasFocus()
    }

    function cancelRenameOnPress(reason) {
        if (root.panel && root.panel.cancelInlineRenameForNavigation) {
            root.panel.cancelInlineRenameForNavigation(reason)
        }
    }

    function itemStateFill(hovered, fallbackColor) {
        if (root.isSelected) {
            return root.selectedStateFill
        }
        if (root.currentItem) {
            return root.currentStateFill
        }
        if (hovered && !root.scrolling) {
            return Theme.itemNeutralHoverFill
        }
        return fallbackColor
    }

    opacity: isHidden ? 0.55 : 1.0

    // ── Background ────────────────────────────────────────────────────────────
    Rectangle {
        id: bgRect
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        radius: Theme.radiusMd

        color: root.itemStateFill(hover.hovered, "transparent")
        border.color: "transparent"
        border.width: 0
        transform: Translate { x: root.visualOffsetX }

        // Zebra striping overlay
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: themeController.isDark ? "white" : "black"
            opacity: themeController.isDark ? 0.015 : 0.025
            visible: root.panel.showZebraStriping && (index % 2 === 1) && !isSelected && !root.currentItem && !(hover.hovered && !root.scrolling)
        }

        Behavior on color {
            enabled: !root.stateAnimationsSuppressed
            ColorAnimation { duration: Theme.motionFast }
        }
        Behavior on border.color {
            enabled: !root.stateAnimationsSuppressed
            ColorAnimation { duration: Theme.motionFast }
        }
    }

    onResizeOptimizedChanged: {
        if (!root.resizeOptimized) {
            _ensureMetaLoaded()
        }
    }

    // Horizontal gridline
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.15 : 0.35)
        visible: root.panel.showGridlines
        z: 1
    }

    HoverHandler {
        id: hover
        enabled: !root.resizeOptimized
        cursorShape: root.panel && root.panel.internalDragEnabled
                     ? root.panel.itemHoverCursorShape(root, point.position.x, point.position.y)
                     : Qt.ArrowCursor
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                root.controller.hoveredPath = root.path
                if (root.panel && root.panel.internalDragEnabled) {
                    root.panel.updateHoverDragCursor(root, point.position.x, point.position.y)
                }
            } else {
                if (root.controller.hoveredPath === root.path) {
                    root.controller.hoveredPath = ""
                }
                if (root.panel) {
                    root.panel.clearHoverDragCursor(root)
                }
            }
        }
        onPointChanged: {
            if (hovered && root.panel && root.panel.internalDragEnabled) {
                root.panel.updateHoverDragCursor(root, point.position.x, point.position.y)
            }
        }
    }

    onScrollingChanged: {
        if (!scrolling && !root.resizeOptimized) {
            Qt.callLater(() => {
                if (hover) {
                    hover.enabled = false
                    hover.enabled = true
                    if (hover.hovered) {
                        root.controller.hoveredPath = root.path
                    }
                }
            })
        }
    }

    Connections {
        target: root.controller ? root.controller.directoryModel : null
        ignoreUnknownSignals: true
        function onLoadingChanged() {
            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading && !root.resizeOptimized) {
                Qt.callLater(() => {
                    if (hover) {
                        hover.enabled = false
                        hover.enabled = true
                        if (hover.hovered) {
                            root.controller.hoveredPath = root.path
                        }
                    }
                })
            }
        }
    }


    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false
        preventStealing: root.dragCandidate || root.badgePressed
        cursorShape: root.panel
                     && root.panel.internalDragEnabled
                     && typeof root.panel.itemDragAffordanceCursor === "function"
                     ? root.panel.itemHoverCursorShape(root, mouseX, mouseY)
                     : Qt.ArrowCursor
        scrollGestureEnabled: false
        onWheel: (wheel) => { wheel.accepted = false }
        onPressed: (mouse) => {
            root.cancelRenameOnPress("table-item-press")
            root.badgePressed = mouse.button === Qt.LeftButton
                                && root.isPointOnBadge(mouse.x, mouse.y)
            root.dragCandidate = root.panel
                                 && root.panel.internalDragEnabled
                                 && mouse.button === Qt.LeftButton
                                 && !root.isRenaming
                                 && !root.badgePressed
                                 && (root.isSelected || root.isPointOnDragSurface(mouse.x, mouse.y))
            root.dragStarted = false
            root.dragStartX = mouse.x
            root.dragStartY = mouse.y
        }

        onPositionChanged: (mouse) => {
            if (root.badgePressed) {
                return
            }
            if (!root.dragCandidate || !root.panel) {
                return
            }
            if (root.dragStarted) {
                root.panel.updateSelectionDragPosition(mouse, root)
            } else {
                root.dragStarted = root.panel.updateSelectionDragCandidate(
                            root.index, root.path, root.dragStartX, root.dragStartY,
                            mouse.x, mouse.y, mouse)
                if (root.dragStarted) {
                    root.panel.updateSelectionDragPosition(mouse, root)
                }
            }
        }

        onReleased: (mouse) => {
            if (root.dragStarted && root.panel) {
                root.panel.finishSelectionDrag(mouse, root)
                root.suppressClickAfterDrag = true
                suppressClickReset.restart()
            }
            root.dragCandidate = false
            root.dragStarted = false
            root.badgePressed = false
        }

        onCanceled: {
            if (root.dragStarted && root.panel && root.panel.internalDragEnabled && root.panel.dragCoordinator) {
                root.panel.dragCoordinator.cancelDrag("Drag canceled.")
            }
            root.dragCandidate = false
            root.dragStarted = false
            root.badgePressed = false
        }

        onClicked: (mouse) => {
            if (root.suppressClickAfterDrag) {
                root.suppressClickAfterDrag = false
                return
            }
            if (mouse.button === Qt.RightButton) {
                var colNamePos = root.mapToItem(colName, mouse.x, mouse.y)
                if (colNamePos.x >= 0 && colNamePos.x <= colName.width) {
                    root.rightClicked()
                } else {
                    root.emptySpaceRightClicked()
                }
            } else if (root.isPointOnBadge(mouse.x, mouse.y)) {
                root.controller.directoryModel.toggleSelected(root.index)
            } else {
                root.clicked(mouse)
            }
        }

        onDoubleClicked: (mouse) => {
            root.doubleClicked()
        }
    }

    Timer {
        id: suppressClickReset
        interval: 0
        repeat: false
        onTriggered: root.suppressClickAfterDrag = false
    }
    function isPointOnBadge(x, y) {
        if (!selectionToggleBadge || !selectionToggleBadge.visible) return false
        const mapped = selectionToggleBadge.mapFromItem(root, x, y)
        return mapped.x >= 0 && mapped.y >= 0 && mapped.x < selectionToggleBadge.width && mapped.y < selectionToggleBadge.height
    }

    function isWithinItem(item, x, y, padding) {
        if (!item || !item.visible) {
            return false
        }
        const mapped = item.mapFromItem(root, x, y)
        const pad = padding || 0
        return mapped.x >= -pad && mapped.y >= -pad
                && mapped.x < item.width + pad && mapped.y < item.height + pad
    }

    function isPointOnDragSurface(x, y) {
        return root.isWithinItem(nameIcon, x, y, 2)
    }

    SelectionToggleBadge {
        id: selectionToggleBadge
        x: 7 + root.visualOffsetX
        y: Math.round((root.height - height) / 2)
        z: 30
        badgeSize: 16
        markSize: 6
        markStroke: 1
        available: root.panel ? root.panel.showSelectionBadges : true
        controller: root.controller
        panel: root.panel
        index: root.index
        selected: root.isSelected
        hovered: hover.hovered
        currentItem: root.currentItem
        scrolling: root.scrolling || root.isRenaming
    }

    // ── Columns Layout ────────────────────────────────────────────────────────
    Item {
        id: columnsContainer
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        transform: Translate { x: root.visualOffsetX }

        // ── Sticky Name Column Background ────────────────────────────────────
        Rectangle {
            id: stickyNameBg
            x: root.panel.horizontalScrollActive && root.panel.horizontalScrollX > 12 ? root.panel.horizontalScrollX - 12 : 0
            width: root.panel.effectiveColWidthName + 12
            height: parent.height
            z: 2
            visible: root.panel.horizontalScrollActive && root.panel.horizontalScrollX > 12
            color: isSelected
                   ? root.selectedStateFill
                   : root.itemStateFill(hover.hovered, Theme.panelSurface)

            // Vertical divider on the right edge
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.border
                opacity: 0.6
            }

            // Zebra striping overlay for sticky column
            Rectangle {
                anchors.fill: parent
                color: themeController.isDark ? "white" : "black"
                opacity: themeController.isDark ? 0.015 : 0.025
                visible: root.panel.showZebraStriping && (index % 2 === 1) && !isSelected && !root.currentItem && !(hover.hovered && !root.scrolling)
            }

            // Drop shadow-like gradient on the right of the sticky column
            Rectangle {
                anchors.left: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                visible: Theme.useGradientColors
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.25 : 0.08) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            Behavior on color {
                enabled: !root.stateAnimationsSuppressed
                ColorAnimation { duration: 90 }
            }
        }

        // ── COLUMN: Name ──────────────────────────────────────────────────────
        Item {
            id: colName
            x: root.panel.horizontalScrollActive && root.panel.horizontalScrollX > 12 ? root.panel.horizontalScrollX - 12 : 0
            width: root.panel.effectiveColWidthName
            height: parent.height
            z: 3
            clip: !root.isRenaming

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.panel && root.panel.showSelectionBadges ? 16 : 4
                anchors.rightMargin: 8
                spacing: 8
                visible: !root.isRenaming

                FileIconCell {
                    id: nameIcon
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    Layout.alignment: Qt.AlignVCenter
                    path: root.path
                    name: root.name
                    iconName: root.iconName
                    mimeType: root.mimeType
                    isDirectory: root.isDirectory
                    suffix: root.suffix
                    useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
                    iconSize: 16
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    clip: true

                    Label {
                        text: {
                            if (root.isDirectory || !root.suffix) return root.name
                            const extLen = root.suffix.length
                            if (extLen > 0 && root.name.endsWith("." + root.suffix)) {
                                return root.name.substring(0, root.name.length - extLen - 1)
                            }
                            return root.name
                        }
                        color: root.isDirectory ? TextColors.folderNameText : TextColors.fileNameText
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: isSelected ? Font.Medium : Font.Normal
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        Layout.maximumWidth: root.isDirectory ? Infinity : Math.ceil(implicitWidth)
                    }

                    Label {
                        visible: !root.isDirectory && !!root.suffix && root.name.endsWith("." + root.suffix)
                        text: "." + root.suffix
                        color: TextColors.fileExtensionText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: isSelected ? Font.Medium : Font.Normal
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                        Layout.fillWidth: false
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            FileNameEditor {
                id: tableRenameEditor
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 8
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                active: root.isRenaming
                name: root.name
                isDirectory: root.isDirectory
                index: root.index
                controller: root.controller
                windowObject: root.panel ? root.panel.Window.window : null
                fontPixelSize: Theme.fontSizeBody
                onCancelRequested: {
                    root.isRenaming = false
                    if (root.panel) {
                        root.panel.cancelInlineRename()
                    }
                }
                onCommitSucceeded: root.isRenaming = false
                onFocusLost: {
                    if (root.panel) {
                        root.panel.recoverInlineRenameFocus("table-editor-focus-lost")
                    }
                }
            }
            CellSeparator {}
        }

        // ── COLUMN: Size ──────────────────────────────────────────────────────
        Item {
            id: colSize
            x: root.panel.effectiveColWidthName
            width: root.panel.effectiveColWidthSize
            height: parent.height
            visible: root.panel.effectiveColShowSize
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.sizeText.length > 0 ? root.sizeText : "—"
                color: TextColors.fileSecondaryText
                opacity: root.sizeText.length > 0 ? 0.85 : 0.35
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            CellSeparator {}
        }

        // ── COLUMN: Type ──────────────────────────────────────────────────────
        Item {
            id: colType
            x: colSize.x + (colSize.visible ? colSize.width : 0)
            width: root.panel.effectiveColWidthType
            height: parent.height
            visible: root.panel.effectiveColShowType
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.controller ? root.controller.fileTypeLabelFor(root.suffix, root.isDirectory) : ""
                color: TextColors.fileSecondaryText
                opacity: 0.85
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            CellSeparator {}
        }

        // ── COLUMN: Date Modified ─────────────────────────────────────────────
        Item {
            id: colDate
            x: colType.x + (colType.visible ? colType.width : 0)
            width: root.panel.effectiveColWidthDate
            height: parent.height
            visible: root.panel.effectiveColShowDate
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.modifiedText
                color: TextColors.fileSecondaryText
                opacity: 0.85
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            CellSeparator {}
        }

        // ── COLUMN: Date Created ──────────────────────────────────────────────
        Item {
            id: colDateCreated
            x: colDate.x + (colDate.visible ? colDate.width : 0)
            width: root.panel.effectiveColWidthDateCreated
            height: parent.height
            visible: root.panel.effectiveColShowDateCreated
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.createdText
                color: TextColors.fileSecondaryText
                opacity: 0.85
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            CellSeparator {}
        }

        // ── COLUMN: Extension ─────────────────────────────────────────────────
        Item {
            id: colExtension
            x: colDateCreated.x + (colDateCreated.visible ? colDateCreated.width : 0)
            width: root.panel.effectiveColWidthExtension
            height: parent.height
            visible: root.panel.effectiveColShowExtension
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.isDirectory ? "" : (root.suffix.length > 0 ? root.suffix.toLowerCase() : "—")
                color: TextColors.fileExtensionText
                opacity: 0.7
                font.pixelSize: Theme.fontSizeCaption
                font.family: "Consolas, Courier New, monospace"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            CellSeparator {}
        }

        // ── COLUMN: Attributes ────────────────────────────────────────────────
        Item {
            id: colAttributes
            x: colExtension.x + (colExtension.visible ? colExtension.width : 0)
            width: root.panel.effectiveColWidthAttributes
            height: parent.height
            visible: root.panel.effectiveColShowAttributes
            clip: true

            FileAttributeBadges {
                anchors.fill: parent
                attributesText: root.attributesText
                showPlaceholder: false
            }

            Label {
                anchors.centerIn: parent
                text: "—"
                color: TextColors.fileSecondaryText
                opacity: 0.3
                visible: false
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
            }
            CellSeparator {}
        }

        // ── COLUMN: Dimensions (lazy media) ───────────────────────────────────
        Item {
            id: colResolution
            x: colAttributes.x + (colAttributes.visible ? colAttributes.width : 0)
            width: root.panel.effectiveColWidthResolution
            height: parent.height
            visible: root.panel.effectiveColShowResolution
            clip: true

            onVisibleChanged: if (visible) root._ensureMetaLoaded()

            FileMetaText {
                anchors.fill: parent
                value: root._meta["dimensions"] || root._meta["resolution"] || ""
                loading: root._metaRequested && !root._metaLoaded
                fontPixelSize: 12
                textOpacity: 0.85
            }
            CellSeparator {}
        }

        // ── COLUMN: Duration (lazy media) ─────────────────────────────────────
        Item {
            id: colDuration
            x: colResolution.x + (colResolution.visible ? colResolution.width : 0)
            width: root.panel.effectiveColWidthDuration
            height: parent.height
            visible: root.panel.effectiveColShowDuration
            clip: true

            onVisibleChanged: if (visible) root._ensureMetaLoaded()

            FileMetaText {
                anchors.fill: parent
                value: root._meta["duration"] || ""
                loading: root._metaRequested && !root._metaLoaded
                fontPixelSize: 12
                textOpacity: 0.85
            }
            CellSeparator {}
        }

        // ── COLUMN: Artist (lazy media) ───────────────────────────────────────
        Item {
            id: colArtist
            x: colDuration.x + (colDuration.visible ? colDuration.width : 0)
            width: root.panel.effectiveColWidthArtist
            height: parent.height
            visible: root.panel.effectiveColShowArtist
            clip: true

            onVisibleChanged: if (visible) root._ensureMetaLoaded()

            FileMetaText {
                anchors.fill: parent
                value: root._meta["artist"] || ""
                loading: root._metaRequested && !root._metaLoaded
                fontPixelSize: 12
                textOpacity: 0.85
            }
            CellSeparator {}
        }

        // ── COLUMN: Album (lazy media) ────────────────────────────────────────
        Item {
            id: colAlbum
            x: colArtist.x + (colArtist.visible ? colArtist.width : 0)
            width: root.panel.effectiveColWidthAlbum
            height: parent.height
            visible: root.panel.effectiveColShowAlbum
            clip: true

            onVisibleChanged: if (visible) root._ensureMetaLoaded()

            FileMetaText {
                anchors.fill: parent
                value: root._meta["album"] || ""
                loading: root._metaRequested && !root._metaLoaded
                fontPixelSize: 12
                textOpacity: 0.85
            }
            CellSeparator {}
        }

        // ── COLUMN: Bitrate (lazy media) ──────────────────────────────────────
        Item {
            id: colBitrate
            x: colAlbum.x + (colAlbum.visible ? colAlbum.width : 0)
            width: root.panel.effectiveColWidthBitrate
            height: parent.height
            visible: root.panel.effectiveColShowBitrate
            clip: true

            onVisibleChanged: if (visible) root._ensureMetaLoaded()

            FileMetaText {
                anchors.fill: parent
                value: root._meta["bitrate"] || ""
                loading: root._metaRequested && !root._metaLoaded
                fontPixelSize: 12
                textOpacity: 0.85
            }
            CellSeparator {}
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
}
