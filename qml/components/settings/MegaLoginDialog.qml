import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

Dialog {
    required property var dialogRoot
    property alias emailText: megaEmailField.text
    property alias passwordText: megaPasswordField.text
    function focusEmail() { megaEmailField.forceActiveFocus() }
    id: megaLoginDialog
    title: "MEGA Login"
    modal: true
    focus: true
    parent: Overlay.overlay
    width: Math.min(420, Math.max(320, megaLoginDialog.dialogRoot.width - 80))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: "Enter your MEGA credentials. The session token is saved after successful authorization."
            wrapMode: Text.WordWrap
            color: megaLoginDialog.dialogRoot.detailText
            font.pixelSize: Theme.fontSizeCaption
        }

        TextField {
            id: megaEmailField
            Layout.fillWidth: true
            placeholderText: "Email"
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: megaPasswordField.forceActiveFocus()
        }

        TextField {
            id: megaPasswordField
            Layout.fillWidth: true
            placeholderText: "Password"
            echoMode: TextInput.Password
            onAccepted: megaLoginDialog.dialogRoot.submitMegaLogin()
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            DialogActionButton {
                text: "Cancel"
                highlighted: false
                onClicked: megaLoginDialog.close()
            }
            DialogActionButton {
                text: "Log in"
                highlighted: true
                primaryColor: megaLoginDialog.dialogRoot.dialogAccent
                enabled: megaEmailField.text.trim().length > 0 && megaPasswordField.text.length > 0
                onClicked: megaLoginDialog.dialogRoot.submitMegaLogin()
            }
        }
    }
}
