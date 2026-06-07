import QtQuick
import "../../style"

Item {
    id: root

    property bool selected: false
    property bool panelActive: true
    property bool currentItem: false
    property bool hovered: false
    property bool scrolling: false
    property bool resizeOptimized: false
    property bool animationsSuppressed: false
    property real visualOffsetX: 0
    property real leftMargin: 6
    property real rightMargin: 6
    property real topMargin: 2
    property real bottomMargin: 2
    property bool showSelectionBar: false
    property real selectionBarLeftMargin: 4
    property real selectionBarTopMargin: 6
    property real selectionBarBottomMargin: 6
    property real selectionBarWidth: 3
    property real selectionBarRadius: 1.5
    readonly property color selectedFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark
            ? (root.panelActive ? 0.34 : 0.20)
            : (root.panelActive ? 0.28 : 0.16))
    readonly property color currentFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark
            ? (root.panelActive ? 0.18 : 0.11)
            : (root.panelActive ? 0.14 : 0.09))

    anchors.fill: parent
    transform: Translate { x: root.visualOffsetX }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: root.leftMargin
        anchors.rightMargin: root.rightMargin
        anchors.topMargin: root.topMargin
        anchors.bottomMargin: root.bottomMargin
        radius: Theme.radiusMd

        color: root.selected
               ? root.selectedFill
               : (root.currentItem
                  ? root.currentFill
                  : ((root.hovered && !root.scrolling) ? Theme.itemNeutralHoverFill : "transparent"))
        border.color: "transparent"
        border.width: 0

        Rectangle {
            visible: root.showSelectionBar && root.selected
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: root.selectionBarTopMargin
            anchors.bottomMargin: root.selectionBarBottomMargin
            anchors.leftMargin: root.selectionBarLeftMargin
            width: root.selected ? root.selectionBarWidth : 0
            radius: root.selectionBarRadius
            color: Theme.accent

            Behavior on width { enabled: !root.resizeOptimized && !root.animationsSuppressed; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutQuad } }
        }

        Behavior on color { enabled: !root.resizeOptimized && !root.animationsSuppressed; ColorAnimation { duration: Theme.motionFast } }
    }
}
