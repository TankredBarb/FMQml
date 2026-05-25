import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property string explicitSource: ""
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048
    property int fillMode: Image.PreserveAspectFit
    property bool showBusyIndicator: true
    property bool showOverlayIcon: false
    property string overlayIconSource: ""
    property int overlayIconSize: 64
    property real overlayIconOpacity: 0.6
    property bool requestThumbnail: true
    property var thumbnailSuffixes: [
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "ico", "tif", "tiff",
        "svg", "svgz", "pdf",
        "ttf", "otf", "woff", "woff2",
        "mp3", "flac", "ogg", "m4a", "m4b", "wav", "wma",
        "mp4", "avi", "mkv", "mov", "wmv", "webm", "m4v"
    ]

    property real zoomLevel: 1.0
    property real zoomStep: 0.12
    property real minimumZoom: 1.0
    property real maximumZoom: 4.0
    property bool resetZoomOnSourceChange: true

    readonly property int imageStatus: previewImage.status
    readonly property bool loading: previewImage.status === Image.Loading
    readonly property string zoomPercentText: Math.round(root.zoomLevel * 100) + "%"

    clip: true

    function resolvedOverlayIconSource() {
        if (!root.overlayIconSource || root.overlayIconSource.length === 0) {
            return ""
        }
        if (root.overlayIconSource.startsWith("qrc:") || root.overlayIconSource.startsWith("image:")) {
            return root.overlayIconSource
        }
        if (root.overlayIconSource.startsWith("../")) {
            return "qrc:/qt/qml/FM/qml/" + root.overlayIconSource.slice(3)
        }
        return root.overlayIconSource
    }

    function canRequestThumbnail(path) {
        if (!path || path.length === 0) {
            return false
        }
        if (path === "devices://" || path.endsWith("/") || path.endsWith("\\")) {
            return false
        }
        const slash = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"))
        const name = slash >= 0 ? path.slice(slash + 1) : path
        const dot = name.lastIndexOf(".")
        if (dot <= 0 || dot >= name.length - 1) {
            return false
        }
        const suffix = name.slice(dot + 1).toLowerCase()
        return root.thumbnailSuffixes.indexOf(suffix) >= 0
    }

    function clampZoom(value) {
        return Math.max(root.minimumZoom, Math.min(root.maximumZoom, value))
    }

    function clampOffsetX(value) {
        const limit = Math.max(0, (root.width * root.zoomLevel - root.width) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function clampOffsetY(value) {
        const limit = Math.max(0, (root.height * root.zoomLevel - root.height) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function resetView() {
        root.zoomLevel = 1.0
        root.offsetX = 0.0
        root.offsetY = 0.0
    }

    function applyZoom(nextZoom) {
        root.zoomLevel = root.clampZoom(nextZoom)
        root.offsetX = root.clampOffsetX(root.offsetX)
        root.offsetY = root.clampOffsetY(root.offsetY)
    }

    onSourcePathChanged: {
        if (root.resetZoomOnSourceChange) {
            root.resetView()
        }
    }

    onExplicitSourceChanged: {
        if (root.resetZoomOnSourceChange) {
            root.resetView()
        }
    }

    property real offsetX: 0.0
    property real offsetY: 0.0

    onWidthChanged: {
        root.offsetX = root.clampOffsetX(root.offsetX)
    }

    onHeightChanged: {
        root.offsetY = root.clampOffsetY(root.offsetY)
    }

    Item {
        id: viewport
        anchors.fill: parent
        clip: true

        Image {
            id: previewImage
            width: viewport.width
            height: viewport.height
            x: root.offsetX
            y: root.offsetY
            scale: root.zoomLevel
            transformOrigin: Item.Center
            source: root.explicitSource.length > 0
                    ? root.explicitSource
                    : (root.requestThumbnail && root.canRequestThumbnail(root.sourcePath)
                       ? "image://thumbnail/" + encodeURIComponent(root.sourcePath)
                       : "")
            fillMode: root.fillMode
            asynchronous: true
            cache: false
            sourceSize.width: root.sourceSizeWidth
            sourceSize.height: root.sourceSizeHeight
            smooth: true
            opacity: status === Image.Ready ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

        Image {
            anchors.centerIn: parent
            source: root.resolvedOverlayIconSource()
            sourceSize: Qt.size(root.overlayIconSize, root.overlayIconSize)
            visible: root.showOverlayIcon && previewImage.status === Image.Ready && root.resolvedOverlayIconSource().length > 0
            opacity: root.overlayIconOpacity
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: root.showBusyIndicator && previewImage.status === Image.Loading
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: previewImage.status !== Image.Error
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        property real pressX: 0.0
        property real pressY: 0.0
        property real startOffsetX: 0.0
        property real startOffsetY: 0.0

        onPressed: (mouse) => {
            pressX = mouse.x
            pressY = mouse.y
            startOffsetX = root.offsetX
            startOffsetY = root.offsetY
        }

        onPositionChanged: (mouse) => {
            if (!pressed) {
                return
            }
            root.offsetX = root.clampOffsetX(startOffsetX + (mouse.x - pressX))
            root.offsetY = root.clampOffsetY(startOffsetY + (mouse.y - pressY))
        }

        onWheel: (wheel) => {
            const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
            if (delta === 0) {
                return
            }

            const step = delta > 0 ? root.zoomStep : -root.zoomStep
            root.applyZoom(root.zoomLevel + step)
            wheel.accepted = true
        }

        onDoubleClicked: {
            root.resetView()
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        width: Math.min(parent.width - 40, 560)
        height: Math.max(74, overlayColumn.implicitHeight + 20)
        radius: 16
        color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg, 0.84)
        border.color: Theme.withAlpha(Theme.border, 0.85)
        border.width: 1
        visible: root.loading || root.imageStatus === Image.Ready
        opacity: previewImage.status === Image.Error ? 0.0 : 1.0

        ColumnLayout {
            id: overlayColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Image {
                    source: "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/info.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: 0.85
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: "Zoom with mouse wheel"
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "Double-click to restore"
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: zoomLabel.implicitWidth + 18
                    implicitHeight: 28
                    radius: 14
                    color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.12)
                    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.32 : 0.22)
                    border.width: 1

                    Label {
                        id: zoomLabel
                        anchors.centerIn: parent
                        text: root.zoomPercentText
                        color: Theme.accentText
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                BusyIndicator {
                    running: root.loading
                    visible: root.loading
                    width: 18
                    height: 18
                }
            }
        }
    }
}
