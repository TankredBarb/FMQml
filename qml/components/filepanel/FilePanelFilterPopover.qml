import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var controller
    readonly property var directoryModel: controller ? controller.directoryModel : null

    function labelFor(value, label) {
        if (directoryModel && directoryModel.categoryFilter === value) {
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
            icon.source: "../assets/lucide-toolbar/funnel.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 0 ? "#14b8a6" : "#64748b"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 0
        }
        ThemedMenuSeparator {}
        ThemedMenuItem {
            text: root.labelFor(1, "Executables")
            icon.source: "../assets/icons/terminal.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 1 ? "#14b8a6" : "#6366f1"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 1
        }
        ThemedMenuItem {
            text: root.labelFor(2, "Libraries")
            icon.source: "../assets/icons/info.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 2 ? "#14b8a6" : "#0ea5e9"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 2
        }
        ThemedMenuItem {
            text: root.labelFor(3, "Images")
            icon.source: "../assets/icons/image.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 3 ? "#14b8a6" : "#22c55e"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 3
        }
        ThemedMenuItem {
            text: root.labelFor(4, "Archives")
            icon.source: "../assets/filetypes/archive.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 4 ? "#14b8a6" : "#f59e0b"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 4
        }
        ThemedMenuItem {
            text: root.labelFor(5, "Media")
            icon.source: "../assets/icons/video.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 5 ? "#14b8a6" : "#a855f7"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 5
        }
        ThemedMenuItem {
            text: root.labelFor(6, "Documents")
            icon.source: "../assets/icons/document.svg"
            iconColor: directoryModel && directoryModel.categoryFilter === 6 ? "#14b8a6" : "#3b82f6"
            onTriggered: if (directoryModel) directoryModel.categoryFilter = 6
        }
    }
}
