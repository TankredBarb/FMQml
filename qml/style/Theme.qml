pragma Singleton

import QtQuick

QtObject {
    function withAlpha(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function contrastChannel(value) {
        return value <= 0.03928 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4)
    }

    function luminance(color) {
        return 0.2126 * contrastChannel(color.r)
             + 0.7152 * contrastChannel(color.g)
             + 0.0722 * contrastChannel(color.b)
    }

    function contrastRatio(first, second) {
        const firstLuminance = luminance(first)
        const secondLuminance = luminance(second)
        const lighter = Math.max(firstLuminance, secondLuminance)
        const darker = Math.min(firstLuminance, secondLuminance)
        return (lighter + 0.05) / (darker + 0.05)
    }

    function readableOn(backgroundColor, preferredColor) {
        let bestColor = preferredColor
        let bestRatio = contrastRatio(backgroundColor, preferredColor)
        const candidates = [
            bg,
            surface,
            textPrimary,
            textSecondary
        ]

        for (let i = 0; i < candidates.length; ++i) {
            const ratio = contrastRatio(backgroundColor, candidates[i])
            if (ratio > bestRatio) {
                bestRatio = ratio
                bestColor = candidates[i]
            }
        }

        return bestColor
    }

    function actionIconColor(role) {
        switch (String(role)) {
        case "back":
        case "info":
        case "help":
        case "copy":
        case "document":
        case "brief":
            return categoryInfo
        case "forward":
        case "navigation":
        case "split":
        case "view-grid":
        case "grid":
            return categoryNavigation
        case "up":
        case "view":
        case "view-details":
        case "hidden":
        case "filter":
        case "search":
        case "utility":
            return categoryUtility
        case "refresh":
        case "action":
        case "paste":
        case "extract":
        case "archive":
            return categoryAction
        case "move":
        case "rename":
        case "settings":
        case "theme":
        case "text-file":
            return warmAccent
        case "folder":
        case "create":
        case "open":
        case "success":
        case "image":
            return success
        case "system":
        case "terminal":
        case "drive":
            return categorySystem
        case "warning":
        case "eject":
            return warning
        case "danger":
        case "delete":
            return danger
        case "muted":
        case "attributes":
        case "sort":
            return textSecondary
        case "primary":
        case "favorite":
        case "selection":
        case "analyze":
        case "default":
            return accent
        case "view-brief":
        case "media":
            return categoryInfo
        default:
            return accent
        }
    }

    function toolbarButtonFill(tone, hovered, pressed, active) {
        if (pressed) {
            return surfaceActive
        }
        if (active) {
            return withAlpha(activeAccent, themeController.isDark
                             ? (hovered ? 0.20 : 0.15)
                             : (hovered ? 0.14 : 0.095))
        }
        return hovered ? withAlpha(tone, themeController.isDark ? 0.13 : 0.09) : "transparent"
    }

    function toolbarButtonBorder(tone, hovered, active) {
        if (active) {
            return withAlpha(activeAccent, themeController.isDark ? 0.58 : 0.44)
        }
        return hovered ? withAlpha(tone, themeController.isDark ? 0.34 : 0.24) : "transparent"
    }

    function toolbarButtonIndicator(active) {
        return active ? withAlpha(activeAccent, themeController.isDark ? 0.95 : 0.86) : "transparent"
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
    readonly property color panelStrokeSubtle: withAlpha(panelBorder, themeController.isDark ? 0.28 : 0.42)
    readonly property color panelStroke: withAlpha(panelBorder, themeController.isDark ? 0.38 : 0.56)
    readonly property color panelStrokeStrong: withAlpha(panelBorder, themeController.isDark ? 0.48 : 0.68)
    readonly property color activePanelStroke: withAlpha(activeAccent, themeController.isDark ? 0.68 : 0.96)
    readonly property color activePanelStrokeSoft: withAlpha(activeAccent, themeController.isDark ? 0.44 : 0.72)
    readonly property color controlSurface: themeController.controlSurface
    readonly property color controlSurfaceActive: themeController.controlSurfaceActive
    readonly property color controlBorder: themeController.controlBorder
    readonly property color glassSurface: themeController.panelSurface
    readonly property color glassSurfaceStrong: themeController.panelSurfaceStrong
    readonly property color glassSurfaceSoft: themeController.panelSurfaceSoft
    readonly property color glassBorder: themeController.panelBorder
    readonly property color glassShadow: themeController.glassShadow
    readonly property color itemHoverFill: themeController.itemHoverFill
    readonly property color itemNeutralHoverFill: withAlpha(textPrimary, themeController.isDark ? 0.055 : 0.045)
    readonly property color itemCurrentFill: themeController.itemCurrentFill
    readonly property color itemCurrentBorder: themeController.itemCurrentBorder
    readonly property color itemSelectedFill: themeController.itemSelectedFill
    readonly property color itemSelectedFillInactive: themeController.itemSelectedFillInactive
    readonly property color itemSelectedBorder: themeController.itemSelectedBorder
    readonly property color itemSelectedBorderInactive: themeController.itemSelectedBorderInactive
    readonly property color statusRailFill: themeController.statusRailFill
    readonly property color menuBorder: themeController.menuBorder
    readonly property color menuSeparator: themeController.menuSeparator
    readonly property color menuItemPressed: themeController.menuItemPressed
    readonly property color chromeGradientStart: themeController.chromeGradientStart
    readonly property color chromeGradientMid: themeController.chromeGradientMid
    readonly property color chromeGradientEnd: themeController.chromeGradientEnd
    readonly property color shadow: themeController.shadow

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

    readonly property real surfaceOpacity: 0.85

    readonly property color menuSurface: themeController.isDark
            ? surface
            : bg

    readonly property color menuItemHover: surfaceHover

    readonly property int radiusXs: 3
    readonly property int radiusSm: 6
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12
    readonly property int radiusXl: 16
    readonly property int radius: radiusMd

    readonly property int controlRadius: radiusLg
    readonly property int panelRadius: radiusXl

    function innerRadius(outerRadius, padding) {
        return Math.max(0, outerRadius - padding)
    }

    function radiusForSide(shortSide) {
        if (shortSide <= 12) return radiusXs
        if (shortSide <= 28) return radiusSm
        if (shortSide <= 48) return radiusMd
        if (shortSide <= 96) return radiusLg
        return radiusXl
    }

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
