import QtQuick
import QtQuick.Controls
import FM
import "../../style"

Button {
    id: root

    property color primaryColor: Theme.accent
    property color primaryHoverColor: root.primaryColor
    property color primaryPressedColor: root.primaryColor
    property color textColor: Theme.accentText
    property color secondaryTextColor: Theme.textSecondary
    property bool destructive: false
    property bool enforceTextContrast: true
    readonly property color effectivePrimaryColor: root.pressed
                                                   ? root.primaryPressedColor
                                                   : (root.hovered ? root.primaryHoverColor : root.primaryColor)
    readonly property color destructiveTextColor: Theme.contrastRatio(root.effectivePrimaryColor, "#fffaf8")
                                                   >= Theme.contrastRatio(root.effectivePrimaryColor, "#251414")
                                                   ? "#fffaf8" : "#251414"
    readonly property color effectiveTextColor: !enforceTextContrast
                                                ? root.textColor
                                                : (root.destructive
                                                   ? root.destructiveTextColor
                                                   : Theme.readableOn(root.effectivePrimaryColor, root.textColor))
    property real activation: root.pressed ? 1 : (root.hovered ? 0.48 : 0)

    implicitWidth: Math.max(92, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 34
    leftPadding: 16
    rightPadding: 16

    contentItem: Label {
        text: root.text
        font.family: root.font.family
        font.pixelSize: Theme.fontSizeLabel
        font.weight: root.highlighted ? Font.Medium : Font.Normal
        color: root.enabled
               ? (root.highlighted ? root.effectiveTextColor : root.secondaryTextColor)
               : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: FmButtonVisual {
        implicitWidth: 92
        implicitHeight: 34
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        activation: root.activation
        primary: root.highlighted
        destructive: root.destructive
        flat: root.flat
        active: root.activeFocus
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        accentColor: root.effectivePrimaryColor
    }

    Behavior on activation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
