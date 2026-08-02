import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    readonly property color buttonSurfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
    readonly property color effectivePrimaryColor: root.pressed
                                                   ? root.primaryPressedColor
                                                   : (root.hovered ? root.primaryHoverColor : root.primaryColor)
    readonly property color effectiveTextColor: !enforceTextContrast
                                                ? root.textColor
                                                : Theme.readableOn(root.buttonSurfaceColor,
                                                                   root.destructive
                                                                   ? root.effectivePrimaryColor
                                                                   : Theme.textPrimary)
    property real activation: root.pressed ? 1 : (root.hovered ? 0.48 : 0)

    implicitWidth: Math.max(92, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 34
    Layout.minimumWidth: root.text.length > 0
                         ? root.implicitContentWidth + root.leftPadding + root.rightPadding
                         : 0
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
        elide: Text.ElideNone
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
        surfaceColor: root.buttonSurfaceColor
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        accentColor: root.effectivePrimaryColor
    }

    Behavior on activation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
