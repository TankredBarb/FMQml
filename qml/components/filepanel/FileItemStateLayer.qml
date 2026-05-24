import QtQuick
import "../../style"

Item {
    id: root

    property bool selected: false
    property bool panelActive: true
    property bool currentItem: false
    property bool hovered: false
    property bool scrolling: false
    property real visualOffsetX: 0
    property real leftMargin: 4
    property real rightMargin: 4
    property real topMargin: 1
    property real bottomMargin: 1
    property bool showSelectionBar: true
    property real selectionBarLeftMargin: 4
    property real selectionBarTopMargin: 4
    property real selectionBarBottomMargin: 4
    property real selectionBarWidth: 3
    property real selectionBarRadius: 1.5

    anchors.fill: parent
    transform: Translate { x: root.visualOffsetX }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: root.leftMargin
        anchors.rightMargin: root.rightMargin
        anchors.topMargin: root.topMargin
        anchors.bottomMargin: root.bottomMargin
        radius: Theme.radiusSm

        color: root.selected
               ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
               : (root.currentItem
                  ? Theme.itemCurrentFill
                  : ((root.hovered && !root.scrolling) ? Theme.itemHoverFill : "transparent"))
        border.color: root.selected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.itemCurrentBorder : "transparent")
        border.width: root.selected || root.currentItem ? 1 : 0

        Rectangle {
            visible: root.showSelectionBar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: root.selectionBarTopMargin
            anchors.bottomMargin: root.selectionBarBottomMargin
            anchors.leftMargin: root.selectionBarLeftMargin
            width: root.selected ? root.selectionBarWidth : 0
            radius: root.selectionBarRadius
            color: Theme.accent

            Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad } }
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }
}
