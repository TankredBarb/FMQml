import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../framework"

FmButton {
    id: actionPill

    property color accentColor: Theme.accent
    property string iconSource: ""
    property int pillWidth: 64
    property bool copyFeedback: false
    property bool menuIndicator: false
    readonly property bool copied: feedbackTimer.running

    function showCopied() {
        feedbackTimer.restart()
    }

    implicitWidth: Math.max(pillWidth, actionContent.implicitWidth + 18)
    implicitHeight: Math.max(30, actionContent.implicitHeight + 10)
    width: implicitWidth
    height: implicitHeight
    padding: 0
    hoverEnabled: true
    primaryColor: accentColor
    flat: true

    onVisibleChanged: if (!visible) feedbackTimer.stop()
    onEnabledChanged: if (!enabled) feedbackTimer.stop()

    Timer {
        id: feedbackTimer
        interval: 1300
    }

    TextMetrics {
        id: normalMetrics
        font: actionLabel.font
        text: actionPill.text
    }

    TextMetrics {
        id: copiedMetrics
        font: actionLabel.font
        text: "Copied"
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.withAlpha(actionPill.accentColor,
                               actionPill.down ? 0.18 : (actionPill.hovered || actionPill.copied ? 0.10 : 0))
        border.width: actionPill.visualFocus ? 1 : 0
        border.color: actionPill.accentColor
    }

    contentItem: RowLayout {
        id: actionContent
        spacing: 5
        opacity: actionPill.enabled ? 1 : 0.45

        Item { Layout.fillWidth: true }

        Item {
            Layout.preferredWidth: 13
            Layout.preferredHeight: 13
            visible: actionPill.iconSource.length > 0

            RecolorSvgIcon {
                anchors.fill: parent
                visible: !actionPill.copied
                sourcePath: actionPill.iconSource
                recolorColor: actionPill.accentColor
                sourceSize: Qt.size(13, 13)
            }

            Label {
                anchors.centerIn: parent
                visible: actionPill.copied
                text: "\u2713"
                color: actionPill.accentColor
                font.pixelSize: 13
            }
        }

        Label {
            id: actionLabel
            Layout.preferredWidth: Math.ceil(actionPill.copyFeedback
                                            ? Math.max(normalMetrics.width, copiedMetrics.width)
                                            : normalMetrics.width)
            text: actionPill.copied ? "Copied" : actionPill.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Label {
            visible: actionPill.menuIndicator
            text: "\u25be"
            color: actionPill.accentColor
            font.pixelSize: Theme.fontSizeCaption
        }

        Item { Layout.fillWidth: true }
    }
}
