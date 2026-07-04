import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "../common"
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property string mediaSourceUrl: ""
    property string name: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string mimeName: ""
    property string extension: ""
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048
    property var extraProperties: []
    property bool metadataHidden: false
    property bool compact: false
    property bool playbackActive: true
    property bool mediaLoaded: false
    property bool playbackFailed: false
    property bool requestThumbnail: true
    property string playbackErrorText: ""

    readonly property color playTone: Theme.actionIconColor("media")
    readonly property color pauseTone: Theme.actionIconColor("navigation")
    readonly property color volumeTone: Theme.actionIconColor("utility")
    readonly property color mutedTone: Theme.actionIconColor("muted")
    readonly property int extraPropertyCount: extraList().length
    readonly property string videoDurationText: extraValue("Duration", root.extraPropertyCount)
    readonly property string videoDimensionsText: extraValue("Dimensions", root.extraPropertyCount)
    readonly property string videoCodecText: extraValue("Video Codec", root.extraPropertyCount)
    readonly property string videoFrameRateText: extraValue("Frame Rate", root.extraPropertyCount)
    readonly property string videoBitrateText: extraValue("Bitrate", root.extraPropertyCount)
    readonly property string videoPixelFormatText: extraValue("Pixel Format", root.extraPropertyCount)
    readonly property string audioCodecText: extraValue("Audio Codec", root.extraPropertyCount)
    readonly property string audioSampleRateText: extraValue("Sample Rate", root.extraPropertyCount)
    readonly property string audioChannelsText: extraValue("Channels", root.extraPropertyCount)
    readonly property string formatText: extension.length > 0 ? extension.toUpperCase() : "VIDEO"
    readonly property var metadataStripItems: [
        root.formatText,
        root.videoDurationText,
        root.videoDimensionsText,
        root.videoCodecText,
        root.videoFrameRateText,
        root.videoBitrateText,
        root.videoPixelFormatText,
        root.audioCodecText,
        root.audioSampleRateText,
        root.audioChannelsText
    ]
    readonly property bool metadataBarReserved: root.mediaLoaded && !root.metadataHidden && root.metadataStripItems.length > 0
    readonly property bool metadataBarVisible: root.metadataBarReserved

    signal hideMetadataRequested()
    signal showMetadataRequested()

    clip: true

    function timeText(ms) {
        if (!Number.isFinite(ms) || ms <= 0) return "0:00"
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function extraList() {
        return root.extraProperties && root.extraProperties.length !== undefined ? root.extraProperties : []
    }

    function extraValue(label, revision) {
        revision
        const extras = root.extraList()
        for (let i = 0; i < extras.length; i++) {
            if (root.safeText(extras[i].label) === label) {
                return root.safeText(extras[i].value)
            }
        }
        return ""
    }

    function ensureMediaLoaded() {
        if (root.mediaLoaded) return
        root.playbackFailed = false
        root.playbackErrorText = ""
        player.source = root.mediaSourceUrl
        root.mediaLoaded = true
    }

    function releaseMedia() {
        player.stop()
        player.source = ""
        root.mediaLoaded = false
        progressRail.value = 0
    }

    function failPlayback(message) {
        root.playbackErrorText = message && message.length > 0 ? message : "Playback unavailable"
        root.playbackFailed = true
        releaseMedia()
    }

    onSourcePathChanged: releaseMedia()
    onMediaSourceUrlChanged: releaseMedia()
    onPlaybackActiveChanged: {
        if (!playbackActive) {
            releaseMedia()
        }
    }
    Component.onDestruction: releaseMedia()

    AudioOutput {
        id: audioOutput
        volume: volumeRail.value
        muted: muteButton.checked || volumeRail.value <= 0
    }

    MediaPlayer {
        id: player
        audioOutput: audioOutput
        videoOutput: videoOutput

        onErrorOccurred: (error, errorString) => {
            console.warn("VideoPlaybackPreview error:", error, errorString, "source:", root.mediaSourceUrl)
            root.failPlayback(errorString)
        }
    }

    VideoPreview {
        id: stillPreview
        anchors.fill: parent
        sourcePath: root.sourcePath
        name: root.name
        sizeText: root.sizeText
        modifiedText: root.modifiedText
        mimeName: root.mimeName
        extension: root.extension
        sourceSizeWidth: root.sourceSizeWidth
        sourceSizeHeight: root.sourceSizeHeight
        compact: root.compact
        extraProperties: root.extraProperties
        metadataHidden: root.metadataHidden
        requestThumbnail: root.requestThumbnail
        opacity: root.mediaLoaded ? 0 : 1
        onHideMetadataRequested: root.hideMetadataRequested()
        onShowMetadataRequested: root.showMetadataRequested()
    }

    Item {
        id: videoViewport
        anchors.fill: parent
        anchors.topMargin: root.metadataBarReserved ? playbackMetaStrip.height : 0
        visible: root.mediaLoaded
        opacity: root.mediaLoaded ? 1 : 0
        clip: true

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
        }
    }

    PreviewMetaStrip {
        id: playbackMetaStrip
        z: 2
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
        compact: root.compact
        backgroundOpacity: 0
        borderOpacity: 0
        cornerRadius: 0
        labelWeight: Font.DemiBold
        showHideButton: true
        wrapItems: !root.compact
        items: root.metadataStripItems
        visible: root.metadataBarVisible
        onHideRequested: root.hideMetadataRequested()
    }

    ToolButton {
        id: showMetadataButton
        z: 3
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: root.compact ? 8 : 14
        width: root.compact ? 24 : 28
        height: width
        visible: root.mediaLoaded && root.metadataHidden
        hoverEnabled: true
        padding: 5
        opacity: hovered ? 1.0 : 0.82
        display: AbstractButton.IconOnly
        ToolTip.visible: hovered
        ToolTip.text: "Show metadata"
        onClicked: root.showMetadataRequested()

        contentItem: Item {
            implicitWidth: root.compact ? 13 : 15
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

    Rectangle {
        id: playbackErrorBadge
        z: 3
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: controlsBar.top
        anchors.bottomMargin: 10
        width: Math.min(parent.width - 32, playbackErrorLabel.implicitWidth + 24)
        height: 30
        radius: Theme.radiusMd
        visible: root.playbackFailed
        color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg, 0.92)
        border.color: Theme.withAlpha(Theme.warning, 0.62)
        border.width: 1

        Label {
            id: playbackErrorLabel
            anchors.centerIn: parent
            width: parent.width - 18
            text: root.playbackErrorText.length > 0 ? root.playbackErrorText : "Playback unavailable"
            font.pixelSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.warning
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Rectangle {
        id: controlsBar
        z: 3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        height: 48
        radius: Theme.radiusMd
        color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg, 0.90)
        border.color: Theme.withAlpha(Theme.border, 0.66)
        border.width: 1
        visible: root.playbackActive && root.mediaSourceUrl.length > 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            MediaIconButton {
                id: playButton
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                iconColor: player.playbackState === MediaPlayer.PlayingState ? root.pauseTone : root.playTone
                iconSource: player.playbackState === MediaPlayer.PlayingState
                            ? "qrc:/qt/qml/FM/qml/assets/toolbar-next/pause.svg"
                            : "qrc:/qt/qml/FM/qml/assets/toolbar-next/play.svg"
                tooltip: player.playbackState === MediaPlayer.PlayingState ? "Pause" : "Play"
                onClicked: {
                    if (player.playbackState === MediaPlayer.PlayingState) {
                        player.pause()
                    } else {
                        root.ensureMediaLoaded()
                        Qt.callLater(() => player.play())
                    }
                }
            }

            TimeLabel {
                Layout.preferredWidth: 42
                text: root.timeText(progressRail.dragging ? progressRail.value : player.position)
                horizontalAlignment: Text.AlignRight
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 24

                MediaRail {
                    id: progressRail
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    from: 0
                    to: Math.max(1, player.duration)
                    value: 0
                    enabled: player.duration > 0
                    accentColor: Theme.accent
                    handleSize: 16
                    trackHeight: 4
                    onCommitted: (newValue) => player.setPosition(Math.round(newValue))
                }
            }

            TimeLabel {
                Layout.preferredWidth: 42
                text: root.timeText(player.duration)
                horizontalAlignment: Text.AlignLeft
            }

            MediaIconButton {
                id: muteButton
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                checkable: true
                iconColor: checked || volumeRail.value <= 0 ? root.mutedTone : root.volumeTone
                iconSource: checked || volumeRail.value <= 0
                            ? "qrc:/qt/qml/FM/qml/assets/toolbar-next/volume-x.svg"
                            : "qrc:/qt/qml/FM/qml/assets/toolbar-next/volume-2.svg"
                tooltip: checked ? "Unmute" : "Mute"
            }

            Item {
                Layout.preferredWidth: 88
                Layout.preferredHeight: 22

                MediaRail {
                    id: volumeRail
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    from: 0
                    to: 1
                    value: 0.18
                    liveWhileDragging: true
                    accentColor: Theme.accent
                    handleSize: 13
                    trackHeight: 4
                }
            }
        }
    }

    Connections {
        target: player
        function onPositionChanged() {
            if (!progressRail.dragging) {
                progressRail.value = player.position
            }
        }
        function onDurationChanged() {
            if (!progressRail.dragging) {
                progressRail.value = player.position
            }
        }
    }

    component TimeLabel: Label {
        font.family: "Consolas"
        font.pixelSize: Theme.fontSizeCaption
        font.bold: true
        color: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component MediaIconButton: ToolButton {
        id: button

        property string iconSource: ""
        property string tooltip: ""
        property color iconColor: Theme.textPrimary

        padding: 0
        hoverEnabled: true

        background: Rectangle {
            radius: width / 2
            color: button.down
                   ? Theme.withAlpha(button.iconColor, themeController.isDark ? 0.24 : 0.18)
                   : (button.hovered ? Theme.withAlpha(button.iconColor, themeController.isDark ? 0.16 : 0.12) : Theme.withAlpha(button.iconColor, themeController.isDark ? 0.07 : 0.05))
            border.color: Theme.withAlpha(button.iconColor, button.hovered ? 0.38 : 0.00)
            border.width: button.hovered ? 1 : 0
        }

        contentItem: Item {
            RecolorSvgIcon {
                anchors.centerIn: parent
                width: 17
                height: width
                sourcePath: button.iconSource
                recolorColor: button.iconColor
                sourceSize: Qt.size(36, 36)
                opacity: button.enabled ? 1 : 0.42
            }
        }

        ToolTip.visible: hovered && tooltip.length > 0
        ToolTip.text: tooltip
    }

    component MediaRail: Item {
        id: rail

        property real from: 0
        property real to: 1
        property real value: 0
        property bool liveWhileDragging: false
        property color accentColor: Theme.accent
        property int handleSize: 16
        property int trackHeight: 6
        readonly property bool dragging: inputArea.pressed
        readonly property real range: Math.max(0.0001, to - from)
        readonly property real progress: Math.max(0, Math.min(1, (value - from) / range))
        readonly property real usableWidth: Math.max(1, width - handleSize)
        readonly property real trackX: handleSize / 2

        signal committed(real newValue)

        function valueAtX(x) {
            const ratio = Math.max(0, Math.min(1, (x - trackX) / usableWidth))
            return from + ratio * range
        }

        function setValueFromX(x, commit) {
            if (!enabled) return
            value = valueAtX(x)
            if (liveWhileDragging || commit) {
                committed(value)
            }
        }

        opacity: enabled ? 1 : 0.55

        Rectangle {
            x: rail.trackX
            y: Math.round((rail.height - height) / 2)
            width: rail.usableWidth
            height: rail.trackHeight
            radius: height / 2
            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.56 : 0.50)
        }

        Rectangle {
            x: rail.trackX
            y: Math.round((rail.height - height) / 2)
            width: rail.usableWidth * rail.progress
            height: rail.trackHeight
            radius: height / 2
            color: rail.accentColor
        }

        Rectangle {
            width: rail.handleSize
            height: rail.handleSize
            radius: width / 2
            x: rail.trackX + rail.usableWidth * rail.progress - width / 2
            y: Math.round((rail.height - height) / 2)
            color: Theme.bg
            border.color: rail.accentColor
            border.width: 2
        }

        MouseArea {
            id: inputArea
            anchors.fill: parent
            preventStealing: true
            onPressed: (mouse) => rail.setValueFromX(mouse.x, false)
            onPositionChanged: (mouse) => rail.setValueFromX(mouse.x, false)
            onReleased: (mouse) => rail.setValueFromX(mouse.x, true)
        }
    }
}
