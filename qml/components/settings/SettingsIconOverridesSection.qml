import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property bool nativeIconsEnabled
    required property var openIconOverrides

    title: "ICON OVERRIDES"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    SettingsContentBlock {
        Label {
            Layout.fillWidth: true
            text: "Icon overrides"
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Label {
            Layout.fillWidth: true
            text: section.nativeIconsEnabled
                  ? "Replace system icons for selected file suffixes. " + fileTypeIconResolver.iconOverrides.length + " rule(s) configured."
                  : "Saved rules are inactive because Native icons is disabled."
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeLabel
        }

        RowLayout {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Manage Icon Overrides"
                highlighted: false
                secondaryTextColor: Theme.accent
                onClicked: section.openIconOverrides()
            }

            Item { Layout.fillWidth: true }
        }
    }
}
