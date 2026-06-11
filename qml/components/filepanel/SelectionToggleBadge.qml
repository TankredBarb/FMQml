import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import "../../style"

Rectangle {
    id: root

    property var controller: null
    property var panel: null
    property int index: -1
    property bool selected: false
    property bool hovered: false
    property bool currentItem: false
    property bool scrolling: false
    property bool available: true
    readonly property bool panelActive: root.panel ? root.panel.active : true

    width: 18
    height: 18
    radius: 9
    visible: root.available && root.hovered && !root.scrolling && root.panelActive
    opacity: visible ? 1.0 : 0.0
    color: root.selected ? Theme.activeAccent : Theme.withAlpha(Theme.textPrimary, 0.09)
    border.color: root.selected ? Theme.activeAccent : Theme.withAlpha(Theme.textPrimary, 0.4)
    border.width: 1

    Behavior on opacity {
        NumberAnimation { duration: Theme.motionFast }
    }

    Behavior on color {
        ColorAnimation { duration: Theme.motionFast }
    }

    Behavior on border.color {
        ColorAnimation { duration: Theme.motionFast }
    }

    // Custom vector plus with 1px thin geometry
    Shape {
        id: plusShape
        anchors.centerIn: parent
        width: 8
        height: 8
        visible: !root.selected
        antialiasing: true

        ShapePath {
            strokeColor: Theme.textPrimary
            strokeWidth: 1
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathMove { x: 0; y: 4 }
            PathLine { x: 8; y: 4 }

            PathMove { x: 4; y: 0 }
            PathLine { x: 4; y: 8 }
        }
    }

    // Custom vector checkmark with 1.2px thin geometry
    Shape {
        id: checkShape
        anchors.centerIn: parent
        width: 8
        height: 6
        visible: root.selected
        antialiasing: true

        ShapePath {
            strokeColor: Theme.accentText
            strokeWidth: 1.2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathMove { x: 0; y: 2.5 }
            PathLine { x: 2.8; y: 5.3 }
            PathLine { x: 8; y: 0.5 }
        }
    }

}
