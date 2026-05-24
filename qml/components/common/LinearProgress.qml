import QtQuick
import "../../style"

Item {
    id: root

    property real value: 0
    property color trackColor: Theme.withAlpha(Theme.panelBorder, 0.5)
    property color fillColor: Theme.accent
    property int barHeight: 6
    property int animationDuration: 400
    property bool preserveMinimumFill: false

    implicitHeight: root.barHeight

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.trackColor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        radius: height / 2
        width: {
            const clamped = Math.max(0, Math.min(1, root.value))
            if (!root.preserveMinimumFill || clamped <= 0) {
                return parent.width * clamped
            }
            return Math.max(height, parent.width * clamped)
        }
        color: root.fillColor

        Behavior on width {
            NumberAnimation {
                duration: root.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        Behavior on color {
            ColorAnimation { duration: Theme.motionFast }
        }
    }
}
