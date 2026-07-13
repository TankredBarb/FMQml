import QtQuick
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property bool splitViewEnabled
    required property bool previewPaneEnabled
    required property var setSplitViewEnabled
    required property var setPreviewPaneEnabled

    title: "WORKSPACE"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    SettingsToggleRow {
        title: "Split view"
        subtitle: "Show the second file panel"
        checked: section.splitViewEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setSplitViewEnabled(checked)
    }

    SettingsToggleRow {
        title: "Preview pane"
        subtitle: "Keep the right preview pane visible"
        checked: section.previewPaneEnabled
        accentColor: Theme.accent
        onToggled: checked => section.setPreviewPaneEnabled(checked)
    }
}
