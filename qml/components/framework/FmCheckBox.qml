import QtQuick
import QtQuick.Controls
import FM
import "../../style"

CheckBox {
    id: root

    property color accentColor: Theme.accent
    property real paintProgress: root.checkState === Qt.Unchecked ? 0 : 1

    hoverEnabled: true
    spacing: 7
    implicitWidth: Math.max(indicator.implicitWidth, contentItem.implicitWidth)
    implicitHeight: Math.max(24, indicator.implicitHeight, contentItem.implicitHeight)

    indicator: Item {
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: 22
        implicitHeight: 22

        FmCheckBoxVisual {
            anchors.fill: parent
            textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
            enabled: root.enabled
            progress: root.paintProgress
            checkState: root.checkState
            active: root.pressed || root.activeFocus
            hovered: root.hovered
            surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
            borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
            accentColor: root.accentColor
            markColor: Theme.readableOn(root.accentColor, Theme.textPrimary)
        }
    }

    contentItem: Label {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    Behavior on paintProgress {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
