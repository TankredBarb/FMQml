import QtQuick

QtObject {
    id: coordinator
    required property var panel

    property bool scrolling: false
    property string pendingRestorePath: ""
    property real pendingRestoreY: -1
    property int pendingRestoreAttempts: 0
    property bool pendingRestoreEnabled: false

    property Timer stopTimer: Timer {
        interval: 50
        onTriggered: {
            if (coordinator.panel.viewMotionActive()) {
                restart()
                return
            }
            coordinator.scrolling = false
            if (coordinator.panel.controller) coordinator.panel.controller.scrolling = false
            coordinator.panel.scheduleFileViewsReuseDisable("scroll-stop")
        }
    }

    property Timer restoreTimer: Timer {
        interval: 0
        repeat: false
        onTriggered: coordinator.panel.restorePendingScrollPosition()
    }
}
