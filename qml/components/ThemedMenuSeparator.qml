import QtQuick
import QtQuick.Controls
import "../style"

MenuSeparator {
    id: root

    implicitHeight: visible ? 10 : 0
    padding: 0

    contentItem: Item {
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.max(0, parent.width - 14)
            height: 1
            radius: 0.5
            color: Theme.withAlpha(Theme.menuSeparator, themeController.isDark ? 0.66 : 0.52)
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenterOffset: 1
            width: Math.max(0, parent.width - 24)
            height: 1
            radius: 0.5
            color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.055 : 0.04)
        }
    }
}
