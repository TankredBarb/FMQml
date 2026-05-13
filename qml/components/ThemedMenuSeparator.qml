import QtQuick
import QtQuick.Controls
import "../style"

MenuSeparator {
    id: root

    implicitHeight: 8
    padding: 0

    contentItem: Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 10
        height: 1
        radius: 0.5

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0
                color: Qt.alpha(Theme.menuSeparator, themeController.isDark ? 0.35 : 0.4)
            }
            GradientStop {
                position: 0.5
                color: Theme.menuSeparator
            }
            GradientStop {
                position: 1
                color: Qt.alpha(Theme.menuSeparator, themeController.isDark ? 0.35 : 0.4)
            }
        }
    }
}
