import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../framework"

Rectangle {
        id: specialToggle

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        property color accentColor: Theme.warning
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: Math.max(46, specialContent.implicitHeight + 12)
        radius: Theme.radiusSm
        color: Theme.withAlpha(Theme.panelSurfaceSoft, specialToggle.checked ? 0.82 : 0.58)
        border.width: 1
        border.color: specialToggle.checked
                      ? Theme.withAlpha(specialToggle.accentColor, 0.46)
                      : Theme.withAlpha(Theme.panelBorder, 0.38)

        RowLayout {
            id: specialContent
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10

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

            FmSwitch {
                checked: specialToggle.checked
                accentColor: specialToggle.accentColor
                onToggled: specialToggle.toggled(checked)
            }
        }
    }
