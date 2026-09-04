import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../framework"
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
    property bool metadataHidden: true
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
    property bool adaptiveLayout: false
    property string baseViewMode: "adaptive"
    property int backgroundMode: 0

    readonly property int imageStatus: previewImage.status
    readonly property bool loading: previewImage.status === Image.Loading
    readonly property real availableImageWidth: Math.max(1, viewport.width - (root.adaptiveLayout ? 32 : 0))
    readonly property real availableImageHeight: Math.max(1, viewport.height - (root.adaptiveLayout ? 32 : 0))
    readonly property real displayedNaturalWidth: root.naturalDimensionsRotated
                                                  ? Math.max(1, root.imageHeight)
                                                  : Math.max(1, root.imageWidth)
    readonly property real displayedNaturalHeight: root.naturalDimensionsRotated
                                                   ? Math.max(1, root.imageWidth)
                                                   : Math.max(1, root.imageHeight)
    readonly property bool naturalDimensionsAvailable: root.imageWidth > 0 && root.imageHeight > 0
    readonly property real rawAspectRatio: root.naturalDimensionsAvailable
                                           ? root.imageWidth / root.imageHeight : 1.0
    readonly property real paintedAspectRatio: previewImage.paintedHeight > 0
                                               ? previewImage.paintedWidth / previewImage.paintedHeight
                                               : root.rawAspectRatio
    readonly property bool naturalDimensionsRotated: root.naturalDimensionsAvailable
                                                     && Math.abs(root.paintedAspectRatio - 1.0 / root.rawAspectRatio)
                                                        < Math.abs(root.paintedAspectRatio - root.rawAspectRatio)
    readonly property real actualScaleRelativeToFit: root.naturalDimensionsAvailable
                                                     && previewImage.paintedWidth > 0
                                                     && previewImage.paintedHeight > 0
                                                     ? Math.min(root.displayedNaturalWidth / previewImage.paintedWidth,
                                                                root.displayedNaturalHeight / previewImage.paintedHeight)
                                                     : 1.0
    readonly property real baseViewScale: !root.adaptiveLayout || root.baseViewMode === "fit"
                                          ? 1.0
                                          : root.baseViewMode === "actual"
                                            ? root.actualScaleRelativeToFit
                                            : Math.min(1.0, root.actualScaleRelativeToFit * 2.0)
    readonly property real effectiveMinimumZoom: root.adaptiveLayout
                                                 ? Math.min(root.minimumZoom,
                                                            root.actualScaleRelativeToFit
                                                            / Math.max(0.0001, root.baseViewScale))
                                                 : root.minimumZoom
    readonly property real effectiveScale: root.baseViewScale * root.zoomLevel
    readonly property string zoomPercentText: Math.round((root.adaptiveLayout
                                                         ? root.effectiveScale
                                                           / Math.max(0.0001, root.actualScaleRelativeToFit)
                                                         : root.zoomLevel) * 100) + "%"
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
    readonly property real paintedContentWidth: Math.max(1, previewImage.paintedWidth * root.effectiveScale)
    readonly property real paintedContentHeight: Math.max(1, previewImage.paintedHeight * root.effectiveScale)
    readonly property real paintedContentLeft: viewport.x + previewImage.x + previewImage.width / 2 - paintedContentWidth / 2
    readonly property real paintedContentTop: viewport.y + previewImage.y + previewImage.height / 2 - paintedContentHeight / 2
    readonly property real paintedContentRight: paintedContentLeft + paintedContentWidth
    readonly property real paintedContentBottom: paintedContentTop + paintedContentHeight
    readonly property real visibleContentLeft: Math.max(0, paintedContentLeft)
    readonly property real visibleContentTop: Math.max(0, paintedContentTop)
    readonly property real visibleContentRight: Math.min(width, paintedContentRight)
    readonly property real visibleContentBottom: Math.min(height, paintedContentBottom)
    readonly property real visibleContentHeight: Math.max(1, visibleContentBottom - visibleContentTop)
    readonly property int dragLayerZ: 1
    readonly property int overlayLayerZ: 2
    readonly property int floatingButtonLayerZ: 3
    readonly property int controlsBottomMargin: 0
    readonly property bool controlsBarVisible: root.controlsVisible && (root.loading || root.imageStatus === Image.Ready)

    clip: true

    signal hideMetadataRequested()
    signal showMetadataRequested()
    signal baseViewModeChangedByUser(string mode)
    signal backgroundModeChangedByUser(int mode)

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
        return [
            { label: "Format", value: root.formatText },
            { label: "Dimensions", value: root.dimensionsText }
        ]
    }

    function fullImageMetaItems() {
        return [
            { label: "Format", value: root.formatText },
            { label: "Dimensions", value: root.dimensionsText },
            { label: "Megapixels", value: root.megapixelsText },
            { label: "Depth", value: root.colorDepthText.replace("CD=", "") },
            { label: "Alpha", value: root.alphaText.replace("AC=", "") },
            { label: "DPI", value: root.dpiText.replace("DPI ", "") },
            { label: "Color space", value: root.colorSpaceText },
            { label: "Pixel format", value: root.pixelFormatText }
        ]
    }

    function canRequestThumbnail(path) {
        if (!quickLookController.canRequestThumbnailForPath(path)) return false
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
        return Math.max(root.effectiveMinimumZoom, Math.min(root.maximumZoom, value))
    }

    function clampOffsetX(value) {
        const contentWidth = root.adaptiveLayout
                           ? root.paintedContentWidth
                           : viewport.width * root.zoomLevel
        const limit = Math.max(0, (contentWidth - viewport.width) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function clampOffsetY(value) {
        const contentHeight = root.adaptiveLayout
                            ? root.paintedContentHeight
                            : viewport.height * root.zoomLevel
        const limit = Math.max(0, (contentHeight - viewport.height) / 2)
        return Math.max(-limit, Math.min(limit, value))
    }

    function resetView() {
        root.zoomLevel = 1.0
        root.offsetX = 0.0
        root.offsetY = 0.0
    }

    function setBaseViewMode(mode) {
        root.baseViewMode = mode
        root.baseViewModeChangedByUser(mode)
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
        root.backgroundModeChangedByUser(root.backgroundMode)
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

    onAdaptiveLayoutChanged: root.resetView()

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
        anchors.topMargin: 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.controlsBarVisible ? imageControls.height + root.controlsBottomMargin : 0
        clip: true

        Rectangle {
            anchors.fill: parent
            color: root.backgroundMode === 0
                   ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.035 : 0.025)
                   : "transparent"
        }

        Image {
            id: backgroundGrid
            anchors.fill: parent
            visible: root.backgroundMode === 1
            source: themeController.isDark
                    ? "qrc:/qt/qml/FM/qml/assets/checkerboard-dark.svg"
                    : "qrc:/qt/qml/FM/qml/assets/checkerboard-light.svg"
            fillMode: Image.Tile
            sourceSize.width: 32
            sourceSize.height: 32
            asynchronous: false
            cache: true
        }

        Image {
            id: previewImage
            width: root.adaptiveLayout ? root.availableImageWidth : viewport.width
            height: root.adaptiveLayout ? root.availableImageHeight : viewport.height
            x: (viewport.width - width) / 2 + root.offsetX
            y: (viewport.height - height) / 2 + root.offsetY
            scale: root.effectiveScale
            transformOrigin: Item.Center
            autoTransform: true
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
            id: loadingIndicator
            anchors.centerIn: parent
            running: root.showBusyIndicator && previewImage.status === Image.Loading
            palette.dark: root.backgroundMode === 1 ? Theme.categoryInfo : Theme.textPrimary
            palette.text: root.backgroundMode === 1 ? Theme.categoryInfo : Theme.textPrimary
            palette.accent: root.backgroundMode === 1 ? Theme.categoryInfo : Theme.textPrimary
        }
    }

    PreviewMetaStrip {
        id: imageMetaStrip
        z: root.overlayLayerZ
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.imageHeaderY(height)
        compact: root.compactMeta
        columnCount: root.compactMeta ? 0 : 4
        backgroundOpacity: 0
        borderOpacity: 0
        cornerRadius: 0
        labelWeight: Font.DemiBold
        showHideButton: true
        floatingCard: true
        accentColor: Theme.categoryInfo
        items: root.compactMeta ? root.compactImageMetaItems() : root.fullImageMetaItems()
        visible: root.metadataBarVisible
        onHideRequested: root.hideMetadataRequested()
    }

    FmIconButton {
        id: showMetadataButton
        z: root.floatingButtonLayerZ
        x: Math.max(8, root.visibleContentRight - width - (root.compactMeta ? 6 : 8))
        y: root.imageHeaderY(height)
        width: root.compactMeta ? 24 : 28
        height: width
        visible: root.metadataHidden && previewImage.status === Image.Ready
        hoverEnabled: true
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/eye.svg"
        iconSize: root.compactMeta ? 13 : 15
        svgRecolorColor: showMetadataButton.hovered ? Theme.chromeIconColor("hidden") : Theme.chromeIconColor("muted")
        showIdleSurface: true
        opacity: hovered ? 1.0 : 0.82
        display: AbstractButton.IconOnly
        ToolTip.visible: hovered
        ToolTip.text: "Show metadata"
        onClicked: root.showMetadataRequested()

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
                enabled: root.zoomLevel > root.effectiveMinimumZoom
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
                visible: root.adaptiveLayout
                text: "Adaptive"
                enabled: root.baseViewMode !== "adaptive" || root.zoomLevel !== 1.0
                implicitWidth: 62
                onClicked: root.setBaseViewMode("adaptive")
                ToolTip.visible: hovered
                ToolTip.text: "Adapt to screen"
            }

            ImageControlButton {
                text: "Fit"
                enabled: root.adaptiveLayout
                         ? root.baseViewMode !== "fit" || root.zoomLevel !== 1.0
                         : root.zoomLevel !== 1.0 || root.offsetX !== 0.0 || root.offsetY !== 0.0
                implicitWidth: 36
                onClicked: root.adaptiveLayout ? root.setBaseViewMode("fit") : root.resetView()
                ToolTip.visible: hovered
                ToolTip.text: "Fit to view"
            }

            ImageControlButton {
                visible: root.adaptiveLayout
                text: "1:1"
                enabled: root.baseViewMode !== "actual" || root.zoomLevel !== 1.0
                implicitWidth: 34
                onClicked: root.setBaseViewMode("actual")
                ToolTip.visible: hovered
                ToolTip.text: "Actual size"
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
        }
    }

    component ImageControlButton: FmButton {
        id: controlButton

        implicitWidth: 28
        implicitHeight: 28
        padding: 0
        hoverEnabled: true
        flat: true
        primaryColor: Theme.accent

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? Theme.accent : Theme.textSecondary
            opacity: controlButton.enabled ? 1.0 : 0.45
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

    }
}
