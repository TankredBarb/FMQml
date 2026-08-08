import QtQuick
import FM
import "../../style"

Item {
    id: root

    property real value: 0
    property bool running: false
    property color accentColor: Theme.accent
    property color trackColor: Theme.withAlpha(Theme.panelBorder, 0.76)
    property real lineWidth: 2.4
    property int rotationDuration: 1100
    property real displayedValue: 0

    implicitWidth: 18
    implicitHeight: 18

    Component.onCompleted: displayedValue = Math.max(0, Math.min(1, value))
    onValueChanged: displayedValue = Math.max(0, Math.min(1, value))

    Behavior on displayedValue {
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: root.rotationDuration
        loops: Animation.Infinite
        running: root.running
    }

    FmProgressRingVisual {
        anchors.fill: parent
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        progress: root.running ? 0.34 : root.displayedValue
        lineWidth: root.lineWidth
        trackColor: root.trackColor
        accentColor: root.accentColor
    }
}
