pragma Singleton

import QtQuick

QtObject {
    function withAlpha(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

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
    readonly property color secondaryAccent: themeController.secondaryAccent
    readonly property color warmAccent: themeController.warmAccent
    readonly property color success: themeController.success
    readonly property color warning: themeController.warning
    readonly property color categoryInfo: themeController.categoryInfo
    readonly property color categoryNavigation: themeController.categoryNavigation
    readonly property color categoryAction: themeController.categoryAction
    readonly property color categoryUtility: themeController.categoryUtility
    readonly property color categorySystem: themeController.categorySystem
    readonly property color overlayScrim: themeController.overlayScrim
    readonly property color focusRing: themeController.focusRing
    readonly property color panelSurface: themeController.panelSurface
    readonly property color panelSurfaceSoft: themeController.panelSurfaceSoft
    readonly property color panelSurfaceStrong: themeController.panelSurfaceStrong
    readonly property color panelBorder: themeController.panelBorder
    readonly property color controlSurface: themeController.controlSurface
    readonly property color controlSurfaceActive: themeController.controlSurfaceActive
    readonly property color controlBorder: themeController.controlBorder
    readonly property color glassSurface: themeController.panelSurface
    readonly property color glassSurfaceStrong: themeController.panelSurfaceStrong
    readonly property color glassSurfaceSoft: themeController.panelSurfaceSoft
    readonly property color glassBorder: themeController.panelBorder
    readonly property color glassShadow: themeController.isDark
            ? Qt.rgba(0, 0, 0, 0.36)
            : Qt.rgba(0, 0, 0, 0.16)
    readonly property color itemHoverFill: themeController.itemHoverFill
    readonly property color itemCurrentFill: themeController.itemCurrentFill
    readonly property color itemCurrentBorder: themeController.itemCurrentBorder
    readonly property color itemSelectedFill: themeController.itemSelectedFill
    readonly property color itemSelectedFillInactive: themeController.itemSelectedFillInactive
    readonly property color itemSelectedBorder: themeController.itemSelectedBorder
    readonly property color itemSelectedBorderInactive: themeController.itemSelectedBorderInactive
    readonly property color statusRailFill: themeController.statusRailFill

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

    readonly property int radiusSm: 8
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 20

    readonly property int controlRadius: 12
    readonly property int panelRadius: 20

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    readonly property int controlHeight: 38
    readonly property int panelHeaderHeight: 80
    readonly property int badgeHeight: 24

    readonly property int fontSizeTitle: 16
    readonly property int fontSizeSubtitle: 14
    readonly property int fontSizeBodyLarge: 13
    readonly property int fontSizeLabel: 12
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeMicro: 10
}
