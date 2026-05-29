import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../style"

ToolButton {
    id: btn
    property string iconSource
    property string iconTone: "default"
    property bool isHighlighted: false
    property int iconSize: 18

    function toneBase(role) {
        switch (String(role)) {
        case "back":
            return "#38bdf8"
        case "forward":
            return "#a78bfa"
        case "up":
            return "#22d3ee"
        case "view":
        case "view-grid":
            return "#a855f7"
        case "view-details":
            return "#22c55e"
        case "view-brief":
            return "#06b6d4"
        case "hidden":
            return "#34d399"
        case "refresh":
            return "#2dd4bf"
        case "copy":
            return "#60a5fa"
        case "move":
            return "#fbbf24"
        case "folder":
            return "#4ade80"
        case "split":
            return "#c084fc"
        case "theme":
            return "#fbbf24"
        case "info":
            return "#38bdf8"
        case "search":
            return "#94a3b8"
        case "filter":
            return "#14b8a6"
        default:
            return Theme.accent
        }
    }

    readonly property color baseTone: toneBase(btn.iconTone)
    readonly property color iconColor: baseTone
    readonly property color hoverFill: {
        if (btn.pressed) {
            return Theme.surfaceActive
        }
        if (btn.hovered || btn.isHighlighted) {
            return Theme.withAlpha(baseTone, themeController.isDark ? 0.22 : 0.18)
        }
        return "transparent"
    }
    readonly property color hoverBorder: (btn.hovered || btn.isHighlighted)
        ? Theme.withAlpha(baseTone, themeController.isDark ? 0.52 : 0.40)
        : "transparent"
    clip: true
    padding: 0
    
    implicitWidth: 32
    implicitHeight: 32
    
    background: Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Theme.radiusSm
        color: btn.hoverFill
        border.color: btn.hoverBorder
        border.width: btn.hovered || btn.isHighlighted || btn.pressed ? 1 : 0
    }
    
    contentItem: Item {
        implicitWidth: btn.iconSize
        implicitHeight: btn.iconSize
        Image {
            id: iconMask
            anchors.centerIn: parent
            width: btn.iconSize
            height: btn.iconSize
            source: btn.iconSource
            sourceSize: Qt.size(36, 36)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: false
            visible: false
        }

        MultiEffect {
            anchors.fill: iconMask
            source: iconMask
            colorization: 1.0
            colorizationColor: btn.iconColor
            opacity: btn.enabled ? 1.0 : 0.45
        }
    }
}
