pragma Singleton

import QtQuick

QtObject {
    readonly property color bg: themeController.bg
    readonly property color surface: themeController.surface
    readonly property color surfaceHover: themeController.surfaceHover
    readonly property color surfaceActive: themeController.surfaceActive
    readonly property color textPrimary: themeController.textPrimary
    readonly property color textSecondary: themeController.textSecondary
    readonly property color border: themeController.border
    readonly property color accent: themeController.accent
    readonly property color accentText: themeController.accentText
    readonly property color danger: themeController.danger
    readonly property color activeAccent: themeController.activeAccent
    readonly property color activeGlow: themeController.activeGlow
    readonly property color glassSurface: themeController.isDark
            ? Qt.rgba(surface.r, surface.g, surface.b, 0.72)
            : Qt.rgba(bg.r, bg.g, bg.b, 0.66)
    readonly property color glassSurfaceStrong: themeController.isDark
            ? Qt.rgba(surface.r, surface.g, surface.b, 0.90)
            : Qt.rgba(bg.r, bg.g, bg.b, 0.84)
    readonly property color glassSurfaceSoft: themeController.isDark
            ? Qt.rgba(surface.r, surface.g, surface.b, 0.56)
            : Qt.rgba(bg.r, bg.g, bg.b, 0.48)
    readonly property color glassBorder: themeController.isDark
            ? Qt.rgba(1, 1, 1, 0.14)
            : Qt.rgba(border.r, border.g, border.b, 0.72)
    readonly property color glassShadow: themeController.isDark
            ? Qt.rgba(0, 0, 0, 0.36)
            : Qt.rgba(0, 0, 0, 0.16)
    readonly property color itemHoverFill: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.10) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.13)
    readonly property color itemCurrentFill: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.08) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.09)
    readonly property color itemCurrentBorder: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.25) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.55)
    readonly property color itemSelectedFill: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.18) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.13)
    readonly property color itemSelectedFillInactive: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.12) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.09)
    readonly property color itemSelectedBorder: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.35) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.85)
    readonly property color itemSelectedBorderInactive: themeController.isDark 
            ? Qt.rgba(1, 1, 1, 0.20) 
            : Qt.rgba(accent.r, accent.g, accent.b, 0.55)
    readonly property color statusRailFill: themeController.isDark
            ? Qt.rgba(surface.r, surface.g, surface.b, 0.98)
            : Qt.rgba(bg.r, bg.g, bg.b, 0.995)

    readonly property int radius: 10
    readonly property int rowHeight: 38
    readonly property int spacing: 8
    readonly property int motionFast: 100
    readonly property int motionNormal: 250
    readonly property int motionSlow: 400

    // Typography
    readonly property string fontFamily: "Segoe UI Variable Text, Segoe UI, Arial, sans-serif"
    readonly property int fontSizeH1: 16
    readonly property int fontSizeH2: 14
    readonly property int fontSizeBody: 13
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeMini: 10

    readonly property int fontLight: Font.Light
    readonly property int fontNormal: Font.Normal
    readonly property int fontMedium: Font.Medium
    readonly property int fontSemiBold: Font.DemiBold
    readonly property int fontBold: Font.Bold

    readonly property color shadow: "#10000000"
    readonly property real surfaceOpacity: 0.85

    readonly property color menuSurface: themeController.isDark
            ? surface
            : bg
    readonly property color menuBorder: themeController.isDark
            ? Qt.lighter(border, 1.25)
            : Qt.darker(border, 1.08)

    readonly property color menuSeparator: themeController.isDark
            ? Qt.lighter(border, 1.75)
            : Qt.darker(border, 1.65)

    readonly property color menuItemHover: surfaceHover
    readonly property color menuItemPressed: Qt.darker(surfaceHover, 1.18)
}
