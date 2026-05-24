import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: root

    property var controller
    property bool active: false

    implicitHeight: 26
    implicitWidth: selectionText.implicitWidth + 18
    radius: Theme.radiusMd
    visible: root.controller ? !root.controller.isDeviceRoot : false
    color: root.controller && root.controller.directoryModel.selectedCount > 0
           ? (root.active ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
           : Theme.withAlpha(Theme.panelBorder, 0.12)
    border.color: root.controller && root.controller.directoryModel.selectedCount > 0
                  ? (root.active ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                  : Theme.panelBorder
    border.width: 1

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Theme.radiusSm
        color: "transparent"
        border.color: root.controller && root.controller.directoryModel.selectedCount > 0
                      ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.10 : 0.14)
                      : "transparent"
        border.width: root.controller && root.controller.directoryModel.selectedCount > 0 ? 1 : 0
    }

    Text {
        id: selectionText
        anchors.centerIn: parent
        text: {
            if (root.controller && root.controller.directoryModel.selectedCount > 0) {
                return root.controller.directoryModel.selectedCount + " selected"
            }
            if (root.controller && root.controller.directoryModel) {
                return root.controller.directoryModel.count + " items"
            }
            return "0 items"
        }
        color: root.controller && root.controller.directoryModel.selectedCount > 0 ? Theme.accent : Theme.textSecondary
        font.pixelSize: 11
        font.bold: true
    }
}
