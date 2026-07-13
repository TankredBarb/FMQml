import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Rectangle {
        id: specialToggle

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: Math.max(46, specialContent.implicitHeight + 12)
        radius: Theme.radiusSm
        color: specialMouse.containsMouse
               ? Theme.withAlpha(Theme.warning, specialToggle.checked ? 0.20 : 0.10)
               : (specialToggle.checked ? Theme.withAlpha(Theme.warning, 0.14) : Theme.panelSurfaceSoft)
        border.width: 1
        border.color: specialToggle.checked
                      ? Theme.withAlpha(Theme.warning, themeController.isDark ? 0.70 : 0.52)
                      : Theme.panelBorder

        RowLayout {
            id: specialContent
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 9
                color: specialToggle.checked ? Theme.warning : "transparent"
                border.width: 1
                border.color: specialToggle.checked ? Theme.warning : Theme.textSecondary

                Label {
                    anchors.centerIn: parent
                    visible: specialToggle.checked
                    text: "!"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Bold
                    color: Theme.readableOn(Theme.warning, Theme.textPrimary)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    text: specialToggle.title
                    Layout.fillWidth: true
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Label {
                    text: specialToggle.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption - 1
                    color: Theme.textSecondary
                }
            }
        }

        MouseArea {
            id: specialMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: specialToggle.toggled(!specialToggle.checked)
        }
    }

