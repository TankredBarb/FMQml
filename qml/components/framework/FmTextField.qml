import QtQuick
import QtQuick.Controls
import FM
import "../../style"

TextField {
    id: root

    property color accentColor: Theme.accent
    property real cornerRadius: Theme.controlRadius
    property color surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
    property color borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
    property bool error: false
    property real paintActivation: root.error ? 1 : (root.activeFocus ? 1 : (root.hovered ? 0.38 : 0))

    hoverEnabled: true
    implicitWidth: 180
    implicitHeight: Theme.controlHeight
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    color: root.enabled ? Theme.textPrimary : Theme.textSecondary
    placeholderTextColor: Theme.textSecondary
    selectionColor: root.accentColor
    selectedTextColor: Theme.readableOn(root.accentColor, Theme.textPrimary)
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBodyLarge
    font.weight: Font.Medium
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    clip: true

    background: FmTextFieldVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        activation: root.paintActivation
        focused: root.activeFocus
        error: root.error
        radius: root.cornerRadius
        surfaceColor: root.surfaceColor
        borderColor: root.borderColor
        accentColor: root.accentColor
        errorColor: Theme.danger
    }

    Behavior on paintActivation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
