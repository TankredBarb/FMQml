import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

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
    required property string sizeText
    required property string modifiedText
    required property string createdText
    required property string attributesText
    required property string suffix
    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false

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
    property var _meta: ({})      // Loaded metadata cache: {resolution, duration, artist, album, bitrate}
    property bool _metaRequested: false
    property bool _metaLoaded: false

    function _ensureMetaLoaded() {
        if (_metaRequested || _metaLoaded) return
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
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    Component.onCompleted: {
        _ensureMetaLoaded()
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
               : (root.currentItem
                  ? Theme.itemCurrentFill
                  : ((hover.hovered && !root.scrolling) ? Theme.itemHoverFill : "transparent"))
        border.color: isSelected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.itemCurrentBorder : "transparent")
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
            
            Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad } }
        }

        // Zebra striping overlay
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: themeController.isDark ? "white" : "black"
            opacity: themeController.isDark ? 0.015 : 0.025
            visible: root.panel.showZebraStriping && (index % 2 === 1) && !isSelected && !root.currentItem && !(hover.hovered && !root.scrolling)
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
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
        enabled: true
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
        if (!scrolling) {
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
            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading) {
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
                   : (root.currentItem
                      ? Theme.itemCurrentFill
                      : ((hover.hovered && !root.scrolling) ? Theme.itemHoverFill : Theme.panelSurface))

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

            Behavior on color { ColorAnimation { duration: 90 } }
        }

        // ── COLUMN: Name ──────────────────────────────────────────────────────
        Item {
            id: colName
            x: root.panel.horizontalScrollActive && root.panel.horizontalScrollX > 12 ? root.panel.horizontalScrollX - 12 : 0
            width: root.panel.colWidthName
            height: parent.height
            z: 3
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 8
                spacing: 8
                visible: !root.isRenaming

                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter

                        Image {
                            anchors.centerIn: parent
                        source: "image://icon/" + encodeURIComponent(root.path + (root.isDirectory ? "?directory=true" : ""))
                            sourceSize: Qt.size(20, 20)
                            asynchronous: true
                            cache: true
                        }
                    }

                Label {
                    Layout.fillWidth: true
                    text: root.name
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    font.weight: isSelected || root.currentItem ? Font.Medium : Font.Normal
                    horizontalAlignment: Text.AlignLeft
                }
            }

            Loader {
                id: renameLoader
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 8
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                active: root.isRenaming
                visible: root.isRenaming
                sourceComponent: TextField {
                    id: renameInput
                    text: root.name
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                    color: Theme.textPrimary
                    selectByMouse: true
                    leftPadding: 8
                    rightPadding: 8
                    
                    opacity: 0
                    scale: 0.96
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

                    background: Rectangle {
                        id: bgRect
                        color: Theme.panelSurfaceStrong
                        radius: Theme.radiusSm
                        border.color: renameInput.activeFocus ? Theme.focusRing : Theme.panelBorder
                        border.width: renameInput.activeFocus ? 1.5 : 1
                        
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                        Behavior on border.width { NumberAnimation { duration: 120 } }

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: renameInput.activeFocus ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.35 : 0.2) : Theme.glassShadow
                            shadowBlur: renameInput.activeFocus ? 12 : 8
                            shadowVerticalOffset: renameInput.activeFocus ? 1 : 2
                            
                            Behavior on shadowColor { ColorAnimation { duration: 120 } }
                            Behavior on shadowBlur { NumberAnimation { duration: 120 } }
                        }
                    }

                    onAccepted: {
                        if (root.index >= 0) {
                            const idx = root.index
                            const txt = text
                            const ctrl = controller
                            Qt.callLater(function() {
                                if (ctrl.rename(idx, txt)) {
                                    root.isRenaming = false
                                } else {
                                    if (renameLoader.item) {
                                        renameLoader.item.forceActiveFocus()
                                        renameLoader.item.selectAll()
                                    }
                                }
                            })
                        }
                    }
                    
                    Keys.onEscapePressed: (event) => {
                        root.isRenaming = false
                        event.accepted = true
                    }

                    onActiveFocusChanged: if (!activeFocus) root.isRenaming = false

                    Component.onCompleted: {
                        opacity = 1.0
                        scale = 1.0
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
                text: getFileTypeString(root.suffix, root.isDirectory)
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

            // Icon-badge row for attributes
            Row {
                anchors.centerIn: parent
                spacing: 3
                visible: badgesRepeater.count > 0

                Repeater {
                    id: badgesRepeater
                    model: {
                        let badges = []
                        const attrs = root.attributesText || ""
                        if (attrs.indexOf('D') >= 0) badges.push({ letter: "D", color: "#3b82f6", tip: "Directory" })
                        if (attrs.indexOf('H') >= 0) badges.push({ letter: "H", color: "#64748b", tip: "Hidden" })
                        if (attrs.indexOf('R') >= 0) badges.push({ letter: "R", color: "#ef4444", tip: "Read-only" })
                        if (attrs.indexOf('L') >= 0) badges.push({ letter: "L", color: "#8b5cf6", tip: "Symlink" })
                        if (attrs.indexOf('S') >= 0) badges.push({ letter: "S", color: "#f59e0b", tip: "System" })
                        return badges
                    }

                    Rectangle {
                        width: 16
                        height: 16
                        radius: Theme.radiusSm
                        color: Qt.rgba(
                            Qt.color(modelData.color).r,
                            Qt.color(modelData.color).g,
                            Qt.color(modelData.color).b, 0.18)
                        border.color: Qt.rgba(
                            Qt.color(modelData.color).r,
                            Qt.color(modelData.color).g,
                            Qt.color(modelData.color).b, 0.5)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.letter
                            color: modelData.color
                            font.pixelSize: 9
                            font.bold: true
                        }

                        ToolTip.visible: attrMa.containsMouse
                        ToolTip.text: modelData.tip
                        ToolTip.delay: 500

                        MouseArea { id: attrMa; anchors.fill: parent; hoverEnabled: true }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: "—"
                color: Theme.textSecondary
                opacity: 0.3
                visible: badgesRepeater.count === 0
                font.pixelSize: 12
            }
            CellSeparator {}
        }

        // ── COLUMN: Resolution (lazy media) ───────────────────────────────────
        Item {
            id: colResolution
            x: colAttributes.x + (colAttributes.visible ? colAttributes.width : 0)
            width: root.panel.colWidthResolution
            height: parent.height
            visible: root.panel.colShowResolution
            clip: true

            onVisibleChanged: if (visible) root._ensureMetaLoaded()

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root._meta["resolution"] || (root._metaRequested && !root._metaLoaded ? "…" : "")
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
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

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root._meta["duration"] || (root._metaRequested && !root._metaLoaded ? "…" : "")
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
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

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root._meta["artist"] || (root._metaRequested && !root._metaLoaded ? "…" : "")
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
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

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root._meta["album"] || (root._metaRequested && !root._metaLoaded ? "…" : "")
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
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

            Label {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root._meta["bitrate"] || (root._metaRequested && !root._metaLoaded ? "…" : "")
                color: Theme.textSecondary
                opacity: 0.85
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            CellSeparator {}
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    function getFileTypeString(suffix, isDirectory) {
        if (isDirectory) return "Folder"
        if (!suffix) return "File"
        const s = suffix.toLowerCase()
        if (s === "png" || s === "jpg" || s === "jpeg" || s === "gif" || s === "webp" || s === "bmp" || s === "ico" || s === "svg" || s === "avif" || s === "heic") return s.toUpperCase() + " Image"
        if (s === "pdf") return "PDF Document"
        if (s === "txt") return "Text File"
        if (s === "md")  return "Markdown"
        if (s === "json") return "JSON"
        if (s === "xml" || s === "html" || s === "htm") return s.toUpperCase()
        if (s === "css") return "CSS Stylesheet"
        if (s === "js" || s === "ts") return s.toUpperCase() + " Script"
        if (s === "cpp" || s === "c" || s === "h" || s === "hpp") return "C/C++ Source"
        if (s === "py") return "Python Script"
        if (s === "rs") return "Rust Source"
        if (s === "go") return "Go Source"
        if (s === "java" || s === "kt") return s === "kt" ? "Kotlin Source" : "Java Source"
        if (s === "mp3" || s === "flac" || s === "ogg" || s === "m4a" || s === "wav" || s === "wma") return s.toUpperCase() + " Audio"
        if (s === "mp4" || s === "mkv" || s === "avi" || s === "mov" || s === "wmv") return s.toUpperCase() + " Video"
        if (s === "zip" || s === "rar" || s === "7z" || s === "tar" || s === "gz" || s === "xz") return s.toUpperCase() + " Archive"
        if (s === "exe" || s === "msi") return s.toUpperCase() + " Application"
        if (s === "bat" || s === "cmd" || s === "ps1" || s === "sh") return "Script"
        if (s === "lnk") return "Shortcut"
        if (s === "iso") return "Disk Image"
        if (s === "ttf" || s === "otf" || s === "woff" || s === "woff2") return "Font"
        return s.toUpperCase() + " File"
    }
}
