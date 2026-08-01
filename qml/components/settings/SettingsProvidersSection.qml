import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

DialogSection {
    id: section

    required property var dialogRoot
    title: "PROVIDERS"
    accentColor: section.dialogRoot.dialogAccent
    fillColor: section.dialogRoot.sectionFill
    borderColor: section.dialogRoot.sectionBorder
    radiusSize: Theme.radiusMd

    Repeater {
        model: section.dialogRoot.pluginSettingsComponents

        ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 4

            Loader {
                id: pluginSettingsLoader
                Layout.fillWidth: true
                source: String(modelData.componentUrl || "")
                onLoaded: item.dialogRoot = section.dialogRoot
            }

            SettingsContentBlock {
                visible: pluginSettingsLoader.status === Loader.Error

                Label {
                    Layout.fillWidth: true
                    text: "Settings for " + String(modelData.title || modelData.pluginId || "this plugin")
                          + " could not be loaded."
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: section.dialogRoot.detailText
                }
            }
        }
    }
}
