import QtQuick
import QtQuick.Controls
import FM
import "../../style"

ProgressBar {
    id: root

    property color fillColor: Theme.accent
    property real indeterminatePhase: 0

    implicitWidth: 220
    implicitHeight: 14
    padding: 0

    background: null
    contentItem: FmProgressBarVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        progress: root.visualPosition
        indeterminate: root.indeterminate
        phase: root.indeterminatePhase
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        idleColor: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)
        liquidColor: root.fillColor
    }

    NumberAnimation on indeterminatePhase {
        running: root.visible && root.indeterminate
        from: 0
        to: 1
        duration: 1100
        loops: Animation.Infinite
    }

    Behavior on value {
        enabled: !root.indeterminate
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
}
