import QtQuick

QtObject {
    id: coordinator
    required property var panel

    property bool open: false
    property var pendingRequest: null
    property Timer popupDelayTimer: Timer {
        interval: 140
        repeat: false
        onTriggered: {
            const request = coordinator.pendingRequest
            coordinator.pendingRequest = null
            coordinator.panel.popupContextMenuRequest(request)
        }
    }
}
