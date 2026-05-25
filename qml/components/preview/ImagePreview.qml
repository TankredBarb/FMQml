import QtQuick
import QtQuick.Controls
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property string explicitSource: ""
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048
    property int fillMode: Image.PreserveAspectFit
    property bool showBusyIndicator: true
    property real contentScale: 1.0
    property real contentOffsetX: 0.0
    property real contentOffsetY: 0.0
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

    readonly property int imageStatus: previewImage.status

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

    Item {
        id: viewport
        anchors.fill: parent
        clip: true

        Image {
            id: previewImage
            width: viewport.width
            height: viewport.height
            x: root.contentOffsetX
            y: root.contentOffsetY
            scale: root.contentScale
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
