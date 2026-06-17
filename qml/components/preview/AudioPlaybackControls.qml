import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "../common"
import "../../style"

Rectangle {
    id: root

    property string path: ""
    property string sourceUrl: ""
    property bool compact: false
    property bool mediaLoaded: false
    readonly property color playTone: Theme.actionIconColor("media")
    readonly property color pauseTone: Theme.actionIconColor("navigation")
    readonly property color volumeTone: Theme.actionIconColor("utility")
    readonly property color mutedTone: Theme.actionIconColor("muted")

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
            Layout.preferredWidth: root.compact ? 36 : 42
            text: root.timeText(progressRail.dragging ? progressRail.value : player.position)
            horizontalAlignment: Text.AlignRight
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 22 : 24

            AudioRail {
                id: progressRail
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                from: 0
                to: Math.max(1, player.duration)
                value: 0
                enabled: player.duration > 0
                liveWhileDragging: false
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
            iconColor: checked || volumeRail.value <= 0 ? root.mutedTone : root.volumeTone
            iconSource: checked || volumeRail.value <= 0
                        ? "qrc:/qt/qml/FM/qml/assets/toolbar-next/volume-x.svg"
                        : "qrc:/qt/qml/FM/qml/assets/toolbar-next/volume-2.svg"
            tooltip: checked ? "Unmute" : "Mute"
        }

        Item {
            Layout.preferredWidth: root.compact ? 72 : 88
            Layout.preferredHeight: root.compact ? 20 : 22

            AudioRail {
                id: volumeRail
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                from: 0
                to: 1
                value: 0.15
                liveWhileDragging: true
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

    component AudioIconButton: ToolButton {
        id: button

        property string iconSource: ""
        property string tooltip: ""
        property bool primary: false
        property color iconColor: Theme.textPrimary

        padding: 0
        hoverEnabled: true

        background: Rectangle {
            radius: width / 2
            color: {
                if (!button.enabled) return Theme.withAlpha(Theme.textSecondary, 0.08)
                if (button.primary) {
                    return button.down
                        ? Theme.withAlpha(Theme.accent, 0.90)
                        : (button.hovered ? Theme.withAlpha(Theme.accent, 0.80) : Theme.accent)
                }
                return button.down
                    ? Theme.withAlpha(button.iconColor, themeController.isDark ? 0.24 : 0.18)
                    : (button.hovered ? Theme.withAlpha(button.iconColor, themeController.isDark ? 0.16 : 0.12) : Theme.withAlpha(button.iconColor, themeController.isDark ? 0.07 : 0.05))
            }
            border.color: button.primary
                          ? Theme.withAlpha(Theme.accent, 0.34)
                          : Theme.withAlpha(button.iconColor, button.hovered ? 0.38 : 0.00)
            border.width: button.primary || button.hovered ? 1 : 0
        }

        contentItem: Item {
            RecolorSvgIcon {
                anchors.centerIn: parent
                width: button.primary ? 18 : 17
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

    component AudioRail: Item {
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

        signal edited(real newValue)
        signal committed(real newValue)

        function valueAtX(x) {
            const ratio = Math.max(0, Math.min(1, (x - trackX) / usableWidth))
            return from + ratio * range
        }

        function setValueFromX(x, commit) {
            if (!enabled) return
            value = valueAtX(x)
            if (liveWhileDragging) {
                edited(value)
            }
            if (commit) {
                committed(value)
            }
        }

        opacity: enabled ? 1 : 0.55

        Rectangle {
            id: baseTrack
            x: rail.trackX
            y: Math.round((rail.height - height) / 2)
            width: rail.usableWidth
            height: rail.trackHeight
            radius: height / 2
            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.56 : 0.50)
        }

        Rectangle {
            x: baseTrack.x
            y: baseTrack.y
            width: baseTrack.width * rail.progress
            height: baseTrack.height
            radius: height / 2
            color: rail.accentColor
        }

        Rectangle {
            x: baseTrack.x + baseTrack.width * rail.progress - width / 2
            y: Math.round((rail.height - height) / 2)
            width: rail.dragging ? rail.handleSize + 4 : rail.handleSize
            height: width
            radius: width / 2
            color: Theme.panelSurface
            border.color: rail.accentColor
            border.width: 1.5

            Behavior on width { NumberAnimation { duration: Theme.motionFast } }
        }

        MouseArea {
            id: inputArea
            anchors.fill: parent
            enabled: rail.enabled
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.PointingHandCursor

            onPressed: (mouse) => {
                rail.setValueFromX(mouse.x, false)
                mouse.accepted = true
            }
            onPositionChanged: (mouse) => {
                if (pressed) {
                    rail.setValueFromX(mouse.x, false)
                }
            }
            onReleased: (mouse) => rail.setValueFromX(mouse.x, true)
            onCanceled: rail.committed(rail.value)
        }
    }
}
