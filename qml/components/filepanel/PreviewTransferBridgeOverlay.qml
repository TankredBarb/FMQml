import QtQuick
import QtQuick.Controls
import "../../style"

Item {
    id: root

    required property var dragCoordinator
    readonly property int destinationSide: dragCoordinator ? dragCoordinator.destinationPanelSide : -1
    readonly property bool allowed: dragCoordinator
                                    && dragCoordinator.canDropOn(destinationSide)
    readonly property bool pointsRight: destinationSide === 1
    readonly property string destinationName: pointsRight ? "right" : "left"

    visible: dragCoordinator && dragCoordinator.active
    enabled: false

    Rectangle {
        anchors.fill: parent
        radius: Theme.panelRadius
        color: Theme.withAlpha(root.allowed ? Theme.accent : Theme.danger,
                               themeController.isDark ? 0.14 : 0.10)
        border.color: Theme.withAlpha(root.allowed ? Theme.accent : Theme.danger,
                                      themeController.isDark ? 0.48 : 0.38)
        border.width: 1
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, directionLabel.implicitWidth + 28)
        height: 38
        radius: 19
        color: Theme.withAlpha(Theme.panelSurfaceStrong,
                               themeController.isDark ? 0.94 : 0.88)
        border.color: Theme.withAlpha(root.allowed ? Theme.accent : Theme.danger, 0.52)
        border.width: 1

        Label {
            id: directionLabel
            anchors.centerIn: parent
            text: root.pointsRight
                  ? "Continue to " + root.destinationName + " panel  →"
                  : "←  Continue to " + root.destinationName + " panel"
            color: root.allowed ? Theme.textPrimary : Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }
}
