import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import ".."
import "../common"
import "../../style"

Rectangle {
    id: root

    property var controller
    property var workspaceController
    readonly property bool editorActiveFocus: searchField.activeFocus

    function focusSearch(initialText) {
        searchField.forceActiveFocus()
        if (initialText !== undefined && initialText !== null && String(initialText).length > 0) {
            const text = String(initialText)
            if (root.controller) {
                root.controller.directoryModel.searchText = text
            } else {
                searchField.text = text
            }
            searchField.cursorPosition = text.length
        } else {
            searchField.selectAll()
        }
        return true
    }

    function switchPanelByTab() {
        if (!root.workspaceController || !root.workspaceController.splitEnabled) {
            return false
        }
        root.workspaceController.activePanel = root.workspaceController.activePanel === 0 ? 1 : 0
        root.workspaceController.focusActivePanel()
        return true
    }

    implicitWidth: searchField.activeFocus ? 200 : 140
    implicitHeight: 32
    radius: Theme.controlRadius
    color: searchField.activeFocus
           ? Theme.panelSurfaceStrong
           : (searchHover.hovered
              ? Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.92 : 0.98)
              : Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.74 : 0.90))
    border.color: searchField.activeFocus
                  ? Theme.withAlpha(Theme.focusRing, themeController.isDark ? 0.86 : 0.76)
                  : (searchHover.hovered
                     ? Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.58 : 0.50)
                     : Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.42 : 0.36))
    border.width: 1

    Behavior on implicitWidth {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutQuint
        }
    }

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    HoverHandler {
        id: searchHover
    }

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Theme.glassShadow
        shadowBlur: 10 + (searchField.activeFocus ? 4 : 0)
        shadowVerticalOffset: 2 + (searchField.activeFocus ? 2 : 0)
    }

    Rectangle {
        id: editGlow
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        visible: searchField.activeFocus
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.categoryInfo, 0.14) }
            GradientStop { position: 0.42; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.warmAccent, 0.06) }
        }
    }

    RecolorSvgIcon {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        sourcePath: "../../assets/toolbar-next/search.svg"
        sourceSize: Qt.size(16, 16)
        recolorEnabled: true
        recolorColor: Theme.actionIconColor("search")
        cacheKey: "toolbar-search"
        opacity: 0.8
    }

    PremiumTextField {
        id: searchField
        anchors.fill: parent
        anchors.leftMargin: 30
        anchors.rightMargin: 8
        placeholderText: "Search..."
        text: root.controller ? root.controller.directoryModel.searchText : ""
        onTextChanged: {
            if (root.controller) {
                root.controller.directoryModel.searchText = text
            }
        }
        background: null

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Tab) {
                event.accepted = root.switchPanelByTab()
            } else if (event.key === Qt.Key_Escape) {
                text = ""
                if (root.controller) {
                    root.controller.directoryModel.searchText = ""
                }
                if (root.workspaceController) {
                    root.workspaceController.focusActivePanel()
                }
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (root.workspaceController) {
                    root.workspaceController.focusActivePanel()
                }
                event.accepted = true
            }
        }
    }
}
