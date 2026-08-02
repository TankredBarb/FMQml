import QtQuick
import QtQuick.Controls
import FM
import "../../style"

Slider {
    id: root

    property color accentColor: Theme.accent
    property real handleSize: 18
    property real trackHeight: 7
    readonly property bool dragging: pressed
    property bool _commitPending: false

    signal committed(real newValue)

    hoverEnabled: true
    implicitWidth: 220
    implicitHeight: 28
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    onPressedChanged: {
        if (pressed) {
            _commitPending = true
        } else if (_commitPending) {
            _commitPending = false
            committed(value)
        }
    }

    background: FmSliderVisual {
        anchors.fill: parent
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        visualPosition: root.visualPosition
        active: root.pressed || root.activeFocus
        hovered: root.hovered
        handleSize: root.handleSize
        trackHeight: root.trackHeight
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        idleColor: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)
        accentColor: root.accentColor
    }

    handle: Item {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: root.handleSize
        implicitHeight: root.handleSize
    }
}
