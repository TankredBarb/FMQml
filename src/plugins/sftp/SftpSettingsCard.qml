import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import FMSftp 1.0

Item {
    id: root
    objectName: "sftpSettingsCard"
    property var dialogRoot: null
    property string initialPath: ""
    property string selectedUrl: ""
    readonly property bool wide: width >= 680
    readonly property color sectionFill: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.30 : 0.56)
    readonly property color sectionBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.24)
    readonly property var selectedConnection: {
        const profiles = connection.connections
        for (let i = 0; i < profiles.length; ++i)
            if (profiles[i].url === selectedUrl) return profiles[i]
        return null
    }
    Layout.fillWidth: true
    implicitHeight: content.implicitHeight
    readonly property bool busy: connection.busy
    readonly property string status: connection.status
    signal navigationRequested(string path, string message, bool saved)
    signal statusReported(string message, bool error)

    function editConnection(url) {
        const profile = connection.describe(url)
        if (!profile.host) return
        root.selectedUrl = url
        hostField.text = profile.host
        portField.text = String(profile.port)
        userField.text = profile.user
        pathField.text = profile.path
        nameField.text = profile.name || ""
        saveConnection.checked = profile.saved !== false
        rememberPassword.checked = profile.rememberPassword === true
        passwordField.clear()
    }
    onInitialPathChanged: editConnection(initialPath)
    Component.onCompleted: {
        if (initialPath.length > 0) editConnection(initialPath)
        else if (connection.connections.length > 0) editConnection(connection.connections[0].url)
    }

    function newConnection() {
        selectedUrl = ""
        hostField.clear()
        portField.text = "22"
        userField.clear()
        passwordField.clear()
        pathField.text = "/"
        nameField.clear()
        saveConnection.checked = true
        rememberPassword.checked = false
        hostField.forceActiveFocus()
    }

    function openSelectedConnection() {
        const profile = root.selectedConnection
        if (!profile) return
        if (profile.connected) {
            root.navigationRequested(profile.url, "", true)
            const panel = root.dialogRoot && root.dialogRoot.appRoot
                        ? root.dialogRoot.appRoot.activePanelController() : null
            if (panel) panel.openPath(profile.url)
            if (root.dialogRoot) root.dialogRoot.close()
        } else if (profile.rememberPassword) connection.connectSaved(profile.url)
        else root.editConnection(profile.url)
    }

    function refreshPlaces() {
        pluginActionController.triggerAction("fm.sftp-provider::refreshPlaces", {})
    }

    function clearSensitiveInput() {
        if (connection.busy || connection.fingerprint.length > 0) connection.cancel()
        passwordField.clear()
    }

    Connections {
        target: root.dialogRoot
        function onClosed() {
            root.clearSensitiveInput()
        }
    }

    SftpConnectionController {
        id: connection
        onConnected: {
            passwordField.clear()
            root.refreshPlaces()
            const saved = connection.status === "Connected."
            root.navigationRequested(openPath, connection.status, saved)
            const panel = root.dialogRoot && root.dialogRoot.appRoot
                        && root.dialogRoot.appRoot.activePanelController
                        ? root.dialogRoot.appRoot.activePanelController() : null
            if (panel) panel.openPath(openPath)
            if (root.dialogRoot && saved) root.dialogRoot.close()
        }
        onChanged: {
            if (!busy) root.refreshPlaces()
            root.statusReported(status, hasError)
        }
    }

    component BodyLabel: Label {
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
        color: Theme.textPrimary
        textFormat: Text.PlainText
    }
    component Caption: BodyLabel {
        font.pixelSize: Theme.fontSizeCaption
        color: Theme.textSecondary
        wrapMode: Text.WordWrap
    }
    component ActionButton: FmButton { font.family: Theme.fontFamily }
    component Field: FmTextField {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: Theme.fontSizeBody
        font.weight: Font.Normal
    }
    component Option: FmCheckBox {
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }

    ColumnLayout {
        id: content
        width: root.width
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                BodyLabel { text: "SFTP"; visible: root.dialogRoot !== null; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
                Caption { Layout.fillWidth: true; text: "Connect to an SSH server with your login and password." }
            }
            ActionButton {
                objectName: "sftpNewConnection"
                text: "New connection"
                enabled: !connection.busy && connection.fingerprint.length === 0
                onClicked: root.newConnection()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.wide ? 2 : 1
            columnSpacing: 16
            rowSpacing: 16

            Rectangle {
                objectName: "sftpConnectionsSection"
                Layout.fillWidth: true
                Layout.preferredWidth: root.wide ? 260 : root.width
                Layout.minimumWidth: root.wide ? 240 : 0
                Layout.alignment: Qt.AlignTop
                implicitHeight: savedContent.implicitHeight + 32
                radius: Theme.radiusSm
                color: root.sectionFill
                border.color: root.sectionBorder

                ColumnLayout {
                    id: savedContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 12

                    BodyLabel { text: "Connections"; font.weight: Font.DemiBold }
                    Caption { Layout.fillWidth: true; text: "Connected servers appear in Cloud." }
                    Caption {
                        Layout.fillWidth: true
                        visible: connection.connections.length === 0
                        text: "No connections yet. Enter your server details to get started."
                    }
                    ScrollView {
                        id: savedScroll
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(savedList.implicitHeight, 240)
                        visible: connection.connections.length > 0
                        contentWidth: availableWidth
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            id: savedList
                            width: savedScroll.availableWidth
                            spacing: 6
                            Repeater {
                                model: connection.connections
                                delegate: Rectangle {
                                    id: profileRow
                                    required property var modelData
                                    readonly property var details: connection.describe(modelData.url)
                                    readonly property bool selected: root.selectedUrl === modelData.url
                                    objectName: "sftpConnectionRow"
                                    activeFocusOnTab: true
                                    enabled: !connection.busy && connection.fingerprint.length === 0
                                    Accessible.role: Accessible.ListItem
                                    Accessible.name: modelData.name + (modelData.connected ? ", connected" : ", saved")
                                    Keys.onReturnPressed: root.editConnection(modelData.url)
                                    Keys.onEnterPressed: root.editConnection(modelData.url)
                                    Keys.onSpacePressed: root.editConnection(modelData.url)
                                    Layout.fillWidth: true
                                    implicitHeight: 68
                                    radius: Theme.radiusSm
                                    color: selected ? Theme.withAlpha(Theme.accent, 0.12)
                                                    : (profileMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.06) : "transparent")
                                    border.color: activeFocus ? Theme.focusRing
                                                : (selected ? Theme.withAlpha(Theme.accent, 0.50) : "transparent")
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 4
                                        RowLayout {
                                            Layout.fillWidth: true
                                            BodyLabel {
                                                Layout.fillWidth: true
                                                text: String(modelData.name).indexOf("sftp://") === 0
                                                    ? profileRow.details.user + "@" + profileRow.details.host : modelData.name
                                                font.weight: Font.Medium
                                                elide: Text.ElideRight
                                            }
                                            Rectangle {
                                                width: 6; height: 6; radius: 3
                                                color: modelData.connected ? Theme.success : Theme.textSecondary
                                            }
                                            Caption { text: modelData.connected ? "Connected" : "Saved" }
                                        }
                                        Caption {
                                            Layout.fillWidth: true
                                            text: profileRow.details.user + "@" + profileRow.details.host
                                                + (profileRow.details.port === 22 ? "" : ":" + profileRow.details.port)
                                                + " · " + profileRow.details.path
                                            wrapMode: Text.NoWrap
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                    MouseArea {
                                        id: profileMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            profileRow.forceActiveFocus()
                                            root.editConnection(profileRow.modelData.url)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 8
                        visible: root.selectedConnection !== null
                        enabled: !connection.busy && connection.fingerprint.length === 0
                        ActionButton {
                            objectName: "sftpOpenConnection"
                            Layout.fillWidth: true
                            text: root.selectedConnection && root.selectedConnection.connected ? "Open" : "Connect"
                            highlighted: true
                            onClicked: root.openSelectedConnection()
                        }
                        ActionButton {
                            Layout.fillWidth: true
                            text: "Edit"
                            onClicked: { root.editConnection(root.selectedUrl); hostField.forceActiveFocus() }
                        }
                        ActionButton {
                            objectName: "sftpDisconnectConnection"
                            Layout.fillWidth: true
                            text: "Disconnect"
                            visible: root.selectedConnection && root.selectedConnection.connected
                            onClicked: connection.disconnectFrom(root.selectedUrl)
                        }
                        ActionButton {
                            objectName: "sftpForgetConnection"
                            Layout.fillWidth: true
                            text: "Forget"
                            visible: root.selectedConnection && root.selectedConnection.saved
                            destructive: true
                            primaryColor: Theme.danger
                            onClicked: connection.forget(root.selectedUrl)
                        }
                    }
                }
            }

            Rectangle {
                objectName: "sftpEditorSection"
                Layout.fillWidth: true
                Layout.preferredWidth: 480
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                implicitHeight: editorContent.implicitHeight + 32
                radius: Theme.radiusSm
                color: root.sectionFill
                border.color: root.sectionBorder

                ColumnLayout {
                    id: editorContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10
                    BodyLabel {
                        text: root.selectedUrl.length > 0 ? "Connection details" : "New connection"
                        font.weight: Font.DemiBold
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        enabled: !connection.busy && connection.fingerprint.length === 0
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                BodyLabel { text: "Host"; color: Theme.textSecondary }
                                Field { id: hostField; objectName: "sftpHostField"; placeholderText: "192.168.1.10" }
                            }
                            ColumnLayout {
                                Layout.preferredWidth: 84
                                Layout.maximumWidth: 84
                                spacing: 5
                                BodyLabel { text: "Port"; color: Theme.textSecondary }
                                Field {
                                    id: portField
                                    objectName: "sftpPortField"
                                    text: "22"
                                    validator: IntValidator { bottom: 1; top: 65535 }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                BodyLabel { text: "Login"; color: Theme.textSecondary }
                                Field { id: userField; objectName: "sftpLoginField"; placeholderText: "Username" }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                BodyLabel { text: "Password"; color: Theme.textSecondary }
                                Field { id: passwordField; objectName: "sftpPasswordField"; echoMode: TextInput.Password }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                BodyLabel { text: "Folder"; color: Theme.textSecondary }
                                Field { id: pathField; objectName: "sftpFolderField"; text: "/" }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                BodyLabel { text: "Name"; color: Theme.textSecondary }
                                Field { id: nameField; objectName: "sftpNameField"; placeholderText: "Optional name" }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.withAlpha(Theme.panelBorder, 0.60) }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Flow {
                            Layout.fillWidth: true
                            spacing: 12
                            Option { id: saveConnection; text: "Save connection"; checked: true; enabled: !connection.busy }
                            Option {
                                id: rememberPassword
                                text: "Remember password"
                                enabled: saveConnection.checked && !connection.busy
                            }
                        }
                        Caption { Layout.fillWidth: true; text: "Remembered passwords use your system password storage." }
                    }
                    Caption {
                        Layout.fillWidth: true
                        visible: connection.fingerprint.length > 0
                        text: "First connection to " + hostField.text + ":" + portField.text
                              + "\nServer fingerprint: " + connection.fingerprint
                              + "\nConfirm this server to continue with password login."
                        color: Theme.textPrimary
                        wrapMode: Text.WrapAnywhere
                    }
                    Caption {
                        objectName: "sftpStatusMessage"
                        Layout.fillWidth: true
                        visible: connection.status.length > 0
                        text: connection.status
                        color: connection.hasError ? Theme.danger : Theme.textSecondary
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        ActionButton {
                            text: "Connect"
                            highlighted: true
                            enabled: !connection.busy && connection.fingerprint.length === 0
                                     && hostField.text.trim().length > 0 && userField.text.length > 0
                                     && portField.acceptableInput
                            onClicked: connection.signIn(hostField.text, Number(portField.text), userField.text,
                                                         passwordField.text, pathField.text, nameField.text,
                                                         saveConnection.checked, rememberPassword.checked)
                        }
                        ActionButton {
                            text: "Trust server and connect"
                            visible: connection.fingerprint.length > 0
                            enabled: !connection.busy
                            onClicked: connection.confirmHost()
                        }
                        ActionButton {
                            text: "Cancel"
                            visible: connection.busy || connection.fingerprint.length > 0
                            onClicked: { connection.cancel(); passwordField.clear() }
                        }
                    }
                }
            }
        }
    }
}
