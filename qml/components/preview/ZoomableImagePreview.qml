import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    property bool compactMeta: false
    property bool metadataHidden: false
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
    property bool controlsVisible: true
    property int backgroundMode: 0

    readonly property int imageStatus: previewImage.status
    readonly property bool loading: previewImage.status === Image.Loading
    readonly property string zoomPercentText: Math.round(root.zoomLevel * 100) + "%"
    readonly property string backgroundModeText: backgroundMode === 0 ? "Soft" : (backgroundMode === 1 ? "Grid" : "Clear")
    readonly property string formatText: imageFormatText.length > 0 ? imageFormatText : (extraValue("Format").length > 0 ? extraValue("Format") : (extension.length > 0 ? extension.toUpperCase() : ""))
    readonly property string dimensionsText: imageWidth > 0 && imageHeight > 0 ? imageWidth + " x " + imageHeight : extraValue("Dimensions")
    readonly property string megapixelsText: megapixelsCompactText()
    readonly property string colorDepthText: colorDepthCompactText()
    readonly property string alphaText: alphaCompactText()
    readonly property string dpiText: {
        const value = imageDpiText.length > 0 ? imageDpiText : extraValue("DPI")
        return value.length > 0 ? "DPI " + value : ""
    }
    readonly property string colorSpaceText: imageColorSpaceText.length > 0 ? imageColorSpaceText : extraValue("Color Space")
    readonly property string pixelFormatText: imagePixelFormatText.length > 0 ? imagePixelFormatText : extraValue("Pixel Format")
    readonly property bool hasMetadataItems: root.compactMeta
                                             ? compactImageMetaItems().length > 0
                                             : fullImageMetaItems().length > 0
    readonly property bool metadataBarReserved: !root.metadataHidden && root.hasMetadataItems
    readonly property bool metadataBarVisible: root.metadataBarReserved && previewImage.status === Image.Ready
    readonly property real paintedContentWidth: Math.max(1, previewImage.paintedWidth * root.zoomLevel)
    readonly property real paintedContentHeight: Math.max(1, previewImage.paintedHeight * root.zoomLevel)
    readonly property real paintedContentLeft: viewport.x + previewImage.x + previewImage.width / 2 - paintedContentWidth / 2
    readonly property real paintedContentTop: viewport.y + previewImage.y + previewImage.height / 2 - paintedContentHeight / 2
    readonly property real paintedContentRight: paintedContentLeft + paintedContentWidth
    readonly property real paintedContentBottom: paintedContentTop + paintedContentHeight
    readonly property real visibleContentLeft: Math.max(0, paintedContentLeft)
    readonly property real visibleContentTop: Math.max(0, paintedContentTop)
    readonly property real visibleContentRight: Math.min(width, paintedContentRight)
    readonly property real visibleContentBottom: Math.min(height, paintedContentBottom)
    readonly property real visibleContentWidth: Math.max(1, visibleContentRight - visibleContentLeft)
    readonly property real visibleContentHeight: Math.max(1, visibleContentBottom - visibleContentTop)
    readonly property int dragLayerZ: 1
    readonly property int overlayLayerZ: 2
    readonly property int floatingButtonLayerZ: 3
    readonly property int controlsBottomMargin: 0
    readonly property bool controlsBarVisible: root.controlsVisible && (root.loading || root.imageStatus === Image.Ready)

    clip: true

    signal hideMetadataRequested()
    signal showMetadataRequested()

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

    function colorDepthCompactText() {
        const value = root.imageColorDepthText.length > 0 ? root.imageColorDepthText : extraValue("Color Depth")
        return value.length > 0 ? "CD=" + value : ""
    }

    function megapixelsCompactText() {
        if (root.imageWidth > 0 && root.imageHeight > 0) {
            const mp = root.imageWidth * root.imageHeight / 1000000.0
            if (mp >= 0.1) {
                return mp.toFixed(1) + " MP"
            }
        }
        return extraValue("Megapixels")
    }

    function compactImageMetaItems() {
        return [root.formatText, root.dimensionsText, root.colorDepthText, root.alphaText]
    }

    function fullImageMetaItems() {
        return [
            root.formatText,
            root.dimensionsText,
            root.megapixelsText,
            root.colorDepthText,
            root.alphaText,
            root.dpiText,
            root.colorSpaceText,
            root.pixelFormatText
        ]
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
        const limit = Math.max(0, (viewport.width * root.zoomLevel - viewport.width) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function clampOffsetY(value) {
        const limit = Math.max(0, (viewport.height * root.zoomLevel - viewport.height) / 2)
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

    function cycleBackground() {
        root.backgroundMode = (root.backgroundMode + 1) % 3
    }

    function imageHeaderY(stripHeight) {
        if (root.visibleContentHeight < stripHeight) {
            return root.visibleContentTop
        }
        return Math.min(root.visibleContentTop, root.visibleContentBottom - stripHeight)
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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root.metadataBarReserved ? imageMetaStrip.height : 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.controlsBarVisible ? imageControls.height + root.controlsBottomMargin : 0
        clip: true

        Rectangle {
            anchors.fill: parent
            color: root.backgroundMode === 0
                   ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.035 : 0.025)
                   : "transparent"
        }

        Item {
            id: backgroundGrid
            anchors.fill: parent
            visible: root.backgroundMode === 1
            readonly property int cellSize: 16
            readonly property int columnCount: Math.max(0, Math.ceil(width / cellSize))
            readonly property int rowCount: Math.max(0, Math.ceil(height / cellSize))

            Repeater {
                model: backgroundGrid.visible ? backgroundGrid.rowCount * backgroundGrid.columnCount : 0

                Rectangle {
                    width: backgroundGrid.cellSize
                    height: backgroundGrid.cellSize
                    x: (index % backgroundGrid.columnCount) * backgroundGrid.cellSize
                    y: Math.floor(index / backgroundGrid.columnCount) * backgroundGrid.cellSize
                    color: (Math.floor(index / backgroundGrid.columnCount) + index % backgroundGrid.columnCount) % 2 === 0
                           ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.075 : 0.055)
                           : Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.025 : 0.018)
                }
            }
        }

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

    PreviewMetaStrip {
        id: imageMetaStrip
        z: root.overlayLayerZ
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
        compact: root.compactMeta
        columnCount: root.compactMeta ? 0 : 4
        backgroundOpacity: 0
        borderOpacity: 0
        cornerRadius: 0
        labelWeight: Font.DemiBold
        showHideButton: true
        items: root.compactMeta ? root.compactImageMetaItems() : root.fullImageMetaItems()
        visible: root.metadataBarVisible
        onHideRequested: root.hideMetadataRequested()
    }

    ToolButton {
        id: showMetadataButton
        z: root.floatingButtonLayerZ
        x: Math.max(8, root.visibleContentRight - width - (root.compactMeta ? 6 : 8))
        y: root.imageHeaderY(height)
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
                sourcePath: "qrc:/qt/qml/FM/qml/assets/toolbar-next/eye.svg"
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

    MouseArea {
        z: root.dragLayerZ
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
        id: imageControls
        z: root.overlayLayerZ
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        height: 42
        radius: 0
        color: "transparent"
        border.color: "transparent"
        border.width: 0
        visible: root.controlsBarVisible
        opacity: previewImage.status === Image.Error ? 0.0 : 1.0

        RowLayout {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(1, Math.min(parent.width - 20, 420))
            height: parent.height
            spacing: 6

            ImageControlButton {
                text: "-"
                enabled: root.zoomLevel > root.minimumZoom
                onClicked: root.applyZoom(root.zoomLevel - root.zoomStep)
                ToolTip.visible: hovered
                ToolTip.text: "Zoom out"
            }

            ImageControlButton {
                text: "+"
                enabled: root.zoomLevel < root.maximumZoom
                onClicked: root.applyZoom(root.zoomLevel + root.zoomStep)
                ToolTip.visible: hovered
                ToolTip.text: "Zoom in"
            }

            ImageControlButton {
                text: "Fit"
                enabled: root.zoomLevel !== 1.0 || root.offsetX !== 0.0 || root.offsetY !== 0.0
                implicitWidth: 36
                onClicked: root.resetView()
                ToolTip.visible: hovered
                ToolTip.text: "Fit to view"
            }

            Label {
                text: root.zoomPercentText
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMicro
                font.bold: true
                Layout.preferredWidth: 44
                horizontalAlignment: Text.AlignHCenter
            }

            Item {
                Layout.fillWidth: true
            }

            ImageControlButton {
                text: root.backgroundModeText
                implicitWidth: 48
                onClicked: root.cycleBackground()
                ToolTip.visible: hovered
                ToolTip.text: "Change background"
            }

            BusyIndicator {
                running: root.loading
                visible: root.loading
                width: 18
                height: 18
            }
        }
    }

    component ImageControlButton: Button {
        id: controlButton

        implicitWidth: 28
        implicitHeight: 28
        padding: 0
        hoverEnabled: true

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? Theme.accent : Theme.textSecondary
            opacity: controlButton.enabled ? 1.0 : 0.45
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: controlButton.down
                   ? Theme.surfaceActive
                   : (controlButton.hovered ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.20 : 0.14) : "transparent")
            border.color: controlButton.hovered ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.48 : 0.34) : "transparent"
            border.width: controlButton.hovered ? 1 : 0
        }
    }
}
