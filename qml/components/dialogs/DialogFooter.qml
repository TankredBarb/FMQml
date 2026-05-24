import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: root

    implicitHeight: 56
    color: "transparent"

    default property alias content: footerRow.data

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.panelBorder
        opacity: 0.4
    }

    RowLayout {
        id: footerRow
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 12
    }
}
