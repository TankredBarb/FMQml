import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../framework"

FmButton {
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
        primaryColor: actionPill.accentColor

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

    }
