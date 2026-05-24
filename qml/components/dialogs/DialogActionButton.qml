import QtQuick
import QtQuick.Controls
import "../../style"

Button {
    id: root

    property color primaryColor: Theme.accent
    property color primaryHoverColor: Qt.lighter(Theme.accent, 1.1)
    property color primaryPressedColor: Qt.darker(Theme.accent, 1.1)
    property color textColor: "white"
    property color secondaryTextColor: Theme.textSecondary

    contentItem: Label {
        text: root.text
        font.pixelSize: 12
        font.weight: root.highlighted ? Font.Medium : Font.Normal
        color: root.enabled ? (root.highlighted ? root.textColor : root.secondaryTextColor) : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 34
        radius: Theme.radiusSm
        color: !root.enabled ? Theme.panelBorder
             : root.highlighted
               ? (root.pressed ? root.primaryPressedColor : (root.hovered ? root.primaryHoverColor : root.primaryColor))
               : (root.pressed ? Theme.surfaceActive : (root.hovered ? Theme.panelSurfaceSoft : "transparent"))
    }
}
