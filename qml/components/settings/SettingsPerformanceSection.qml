import QtQuick
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property bool nativeIconsEnabled
    required property bool highQualitySystemIconsEnabled
    required property bool thumbnailsEnabled
    required property bool ultraLightModeEnabled
    required property bool gradientColorsEnabled
    required property bool commandPaletteTransparencyEnabled
    required property bool shellFirstQmlRestoreEnabled
    required property var setNativeIconsEnabled
    required property var setHighQualitySystemIconsEnabled
    required property var setThumbnailsEnabled
    required property var setUltraLightModeEnabled
    required property var setGradientColorsEnabled
    required property var setCommandPaletteTransparencyEnabled
    required property var setShellFirstQmlRestoreEnabled

    title: "PERFORMANCE"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    SettingsToggleRow {
        title: "Native icons"
        subtitle: "Use Windows Shell icons instead of bundled file type icons"
        checked: section.nativeIconsEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setNativeIconsEnabled(checked)
    }

    SettingsToggleRow {
        title: "Use high quality system icons"
        subtitle: "Request larger Windows Shell icons for big icon views to avoid scaling artifacts"
        checked: section.highQualitySystemIconsEnabled
        toggleEnabled: section.nativeIconsEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setHighQualitySystemIconsEnabled(checked)
    }

    SettingsToggleRow {
        title: "Thumbnails"
        subtitle: "Show generated previews in Grid and Brief views when native icons are enabled"
        checked: section.nativeIconsEnabled && section.thumbnailsEnabled
        toggleEnabled: section.nativeIconsEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setThumbnailsEnabled(checked)
    }

    SettingsToggleRow {
        title: "Ultra light mode"
        subtitle: "Use lightweight preview, disable thumbnails, and reduce decorative effects"
        checked: section.ultraLightModeEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setUltraLightModeEnabled(checked)
    }

    SettingsToggleRow {
        title: "Gradient colors"
        subtitle: "Use subtle gradient surfaces in app chrome"
        checked: section.gradientColorsEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setGradientColorsEnabled(checked)
    }

    SettingsToggleRow {
        title: "Command palette transparency"
        subtitle: "Use the ambient translucent command palette surface"
        checked: section.commandPaletteTransparencyEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setCommandPaletteTransparencyEnabled(checked)
    }

    SettingsToggleRow {
        title: "Shell-first startup"
        subtitle: "Show the main shell before QML layout restore; applies after restart"
        checked: section.shellFirstQmlRestoreEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setShellFirstQmlRestoreEnabled(checked)
    }
}
