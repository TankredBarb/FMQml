import QtQuick
import "../../style"

Item {
    id: root

    property int cornerRadius: Theme.radiusLg
    property color accentColor: Theme.activeAccent
    property color baseColor: Theme.opaque(Theme.mixColors(Theme.panelSurface,
                                                           Theme.panelSurfaceStrong,
                                                           themeController.isDark ? 0.82 : 0.68))
    property bool dividerVisible: true
    property bool roundBottomCorners: false

    Rectangle {
        anchors.fill: parent
        topLeftRadius: root.cornerRadius
        topRightRadius: root.cornerRadius
        bottomLeftRadius: root.roundBottomCorners ? root.cornerRadius : 0
        bottomRightRadius: root.roundBottomCorners ? root.cornerRadius : 0
        color: root.baseColor
    }

    Rectangle {
        anchors.fill: parent
        topLeftRadius: root.cornerRadius
        topRightRadius: root.cornerRadius
        bottomLeftRadius: root.roundBottomCorners ? root.cornerRadius : 0
        bottomRightRadius: root.roundBottomCorners ? root.cornerRadius : 0
        color: "transparent"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0
                color: Theme.withAlpha(root.accentColor,
                                       themeController.isDark ? 0.15 : 0.08)
            }
            GradientStop {
                position: 1
                color: Theme.withAlpha(Theme.panelSurfaceStrong, 0.02)
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        visible: root.dividerVisible
        color: Theme.withAlpha(Theme.panelBorder,
                               themeController.isDark ? 0.30 : 0.24)
    }
}
