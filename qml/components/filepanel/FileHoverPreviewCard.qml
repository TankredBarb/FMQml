import QtQuick
import QtQuick.Controls
import "../common"
import "../../style"

Item {
    id: root

    property string path: ""
    property var info: ({})
    property var controller: null
    property bool requested: false
    property bool suppressed: false
    property int thumbnailRevision: info && info.thumbnailRevision !== undefined ? Number(info.thumbnailRevision) : 0
    property rect anchorRect: Qt.rect(0, 0, 1, 1)
    property int boundaryTopInset: 0
    property int boundaryBottomInset: 0
    property int delayMs: 320
    readonly property bool pointerInside: cardHover.hovered

    signal quickLookRequested(string path)
    signal openRequested(string path)
    signal wallpaperRequested(string path)

    readonly property bool hasPath: path.length > 0
    readonly property string pathFileName: {
        if (!hasPath) return ""
        const slash = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"))
        return slash >= 0 ? path.slice(slash + 1) : path
    }
    readonly property string fileName: info && info.name ? String(info.name) : pathFileName
    readonly property string suffix: {
        const modelSuffix = info && info.suffix ? String(info.suffix) : ""
        if (modelSuffix.length > 0) return modelSuffix.toUpperCase()
        const dot = pathFileName.lastIndexOf(".")
        return dot > 0 && dot < pathFileName.length - 1 ? pathFileName.slice(dot + 1).toUpperCase() : ""
    }
    readonly property string suffixLower: suffix.toLowerCase()
    readonly property bool mediaEligible: [
        "jpg", "jpeg", "png", "gif", "webp", "bmp", "tif", "tiff", "heic", "heif",
        "mp4", "m4v", "mov", "webm", "mkv", "avi"
    ].indexOf(suffixLower) >= 0
    readonly property bool videoFile: ["mp4", "m4v", "mov", "webm", "mkv", "avi"].indexOf(suffixLower) >= 0
    readonly property string typeLabel: info && info.typeLabel ? String(info.typeLabel) : (videoFile ? "Video" : "Image")
    readonly property string sizeText: info && info.sizeText ? String(info.sizeText) : ""
    readonly property string modifiedText: info && info.modifiedText ? String(info.modifiedText) : ""
    property var mediaMeta: ({})
    property string mediaMetaPath: ""
    property bool mediaMetaRequested: false
    property bool mediaMetaLoaded: false
    readonly property string dimensionsText: mediaMeta && mediaMeta.dimensions ? String(mediaMeta.dimensions) : ""
    readonly property string durationText: mediaMeta && mediaMeta.duration ? String(mediaMeta.duration) : ""
    readonly property string mediaFactsText: [videoFile ? durationText : "", dimensionsText].filter(function(value) {
        return value.length > 0
    }).join(" • ")
    readonly property var factRows: [
        { "label": "Name", "value": fileName },
        { "label": "Type", "value": typeLabel },
        { "label": "Duration", "value": videoFile ? durationText : "" },
        { "label": "Size", "value": sizeText },
        { "label": "Modified", "value": modifiedText }
    ].filter(function(row) { return String(row.value || "").length > 0 })
    readonly property bool wallpaperAvailable: controller && controller.canSetWallpaperPath
                                               && !videoFile
                                               && controller.canSetWallpaperPath(path)
    readonly property color cardAccent: Theme.accent
    readonly property color cardAccentSoft: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.12)
    readonly property color cardInk: Theme.textPrimary
    readonly property bool mediaReady: previewImage.status === Image.Ready
    readonly property bool loading: previewImage.status === Image.Loading

    readonly property int margin: 12
    readonly property int cursorGap: 18
    readonly property real availableWidth: parent ? parent.width : width
    readonly property real availableHeight: parent ? Math.max(0, parent.height - boundaryBottomInset) : height
    readonly property bool placeLeft: anchorRect.x + anchorRect.width + width + cursorGap > availableWidth
    readonly property bool placeBelow: anchorRect.y + height + margin > availableHeight
    readonly property real preferredX: placeLeft
                                      ? anchorRect.x - width - cursorGap
                                      : anchorRect.x + anchorRect.width + cursorGap
    readonly property real preferredY: placeBelow
                                      ? anchorRect.y + anchorRect.height - height
                                      : anchorRect.y

    width: Math.min(282, Math.max(236, parent ? parent.width * 0.34 : 248))
    height: Math.max(326, contentColumn.implicitHeight + 20)
    x: Math.max(margin, Math.min(preferredX, availableWidth - width - margin))
    y: Math.max(root.margin + boundaryTopInset,
                Math.min(preferredY, availableHeight - height - margin))
    visible: opacity > 0
    opacity: activeTimer.ready && hasPath && mediaEligible && requested && !suppressed ? 1 : 0
    enabled: opacity > 0 && !suppressed

    Behavior on opacity { NumberAnimation { duration: 120 } }

    function resetMediaMeta() {
        mediaMeta = {}
        mediaMetaPath = ""
        mediaMetaRequested = false
        mediaMetaLoaded = false
    }

    function requestMediaMeta() {
        if (!controller || !hasPath || !mediaEligible || mediaMetaRequested || mediaMetaLoaded) {
            return
        }
        mediaMetaRequested = true
        mediaMetaPath = path
        controller.fetchMetadataAsync(path)
    }

    onPathChanged: {
        resetMediaMeta()
        activeTimer.restart()
    }
    onRequestedChanged: {
        if (requested) activeTimer.restart()
        else activeTimer.stop()
    }
    onSuppressedChanged: {
        if (suppressed) activeTimer.stop()
        else if (requested) activeTimer.restart()
    }

    Timer {
        id: activeTimer
        property bool ready: false
        interval: root.delayMs
        repeat: false
        onTriggered: {
            ready = root.requested && root.hasPath && root.mediaEligible && !root.suppressed
            if (ready) {
                root.requestMediaMeta()
            }
        }
        onRunningChanged: if (running) ready = false
    }

    HoverHandler {
        id: cardHover
        enabled: root.enabled
    }

    Connections {
        target: root.controller
        ignoreUnknownSignals: true
        function onMetadataReady(filePath, meta) {
            if (filePath !== root.mediaMetaPath || filePath !== root.path) {
                return
            }
            root.mediaMeta = meta || {}
            root.mediaMetaLoaded = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.98 : 0.99)
        border.color: Theme.withAlpha(root.cardAccent, themeController.isDark ? 0.58 : 0.42)
        border.width: 2

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: 6
            color: "transparent"
            border.color: Theme.withAlpha(root.cardAccent, themeController.isDark ? 0.18 : 0.14)
            border.width: 1
        }
    }

    Column {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Rectangle {
            width: parent.width
            height: 158
            radius: 5
            color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.07 : 0.04)
            clip: true

            Image {
                id: previewImage
                anchors.fill: parent
                source: root.hasPath && root.mediaEligible ? "image://thumbnail/" + encodeURIComponent(root.path + "::thumbrev=" + root.thumbnailRevision) : ""
                sourceSize.width: 512
                sourceSize.height: 512
                asynchronous: true
                cache: false
                fillMode: Image.PreserveAspectFit
                opacity: status === Image.Ready ? 1 : 0
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.loading
                visible: running
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 160)
                spacing: 8
                visible: !root.loading && !root.mediaReady

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 42
                    height: 42
                    source: root.videoFile
                            ? "../../assets/filetypes-next/video.svg"
                            : "../../assets/filetypes-next/image.svg"
                    sourceSize.width: 42
                    sourceSize.height: 42
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.74
                }

                Text {
                    width: parent.width
                    text: root.suffix.length > 0 ? root.suffix : "Preview unavailable"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                height: 20
                width: Math.max(58, resolutionBadgeText.implicitWidth + 16)
                radius: 4
                visible: root.dimensionsText.length > 0
                color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.82 : 0.88)
                border.color: Theme.withAlpha(root.cardAccent, themeController.isDark ? 0.52 : 0.36)

                Text {
                    id: resolutionBadgeText
                    anchors.centerIn: parent
                    text: root.dimensionsText.replace(" × ", "x")
                    color: root.cardInk
                    font.pixelSize: Theme.fontSizeMini
                    font.weight: Font.DemiBold
                }
            }

            Canvas {
                anchors.centerIn: parent
                width: 28
                height: 28
                visible: root.videoFile && !root.loading
                opacity: 0.88
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = root.cardInk
                    ctx.beginPath()
                    ctx.moveTo(width * 0.34, height * 0.24)
                    ctx.lineTo(width * 0.34, height * 0.76)
                    ctx.lineTo(width * 0.76, height * 0.50)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        Text {
            width: parent.width
            text: root.typeLabel
            color: root.cardInk
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Column {
            width: parent.width
            spacing: 3

            Repeater {
                model: root.factRows.slice(0, 5)

                Row {
                    width: parent.width
                    height: Math.max(17, Theme.fontSizeMini + 5)
                    spacing: 8

                    Text {
                        width: Math.floor(parent.width * 0.43)
                        text: modelData.label
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeMini
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        width: parent.width - Math.floor(parent.width * 0.43) - parent.spacing
                        text: modelData.value
                        color: root.cardInk
                        font.pixelSize: Theme.fontSizeMini
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                        maximumLineCount: 1
                    }
                }
            }
        }

        Row {
            width: parent.width
            height: 34
            spacing: 8
            visible: !root.suppressed

            ToolButton {
                width: Math.floor((parent.width - parent.spacing) / 2)
                height: parent.height
                text: root.wallpaperAvailable ? "Set Wallpaper" : "Open"
                padding: 0
                enabled: root.mediaReady
                onClicked: root.wallpaperAvailable ? root.wallpaperRequested(root.path) : root.openRequested(root.path)
                background: Rectangle {
                    radius: 5
                    color: parent.pressed
                           ? Theme.withAlpha(root.cardAccent, themeController.isDark ? 0.30 : 0.24)
                           : parent.hovered ? Theme.withAlpha(root.cardAccent, themeController.isDark ? 0.22 : 0.16)
                                            : Theme.withAlpha(root.cardAccent, themeController.isDark ? 0.16 : 0.10)
                    border.color: Theme.withAlpha(root.cardAccent, parent.hovered ? 0.46 : 0.28)
                }
                contentItem: Text {
                    text: parent.text
                    color: root.cardInk
                    font.pixelSize: Theme.fontSizeMini
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            ToolButton {
                width: Math.floor((parent.width - parent.spacing) / 2)
                height: parent.height
                text: "Quick Look"
                padding: 0
                enabled: root.mediaReady
                onClicked: root.quickLookRequested(root.path)
                background: Rectangle {
                    radius: 5
                    color: parent.pressed
                           ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.18 : 0.12)
                           : parent.hovered ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.13 : 0.08)
                                            : Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.08 : 0.045)
                    border.color: Theme.withAlpha(Theme.border, parent.hovered ? 0.58 : 0.34)
                }
                contentItem: Text {
                    text: parent.text
                    color: root.cardInk
                    font.pixelSize: Theme.fontSizeMini
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }
}
