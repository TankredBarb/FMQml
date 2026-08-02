import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "../common"
import "../framework"
import "../../style"

Rectangle {
    id: root

    property string path: ""
    property string sourceUrl: ""
    property bool compact: false
    property bool mediaLoaded: false
    readonly property color playTone: Theme.chromeIconColor("media")
    readonly property color pauseTone: Theme.chromeIconColor("navigation")
    readonly property color volumeTone: Theme.chromeIconColor("utility")
    readonly property color mutedTone: Theme.chromeIconColor("muted")

    radius: 0
    color: "transparent"
    border.width: 0
    clip: true

    function timeText(ms) {
        if (!Number.isFinite(ms) || ms <= 0) return "0:00"
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function ensureMediaLoaded() {
        if (mediaLoaded) return
        player.source = sourceUrl
        mediaLoaded = true
    }

    function releaseMedia() {
        player.stop()
        player.source = ""
        mediaLoaded = false
    }

    function resetMedia() {
        releaseMedia()
        progressRail.value = 0
    }

    onPathChanged: resetMedia()
    onSourceUrlChanged: resetMedia()
    Component.onDestruction: releaseMedia()

    AudioOutput {
        id: audioOutput
        volume: volumeRail.value
        muted: muteButton.checked || volumeRail.value <= 0
    }

    MediaPlayer {
        id: player
        audioOutput: audioOutput

        onErrorOccurred: (error, errorString) => {
            console.warn("AudioPlaybackControls error:", error, errorString, "source:", root.sourceUrl)
        }
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.leftMargin: root.compact ? 8 : 10
        anchors.rightMargin: root.compact ? 8 : 10
        anchors.topMargin: root.compact ? 5 : 6
        anchors.bottomMargin: root.compact ? 5 : 6
        spacing: root.compact ? 8 : 10

        readonly property real buttonSize: root.compact ? 28 : 30
        readonly property real muteSize: root.compact ? 24 : 26

        AudioIconButton {
            id: playButton
            Layout.preferredWidth: content.buttonSize
            Layout.preferredHeight: content.buttonSize
            enabled: root.sourceUrl.length > 0
            svgRecolorColor: player.playbackState === MediaPlayer.PlayingState ? root.pauseTone : root.playTone
            iconSource: player.playbackState === MediaPlayer.PlayingState
                        ? "qrc:/qt/qml/FM/qml/assets/icons-classic/pause.svg"
                        : "qrc:/qt/qml/FM/qml/assets/icons-classic/play.svg"
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
            Layout.preferredWidth: root.compact ? 36 : 42
            text: root.timeText(progressRail.dragging ? progressRail.value : player.position)
            horizontalAlignment: Text.AlignRight
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 22 : 24

            FmSlider {
                id: progressRail
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                from: 0
                to: Math.max(1, player.duration)
                value: 0
                enabled: player.duration > 0
                accentColor: Theme.accent
                handleSize: root.compact ? 14 : 16
                trackHeight: 4
                onCommitted: (newValue) => player.setPosition(Math.round(newValue))
            }
        }

        TimeLabel {
            Layout.preferredWidth: root.compact ? 36 : 42
            text: root.timeText(player.duration)
            horizontalAlignment: Text.AlignLeft
        }

        AudioIconButton {
            id: muteButton
            Layout.preferredWidth: content.muteSize
            Layout.preferredHeight: content.muteSize
            checkable: true
            svgRecolorColor: checked || volumeRail.value <= 0 ? root.mutedTone : root.volumeTone
            iconSource: checked || volumeRail.value <= 0
                        ? "qrc:/qt/qml/FM/qml/assets/icons-classic/volume-x.svg"
                        : "qrc:/qt/qml/FM/qml/assets/icons-classic/volume-2.svg"
            tooltip: checked ? "Unmute" : "Mute"
        }

        Item {
            Layout.preferredWidth: root.compact ? 72 : 88
            Layout.preferredHeight: root.compact ? 20 : 22

            FmSlider {
                id: volumeRail
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                from: 0
                to: 1
                value: 0.15
                accentColor: Theme.accent
                handleSize: root.compact ? 11 : 13
                trackHeight: 4
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

    component AudioIconButton: FmIconButton {
        id: button

        property string tooltip: ""
        property bool primary: false

        hoverEnabled: true
        iconSize: button.primary ? 18 : 17
        isHighlighted: button.primary
        showIdleSurface: true

        ToolTip.visible: hovered && tooltip.length > 0
        ToolTip.text: tooltip
    }

}
