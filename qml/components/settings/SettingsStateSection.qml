import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

DialogSection {
    id: section

    required property var dialogRoot
    title: "SETTINGS AND STATE"
    accentColor: section.dialogRoot.dialogAccent
    fillColor: section.dialogRoot.sectionFill
    borderColor: section.dialogRoot.sectionBorder
    radiusSize: Theme.radiusMd

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SettingsContentBlock {
            Label {
                text: "Settings file"
                Layout.fillWidth: true
                font.pixelSize: Theme.fontSizeLabel
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                text: "One settings file includes window geometry, both panels, split layout, preview state, theme, app preferences, and command palette history."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeLabel
                color: section.dialogRoot.detailText
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                DialogActionButton {
                    text: "Export settings"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    onClicked: section.dialogRoot.openExportDialog()
                }

                DialogActionButton {
                    text: "Import settings"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    onClicked: section.dialogRoot.openImportDialog()
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Label {
                text: "Import applies the saved workspace, panel modes, theme, and preferences to the current session."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeCaption
                color: section.dialogRoot.detailText
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: statusLayout.implicitHeight + 16
            radius: Theme.radiusSm
            color: section.dialogRoot.maintenanceStatus.length > 0
                   ? Theme.withAlpha(section.dialogRoot.maintenanceStatusIsError ? Theme.danger : Theme.success,
                                     themeController.isDark ? 0.10 : 0.07)
                   : section.dialogRoot.rowFill
            border.color: section.dialogRoot.maintenanceStatus.length > 0
                          ? Theme.withAlpha(section.dialogRoot.maintenanceStatusIsError ? Theme.danger : Theme.success, 0.32)
                          : section.dialogRoot.rowBorder
            border.width: 1

            ColumnLayout {
                id: statusLayout
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2

                Label {
                    text: section.dialogRoot.maintenanceStatus.length > 0
                          ? section.dialogRoot.maintenanceStatus
                          : "Export creates a portable backup. Import restores it immediately."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: section.dialogRoot.maintenanceStatus.length > 0 ? Font.DemiBold : Font.Normal
                    color: section.dialogRoot.maintenanceStatus.length > 0
                           ? (section.dialogRoot.maintenanceStatusIsError ? Theme.danger : Theme.success)
                           : section.dialogRoot.detailText
                }

                Label {
                    text: "Settings format v" + section.dialogRoot.settingsFormatVersion
                    visible: section.dialogRoot.settingsFormatVersion > 0
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeMicro
                    color: section.dialogRoot.detailText
                }
            }
        }

        SettingsContentBlock {
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Reset saved workspace"
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: section.dialogRoot.workspaceResetPending
                              ? "Saved workspace and theme will reset on the next launch. The current session keeps running as-is until restart."
                              : "Clear saved workspace state and return to the default theme on the next launch. Current session and other preferences are kept."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeCaption
                        color: section.dialogRoot.workspaceResetPending ? Theme.success : section.dialogRoot.detailText
                    }
                }

                DialogActionButton {
                    text: "Reset"
                    highlighted: false
                    enabled: !section.dialogRoot.workspaceResetPending
                    secondaryTextColor: section.dialogRoot.workspaceResetPending ? section.dialogRoot.detailText : section.dialogRoot.dialogAccent
                    onClicked: {
                        if (section.dialogRoot.appRoot) {
                            section.dialogRoot.appRoot.resetSavedWorkspaceState()
                            section.dialogRoot.workspaceResetPending = true
                        }
                    }
                }
            }
        }

        SettingsContentBlock {
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Command palette history"
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: "Clear recent and frequent command ranking data. Commands stay available and future usage will build a fresh history."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeCaption
                        color: section.dialogRoot.detailText
                    }
                }

                DialogActionButton {
                    text: "Clear"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    onClicked: {
                        if (section.dialogRoot.appRoot) {
                            section.dialogRoot.appRoot.resetCommandUsageStats()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.withAlpha(Theme.border, 0.55)
            radius: 0.5
        }

        SettingsContentBlock {
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "App data folder"
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: section.dialogRoot.appDataLocation.length > 0 ? section.dialogRoot.displayPath(section.dialogRoot.appDataLocation) : "App data path is not available."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeCaption
                        color: section.dialogRoot.detailText
                    }
                }

                DialogActionButton {
                    text: "Open folder"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    onClicked: section.dialogRoot.openDataFolder()
                }
            }
        }
    }
}
