import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property bool panelActive: false
    readonly property bool showActiveHighlight: root.panelActive
    default property alias contentData: body.data

    implicitWidth: 100
    implicitHeight: 100

    Rectangle {
        anchors.fill: parent
        color: Theme.panelSurface
        radius: Theme.radiusMd
        border.color: root.showActiveHighlight ? Theme.activeAccent : Theme.panelBorder
        border.width: root.showActiveHighlight ? 3 : 1
    }

    Item {
        id: body
        anchors.fill: parent
        clip: true
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: "transparent"
        border.color: root.showActiveHighlight ? Theme.activeAccent : Theme.panelBorder
        border.width: root.showActiveHighlight ? 3 : 1
        z: 9999

        Behavior on border.color {
            ColorAnimation { duration: Theme.motionFast }
        }
        Behavior on border.width {
            NumberAnimation { duration: Theme.motionFast }
        }
    }
}
