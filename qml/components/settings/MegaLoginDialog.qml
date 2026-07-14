import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

Dialog {
    required property var dialogRoot
    property alias emailText: megaEmailField.text
    property alias passwordText: megaPasswordField.text
    property bool passwordVisible: false
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: megaPasswordField
                Layout.fillWidth: true
                placeholderText: "Password"
                echoMode: megaLoginDialog.passwordVisible ? TextInput.Normal : TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onAccepted: megaLoginDialog.dialogRoot.submitMegaLogin()
            }

            IconButton {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                iconSource: megaLoginDialog.passwordVisible
                            ? "qrc:/qt/qml/FM/qml/assets/icons/eye-off.svg"
                            : "qrc:/qt/qml/FM/qml/assets/icons/eye.svg"
                iconTone: "action"
                iconSize: 16
                onClicked: megaLoginDialog.passwordVisible = !megaLoginDialog.passwordVisible
                ToolTip.visible: hovered
                ToolTip.text: megaLoginDialog.passwordVisible ? "Hide password" : "Show password"
            }
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
