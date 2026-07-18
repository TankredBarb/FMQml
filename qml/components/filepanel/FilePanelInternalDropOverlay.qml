import QtQuick
import "../../style"

Rectangle {
    id: root

    property bool dropAllowed: false

    anchors.fill: parent
    anchors.margins: 1
    radius: Theme.innerRadius(Theme.panelRadius, 1)
    color: Theme.withAlpha(root.dropAllowed ? Theme.accent : Theme.danger,
                           themeController.isDark ? 0.09 : 0.065)
    border.color: Theme.withAlpha(root.dropAllowed ? Theme.accent : Theme.danger,
                                  themeController.isDark ? 0.62 : 0.48)
    border.width: 2
    antialiasing: true
    enabled: false
}
