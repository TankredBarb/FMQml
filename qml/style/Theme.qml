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
    readonly property int radius: 8
    readonly property int rowHeight: 38
    readonly property int spacing: 8
    readonly property int motionFast: 100
    readonly property int motionNormal: 250
    readonly property int motionSlow: 400

    readonly property color shadow: "#10000000"
    readonly property real surfaceOpacity: 0.85

    readonly property color menuSurface: themeController.isDark
            ? Qt.lighter(bg, 1.14)
            : surface
    readonly property color menuBorder: themeController.isDark
            ? Qt.lighter(border, 1.25)
            : Qt.darker(border, 1.08)

    readonly property color menuSeparator: themeController.isDark
            ? Qt.lighter(border, 1.75)
            : Qt.darker(border, 1.65)

    readonly property color menuItemHover: surfaceHover
    readonly property color menuItemPressed: Qt.darker(surfaceHover, 1.18)
}

