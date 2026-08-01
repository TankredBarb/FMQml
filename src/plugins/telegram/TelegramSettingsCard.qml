import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM

SettingsContentBlock {
    id: root
    property var dialogRoot: null

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
                    text: root.dialogRoot ? root.dialogRoot.telegramStatusText : ""
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: root.dialogRoot ? root.dialogRoot.detailText : Theme.textSecondary
                }
            }

            DialogActionButton {
                text: root.dialogRoot && root.dialogRoot.telegramAuthorized ? "Sign out" : "Log in"
                highlighted: false
                secondaryTextColor: root.dialogRoot && root.dialogRoot.telegramAuthorized
                                    ? Theme.danger
                                    : (root.dialogRoot ? root.dialogRoot.dialogAccent : Theme.accent)
                onClicked: {
                    if (!root.dialogRoot) return
                    if (root.dialogRoot.telegramAuthorized) root.dialogRoot.signOutTelegram()
                    else root.dialogRoot.openTelegramLoginDialog()
                }
            }

            DialogActionButton {
                text: "Forget data"
                highlighted: false
                secondaryTextColor: Theme.danger
                onClicked: if (root.dialogRoot) root.dialogRoot.openForgetTelegramLocalDataDialog()
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
                onAccepted: if (root.dialogRoot) root.dialogRoot.openTelegramSource(text)
            }

            DialogActionButton {
                text: "Open"
                highlighted: false
                secondaryTextColor: root.dialogRoot ? root.dialogRoot.dialogAccent : Theme.accent
                enabled: telegramSourceField.text.trim().length > 0
                onClicked: if (root.dialogRoot) root.dialogRoot.openTelegramSource(telegramSourceField.text)
            }
        }
    }
}
