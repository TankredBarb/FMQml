import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property bool panelActive: false
    default property alias contentData: body.data

    implicitWidth: 100
    implicitHeight: 100

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        color: "transparent"
        radius: Math.max(0, Theme.radiusMd - 2)
        clip: true

        Item {
            id: body
            anchors.fill: parent
        }
    }
}
