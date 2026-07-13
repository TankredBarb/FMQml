import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

DialogSection {
    id: section

    required property var dialogRoot
    title: "APP"
    accentColor: section.dialogRoot.dialogAccent
    fillColor: section.dialogRoot.sectionFill
    borderColor: section.dialogRoot.sectionBorder
    radiusSize: Theme.radiusMd

    SettingsToggleRow {
        title: "Use system tray icon"
        subtitle: "Keep FM running in the notification area when the window is closed"
        checked: section.dialogRoot.systemTrayIconEnabled
        accentColor: section.dialogRoot.dialogAccent
        onToggled: (checked) => section.dialogRoot.setSystemTrayIconEnabled(checked)
    }

    SettingsToggleRow {
        title: "Allow only 1 instance"
        subtitle: "Show a short splash and close when FM is already running"
        checked: section.dialogRoot.allowOnlyOneInstanceEnabled
        accentColor: section.dialogRoot.dialogAccent
        onToggled: (checked) => section.dialogRoot.setAllowOnlyOneInstanceEnabled(checked)
    }

    SettingsToggleRow {
        title: "Experimental panel drag and drop"
        subtitle: "Allow dragging selected items to the opposite panel"
        checked: section.dialogRoot.limitedDragNDropEnabled
        accentColor: section.dialogRoot.dialogAccent
        onToggled: (checked) => section.dialogRoot.setLimitedDragNDropEnabled(checked)
    }

    SettingsContentBlock {
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "Plugins"
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Label {
                    text: "View loaded provider/action plugins and load plugin files for this session."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: section.dialogRoot.detailText
                }
            }

            DialogActionButton {
                text: "Manage"
                highlighted: false
                secondaryTextColor: section.dialogRoot.dialogAccent
                onClicked: section.dialogRoot.openPluginManager()
            }
        }
    }

}
