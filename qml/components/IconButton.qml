import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../style"
import "common"

ToolButton {
    id: btn
    property string iconSource
    property string iconTone: "default"
    property bool isHighlighted: false
    property int iconSize: 18
    property bool svgRecolorEnabled: true
    property color svgRecolorColor: baseTone
    property bool svgRecolorStroke: true
    property bool svgRecolorFill: true

    readonly property color baseTone: Theme.actionIconColor(btn.iconTone)
    readonly property color iconColor: svgRecolorEnabled ? svgRecolorColor : baseTone
    readonly property bool useSvgRecolor: svgRecolorEnabled && iconSource.toLowerCase().endsWith(".svg")
    readonly property color hoverFill: {
        if (btn.pressed) {
            return Theme.surfaceActive
        }
        if (btn.hovered || btn.isHighlighted) {
            return Theme.withAlpha(iconColor, themeController.isDark ? 0.22 : 0.18)
        }
        return "transparent"
    }
    readonly property color hoverBorder: (btn.hovered || btn.isHighlighted)
        ? Theme.withAlpha(iconColor, themeController.isDark ? 0.52 : 0.40)
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
        RecolorSvgIcon {
            id: icon
            anchors.centerIn: parent
            width: btn.iconSize
            height: btn.iconSize
            sourcePath: btn.iconSource
            recolorEnabled: btn.useSvgRecolor
            recolorColor: btn.svgRecolorColor
            recolorStroke: btn.svgRecolorStroke
            recolorFill: btn.svgRecolorFill
            cacheKey: "icon-button"
            sourceSize: Qt.size(36, 36)
            visible: btn.useSvgRecolor
            opacity: btn.useSvgRecolor && !btn.enabled ? 0.45 : 1.0
        }

        MultiEffect {
            anchors.fill: icon
            source: icon
            visible: !btn.useSvgRecolor
            colorization: 1.0
            colorizationColor: btn.iconColor
            opacity: btn.enabled ? 1.0 : 0.45
        }
    }
}
