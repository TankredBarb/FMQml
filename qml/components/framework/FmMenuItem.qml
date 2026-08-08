import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../../style"
import "../common"

MenuItem {
    id: root

    implicitWidth: 245
    implicitHeight: visible ? 26 : 0
    clip: true

    property bool destructive: false
    property bool active: false
    property string shortcut: ""
    property color iconColor: destructive ? Theme.danger : Theme.textSecondary
    property bool recolorEnabled: true
    property url iconFallbackSource: ""
    property bool iconCircular: false
    property bool useHighlightedState: false
    property real paintActivation: root.down ? 1
                                          : (root.hovered || (root.useHighlightedState && root.highlighted) ? 1 : 0)

    readonly property string displayText: root.text === undefined || root.text === null ? "" : String(root.text)
    readonly property string displayShortcut: root.shortcut
    function resolvedSource(value) {
        const source = value ? value.toString() : ""
        const misplacedQrcPrefix = "qrc:/qt/qml/FM/qml/components/assets/"
        if (source.indexOf(misplacedQrcPrefix) === 0) {
            return "qrc:/qt/qml/FM/qml/assets/" + source.slice(misplacedQrcPrefix.length)
        }
        return source.indexOf("../assets/") === 0 ? "../../assets/" + source.slice(10) : source
    }

    readonly property string iconSourceText: root.resolvedSource(root.icon && root.icon.source
                                                                  ? root.icon.source : "")
    readonly property string iconFallbackSourceText: root.resolvedSource(root.iconFallbackSource)
    readonly property bool hasIconSource: iconSourceText.length > 0
    readonly property bool useSvgRecolor: hasIconSource
                                           && iconSourceText.toLowerCase().endsWith(".svg")
                                           && root.recolorEnabled

    background: FmMenuItemVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        activation: root.paintActivation
        pressed: root.down
        hoverColor: Theme.menuItemHover
        pressedColor: Theme.menuItemPressed
        accentColor: root.active ? root.iconColor : (root.destructive ? Theme.danger : Theme.accent)
    }

    contentItem: RowLayout {
        spacing: 6

        Item {
            Layout.preferredWidth: root.hasIconSource ? 14 : 0
            Layout.preferredHeight: 14
            visible: root.hasIconSource
            opacity: root.enabled ? 1.0 : 0.35

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                radius: root.iconCircular ? width / 2 : 0
                clip: root.iconCircular

                Image {
                    anchors.fill: parent
                    source: root.iconFallbackSourceText
                    sourceSize: Qt.size(16, 16)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: root.iconFallbackSourceText.length > 0
                             && fallbackIcon.status !== Image.Ready
                }

                RecolorSvgIcon {
                    anchors.fill: parent
                    sourcePath: root.iconSourceText
                    sourceSize: Qt.size(16, 16)
                    recolorEnabled: root.useSvgRecolor
                    recolorColor: root.iconColor
                    cacheKey: "fm-menu-item"
                    visible: root.useSvgRecolor
                }

                Image {
                    id: fallbackIcon
                    anchors.fill: parent
                    source: root.useSvgRecolor ? "" : root.iconSourceText
                    sourceSize: Qt.size(16, 16)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: false
                    visible: !root.useSvgRecolor
                    layer.enabled: root.hasIconSource && !root.useSvgRecolor && root.recolorEnabled
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: root.iconColor
                    }
                }
            }
        }

        Label {
            text: root.displayText
            color: !root.enabled ? Theme.textSecondary
                   : root.destructive && (root.hovered || root.highlighted) ? Theme.danger
                   : Theme.textPrimary
            font.family: root.font.family
            font.pixelSize: root.font.pixelSize > 0 ? root.font.pixelSize : Theme.fontSizeLabel
            font.weight: root.active ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Label {
            text: root.displayShortcut
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeMicro
            font.italic: true
            verticalAlignment: Text.AlignVCenter
            visible: root.displayShortcut.length > 0
            Layout.alignment: Qt.AlignRight
        }
    }

    Behavior on paintActivation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
