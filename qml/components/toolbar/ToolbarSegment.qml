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
    radius: Theme.radiusForSide(Math.min(width, height))
    color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.34 : 0.30)
    border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.36 : 0.24)
    border.width: 1

    RowLayout {
        id: contentRow
        anchors.fill: parent
        spacing: 0
    }
}
