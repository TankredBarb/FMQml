import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

ToolbarSegment {
    id: root

    property var controller

    segmentWidth: 32 * 3 + 2
    segmentHeight: 32

    IconButton {
        id: backBtn
        iconSource: "../assets/lucide-toolbar/arrow-left.svg"
        iconTone: "back"
        enabled: root.controller ? root.controller.canGoBack : false
        onClicked: root.controller.goBack()
        ToolTip.visible: hovered
        ToolTip.text: "Back (Alt+Left)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: backBtn.pressed ? Theme.surfaceActive : (backBtn.hovered ? Theme.withAlpha(backBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }

    Rectangle {
        width: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.withAlpha(Theme.border, 0.35)
    }

    IconButton {
        id: forwardBtn
        iconSource: "../assets/lucide-toolbar/arrow-right.svg"
        iconTone: "forward"
        enabled: root.controller ? root.controller.canGoForward : false
        onClicked: root.controller.goForward()
        ToolTip.visible: hovered
        ToolTip.text: "Forward (Alt+Right)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: forwardBtn.pressed ? Theme.surfaceActive : (forwardBtn.hovered ? Theme.withAlpha(forwardBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }

    Rectangle {
        width: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.withAlpha(Theme.border, 0.35)
    }

    IconButton {
        id: upBtn
        iconSource: "../assets/lucide-toolbar/arrow-up.svg"
        iconTone: "up"
        enabled: !!root.controller
        onClicked: root.controller.goUp()
        ToolTip.visible: hovered
        ToolTip.text: "Up (Alt+Up)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: upBtn.pressed ? Theme.surfaceActive : (upBtn.hovered ? Theme.withAlpha(upBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }
}
