import QtQuick
import QtQuick.Controls
import "../style"

ToolButton {
    id: btn
    property string iconSource
    property string iconTone: "default"
    property bool isHighlighted: false
    property int iconSize: 16
    
    readonly property color hoverFill: {
        if (btn.pressed) {
            return Theme.surfaceActive
        }
        if (btn.hovered || btn.isHighlighted) {
            return themeController.isDark
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
        }
        return "transparent"
    }
    readonly property color hoverBorder: (btn.hovered || btn.isHighlighted)
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.36 : 0.42)
        : "transparent"
    clip: true
    padding: 0
    
    implicitWidth: 32
    implicitHeight: 32
    
    background: Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: 7
        color: btn.hoverFill
        border.color: btn.hoverBorder
        border.width: btn.hovered || btn.isHighlighted || btn.pressed ? 1 : 0
    }
    
    contentItem: Item {
        implicitWidth: btn.iconSize
        implicitHeight: btn.iconSize
        Image {
            anchors.centerIn: parent
            width: btn.iconSize
            height: btn.iconSize
            source: btn.iconSource
            sourceSize: Qt.size(32, 32)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: false
            opacity: btn.enabled ? 1.0 : 0.35
        }
    }
}
