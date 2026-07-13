import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

RowLayout {
        required property string label
        required property string value
        property color valueColor: Theme.textPrimary

        Layout.fillWidth: true
        spacing: 12

        Label {
            text: label
            Layout.preferredWidth: 112
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            color: Theme.textSecondary
            elide: Text.ElideRight
        }

        Label {
            text: value
            Layout.fillWidth: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Medium
            color: valueColor
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideMiddle
        }
    }

