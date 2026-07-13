import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

DialogSection {
    id: section

    required property var dialogRoot
    title: "PROVIDERS"
    accentColor: section.dialogRoot.dialogAccent
    fillColor: section.dialogRoot.sectionFill
    borderColor: section.dialogRoot.sectionBorder
    radiusSize: Theme.radiusMd

    SettingsContentBlock {
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "Google Drive"
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Label {
                    text: "Authorization is kept in Windows Credential Manager until you sign out."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: section.dialogRoot.detailText
                }
            }

            DialogActionButton {
                text: section.dialogRoot.googleDriveAuthorized ? "Sign out" : "Log in"
                highlighted: false
                secondaryTextColor: section.dialogRoot.googleDriveAuthorized ? Theme.danger : section.dialogRoot.dialogAccent
                onClicked: section.dialogRoot.googleDriveAuthorized ? section.dialogRoot.signOutGoogleDrive() : section.dialogRoot.logInGoogleDrive()
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
                    text: "MEGA"
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Label {
                    text: section.dialogRoot.megaStatusText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: section.dialogRoot.detailText
                }
            }

            DialogActionButton {
                text: section.dialogRoot.megaAuthorized ? "Sign out" : "Log in"
                highlighted: false
                secondaryTextColor: section.dialogRoot.megaAuthorized ? Theme.danger : section.dialogRoot.dialogAccent
                onClicked: section.dialogRoot.megaAuthorized ? section.dialogRoot.signOutMega() : section.dialogRoot.openMegaLoginDialog()
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
                    text: "Instagram"
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Label {
                    text: section.dialogRoot.instagramStatusText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: section.dialogRoot.detailText
                }
            }

            DialogActionButton {
                text: section.dialogRoot.instagramAuthorized ? "Sign out" : "Log in"
                highlighted: false
                secondaryTextColor: section.dialogRoot.instagramAuthorized ? Theme.danger : section.dialogRoot.dialogAccent
                onClicked: section.dialogRoot.instagramAuthorized ? section.dialogRoot.signOutInstagram() : section.dialogRoot.openInstagramSessionImportDialog()
            }
        }
    }

    SettingsContentBlock {
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Telegram"
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: section.dialogRoot.telegramStatusText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeCaption
                        color: section.dialogRoot.detailText
                    }
                }

                DialogActionButton {
                    text: section.dialogRoot.telegramAuthorized ? "Sign out" : "Log in"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.telegramAuthorized ? Theme.danger : section.dialogRoot.dialogAccent
                    onClicked: section.dialogRoot.telegramAuthorized ? section.dialogRoot.signOutTelegram() : section.dialogRoot.openTelegramLoginDialog()
                }

                DialogActionButton {
                    text: "Forget data"
                    highlighted: false
                    secondaryTextColor: Theme.danger
                    onClicked: section.dialogRoot.openForgetTelegramLocalDataDialog()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: telegramSourceField
                    Layout.fillWidth: true
                    placeholderText: "Chat id, @username, or t.me link"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    selectByMouse: true
                    onAccepted: section.dialogRoot.openTelegramSource(telegramSourceField.text)
                }

                DialogActionButton {
                    text: "Open"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    enabled: telegramSourceField.text.trim().length > 0
                    onClicked: section.dialogRoot.openTelegramSource(telegramSourceField.text)
                }
            }
        }
    }
}
