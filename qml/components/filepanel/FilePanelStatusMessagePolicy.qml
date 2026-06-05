import QtQuick

Item {
    id: root

    property var panel: null
    property var operationQueue: null
    property var controller: null

    visible: false
    width: 0
    height: 0

    function showMessage(message) {
        if (!root.panel) {
            return
        }
        root.panel.statusMessage = message || ""
        statusTimer.restart()
    }

    Timer {
        id: statusTimer
        interval: 2500
        onTriggered: {
            if (root.panel) {
                root.panel.statusMessage = ""
            }
        }
    }

    Connections {
        target: root.operationQueue
        function onStatusMessageChanged() {
            root.showMessage(root.operationQueue.statusMessage)
        }
        function onBusyChanged() {
            if (!root.operationQueue.busy) {
                statusTimer.restart()
            }
        }
    }

    Connections {
        target: root.controller
        function onStatusMessageChanged() {
            if (root.controller.statusMessage.length > 0) {
                root.showMessage(root.controller.statusMessage)
            }
        }
    }
}
