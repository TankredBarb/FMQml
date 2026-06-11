import QtQuick
import QtQuick.Controls
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

    width: 18
    height: 18
    radius: 5
    visible: root.available && root.hovered && !root.scrolling
    opacity: visible ? 1.0 : 0.0
    color: root.selected
           ? Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.88 : 0.92)
           : Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.92 : 0.96)
    border.color: root.selected
                  ? Theme.withAlpha(Theme.activeAccent, 0.96)
                  : Theme.withAlpha(Theme.panelBorder, 0.72)
    border.width: 1

    Behavior on opacity {
        NumberAnimation { duration: Theme.motionFast }
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.selected ? -1 : 0
        text: root.selected ? "-" : "+"
        color: root.selected ? Theme.accentText : Theme.textPrimary
        font.pixelSize: 14
        font.bold: true
    }

}
