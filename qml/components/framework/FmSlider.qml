import QtQuick
import QtQuick.Controls
import FM
import "../../style"

Slider {
    id: root

    property color accentColor: Theme.accent

    hoverEnabled: true
    implicitWidth: 220
    implicitHeight: 28
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    background: FmSliderVisual {
        anchors.fill: parent
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        visualPosition: root.visualPosition
        active: root.pressed || root.activeFocus
        hovered: root.hovered
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        idleColor: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)
        accentColor: root.accentColor
    }

    handle: Item {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: 18
        implicitHeight: 18
    }
}
