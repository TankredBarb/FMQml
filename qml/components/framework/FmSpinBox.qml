import QtQuick
import QtQuick.Controls
import FM
import "../../style"

SpinBox {
    id: root

    property color accentColor: Theme.accent
    property real decreaseActivation: root.down.pressed ? 1 : (root.down.hovered ? 0.42 : 0)
    property real increaseActivation: root.up.pressed ? 1 : (root.up.hovered ? 0.42 : 0)

    editable: true
    implicitWidth: 100
    implicitHeight: Theme.controlHeight
    leftPadding: 30
    rightPadding: 30

    contentItem: TextInput {
        z: 1
        text: root.textFromValue(root.value, root.locale)
        font: root.font
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        selectionColor: root.accentColor
        selectedTextColor: Theme.readableOn(root.accentColor, Theme.textPrimary)
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
    }

    down.indicator: Item {
        x: 0
        y: 0
        implicitWidth: 30
        implicitHeight: root.height
    }

    up.indicator: Item {
        x: root.width - width
        y: 0
        implicitWidth: 30
        implicitHeight: root.height
    }

    background: FmSpinBoxVisual {
        implicitWidth: 100
        implicitHeight: Theme.controlHeight
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        decreaseActivation: root.decreaseActivation
        increaseActivation: root.increaseActivation
        active: root.activeFocus
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        accentColor: root.accentColor
        indicatorColor: root.enabled ? Theme.textPrimary : Theme.textSecondary
    }

    Behavior on decreaseActivation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    Behavior on increaseActivation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
