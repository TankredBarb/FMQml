import QtQuick
import QtQuick.Controls
import "../common"
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
    property string extension: ""
    property string sizeText: ""
    property string modifiedText: ""
    property int imageWidth: 0
    property int imageHeight: 0
    property string imageFormatText: ""
    property string imageColorDepthText: ""
    property string imageAlphaChannelText: ""
    property string imageDpiText: ""
    property string imageColorSpaceText: ""
    property string imagePixelFormatText: ""
    property var extraProperties: []
    property bool compactMeta: true
    property bool metadataHidden: false
    property var thumbnailSuffixes: [
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "ico", "tif", "tiff",
        "svg", "svgz", "pdf",
        "ttf", "otf", "woff", "woff2",
        "mp3", "flac", "ogg", "m4a", "m4b", "wav", "wma",
        "mp4", "avi", "mkv", "mov", "wmv", "webm", "m4v"
    ]

    readonly property int imageStatus: previewImage.status
    readonly property string formatText: imageFormatText.length > 0 ? imageFormatText : (extraValue("Format").length > 0 ? extraValue("Format") : (extension.length > 0 ? extension.toUpperCase() : ""))
    readonly property string dimensionsText: imageWidth > 0 && imageHeight > 0 ? imageWidth + " x " + imageHeight : extraValue("Dimensions")
    readonly property string colorDepthText: imageColorDepthText.length > 0 ? "CD=" + imageColorDepthText : (extraValue("Color Depth").length > 0 ? "CD=" + extraValue("Color Depth") : "")
    readonly property string alphaText: alphaCompactText()
    readonly property bool hasMetadataItems: formatText.length > 0
                                             || dimensionsText.length > 0
                                             || colorDepthText.length > 0
                                             || alphaText.length > 0

    signal hideMetadataRequested()
    signal showMetadataRequested()

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

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function extraValue(label) {
        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        for (let i = 0; i < extras.length; i++) {
            if (safeText(extras[i].label) === label) {
                return safeText(extras[i].value)
            }
        }
        return ""
    }

    function alphaCompactText() {
        const value = root.imageAlphaChannelText.length > 0 ? root.imageAlphaChannelText : extraValue("Alpha Channel")
        if (value.length === 0) {
            return ""
        }
        const lower = value.toLowerCase()
        return "AC=" + (lower === "yes" || lower === "true" || lower === "1" ? "1" : "0")
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

    PreviewMetaStrip {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: root.compactMeta ? 8 : 14
        width: Math.max(1, Math.min(parent.width - (root.compactMeta ? 16 : 28), root.compactMeta ? 260 : 420))
        compact: root.compactMeta
        backgroundOpacity: themeController.isDark ? 0.54 : 0.62
        borderOpacity: themeController.isDark ? 0.42 : 0.50
        labelWeight: Font.DemiBold
        showHideButton: true
        items: [root.formatText, root.dimensionsText, root.colorDepthText, root.alphaText]
        visible: !root.metadataHidden && previewImage.status === Image.Ready && visibleItems.length > 0
        onHideRequested: root.hideMetadataRequested()
    }

    ToolButton {
        id: showMetadataButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: root.compactMeta ? 8 : 14
        width: root.compactMeta ? 24 : 28
        height: width
        visible: root.metadataHidden && previewImage.status === Image.Ready
        hoverEnabled: true
        padding: 5
        opacity: hovered ? 1.0 : 0.82
        display: AbstractButton.IconOnly
        ToolTip.visible: hovered
        ToolTip.text: "Show metadata"
        onClicked: root.showMetadataRequested()

        contentItem: Item {
            implicitWidth: root.compactMeta ? 13 : 15
            implicitHeight: implicitWidth

            RecolorSvgIcon {
                anchors.centerIn: parent
                width: parent.implicitWidth
                height: parent.implicitHeight
                sourcePath: "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/eye.svg"
                recolorColor: showMetadataButton.hovered ? Theme.actionIconColor("hidden") : Theme.actionIconColor("muted")
                sourceSize: Qt.size(32, 32)
                opacity: showMetadataButton.enabled ? 1.0 : 0.42
            }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg,
                                   showMetadataButton.hovered ? 0.72 : 0.54)
            border.color: Theme.withAlpha(Theme.border, showMetadataButton.hovered ? 0.58 : 0.42)
            border.width: 1
        }
    }
}
