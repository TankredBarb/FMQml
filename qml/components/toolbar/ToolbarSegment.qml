import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: root

    default property alias content: contentRow.data
    property int segmentWidth: 32
    property int segmentHeight: 32

    implicitWidth: segmentWidth
    implicitHeight: segmentHeight
    radius: Theme.radiusSm
    color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.18)
    border.color: Theme.withAlpha(Theme.border, 0.85)
    border.width: 1

    RowLayout {
        id: contentRow
        anchors.fill: parent
        spacing: 0
    }
}
