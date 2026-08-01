import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM

SettingsContentBlock {
    id: root

    property var dialogRoot: null

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
                text: root.dialogRoot ? root.dialogRoot.megaStatusText : ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeCaption
                color: root.dialogRoot ? root.dialogRoot.detailText : Theme.textSecondary
            }
        }

        DialogActionButton {
            text: root.dialogRoot && root.dialogRoot.megaAuthorized ? "Sign out" : "Log in"
            highlighted: false
            secondaryTextColor: root.dialogRoot && root.dialogRoot.megaAuthorized
                                ? Theme.danger
                                : (root.dialogRoot ? root.dialogRoot.dialogAccent : Theme.accent)
            onClicked: {
                if (!root.dialogRoot) return
                if (root.dialogRoot.megaAuthorized) root.dialogRoot.signOutMega()
                else root.dialogRoot.openMegaLoginDialog()
            }
        }
    }
}
