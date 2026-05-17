import QtQuick
import QtQuick.Controls
import "../style"

Item {
    id: root

    SplitView {
        anchors.fill: parent
        anchors.margins: 10
        orientation: Qt.Horizontal

        FilePanel {
            id: leftPanel
            SplitView.preferredWidth: parent.width / (workspaceController.splitEnabled ? 2 : 1)
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
            controller: workspaceController.leftPanel
            active: workspaceController.activePanel === 0
            onActivated: {
                workspaceController.activateLeft()
                focusContent()
            }
        }

        FilePanel {
            id: rightPanel
            SplitView.preferredWidth: workspaceController.splitEnabled ? parent.width / 2 : 0
            SplitView.fillWidth: workspaceController.splitEnabled
            SplitView.minimumWidth: workspaceController.splitEnabled ? 280 : 0
            visible: workspaceController.splitEnabled || width > 0
            opacity: workspaceController.splitEnabled ? 1 : 0
            
            Behavior on opacity { OpacityAnimator { duration: Theme.motionNormal } }

            controller: workspaceController.rightPanel
            active: workspaceController.activePanel === 1
            onActivated: {
                workspaceController.activateRight()
                focusContent()
            }
        }

        handle: Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            color: "transparent"
            
            // Interaction overlay
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                color: Theme.accent
                opacity: SplitHandle.pressed ? 0.12 : (SplitHandle.hovered ? 0.06 : 0)
                radius: 4
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            // The actual divider line
            Rectangle {
                anchors.centerIn: parent
                width: (SplitHandle.hovered || SplitHandle.pressed) ? 2 : 1
                height: parent.height - 12
                radius: 1
                color: (SplitHandle.hovered || SplitHandle.pressed) ? Theme.accent : Theme.border
                opacity: (SplitHandle.hovered || SplitHandle.pressed) ? 1.0 : 0.4
                
                Behavior on width { NumberAnimation { duration: 100 } }
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
        }
    }

    OperationsDrawer {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        width: 320
        z: 20
    }

    Connections {
        target: workspaceController
        function onFocusActivePanelRequested() {
            if (workspaceController.activePanel === 0) {
                leftPanel.focusContent()
            } else {
                rightPanel.focusContent()
            }
        }
    }
}
