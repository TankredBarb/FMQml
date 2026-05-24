import QtQuick
import "../../style"

Rectangle {
    id: root

    property color surfaceColor: Theme.panelSurfaceSoft
    property color strokeColor: Theme.panelBorder
    property int cornerRadius: Theme.radiusMd
    property bool clipped: true

    color: root.surfaceColor
    border.color: root.strokeColor
    border.width: 1
    radius: root.cornerRadius
    clip: root.clipped
}
