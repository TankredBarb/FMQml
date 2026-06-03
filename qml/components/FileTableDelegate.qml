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
    z: root.isRenaming ? 100 : 0

    // Signals
    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()
    signal emptySpaceRightClicked()

    implicitHeight: Theme.rowHeight

    property bool isRenaming: false
    property real visualOffsetX: 0

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

    opacity: isHidden ? 0.55 : 1.0

    // ── Background ────────────────────────────────────────────────────────────
    Rectangle {
        id: bgRect
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: Theme.radiusSm

        color: isSelected
               ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
               : ((hover.hovered && !root.scrolling) ? Theme.itemHoverFill : "transparent")
        border.color: isSelected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.withAlpha(Theme.focusRing, root.panelActive ? 0.82 : 0.38) : "transparent")
        border.width: isSelected || root.currentItem ? 1 : 0
        transform: Translate { x: root.visualOffsetX }

        // Subtle vertical indicator bar for selected rows
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            anchors.leftMargin: 4
            width: isSelected ? 3 : 0
            radius: 1.5
            color: Theme.accent
            
            Behavior on width {
                enabled: !root.resizeOptimized
                NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad }
            }
        }

        // Zebra striping overlay
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: themeController.isDark ? "white" : "black"
            opacity: themeController.isDark ? 0.015 : 0.025
            visible: root.panel.showZebraStriping && (index % 2 === 1) && !isSelected && !root.currentItem && !(hover.hovered && !root.scrolling)
        }

        Behavior on color {
            enabled: !root.resizeOptimized
            ColorAnimation { duration: Theme.motionFast }
        }
        Behavior on border.color {
            enabled: !root.resizeOptimized
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
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                root.controller.hoveredPath = root.path
            } else if (root.controller.hoveredPath === root.path) {
                root.controller.hoveredPath = ""
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
        scrollGestureEnabled: false
        onWheel: (wheel) => { wheel.accepted = false }

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                var colNamePos = root.mapToItem(colName, mouse.x, mouse.y)
                if (colNamePos.x >= 0 && colNamePos.x <= colName.width) {
                    root.rightClicked()
                } else {
                    root.emptySpaceRightClicked()
                }
            } else {
                root.clicked(mouse)
            }
        }

        onDoubleClicked: root.doubleClicked()
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
            width: root.panel.colWidthName + 12
            height: parent.height
            z: 2
            visible: root.panel.horizontalScrollActive && root.panel.horizontalScrollX > 12
            color: isSelected
                   ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
                   : ((hover.hovered && !root.scrolling) ? Theme.itemHoverFill : Theme.panelSurface)

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
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: themeController.isDark ? Qt.rgba(0,0,0,0.25) : Qt.rgba(0,0,0,0.08) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            Behavior on color {
                enabled: !root.resizeOptimized
                ColorAnimation { duration: 90 }
            }
        }

        // ── COLUMN: Name ──────────────────────────────────────────────────────
        Item {
            id: colName
            x: root.panel.horizontalScrollActive && root.panel.horizontalScrollX > 12 ? root.panel.horizontalScrollX - 12 : 0
            width: root.panel.colWidthName
            height: parent.height
            z: 3
            clip: !root.isRenaming

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 8
                spacing: 8
                visible: !root.isRenaming

                FileIconCell {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    Layout.alignment: Qt.AlignVCenter
                    path: root.path
                    isDirectory: root.isDirectory
                    suffix: root.suffix
                    useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
                    iconSize: 16
                }

                Label {
                    Layout.fillWidth: true
                    text: root.name
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    font.weight: isSelected ? Font.Medium : Font.Normal
                    horizontalAlignment: Text.AlignLeft
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
                fontPixelSize: 13
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
            x: root.panel.colWidthName
            width: root.panel.colWidthSize
            height: parent.height
            visible: root.panel.colShowSize
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.sizeText.length > 0 ? root.sizeText : "—"
                color: Theme.textSecondary
                opacity: root.sizeText.length > 0 ? 0.85 : 0.35
                font.pixelSize: 12
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
            width: root.panel.colWidthType
            height: parent.height
            visible: root.panel.colShowType
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.controller ? root.controller.fileTypeLabelFor(root.suffix, root.isDirectory) : ""
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
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
            width: root.panel.colWidthDate
            height: parent.height
            visible: root.panel.colShowDate
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.modifiedText
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
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
            width: root.panel.colWidthDateCreated
            height: parent.height
            visible: root.panel.colShowDateCreated
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.createdText
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
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
            width: root.panel.colWidthExtension
            height: parent.height
            visible: root.panel.colShowExtension
            clip: true

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.isDirectory ? "" : (root.suffix.length > 0 ? root.suffix.toLowerCase() : "—")
                color: Theme.textSecondary
                opacity: 0.7
                font.pixelSize: 11
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
            width: root.panel.colWidthAttributes
            height: parent.height
            visible: root.panel.colShowAttributes
            clip: true

            FileAttributeBadges {
                anchors.fill: parent
                attributesText: root.attributesText
                showPlaceholder: false
            }

            Label {
                anchors.centerIn: parent
                text: "—"
                color: Theme.textSecondary
                opacity: 0.3
                visible: false
                font.pixelSize: 12
            }
            CellSeparator {}
        }

        // ── COLUMN: Dimensions (lazy media) ───────────────────────────────────
        Item {
            id: colResolution
            x: colAttributes.x + (colAttributes.visible ? colAttributes.width : 0)
            width: root.panel.colWidthResolution
            height: parent.height
            visible: root.panel.colShowResolution
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
            width: root.panel.colWidthDuration
            height: parent.height
            visible: root.panel.colShowDuration
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
            width: root.panel.colWidthArtist
            height: parent.height
            visible: root.panel.colShowArtist
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
            width: root.panel.colWidthAlbum
            height: parent.height
            visible: root.panel.colShowAlbum
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
            width: root.panel.colWidthBitrate
            height: parent.height
            visible: root.panel.colShowBitrate
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
