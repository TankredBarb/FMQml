import QtQuick
import QtQuick.Controls
import "framework"
import "../style"

FmTextField {
    id: root

    property int premiumRadius: Theme.controlRadius
    property color fillColor: Theme.controlSurface
    property color activeFillColor: Theme.controlSurfaceActive
    property color normalBorderColor: Theme.controlBorder
    property color focusedBorderColor: Theme.focusRing

    cornerRadius: root.premiumRadius
    surfaceColor: root.activeFocus ? root.activeFillColor : root.fillColor
    borderColor: root.activeFocus ? root.focusedBorderColor : root.normalBorderColor
}
