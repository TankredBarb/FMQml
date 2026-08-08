import QtQuick
import QtQuick.Controls
import FM
import "../../style"

ProgressBar {
    id: root

    property color fillColor: Theme.accent
    property color trackColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
    property color trackBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
    property real trackHeight: 9
    property int animationDuration: 180
    property bool preserveMinimumFill: false
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
        trackHeight: root.trackHeight
        preserveMinimumFill: root.preserveMinimumFill
        surfaceColor: root.trackColor
        borderColor: root.trackBorderColor
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
        NumberAnimation { duration: root.animationDuration; easing.type: Easing.OutCubic }
    }
}
