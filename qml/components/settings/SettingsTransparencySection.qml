import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property var openTransparencySettings

    title: "SURFACE EFFECTS"
    accentColor: Theme.accent
    fillColor: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    radiusSize: Theme.radiusMd

    SettingsContentBlock {
        Label {
            Layout.fillWidth: true
            text: "Surface effects"
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Label {
            Layout.fillWidth: true
            text: "Configure gradients, transparency, blur, and where surface effects are applied."
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeLabel
        }

        RowLayout {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Open Surface Effects"
                highlighted: false
                secondaryTextColor: section.accentColor
                onClicked: section.openTransparencySettings()
            }

            Item { Layout.fillWidth: true }
        }
    }
}
