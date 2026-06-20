import QtQuick
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
    property int badgeSize: 18
    property int markSize: 7
    property real markStroke: 1
    readonly property bool panelActive: root.panel ? root.panel.active : true
    readonly property color selectedFill: Theme.activeAccent
    readonly property color selectedMarkColor: Theme.readableOn(root.selectedFill, Theme.accentText)

    width: root.badgeSize
    height: root.badgeSize
    radius: width / 2
    visible: root.available && root.hovered && !root.scrolling && root.panelActive
    opacity: visible ? 1.0 : 0.0
    color: root.selected ? root.selectedFill : Theme.withAlpha(Theme.textPrimary, 0.09)
    border.color: root.selected ? "transparent" : Theme.withAlpha(Theme.textPrimary, 0.4)
    border.width: root.selected ? 0 : 1

    Behavior on opacity {
        NumberAnimation { duration: Theme.motionFast }
    }

    Behavior on color {
        ColorAnimation { duration: Theme.motionFast }
    }

    Behavior on border.color {
        ColorAnimation { duration: Theme.motionFast }
    }

    Item {
        id: plusMark
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        width: root.markSize
        height: root.markSize
        visible: !root.selected

        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: root.markStroke
            radius: height / 2
            color: Theme.textPrimary
            opacity: 0.72
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.markStroke
            height: parent.height
            radius: width / 2
            color: Theme.textPrimary
            opacity: 0.72
        }
    }

    Item {
        id: checkMark
        anchors.centerIn: parent
        visible: root.selected
        width: Math.max(7, Math.round(root.badgeSize * 0.58))
        height: Math.max(5, Math.round(root.badgeSize * 0.42))

        Rectangle {
            x: Math.round(parent.width * 0.08)
            y: Math.round(parent.height * 0.40)
            width: Math.round(parent.width * 0.40)
            height: Math.max(1.35, root.markStroke + 0.35)
            radius: height / 2
            rotation: 45
            transformOrigin: Item.Left
            color: root.selectedMarkColor
        }

        Rectangle {
            x: Math.round(parent.width * 0.36)
            y: Math.round(parent.height * 0.72)
            width: Math.round(parent.width * 0.68)
            height: Math.max(1.35, root.markStroke + 0.35)
            radius: height / 2
            rotation: -45
            transformOrigin: Item.Left
            color: root.selectedMarkColor
        }
    }

}
