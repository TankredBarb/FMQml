import QtQuick
import QtQuick.Controls
import FM
import "../../style"

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
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90; easing.type: Easing.InQuad }
            NumberAnimation { property: "scale"; from: 1; to: 0.98; duration: 90; easing.type: Easing.InQuad }
        }
    }

    background: FmMenuVisual {
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        surfaceColor: Theme.menuSurface
        borderColor: Theme.menuBorder
        accentColor: Theme.accent
        shadowColor: Theme.shadow
        dark: themeController.isDark
    }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: root.contentModel
        clip: true
        interactive: root.height >= (Window.window ? Window.window.height : Screen.height)
        currentIndex: root.currentIndex
        spacing: 0

        ScrollIndicator.vertical: ScrollIndicator {}
    }
}
