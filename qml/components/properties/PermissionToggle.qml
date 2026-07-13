import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Rectangle {
        id: permissionToggle

        property string text: ""
        property bool checked: false
        signal toggled(bool checked)

        implicitWidth: Math.max(74, toggleContent.implicitWidth + 18)
        implicitHeight: 30
        radius: Theme.radiusSm
        color: permissionMouse.containsMouse
               ? Theme.withAlpha(Theme.accent, permissionToggle.checked ? 0.22 : 0.10)
               : (permissionToggle.checked ? Theme.withAlpha(Theme.accent, 0.16) : Theme.panelSurfaceSoft)
        border.width: 1
        border.color: permissionToggle.checked
                      ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.70 : 0.55)
                      : Theme.panelBorder

        RowLayout {
            id: toggleContent
            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: permissionToggle.checked ? Theme.accent : "transparent"
                border.width: 1
                border.color: permissionToggle.checked ? Theme.accent : Theme.textSecondary

                Label {
                    anchors.centerIn: parent
                    visible: permissionToggle.checked
                    text: "✓"
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    color: Theme.accentText
                }
            }

            Label {
                text: permissionToggle.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.weight: permissionToggle.checked ? Font.DemiBold : Font.Normal
                color: permissionToggle.checked ? Theme.textPrimary : Theme.textSecondary
            }
        }

        MouseArea {
            id: permissionMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: permissionToggle.toggled(!permissionToggle.checked)
        }
    }

