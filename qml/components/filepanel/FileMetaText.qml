import QtQuick
import QtQuick.Controls
import "../../style"

Item {
    id: root

    property string value: ""
    property bool loading: false
    property int fontPixelSize: Theme.fontSizeLabel
    property int leftMargin: 4
    property int rightMargin: 4
    property color textColor: Theme.textSecondary
    property real textOpacity: 0.85

    Label {
        anchors.fill: parent
        anchors.leftMargin: root.leftMargin
        anchors.rightMargin: root.rightMargin
        text: root.value.length > 0 ? root.value : (root.loading ? "…" : "")
        color: root.textColor
        opacity: root.textOpacity
        font.family: Theme.fontFamily
        font.pixelSize: root.fontPixelSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
