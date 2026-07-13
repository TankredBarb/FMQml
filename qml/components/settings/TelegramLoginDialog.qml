import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

Dialog {
    required property var dialogRoot
    property alias apiIdText: telegramApiIdField.text
    property alias apiHashText: telegramApiHashField.text
    property alias phoneText: telegramPhoneField.text
    property alias codeText: telegramCodeField.text
    property alias passwordText: telegramPasswordField.text
    function focusApiId() { telegramApiIdField.forceActiveFocus() }
    id: telegramLoginDialog
    title: "Telegram Login"
    modal: true
    focus: true
    parent: Overlay.overlay
    width: Math.min(440, Math.max(320, telegramLoginDialog.dialogRoot.width - 80))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            id: telegramStatusLabel
            Layout.fillWidth: true
            text: telegramLoginDialog.dialogRoot.telegramStatusText
            wrapMode: Text.WordWrap
            color: telegramLoginDialog.dialogRoot.detailText
            font.pixelSize: Theme.fontSizeCaption
        }

        TextField {
            id: telegramApiIdField
            Layout.fillWidth: true
            placeholderText: "API ID"
            inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoAutoUppercase
            onAccepted: telegramApiHashField.forceActiveFocus()
        }

        TextField {
            id: telegramApiHashField
            Layout.fillWidth: true
            placeholderText: "API hash"
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhNoAutoUppercase
            onAccepted: telegramPhoneField.forceActiveFocus()
        }

        TextField {
            id: telegramPhoneField
            Layout.fillWidth: true
            placeholderText: "Phone number"
            inputMethodHints: Qt.ImhDialableCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: telegramLoginDialog.dialogRoot.submitTelegramPhone()
        }

        DialogActionButton {
            text: "Send code"
            highlighted: true
            primaryColor: telegramLoginDialog.dialogRoot.dialogAccent
            enabled: telegramApiIdField.text.trim().length > 0
                     && telegramApiHashField.text.trim().length > 0
                     && telegramPhoneField.text.trim().length > 0
            onClicked: telegramLoginDialog.dialogRoot.submitTelegramPhone()
        }

        TextField {
            id: telegramCodeField
            Layout.fillWidth: true
            placeholderText: "Login code"
            inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoAutoUppercase
            onAccepted: telegramLoginDialog.dialogRoot.submitTelegramCode()
        }

        DialogActionButton {
            text: "Submit code"
            highlighted: false
            secondaryTextColor: telegramLoginDialog.dialogRoot.dialogAccent
            enabled: telegramCodeField.text.trim().length > 0
            onClicked: telegramLoginDialog.dialogRoot.submitTelegramCode()
        }

        TextField {
            id: telegramPasswordField
            Layout.fillWidth: true
            placeholderText: "2FA password"
            echoMode: TextInput.Password
            onAccepted: telegramLoginDialog.dialogRoot.submitTelegramPassword()
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            DialogActionButton {
                text: "Cancel"
                highlighted: false
                onClicked: telegramLoginDialog.close()
            }
            DialogActionButton {
                text: "Submit password"
                highlighted: false
                secondaryTextColor: telegramLoginDialog.dialogRoot.dialogAccent
                enabled: telegramPasswordField.text.length > 0
                onClicked: telegramLoginDialog.dialogRoot.submitTelegramPassword()
            }
        }
    }
}
