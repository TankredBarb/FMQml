import QtQuick
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property bool nativeIconsEnabled
    required property bool thumbnailsEnabled
    required property var setNativeIconsEnabled
    required property var setThumbnailsEnabled

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
        title: "Thumbnails"
        subtitle: "Show generated previews in Grid and Brief views when native icons are enabled"
        checked: section.nativeIconsEnabled && section.thumbnailsEnabled
        toggleEnabled: section.nativeIconsEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setThumbnailsEnabled(checked)
    }

}
