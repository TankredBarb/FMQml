import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "framework"
import "dialogs"

Dialog {
    id: root
    objectName: "mapRenameDialog"
    property var controller: null
    property var panel: null
    property var appRoot: null
    property var backdropSource: null
    property string sourcePath: ""
    property string originalName: ""
    property string errorMessage: ""
    property bool directory: false

    title: "Rename"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 460)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(targetController, targetPanel, path, isDirectory) {
        controller = targetController
        panel = targetPanel
        sourcePath = path
        originalName = path.substring(path.lastIndexOf("/") + 1)
        directory = isDirectory
        errorMessage = ""
        nameField.text = originalName
        open()
    }

    function submit() {
        if (!renameButton.enabled) return
        if (!controller.renamePath(sourcePath, nameField.text)) {
            errorMessage = controller.lastError.message || "The item could not be renamed."
            nameField.forceActiveFocus()
            return
        }
        const newPath = sourcePath.substring(0, sourcePath.lastIndexOf("/") + 1) + nameField.text.trim()
        controller.directoryModel.selectMapPath(newPath, false)
        panel.activeView().focusEntry({path: newPath})
        accept()
    }

    onOpened: Qt.callLater(() => {
        nameField.forceActiveFocus()
        const dot = originalName.lastIndexOf(".")
        nameField.select(0, !directory && dot > 0 ? dot : originalName.length)
    })
    onClosed: {
        appRoot.finishRenamePreviewSuppression(true)
        Qt.callLater(() => panel.focusContent())
    }

    background: DialogShell {
        translucent: typeof appSettings !== "undefined" && appSettings
                     ? appSettings.workspaceDialogsTransparency : false
        backdropSource: root.backdropSource
        backdropTransformItem: root
        accentColor: Theme.categoryAction
        shellBorderColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.28 : 0.20)
    }
    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/rename.svg"
        iconTint: Theme.categoryAction
        accentColor: Theme.categoryAction
        title: "Rename"
        subtitle: root.originalName
        onCloseRequested: root.reject()
    }
    contentItem: ColumnLayout {
        spacing: 8
        FmTextField {
            id: nameField
            objectName: "mapRenameNameField"
            Layout.fillWidth: true
            Layout.margins: 20
            placeholderText: "New name"
            error: root.errorMessage.length > 0
            onTextEdited: root.errorMessage = ""
            onAccepted: root.submit()
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 12
            visible: root.errorMessage.length > 0
            text: root.errorMessage
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            wrapMode: Text.WordWrap
        }
    }
    footer: DialogFooter {
        Item { Layout.fillWidth: true }
        FmButton {
            text: "Cancel"
            onClicked: root.reject()
        }
        FmButton {
            id: renameButton
            text: "Rename"
            highlighted: true
            primaryColor: Theme.categoryAction
            enabled: nameField.text.trim().length > 0
                     && nameField.text.trim() !== "." && nameField.text.trim() !== ".."
                     && !nameField.text.includes("/") && !nameField.text.includes("\\")
            onClicked: root.submit()
        }
    }
}
