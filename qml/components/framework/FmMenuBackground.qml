import QtQuick
import FM
import "../../style"
import "../common"

Item {
    id: root

    property bool active: visible
    property var backdropTransformItem: null
    readonly property bool transparentSurface: typeof appSettings !== "undefined" && appSettings
                                               ? appSettings.menusTransparency : false
    readonly property real transparencyStrength: typeof appSettings !== "undefined" && appSettings
                                                 ? appSettings.commandPaletteTransparencyStrength / 100.0 : 0.6
    readonly property real surfaceAlpha: transparentSurface
                                         ? 1.0 - transparencyStrength * (themeController.isDark ? 0.70 : 0.60)
                                         : 1.0
    property var backdropSource: Window.window && Window.window.menuBackdropSource !== undefined
                                          ? Window.window.menuBackdropSource : null

    TranslucentSurface {
        x: 0.5
        y: 0.5
        width: parent.width - 1
        height: parent.height - 2
        active: root.active
        liveBackdrop: true
        translucent: root.transparentSurface
        backgroundBlurEnabled: typeof appSettings !== "undefined" && appSettings
                               ? appSettings.surfaceBlur : false
        backdropSource: root.backdropSource
        backdropTransformItem: root.backdropTransformItem
        cornerRadius: 9
        baseColor: "transparent"
        startColor: "transparent"
        midColor: "transparent"
        endColor: "transparent"
        gradientStrength: 0
        borderWidth: 0
        shadowEnabled: false
    }

    FmMenuVisual {
        anchors.fill: parent
        opacity: root.surfaceAlpha
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        surfaceColor: Theme.menuSurface
        borderColor: Theme.menuBorder
        accentColor: Theme.accent
        shadowColor: Theme.shadow
        dark: themeController.isDark
    }
}
