import QtQuick
import "../../style"
import "../common"

TranslucentSurface {
    id: root

    translucent: false

    property color shellColor: Theme.panelSurface
    readonly property real transparencyStrength: typeof appSettings !== "undefined" && appSettings
                                                 ? appSettings.commandPaletteTransparencyStrength / 100.0
                                                 : 0.6
    readonly property real translucentAlpha: themeController.isDark
                                             ? 1.0 - transparencyStrength * 0.32
                                             : 1.0 - transparencyStrength * 0.26
    property color shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
    property color accentColor: Theme.accent
    property bool accentVisible: true
    property int shellRadius: Theme.radiusLg
    backgroundBlurEnabled: root.translucent
                           && typeof appSettings !== "undefined" && appSettings
                           && appSettings.surfaceBlur && root.backdropSource
    baseColor: root.translucent
               ? Theme.withAlpha(Theme.panelSurfaceStrong, root.translucentAlpha)
               : root.shellColor
    gradientStrength: 0.5
    cornerRadius: root.shellRadius
    borderColor: root.shellBorderColor
    borderWidth: 1
    shadowBlur: 20
    shadowVerticalOffset: 8

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 1
        height: 1
        radius: 0.5
        visible: root.accentVisible
        color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.42 : 0.30)
    }
}
