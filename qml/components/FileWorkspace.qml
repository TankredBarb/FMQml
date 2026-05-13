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
            SplitView.preferredWidth: parent.width / (workspaceController.splitEnabled ? 2 : 1)
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
            controller: workspaceController.leftPanel
            active: workspaceController.activePanel === 0
            onActivated: workspaceController.activateLeft()
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
            onActivated: workspaceController.activateRight()
        }

        handle: Rectangle {
            implicitWidth: 10
            implicitHeight: 10
            color: SplitHandle.hovered || SplitHandle.pressed ? Theme.accent : "transparent"

            Rectangle {
                anchors.centerIn: parent
                width: 1
                height: parent.height
                color: SplitHandle.hovered || SplitHandle.pressed ? Theme.accent : Theme.border
            }
        }
    }
}
