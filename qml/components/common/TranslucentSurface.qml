import QtQuick
import QtQuick.Effects
import "../../style"

Item {
    id: root

    property bool translucent: true
    property bool active: visible
    property bool backgroundBlurEnabled: false
    property int blurStrength: typeof appSettings !== "undefined" && appSettings
                               ? appSettings.surfaceBlurStrength
                               : 72
    property var backdropSource: null
    property var backdropTransformItem: null
    property int cornerRadius: Theme.panelRadius
    property color baseColor: Theme.panelSurface
    property color startColor: Theme.chromeGradientStart
    property color midColor: Theme.chromeGradientMid
    property color endColor: Theme.panelSurface
    property real gradientStrength: 0.62
    property color borderColor: Theme.panelBorder
    property int borderWidth: 1
    property color highlightColor: "transparent"
    property color shadowColor: Theme.glassShadow
    property int shadowBlur: 28
    property int shadowVerticalOffset: 12
    property bool shadowEnabled: true
    property bool backdropUpdatePending: false
    readonly property int backdropPadding: 32
    readonly property bool blurActive: translucent && backgroundBlurEnabled && backdropSource

    function refreshBackdrop() {
        if (!root.active || !root.blurActive || root.backdropUpdatePending) {
            return
        }
        root.backdropUpdatePending = true
        Qt.callLater(() => {
            root.backdropUpdatePending = false
            if (root.active && root.blurActive) {
                backdropTexture.scheduleUpdate()
            }
        })
    }

    onActiveChanged: root.refreshBackdrop()
    onBlurActiveChanged: root.refreshBackdrop()
    onXChanged: root.refreshBackdrop()
    onYChanged: root.refreshBackdrop()
    onWidthChanged: root.refreshBackdrop()
    onHeightChanged: root.refreshBackdrop()

    ShaderEffectSource {
        id: backdropTexture
        width: root.width + root.backdropPadding * 2
        height: root.height + root.backdropPadding * 2
        visible: false
        sourceItem: root.active && root.blurActive ? root.backdropSource : null
        sourceRect: {
            if (!root.backdropSource) return Qt.rect(0, 0, 0, 0)
            if (root.backdropTransformItem) {
                root.backdropTransformItem.x
                root.backdropTransformItem.y
                root.backdropTransformItem.scale
            }
            root.x
            root.y
            root.width
            root.height
            root.backdropSource.x
            root.backdropSource.y
            root.backdropSource.width
            root.backdropSource.height
            const origin = root.mapToItem(root.backdropSource, 0, 0)
            return Qt.rect(origin.x - root.backdropPadding,
                           origin.y - root.backdropPadding,
                           backdropTexture.width,
                           backdropTexture.height)
        }
        textureSize: Qt.size(Math.max(1, Math.round(width)),
                             Math.max(1, Math.round(height)))
        live: false
        smooth: true

        onSourceItemChanged: root.refreshBackdrop()
        onSourceRectChanged: root.refreshBackdrop()
    }

    Item {
        id: backdropMask
        width: backdropTexture.width
        height: backdropTexture.height
        visible: false
        layer.enabled: true

        Rectangle {
            x: root.backdropPadding
            y: root.backdropPadding
            width: root.width
            height: root.height
            radius: root.cornerRadius
        }
    }

    MultiEffect {
        x: -root.backdropPadding
        y: -root.backdropPadding
        width: backdropTexture.width
        height: backdropTexture.height
        visible: root.blurActive
        source: backdropTexture
        blurEnabled: true
        blur: Math.max(0, Math.min(1, root.blurStrength / 100.0))
        blurMax: 64
        saturation: -0.12
        maskEnabled: true
        maskSource: backdropMask
        autoPaddingEnabled: false
    }

    AmbientPanelBackground {
        anchors.fill: parent
        cornerRadius: root.cornerRadius
        baseColor: root.baseColor
        startColor: root.startColor
        midColor: root.midColor
        endColor: root.endColor
        strength: root.gradientStrength
        border.color: root.borderColor
        border.width: root.borderWidth

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: root.cornerRadius
            anchors.rightMargin: root.cornerRadius
            anchors.topMargin: 1
            height: 1
            radius: 0.5
            visible: root.translucent
            color: root.highlightColor
        }

        layer.enabled: root.shadowEnabled
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: root.shadowColor
            shadowBlur: root.shadowBlur
            shadowVerticalOffset: root.shadowVerticalOffset
        }
    }
}
