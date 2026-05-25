import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property var workspaceController
    property string currentPath: ""

    readonly property bool readOnlyDestination: {
        if (!root.currentPath) return false
        if (root.currentPath.toLowerCase().startsWith("archive://")) return true
        return root.workspaceController && root.workspaceController.isInsideManagedIsoMount(root.currentPath)
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        enabled: !root.readOnlyDestination
        onDropped: (drop) => {
            if (drop.hasText) {
                const paths = [drop.text]
                root.workspaceController.operationQueue.copyTo(paths, root.currentPath)
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.accent
            opacity: parent.containsDrag ? 0.1 : 0
            visible: parent.containsDrag
            border.color: Theme.accent
            border.width: 2
        }
    }
}
