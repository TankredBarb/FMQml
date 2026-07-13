import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property var openThemeEditor

    title: "THEMES"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    SettingsContentBlock {
        Label {
            text: "Theme Editor"
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            elide: Text.ElideRight
        }

        Label {
            text: "Theme Editor starts from a neutral blank draft, never edits built-in themes, and saves separate custom files that later appear in the theme picker."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeLabel
            color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.74 : 0.82)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            DialogActionButton {
                text: "Open Theme Editor"
                highlighted: false
                secondaryTextColor: Theme.accent
                onClicked: section.openThemeEditor()
            }

            Item { Layout.fillWidth: true }
        }
    }
}
