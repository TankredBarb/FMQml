import "../style"
import QtQuick
import QtQuick.Controls
import "common"
import "framework"

FmButton {
    id: modeButton

    property string title: ""
    property bool selected: false
    property color accentColor: Theme.accent

    text: title
    implicitHeight: 34
    implicitWidth: 88
    highlighted: modeButton.selected
    primaryColor: modeButton.accentColor

    contentItem: Label {
        text: modeButton.text
        opacity: modeButton.enabled ? 1 : 0.55
        color: modeButton.selected ? Theme.accentText : Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: Theme.fontSizeLabel
        font.weight: modeButton.selected ? Font.DemiBold : Font.Medium
    }

}
