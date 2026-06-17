import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"

MenuItem {
    id: root

    implicitWidth: 196
    implicitHeight: visible ? 26 : 0
    clip: true

    property bool destructive: false
    property bool active: false
    property string shortcut: ""
    property color iconColor: destructive ? Theme.danger : Theme.textSecondary

    readonly property string displayText: {
        const t = root.text
        return (t === undefined || t === null) ? "" : String(t)
    }
    readonly property string displayShortcut: {
        return root.shortcut
    }

    readonly property color hoverFill: Theme.menuItemHover
    readonly property color pressedFill: Theme.menuItemPressed
    readonly property string iconSourceText: {
        if (!root.icon || !root.icon.source) {
            return ""
        }
        return root.icon.source.toString()
    }
    readonly property bool hasIconSource: iconSourceText.length > 0
    readonly property bool useSvgRecolor: hasIconSource && iconSourceText.toLowerCase().endsWith(".svg")

    background: Item {
        anchors.fill: parent
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        anchors.topMargin: 0
        anchors.bottomMargin: 0
        clip: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 0
            color: !root.enabled
                    ? "transparent"
                    : root.down
                            ? root.pressedFill
                            : root.hovered
                                    ? root.hoverFill
                                    : "transparent"
            radius: 2
        }

        Rectangle {
            width: 3
            height: Math.max(10, Math.round(parent.height * 0.58))
            radius: 2
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            color: root.destructive ? Theme.danger : Theme.accent
            opacity: root.enabled && root.active ? 0.85 : (root.enabled && (root.hovered || root.down) ? 1 : 0)
        }
    }

    contentItem: RowLayout {
        spacing: 6
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        Item {
            id: menuIcon
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            visible: root.hasIconSource
            opacity: root.enabled ? 1.0 : 0.35

            RecolorSvgIcon {
                id: recoloredIcon
                anchors.fill: parent
                sourcePath: root.iconSourceText
                sourceSize: Qt.size(16, 16)
                recolorEnabled: root.useSvgRecolor
                recolorColor: root.iconColor
                cacheKey: "themed-menu-item"
                visible: root.useSvgRecolor
            }

            Image {
                id: fallbackIcon
                anchors.fill: parent
                source: root.iconSourceText
                sourceSize: Qt.size(16, 16)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: false
                visible: !root.useSvgRecolor
                layer.enabled: root.hasIconSource && !root.useSvgRecolor
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: root.iconColor
                }
            }
        }

        Label {
            text: root.displayText
            color: !root.enabled ? Theme.textSecondary
                   : (root.destructive && root.hovered) ? Theme.danger : Theme.textPrimary
            font.pixelSize: Theme.fontSizeLabel
            font.letterSpacing: 0
            font.weight: root.highlighted || root.active ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Label {
            text: root.displayShortcut
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeMicro
            font.letterSpacing: 0
            font.italic: true
            verticalAlignment: Text.AlignVCenter
            visible: root.displayShortcut.length > 0
            Layout.alignment: Qt.AlignRight
        }
    }
}
