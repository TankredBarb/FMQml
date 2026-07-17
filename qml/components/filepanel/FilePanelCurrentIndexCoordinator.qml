import QtQuick

QtObject {
    id: coordinator
    required property var panel

    property string pendingRevealPath: ""
    property string pendingRevealPurpose: ""
    property int pendingRevealAttempts: 0
    property int ensureAttempts: 0
    property bool pendingInit: false
    property bool disableSelectionOnChange: false
    property bool suppressAutoPosition: false

    property Timer ensureTimer: Timer {
        interval: 0
        repeat: false
        onTriggered: coordinator.panel.ensureCurrentIndexWithoutSelection()
    }

    property Timer revealTimer: Timer {
        interval: 16
        repeat: false
        onTriggered: {
            if (coordinator.pendingRevealPath.length === 0) return
            const path = coordinator.pendingRevealPath
            const purpose = coordinator.pendingRevealPurpose
            if (coordinator.panel.revealPathInView(path)) {
                coordinator.pendingRevealPath = ""
                coordinator.pendingRevealPurpose = ""
                coordinator.pendingRevealAttempts = 0
                coordinator.panel.handlePendingRevealFinished(path, purpose, true)
                return
            }
            if (++coordinator.pendingRevealAttempts <= 120) {
                restart()
            } else {
                coordinator.pendingRevealPath = ""
                coordinator.pendingRevealPurpose = ""
                coordinator.pendingRevealAttempts = 0
                coordinator.panel.handlePendingRevealFinished(path, purpose, false)
                coordinator.panel.queueCurrentIndexEnsure()
            }
        }
    }
}
