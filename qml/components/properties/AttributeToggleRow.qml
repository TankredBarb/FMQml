import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../framework"

Rectangle {
        id: row

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        property bool toggleEnabled: true
        property color accentColor: Theme.accent
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: Math.max(54, rowLayout.implicitHeight + 12)
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: row.checked
                      ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.42 : 0.34)
                      : Theme.panelBorder
        border.width: 1
        opacity: row.toggleEnabled ? 1.0 : 0.55

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: row.title
                    Layout.fillWidth: true
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: row.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                    visible: text.length > 0
                }
            }

            FmSwitch {
                id: switchControl
                checked: row.checked
                enabled: row.toggleEnabled
                accentColor: row.accentColor
                Layout.preferredWidth: 46
                Layout.preferredHeight: 26
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            enabled: row.toggleEnabled
            cursorShape: row.toggleEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: row.toggled(!row.checked)
        }
    }
