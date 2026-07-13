import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Button {
        id: actionPill

        property color accentColor: Theme.accent
        property string iconSource: ""
        property int pillWidth: 64

        implicitWidth: Math.max(actionPill.pillWidth, actionContent.implicitWidth + 18)
        implicitHeight: Math.max(30, actionContent.implicitHeight + 10)
        width: implicitWidth
        height: implicitHeight
        padding: 0
        hoverEnabled: true

        contentItem: RowLayout {
            id: actionContent
            spacing: 5

            Item { Layout.fillWidth: true }

            RecolorSvgIcon {
                Layout.preferredWidth: 13
                Layout.preferredHeight: 13
                visible: actionPill.iconSource.length > 0
                sourcePath: actionPill.iconSource
                recolorColor: actionPill.enabled ? actionPill.accentColor : Theme.textSecondary
                sourceSize.width: 13
                sourceSize.height: 13
                opacity: actionPill.enabled ? 0.95 : 0.45
            }

            Label {
                text: actionPill.text
                color: actionPill.enabled ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Item { Layout.fillWidth: true }
        }

        background: Rectangle {
            implicitHeight: actionPill.implicitHeight
            radius: Theme.radiusSm
            color: !actionPill.enabled
                   ? Theme.withAlpha(Theme.panelBorder, 0.45)
                   : (actionPill.pressed
                      ? Theme.surfaceActive
                      : (actionPill.hovered ? Theme.panelSurfaceSoft : Theme.panelSurface))
            border.color: Theme.withAlpha(actionPill.enabled ? actionPill.accentColor : Theme.panelBorder,
                                          actionPill.hovered ? 0.72 : (actionPill.enabled ? 0.46 : 0.55))
            border.width: 1

            Rectangle {
                visible: actionPill.enabled
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 3
                radius: 2
                color: actionPill.accentColor
                opacity: actionPill.hovered || actionPill.pressed ? 0.9 : 0.48
            }
        }
    }

