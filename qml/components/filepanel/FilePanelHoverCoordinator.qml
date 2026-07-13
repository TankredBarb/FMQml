import QtQuick

QtObject {
    id: coordinator
    required property var panel

    property bool suppressed: false
    property string pendingClearPath: ""
    property Timer suppressTimer: Timer {
        interval: 50
        repeat: false
        onTriggered: {
            if (coordinator.panel.viewMotionActive()) {
                coordinator.suppressed = true
                restart()
                return
            }
            coordinator.suppressed = false
        }
    }

    property Timer clearTimer: Timer {
        interval: 140
        repeat: false
        onTriggered: {
            if (!coordinator.panel.controller || coordinator.pendingClearPath.length === 0) return
            if (coordinator.panel.hoverPreviewPointerInside() || coordinator.panel.contextMenuOpen) {
                restart()
                return
            }
            if (coordinator.panel.controller.hoveredPath === coordinator.pendingClearPath) {
                coordinator.panel.controller.hoveredPath = ""
            }
            coordinator.pendingClearPath = ""
        }
    }
}
