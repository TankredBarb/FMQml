import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../framework"

Item {
    id: root

    default property alias content: contentColumn.data

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        Component.onCompleted: contentItem.pixelAligned = true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: FmScrollBar {
            id: verticalScrollBar
            parent: scrollView.contentItem
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            policy: ScrollBar.AsNeeded
        }

        Pane {
            width: verticalScrollBar.scrollNeeded
                   ? Math.max(0, verticalScrollBar.x - 6)
                   : scrollView.availableWidth
            padding: 16
            background: null

            ColumnLayout {
                id: contentColumn
                width: parent.width
                spacing: 12
            }
        }
    }
}
