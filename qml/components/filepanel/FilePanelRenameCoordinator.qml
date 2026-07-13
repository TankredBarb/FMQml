import QtQuick

QtObject {
    id: coordinator
    required property var panel

    property int createSessionId: 0
    property string createPath: ""
    property int createAttempts: 0
    property bool createRevealReady: false
    property bool createStarted: false
    property string pendingFocusPath: ""
    property int pendingFocusAttempts: 0
    property bool pendingFocusSelectText: false
    property string pendingInlinePath: ""

    property Timer createTimer: Timer {
        interval: 16
        repeat: false
        onTriggered: coordinator.panel.tryStartCreateRename()
    }

    property Timer focusTimer: Timer {
        interval: 16
        repeat: false
        onTriggered: coordinator.panel.tryFocusPendingInlineRename()
    }
}
