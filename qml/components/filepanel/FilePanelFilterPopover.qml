import QtQuick
import ".."
import "../../style"

Item {
    id: root

    property var controller
    readonly property var directoryModel: controller ? controller.directoryModel : null

    function labelFor(value, label) {
        if (controller && controller.categoryFilter === value) {
            return label + "  [Active]"
        }
        return label
    }

    function openAt(item) {
        if (item) {
            filterMenu.popup(item, 0, item.height + 8)
        } else {
            filterMenu.popup()
        }
    }

    ThemedContextMenu {
        id: filterMenu

        ThemedMenuItem {
            text: root.labelFor(0, "All Files")
            icon.source: "../assets/icons-classic/funnel.svg"
            iconColor: controller && controller.categoryFilter === 0 ? Theme.actionIconColor("filter") : Theme.actionIconColor("muted")
            onTriggered: if (controller) controller.setCategoryFilter(0)
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: root.labelFor(1, "Executables")
            icon.source: "../assets/icons-classic/terminal.svg"
            iconColor: controller && controller.categoryFilter === 1 ? Theme.actionIconColor("filter") : Theme.actionIconColor("terminal")
            onTriggered: if (controller) controller.setCategoryFilter(1)
        }
        ThemedMenuItem {
            text: root.labelFor(2, "Libraries")
            icon.source: "../assets/icons-classic/info.svg"
            iconColor: controller && controller.categoryFilter === 2 ? Theme.actionIconColor("filter") : Theme.actionIconColor("info")
            onTriggered: if (controller) controller.setCategoryFilter(2)
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: root.labelFor(3, "Images")
            icon.source: "../assets/icons-classic/image.svg"
            iconColor: controller && controller.categoryFilter === 3 ? Theme.actionIconColor("filter") : Theme.actionIconColor("image")
            onTriggered: if (controller) controller.setCategoryFilter(3)
        }
        ThemedMenuItem {
            text: root.labelFor(4, "Archives")
            icon.source: "../assets/icons-classic/archive.svg"
            iconColor: controller && controller.categoryFilter === 4 ? Theme.actionIconColor("filter") : Theme.actionIconColor("archive")
            onTriggered: if (controller) controller.setCategoryFilter(4)
        }
        ThemedMenuItem {
            text: root.labelFor(5, "Media")
            icon.source: "../assets/icons-classic/video.svg"
            iconColor: controller && controller.categoryFilter === 5 ? Theme.actionIconColor("filter") : Theme.actionIconColor("media")
            onTriggered: if (controller) controller.setCategoryFilter(5)
        }
        ThemedMenuItem {
            text: root.labelFor(6, "Documents")
            icon.source: "../assets/icons-classic/document.svg"
            iconColor: controller && controller.categoryFilter === 6 ? Theme.actionIconColor("filter") : Theme.actionIconColor("document")
            onTriggered: if (controller) controller.setCategoryFilter(6)
        }
    }
}
