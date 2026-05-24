import QtQuick
import QtQuick.Controls
import "../../style"

Rectangle {
    id: root

    property string text: ""
    property color textColor: Theme.textSecondary
    property color fillColor: Theme.panelSurface
    property color strokeColor: Theme.panelBorder
    property int horizontalPadding: 10
    property int badgeHeight: 18
    property int fontSize: 10
    property int fontWeight: Font.Medium
    property real letterSpacing: 0

    implicitWidth: badgeLabel.implicitWidth + root.horizontalPadding
    implicitHeight: root.badgeHeight
    radius: Theme.radiusSm
    color: root.fillColor
    border.color: root.strokeColor
    border.width: 1

    Label {
        id: badgeLabel
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        font.pixelSize: root.fontSize
        font.weight: root.fontWeight
        font.letterSpacing: root.letterSpacing
    }
}
