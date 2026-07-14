import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    property var controller: null
    property string archivePath: ""
    property string displayName: ""
    property string message: ""
    property bool acceptedPassword: false
    property bool passwordVisible: false
    readonly property color dialogAccent: Theme.accent

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.9, 460)
    padding: 20

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(targetController, path, name, promptMessage) {
        root.controller = targetController
        root.archivePath = path || ""
        root.displayName = name || fileNameFor(path)
        root.message = promptMessage || "Archive password required"
        root.acceptedPassword = false
        root.passwordVisible = false
        passwordField.text = ""
        if (root.controller && root.archivePath.length > 0) {
            root.open()
        }
    }

    function fileNameFor(path) {
        if (!path) return ""
        const text = String(path)
        const parts = text.split(/[|/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : text
    }

    function submitPassword() {
        const value = passwordField.text
        if (!root.controller || root.archivePath.length === 0 || value.length === 0) {
            return
        }
        root.acceptedPassword = true
        root.controller.submitArchivePassword(root.archivePath, value)
        root.close()
    }

    onOpened: Qt.callLater(() => passwordField.forceActiveFocus())
    onClosed: {
        if (!root.acceptedPassword && root.controller && root.archivePath.length > 0) {
            root.controller.cancelArchivePassword(root.archivePath)
        }
        root.acceptedPassword = false
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellBorderColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.30 : 0.22)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if ((event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
                       && passwordField.text.length > 0) {
                root.submitPassword()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/archive.svg"
            iconTint: root.dialogAccent
            accentColor: root.dialogAccent
            title: "Archive Password"
            subtitle: root.displayName
            showCloseButton: false
        }

        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: infoLayout.implicitHeight + 18
            surfaceColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.08 : 0.045)
            strokeColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.24 : 0.18)

            ColumnLayout {
                id: infoLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.message
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    PremiumTextField {
                        id: passwordField
                        Layout.fillWidth: true
                        placeholderText: "Password"
                        echoMode: root.passwordVisible ? TextInput.Normal : TextInput.Password
                        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                        onAccepted: root.submitPassword()
                    }

                    IconButton {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        iconSource: root.passwordVisible
                                    ? "qrc:/qt/qml/FM/qml/assets/icons/eye-off.svg"
                                    : "qrc:/qt/qml/FM/qml/assets/icons/eye.svg"
                        iconTone: "action"
                        iconSize: 16
                        onClicked: root.passwordVisible = !root.passwordVisible
                        ToolTip.visible: hovered
                        ToolTip.text: root.passwordVisible ? "Hide password" : "Show password"
                    }
                }
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Cancel"
                Layout.fillWidth: true
                highlighted: false
                onClicked: root.close()
            }

            DialogActionButton {
                text: "Open"
                Layout.fillWidth: true
                highlighted: true
                enabled: passwordField.text.length > 0
                primaryColor: root.dialogAccent
                primaryHoverColor: root.dialogAccent
                primaryPressedColor: root.dialogAccent
                onClicked: root.submitPassword()
            }
        }
    }
}
