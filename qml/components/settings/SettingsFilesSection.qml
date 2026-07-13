import QtQuick
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property bool hiddenFilesEnabled
    required property var setHiddenFilesEnabled

    title: "FILES"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    SettingsToggleRow {
        title: "Hidden files"
        subtitle: "Show hidden entries in panels and folder tree"
        checked: section.hiddenFilesEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setHiddenFilesEnabled(checked)
    }
}
