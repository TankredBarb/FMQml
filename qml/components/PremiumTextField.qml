import QtQuick
import QtQuick.Controls
import "../style"

TextField {
    id: root

    property int premiumRadius: Theme.controlRadius
    property color fillColor: Theme.controlSurface
    property color activeFillColor: Theme.controlSurfaceActive
    property color normalBorderColor: Theme.controlBorder
    property color focusedBorderColor: Theme.focusRing

    implicitHeight: Theme.controlHeight
    padding: 0
    leftPadding: 14
    rightPadding: 14
    topPadding: 0
    bottomPadding: 0
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    font.pixelSize: Theme.fontSizeBodyLarge
    font.weight: Font.Medium
    verticalAlignment: TextInput.AlignVCenter
    clip: true

    background: Rectangle {
        radius: root.premiumRadius
        color: root.activeFocus ? root.activeFillColor : root.fillColor
        border.color: root.activeFocus ? root.focusedBorderColor : root.normalBorderColor
        border.width: root.activeFocus ? 2 : 1
    }
}
