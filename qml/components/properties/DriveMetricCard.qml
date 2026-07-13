import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Rectangle {
        required property string label
        required property string value
        property string subtext: ""
        property color accentColor: Theme.accent

        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        implicitHeight: Math.max(76, metricContent.implicitHeight + 28)
        radius: Theme.radiusLg
        color: Theme.withAlpha(accentColor, themeController.isDark ? 0.13 : 0.08)
        border.color: Theme.withAlpha(accentColor, themeController.isDark ? 0.28 : 0.18)
        border.width: 1

        ColumnLayout {
            id: metricContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 3

            Label {
                text: label
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMicro
                font.bold: true
                font.letterSpacing: 0
                color: Theme.withAlpha(accentColor, 0.95)
            }

            Label {
                text: value
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.scaledSize(19)
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                visible: subtext.length > 0
                text: subtext
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
        }
    }

