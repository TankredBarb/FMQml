import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

Dialog {
    required property var dialogRoot
    id: telegramForgetLocalDataDialog
    title: "Forget Telegram Local Data"
    modal: true
    focus: true
    parent: Overlay.overlay
    width: Math.min(460, Math.max(320, telegramForgetLocalDataDialog.dialogRoot.width - 80))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: "This will close Telegram, remove saved API credentials, and delete the local TDLib database and downloaded Telegram files for FMQml."
            wrapMode: Text.WordWrap
            color: telegramForgetLocalDataDialog.dialogRoot.detailText
            font.pixelSize: Theme.fontSizeCaption
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            DialogActionButton {
                text: "Cancel"
                highlighted: false
                onClicked: telegramForgetLocalDataDialog.close()
            }
            DialogActionButton {
                text: "Forget data"
                highlighted: false
                secondaryTextColor: Theme.danger
                onClicked: telegramForgetLocalDataDialog.dialogRoot.forgetTelegramLocalData()
            }
        }
    }
}
