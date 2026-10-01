import QtQuick

MouseArea {
    id: root

    required property var view
    readonly property bool active: finishTimer.running
    property real destination: 0
    property real lastDelta: 0

    acceptedButtons: Qt.NoButton
    // Observe gestures to stop pending wheel motion before handing them back.
    scrollGestureEnabled: true

    function cancel() {
        wheelAnimation.stop()
        finishTimer.stop()
        lastDelta = 0
    }

    onEnabledChanged: if (!enabled) cancel()

    onWheel: (event) => root.routeWheel(event)

    function routeWheel(event) {
        // Pixel gestures retain Flickable's native handling. Mouse-wheel
        // deltas already include the platform's scrolling direction.
        if (event.pixelDelta.x !== 0 || event.pixelDelta.y !== 0
                || event.angleDelta.y === 0) {
            root.cancel()
            event.accepted = false
            return
        }

        const delta = -event.angleDelta.y / 120 * Qt.styleHints.wheelScrollLines * 20
        const minimum = root.view.originY - root.view.topMargin
        const maximum = Math.max(minimum, root.view.originY + root.view.contentHeight - root.view.height + root.view.bottomMargin)
        if (!wheelAnimation.running || delta * root.lastDelta < 0) {
            root.view.cancelFlick()
            root.destination = root.view.contentY
        }
        root.lastDelta = delta
        root.destination = Math.max(minimum, Math.min(maximum, root.destination + delta))

        wheelAnimation.stop()
        wheelAnimation.from = root.view.contentY
        wheelAnimation.to = root.destination
        finishTimer.restart()
        wheelAnimation.start()
        event.accepted = true
    }

    Connections {
        target: root.view
        function onMovementStarted() { root.cancel() }
    }

    NumberAnimation {
        id: wheelAnimation
        target: root.view
        property: "contentY"
        duration: 300
        easing.type: Easing.OutExpo
    }

    Timer {
        id: finishTimer
        interval: 350
    }
}
