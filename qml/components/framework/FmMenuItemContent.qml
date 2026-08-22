import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

RowLayout {
    id: contentRoot

    required property var control
    property bool showSubMenuArrow: false

    spacing: 6

    Item {
        Layout.preferredWidth: contentRoot.control.hasIconSource ? 14 : 0
        Layout.preferredHeight: 14
        visible: contentRoot.control.hasIconSource
        opacity: contentRoot.control.enabled ? 1.0 : 0.35

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: contentRoot.control.iconCircular ? width / 2 : 0
            clip: contentRoot.control.iconCircular

            Image {
                anchors.fill: parent
                source: contentRoot.control.iconFallbackSourceText
                sourceSize: Qt.size(16, 16)
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: contentRoot.control.iconFallbackSourceText.length > 0
                         && fallbackIcon.status !== Image.Ready
            }

            RecolorSvgIcon {
                anchors.fill: parent
                sourcePath: contentRoot.control.iconSourceText
                sourceSize: Qt.size(16, 16)
                recolorEnabled: contentRoot.control.useSvgRecolor
                recolorColor: contentRoot.control.iconColor
                cacheKey: "fm-menu-item"
                visible: contentRoot.control.useSvgRecolor
            }

            Image {
                id: fallbackIcon
                anchors.fill: parent
                source: contentRoot.control.useSvgRecolor ? "" : contentRoot.control.iconSourceText
                sourceSize: Qt.size(16, 16)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: false
                visible: !contentRoot.control.useSvgRecolor
                layer.enabled: contentRoot.control.hasIconSource
                               && !contentRoot.control.useSvgRecolor
                               && contentRoot.control.recolorEnabled
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: contentRoot.control.iconColor
                }
            }
        }
    }

    Label {
        text: contentRoot.control.displayText
        color: !contentRoot.control.enabled ? Theme.textSecondary
               : contentRoot.control.destructive
                 && (contentRoot.control.hovered || contentRoot.control.highlighted) ? Theme.danger
               : Theme.textPrimary
        font.family: contentRoot.control.font.family
        font.pixelSize: contentRoot.control.font.pixelSize > 0
                        ? contentRoot.control.font.pixelSize : Theme.fontSizeLabel
        font.weight: contentRoot.control.active ? Font.DemiBold : Font.Normal
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        Layout.fillWidth: true
    }

    Label {
        text: contentRoot.control.displayShortcut
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeMicro
        font.italic: true
        verticalAlignment: Text.AlignVCenter
        visible: contentRoot.control.displayShortcut.length > 0
        Layout.alignment: Qt.AlignRight
    }

    RecolorSvgIcon {
        Layout.preferredWidth: 12
        Layout.preferredHeight: 12
        sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-right.svg"
        sourceSize: Qt.size(12, 12)
        recolorColor: Theme.textSecondary
        cacheKey: "fm-menu-submenu-arrow"
        visible: contentRoot.showSubMenuArrow
        opacity: contentRoot.control.enabled ? 1.0 : 0.35
    }
}
