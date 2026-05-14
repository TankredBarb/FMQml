import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Pane {
    id: root
    
    padding: 0
    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
        radius: Theme.radius
    }

    implicitHeight: content.implicitHeight + 20
    visible: workspaceController.operationQueue.busy || workspaceController.operationQueue.error.length > 0

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: workspaceController.operationQueue.error.length > 0 ? "Operation Failed" : "File Operations"
                font.bold: true
                color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                text: "Dismiss"
                visible: workspaceController.operationQueue.error.length > 0
                onClicked: workspaceController.operationQueue.error = ""
                flat: true
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: workspaceController.operationQueue.busy
            from: 0
            to: 1
            value: workspaceController.operationQueue.progress
        }

        Label {
            Layout.fillWidth: true
            text: workspaceController.operationQueue.error.length > 0
                  ? workspaceController.operationQueue.error
                  : workspaceController.operationQueue.currentLabel
            color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.Wrap
            elide: Text.ElideMiddle
            maximumLineCount: 2
        }

        Label {
            Layout.fillWidth: true
            visible: workspaceController.operationQueue.busy && workspaceController.operationQueue.totalItems > 0
            text: workspaceController.operationQueue.completedItems + " / " + workspaceController.operationQueue.totalItems
            color: Theme.textSecondary
            font.pixelSize: 11
        }
    }
}
