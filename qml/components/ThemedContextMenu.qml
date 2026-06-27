import QtQuick
import QtQuick.Controls
import "../style"

Menu {
    id: root
    z: 999

    implicitWidth: 255

    padding: 2
    topPadding: 3
    bottomPadding: 3

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: false

    transformOrigin: Item.TopLeft

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 150
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.96
                to: 1
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 90
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                property: "scale"
                from: 1
                to: 0.98
                duration: 90
                easing.type: Easing.InQuad
            }
        }
    }

    background: Item {

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.bottomMargin: 1
            radius: Theme.radiusLg
            color: Theme.shadow
            opacity: themeController.isDark ? 0.28 : 0.10
        }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            radius: Theme.innerRadius(Theme.radiusLg, 1)
            color: Theme.accent
            opacity: themeController.isDark ? 0.08 : 0.04
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.menuSurface
            radius: Theme.radiusLg
            border.color: Theme.withAlpha(Theme.menuBorder, themeController.isDark ? 0.40 : 0.28)
            border.width: 1
            antialiasing: true
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            height: 1
            radius: 0.5
            color: Theme.withAlpha(Theme.accentText, themeController.isDark ? 0.045 : 0.14)
        }
    }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: root.contentModel
        clip: true
        interactive: root.height >= (Window.window ? Window.window.height : screen.height)
        currentIndex: root.currentIndex
        spacing: 0

        ScrollIndicator.vertical: ScrollIndicator {}
    }
}
