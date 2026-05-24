import QtQuick
import QtQuick.Effects
import "../../style"

Rectangle {
    id: root

    property color shellColor: Theme.panelSurface
    property color shellBorderColor: Theme.panelBorder
    property color accentColor: Theme.accent
    property bool accentVisible: true
    property int shellRadius: Theme.radiusLg
    property bool shadowEnabled: true
    property color shadowColor: Theme.glassShadow
    property int shadowBlur: 20
    property int shadowVerticalOffset: 8

    color: root.shellColor
    radius: root.shellRadius
    border.color: root.shellBorderColor
    border.width: 1

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        radius: root.shellRadius
        visible: root.accentVisible
        color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.75 : 0.55)
    }

    layer.enabled: root.shadowEnabled
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: root.shadowColor
        shadowBlur: root.shadowBlur
        shadowVerticalOffset: root.shadowVerticalOffset
    }
}
