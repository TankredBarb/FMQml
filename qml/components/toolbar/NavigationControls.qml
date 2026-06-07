import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../common"
import "../../style"

ToolbarSegment {
    id: root

    property var controller
    property var panelView
    property bool searchReturnVisible: false
    readonly property int searchReturnButtonWidth: 132

    signal searchReturnRequested()

    function goBack() {
        if (root.panelView && root.panelView.goBack) {
            root.panelView.goBack()
        } else if (root.controller) {
            root.controller.goBack()
        }
    }

    function goForward() {
        if (root.panelView && root.panelView.goForward) {
            root.panelView.goForward()
        } else if (root.controller) {
            root.controller.goForward()
        }
    }

    function goUp() {
        if (root.panelView && root.panelView.goUp) {
            root.panelView.goUp()
        } else if (root.controller) {
            root.controller.goUp()
        }
    }

    function prepareNavigation(reason) {
        if (root.panelView && root.panelView.cancelInlineRenameForNavigation) {
            root.panelView.cancelInlineRenameForNavigation(reason)
        }
    }

    segmentWidth: 32 * 3 + 2 + (root.searchReturnVisible ? root.searchReturnButtonWidth + 1 : 0)
    segmentHeight: 32

    Button {
        id: searchResultsBtn

        visible: root.searchReturnVisible
        enabled: visible
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        Layout.preferredWidth: root.searchReturnButtonWidth
        Layout.fillHeight: true
        padding: 0
        onClicked: root.searchReturnRequested()
        ToolTip.visible: hovered
        ToolTip.text: "Back to Search Results"

        contentItem: Item {
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7

                RecolorSvgIcon {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    sourcePath: "qrc:/qt/qml/FM/qml/assets/toolbar-next/search.svg"
                    sourceSize: Qt.size(16, 16)
                    recolorEnabled: true
                    recolorColor: Theme.readableOn(Theme.accent, Theme.accentText)
                }

                Label {
                    Layout.fillWidth: true
                    text: "Search Results"
                    color: Theme.readableOn(Theme.accent, Theme.accentText)
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        background: Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.radiusForSide(Math.min(width, height))
            color: searchResultsBtn.pressed
                   ? Theme.withAlpha(Theme.accent, 0.78)
                   : searchResultsBtn.hovered
                     ? Theme.withAlpha(Theme.accent, 0.94)
                     : Theme.accent
            border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.90 : 0.72)
            border.width: 1
        }
    }

    Rectangle {
        visible: root.searchReturnVisible
        width: visible ? 1 : 0
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
    }

    IconButton {
        id: backBtn
        iconSource: "../assets/toolbar-next/arrow-left.svg"
        iconTone: "back"
        enabled: root.controller ? root.controller.canGoBack : false
        onPressedChanged: if (pressed) root.prepareNavigation("toolbar-back-press")
        onClicked: root.goBack()
        ToolTip.visible: hovered
        ToolTip.text: "Back (Alt+Left)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusForSide(Math.min(width, height))
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
        color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
    }

    IconButton {
        id: forwardBtn
        iconSource: "../assets/toolbar-next/arrow-right.svg"
        iconTone: "forward"
        enabled: root.controller ? root.controller.canGoForward : false
        onPressedChanged: if (pressed) root.prepareNavigation("toolbar-forward-press")
        onClicked: root.goForward()
        ToolTip.visible: hovered
        ToolTip.text: "Forward (Alt+Right)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusForSide(Math.min(width, height))
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
        color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.28 : 0.20)
    }

    IconButton {
        id: upBtn
        iconSource: "../assets/toolbar-next/arrow-up.svg"
        iconTone: "up"
        enabled: !!root.controller
        onPressedChanged: if (pressed) root.prepareNavigation("toolbar-up-press")
        onClicked: root.goUp()
        ToolTip.visible: hovered
        ToolTip.text: "Up (Alt+Up)"
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            radius: Theme.radiusForSide(Math.min(width, height))
            color: upBtn.pressed ? Theme.surfaceActive : (upBtn.hovered ? Theme.withAlpha(upBtn.baseTone, themeController.isDark ? 0.14 : 0.10) : "transparent")
            anchors.fill: parent
            anchors.margins: 1
        }
    }
}
