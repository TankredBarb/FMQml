import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import FM
import "../../style"
import "../common"

ToolButton {
    id: root

    property string iconSource
    property string iconTone: "default"
    property bool isHighlighted: false
    property bool showIdleSurface: false
    property int iconSize: 18
    property bool svgRecolorEnabled: true
    property color svgRecolorColor: root.baseTone
    property bool svgRecolorStroke: true
    property bool svgRecolorFill: true
    property real activation: root.pressed ? 1 : (root.hovered ? 0.52 : 0)

    readonly property color baseTone: Theme.actionIconColor(root.iconTone)
    readonly property color iconColor: root.svgRecolorEnabled ? root.svgRecolorColor : root.baseTone
    readonly property bool useSvgRecolor: root.svgRecolorEnabled && root.iconSource.toLowerCase().endsWith(".svg")
    readonly property bool activeVisual: root.enabled && root.isHighlighted

    clip: true
    padding: 0
    implicitWidth: 32
    implicitHeight: 32

    background: FmIconButtonVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        activation: root.activation
        active: root.activeVisual
        focused: root.activeFocus
        showIdleSurface: root.showIdleSurface
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        accentColor: root.iconColor
    }

    contentItem: Item {
        implicitWidth: root.iconSize
        implicitHeight: root.iconSize

        RecolorSvgIcon {
            id: icon
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize
            sourcePath: root.iconSource
            recolorEnabled: root.useSvgRecolor
            recolorColor: root.svgRecolorColor
            recolorStroke: root.svgRecolorStroke
            recolorFill: root.svgRecolorFill
            cacheKey: "fm-icon-button"
            sourceSize: Qt.size(36, 36)
            visible: root.useSvgRecolor
            opacity: root.enabled ? 1.0 : 0.45
        }

        MultiEffect {
            anchors.fill: icon
            source: icon
            visible: !root.useSvgRecolor
            colorization: 1.0
            colorizationColor: root.iconColor
            opacity: root.enabled ? 1.0 : 0.45
        }
    }

    Behavior on activation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
