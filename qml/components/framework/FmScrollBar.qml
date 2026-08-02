import QtQuick
import QtQuick.Controls
import FM
import "../../style"

ScrollBar {
    id: root

    property bool flat: false
    property var wheelTarget: null
    property var scrollNeededOverride: undefined
    property real wheelDestination: 0
    property bool wheelRoutingActive: false
    readonly property bool scrollNeeded: root.policy === ScrollBar.AlwaysOn
                                         || (root.policy === ScrollBar.AsNeeded
                                             && (root.scrollNeededOverride !== undefined
                                                 ? Boolean(root.scrollNeededOverride)
                                                 : (root.size > 0 && root.size < 0.999999)))
    readonly property bool liquidActive: root.enabled && (root.active || root.hovered || root.pressed)
    readonly property color idleColor: Theme.readableOn(Theme.panelSurface, Theme.textSecondary)

    implicitWidth: orientation === Qt.Vertical ? (scrollNeeded ? (flat ? 7 : 13) : 0) : 80
    implicitHeight: orientation === Qt.Vertical ? 80 : (scrollNeeded ? (flat ? 7 : 13) : 0)
    topPadding: orientation === Qt.Vertical && !flat ? 14 : 0
    bottomPadding: orientation === Qt.Vertical && !flat ? 14 : 0
    leftPadding: orientation === Qt.Horizontal && !flat ? 14 : 0
    rightPadding: orientation === Qt.Horizontal && !flat ? 14 : 0
    minimumSize: 0.08
    stepSize: 0.08
    interactive: scrollNeeded
    wheelEnabled: false

    background: null

    function routeWheel(event) {
        if (!wheelTarget)
            return

        const pixelDelta = orientation === Qt.Horizontal && event.pixelDelta.x !== 0
                         ? event.pixelDelta.x : event.pixelDelta.y
        const angleDelta = orientation === Qt.Horizontal && event.angleDelta.x !== 0
                         ? event.angleDelta.x : event.angleDelta.y
        const rawDelta = pixelDelta !== 0 ? pixelDelta : angleDelta
        if (rawDelta === 0)
            return

        const horizontal = orientation === Qt.Horizontal
        const origin = horizontal ? wheelTarget.originX : wheelTarget.originY
        const leadingMargin = horizontal ? wheelTarget.leftMargin : wheelTarget.topMargin
        const trailingMargin = horizontal ? wheelTarget.rightMargin : wheelTarget.bottomMargin
        const contentExtent = horizontal ? wheelTarget.contentWidth : wheelTarget.contentHeight
        const viewportExtent = horizontal ? wheelTarget.width : wheelTarget.height
        const currentPosition = horizontal ? wheelTarget.contentX : wheelTarget.contentY
        const minimum = origin - leadingMargin
        const maximum = Math.max(minimum,
                                 origin + contentExtent - viewportExtent + trailingMargin)
        if (!wheelAnimation.running)
            wheelDestination = currentPosition

        const directedDelta = (event.inverted ? rawDelta : -rawDelta) * 0.4
        wheelDestination = Math.max(minimum, Math.min(maximum, wheelDestination + directedDelta))
        wheelAnimation.stop()
        wheelAnimation.from = currentPosition
        wheelAnimation.to = wheelDestination
        wheelAnimation.start()
        wheelRouteFinish.restart()
        wheelRoutingActive = true
        event.accepted = true
    }

    WheelHandler {
        enabled: root.scrollNeeded && root.wheelTarget !== null
        target: null
        orientation: Qt.Vertical
        blocking: true
        onWheel: (event) => root.routeWheel(event)
    }

    NumberAnimation {
        id: wheelAnimation
        target: root.wheelTarget
        property: root.orientation === Qt.Horizontal ? "contentX" : "contentY"
        duration: 110
        easing.type: Easing.OutCubic
    }

    Timer {
        id: wheelRouteFinish
        interval: 140
        onTriggered: root.wheelRoutingActive = false
    }

    contentItem: Item {
        implicitWidth: root.orientation === Qt.Vertical
                       ? 7
                       : Math.max(28, root.availableWidth * root.visualSize)
        implicitHeight: root.orientation === Qt.Vertical
                        ? Math.max(28, root.availableHeight * root.visualSize)
                        : 7
    }

    FmScrollBarVisual {
        anchors.fill: parent
        visible: root.scrollNeeded
        visualPosition: root.visualPosition
        visualSize: root.visualSize
        orientation: root.orientation
        flat: root.flat
        active: root.liquidActive
        decreaseHovered: decreaseMouse.containsMouse
        increaseHovered: increaseMouse.containsMouse
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.64 : 0.74)
        idleColor: root.idleColor
        accentColor: Theme.accent
    }

    function stopArrowRepeat() {
        arrowRepeatDelay.stop()
        arrowRepeat.stop()
    }

    Timer {
        id: arrowRepeatDelay
        interval: 350
        onTriggered: arrowRepeat.start()
    }

    Timer {
        id: arrowRepeat
        interval: 55
        repeat: true
        onTriggered: decreaseMouse.pressed ? root.decrease() : root.increase()
    }

    Item {
        visible: root.scrollNeeded && !root.flat
        x: 0
        y: 0
        width: root.orientation === Qt.Vertical ? parent.width : 14
        height: root.orientation === Qt.Vertical ? 14 : parent.height

        MouseArea {
            id: decreaseMouse
            anchors.fill: parent
            hoverEnabled: true
            onPressed: {
                root.decrease()
                arrowRepeatDelay.restart()
            }
            onReleased: root.stopArrowRepeat()
            onCanceled: root.stopArrowRepeat()
            onExited: root.stopArrowRepeat()
        }
    }

    Item {
        visible: root.scrollNeeded && !root.flat
        x: root.orientation === Qt.Vertical ? 0 : parent.width - width
        y: root.orientation === Qt.Vertical ? parent.height - height : 0
        width: root.orientation === Qt.Vertical ? parent.width : 14
        height: root.orientation === Qt.Vertical ? 14 : parent.height

        MouseArea {
            id: increaseMouse
            anchors.fill: parent
            hoverEnabled: true
            onPressed: {
                root.increase()
                arrowRepeatDelay.restart()
            }
            onReleased: root.stopArrowRepeat()
            onCanceled: root.stopArrowRepeat()
            onExited: root.stopArrowRepeat()
        }
    }
}
