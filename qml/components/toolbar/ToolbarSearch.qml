import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../style"

Rectangle {
    id: root

    property var controller
    property var workspaceController
    readonly property bool editorActiveFocus: searchField.activeFocus

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    implicitWidth: searchField.activeFocus ? 200 : 140
    implicitHeight: 32
    radius: Theme.controlRadius
    color: Theme.panelSurfaceSoft
    border.color: searchField.activeFocus ? Theme.focusRing : Theme.withAlpha(Theme.border, 0.5)
    border.width: 1

    Behavior on implicitWidth {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutQuint
        }
    }

    Image {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        source: "../../assets/lucide-toolbar/search.svg"
        sourceSize: Qt.size(16, 16)
        smooth: true
        mipmap: false
        opacity: 0.8
    }

    PremiumTextField {
        id: searchField
        anchors.fill: parent
        anchors.leftMargin: 30
        anchors.rightMargin: 8
        placeholderText: "Search..."
        text: root.controller ? root.controller.directoryModel.filterText : ""
        onTextChanged: {
            if (root.controller) {
                root.controller.directoryModel.filterText = text
            }
        }
        background: null

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                text = ""
                if (root.controller) {
                    root.controller.directoryModel.filterText = ""
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
