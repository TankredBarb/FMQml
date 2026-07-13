import QtQuick
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: block

    default property alias content: blockContent.data

    Layout.fillWidth: true
    implicitHeight: blockContent.implicitHeight + 16
    radius: Theme.radiusSm
    color: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.30 : 0.52)
    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.30 : 0.24)
    border.width: 1

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 7
        width: 2
        radius: 1
        color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.80 : 0.68)
    }

    ColumnLayout {
        id: blockContent
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 6
    }
}
