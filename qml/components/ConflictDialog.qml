import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../style"

Dialog {
    id: root
    title: "File Conflict"
    modal: true
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 480

    property string sourcePath: ""
    property string destinationPath: ""

    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
        radius: Theme.radius
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Label {
            text: "A file with the same name already exists in the destination folder."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: Theme.textPrimary
            font.bold: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label { text: "Source:"; color: Theme.textSecondary; font.pixelSize: 11 }
            Label { text: root.sourcePath; wrapMode: Text.WrapAtWordBoundaryOrAnywhere; Layout.fillWidth: true; font.pixelSize: 12; color: Theme.textPrimary }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label { text: "Existing:"; color: Theme.textSecondary; font.pixelSize: 11 }
            Label { text: root.destinationPath; wrapMode: Text.WrapAtWordBoundaryOrAnywhere; Layout.fillWidth: true; font.pixelSize: 12; color: Theme.textPrimary }
        }

        CheckBox {
            id: applyToAll
            text: "Apply to all remaining conflicts"
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Layout.topMargin: 10

            Button {
                text: "Replace"
                Layout.fillWidth: true
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, applyToAll.checked)
                    root.close()
                }
            }

            Button {
                text: "Skip"
                Layout.fillWidth: true
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Skip, applyToAll.checked)
                    root.close()
                }
            }

            Button {
                text: "Keep Both"
                Layout.fillWidth: true
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.KeepBoth, applyToAll.checked)
                    root.close()
                }
            }

            Button {
                text: "Cancel"
                Layout.fillWidth: true
                onClicked: {
                    workspaceController.operationQueue.cancel()
                    root.close()
                }
            }
        }
    }
}
