import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../../style"

Switch {
    id: root

    property color accentColor: Theme.accent
    property real paintPosition: root.pressed
                                 ? root.visualPosition
                                 : (root.checked ? 1 : 0)

    hoverEnabled: true
    spacing: 8
    implicitWidth: Math.max(46,
                            indicator.implicitWidth
                            + spacing
                            + labelMetrics.advanceWidth)
    implicitHeight: Math.max(26, indicator.implicitHeight, contentItem.implicitHeight)
    Layout.minimumWidth: root.text.length > 0 ? root.implicitWidth : 0

    TextMetrics {
        id: labelMetrics
        font: root.font
        text: root.text
    }

    indicator: Item {
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: 46
        implicitHeight: 26

        FmSwitchVisual {
            anchors.fill: parent
            textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
            enabled: root.enabled
            position: root.paintPosition
            active: root.pressed || root.activeFocus
            hovered: root.hovered
            surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
            borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
            idleColor: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)
            accentColor: root.accentColor
        }
    }

    contentItem: Label {
        id: switchLabel
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideNone
    }

    Behavior on paintPosition {
        enabled: !root.pressed
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
