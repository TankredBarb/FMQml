import QtQuick
import QtQuick.Controls
import ".."
import "../framework"
import "../../style"

Item {
    id: root

    property var controller
    property bool showActionBar: true
    property bool showSelectionBadges: true
    property bool showMediaHoverPreviews: false
    property bool showFolderHoverPreviews: false
    property bool folderPeekEnabled: false
    readonly property var directoryModel: root.controller ? root.controller.directoryModel : null
    signal actionBarVisibilityRequested(bool visible)
    signal selectionBadgesVisibilityRequested(bool visible)
    signal mediaHoverPreviewsVisibilityRequested(bool visible)
    signal folderHoverPreviewsVisibilityRequested(bool visible)
    signal folderPeekEnabledRequested(bool enabled)
    signal viewModeSelected()
    property bool pendingViewModeFocusRestore: false
    readonly property bool startupLazyToolMenus: true
    readonly property var activeSortMenu: root.startupLazyToolMenus ? root.sortMenuItem : sortMenuLoader.item
    property var filterPopoverItem: null
    property var viewMenuItem: null
    property var sortMenuItem: null
    property var panelOptionsMenuItem: null

    implicitWidth: 146
    implicitHeight: 32
    visible: root.controller ? !root.controller.isDeviceRoot && !root.controller.isFavoritesRoot : false

    function sortRoleLabel(role) {
        switch (role) {
        case 0: return "Name"
        case 1: return "Size"
        case 2: return "Type"
        case 3: return "Date Modified"
        case 4: return "Date Created"
        case 5: return "Extension"
        default: return "Name"
        }
    }

    function sortRoleIcon(role) {
        switch (role) {
        case 0: return "../assets/icons-classic/sort-name.svg"
        case 1: return "../assets/icons-classic/sort-size.svg"
        case 2: return "../assets/icons-classic/sort-type.svg"
        case 3: return "../assets/icons-classic/sort-date-modified.svg"
        case 4: return "../assets/icons-classic/sort-date-created.svg"
        case 5: return "../assets/icons-classic/sort-extension.svg"
        default: return "../assets/icons-classic/sort-mixed.svg"
        }
    }

    function sortRoleTone(role) {
        switch (role) {
        case 0: return "primary"
        case 1: return "info"
        case 2: return "document"
        case 3: return "refresh"
        case 4: return "create"
        case 5: return "rename"
        default: return "sort"
        }
    }

    function defaultSortOrder(role) {
        return role === 1 || role === 3 || role === 4
                ? Qt.DescendingOrder
                : Qt.AscendingOrder
    }

    function activeSortRole() {
        return root.directoryModel ? root.directoryModel.sortRole : 0
    }

    function activeSortOrder() {
        return root.directoryModel ? root.directoryModel.sortOrder : Qt.AscendingOrder
    }

    function isSortRole(role) {
        return root.directoryModel && root.directoryModel.sortRole === role
    }

    function isDefaultSort() {
        return root.activeSortRole() === 0 && root.activeSortOrder() === Qt.AscendingOrder
    }

    function sortShortcut(role) {
        if (!isSortRole(role)) {
            return ""
        }

        const ascending = activeSortOrder() === Qt.AscendingOrder
        switch (role) {
        case 0:
        case 2:
        case 5:
            return ascending ? "A-Z" : "Z-A"
        case 1:
            return ascending ? "Small" : "Large"
        case 3:
        case 4:
            return ascending ? "Oldest" : "Newest"
        default:
            return ascending ? "Asc" : "Desc"
        }
    }

    function directionLabel(order) {
        const role = activeSortRole()
        const ascending = order === Qt.AscendingOrder
        switch (role) {
        case 0:
        case 2:
        case 5:
            return ascending ? "A-Z" : "Z-A"
        case 1:
            return ascending ? "Small first" : "Large first"
        case 3:
        case 4:
            return ascending ? "Oldest first" : "Newest first"
        default:
            return ascending ? "Ascending" : "Descending"
        }
    }

    function setSort(role) {
        if (!root.controller) {
            return
        }

        if (root.controller.panelSortRole === role) {
            const nextOrder = root.controller.panelSortOrder === Qt.AscendingOrder
                    ? Qt.DescendingOrder
                    : Qt.AscendingOrder
            root.controller.setPanelSortPolicy(role, nextOrder)
            return
        }

        root.controller.setPanelSortPolicy(role, defaultSortOrder(role))
    }

    function setSortOrder(order) {
        if (root.controller) {
            if (root.activeSortOrder() === order) {
                return
            }
            root.controller.setPanelSortPolicy(activeSortRole(), order)
        }
    }

    function setDefaultSort() {
        if (!root.controller || root.isDefaultSort()) {
            return
        }
        root.controller.setPanelSortPolicy(0, Qt.AscendingOrder)
    }

    function selectViewMode(mode) {
        if (!root.controller) {
            return
        }
        root.pendingViewModeFocusRestore = true
        root.controller.viewMode = mode
    }

    function ensureFilterPopover() {
        if (root.startupLazyToolMenus) {
            if (!root.filterPopoverItem) {
                root.filterPopoverItem = filterPopoverComponent.createObject(root)
            }
            return root.filterPopoverItem
        }
        return filterPopoverLoader.item
    }

    function ensureViewMenu() {
        if (root.startupLazyToolMenus) {
            if (!root.viewMenuItem) {
                root.viewMenuItem = viewMenuComponent.createObject(root)
            }
            return root.viewMenuItem
        }
        return viewMenuLoader.item
    }

    function ensureSortMenu() {
        if (root.startupLazyToolMenus) {
            if (!root.sortMenuItem) {
                root.sortMenuItem = sortMenuComponent.createObject(root)
            }
            return root.sortMenuItem
        }
        return sortMenuLoader.item
    }

    function ensurePanelOptionsMenu() {
        if (!root.panelOptionsMenuItem) {
            root.panelOptionsMenuItem = panelOptionsMenuComponent.createObject(root)
        }
        return root.panelOptionsMenuItem
    }

    function openFilterPopover(anchorItem) {
        const popover = root.ensureFilterPopover()
        if (popover) {
            popover.openAt(anchorItem)
        }
    }

    function openViewMenu(anchorItem) {
        const menu = root.ensureViewMenu()
        if (menu) {
            menu.popup(anchorItem, 0, anchorItem.height + 8)
        }
    }

    function openSortMenu(anchorItem) {
        const menu = root.ensureSortMenu()
        if (menu) {
            menu.popup(anchorItem, 0, anchorItem.height + 8)
        }
    }

    function openPanelOptionsMenu(anchorItem) {
        const menu = root.ensurePanelOptionsMenu()
        if (menu) {
            menu.popup(anchorItem, 0, anchorItem.height + 8)
        }
    }

    Component {
        id: filterPopoverComponent

        FilePanelFilterPopover {
            controller: root.controller
        }
    }

    Loader {
        id: filterPopoverLoader
        active: !root.startupLazyToolMenus
        sourceComponent: filterPopoverComponent
    }

    Component {
        id: viewMenuComponent

        FmMenu {
            onClosed: {
                if (root.pendingViewModeFocusRestore) {
                    root.pendingViewModeFocusRestore = false
                    root.viewModeSelected()
                }
            }

            FmMenuItem {
                text: "Details"
                active: root.controller && root.controller.viewMode === 0
                icon.source: "../assets/icons-classic/list.svg"
                iconColor: Theme.actionIconColor("view-details")
                onTriggered: root.selectViewMode(0)
            }
            FmMenuItem {
                text: "Grid"
                active: root.controller && root.controller.viewMode === 1
                icon.source: "../assets/icons-classic/layout-grid.svg"
                iconColor: Theme.actionIconColor("view-grid")
                onTriggered: root.selectViewMode(1)
            }
            FmMenuItem {
                text: "Brief"
                active: root.controller && root.controller.viewMode === 2
                icon.source: "../assets/icons-classic/layout-list.svg"
                iconColor: Theme.actionIconColor("view-brief")
                onTriggered: root.selectViewMode(2)
            }
        }
    }

    Loader {
        id: viewMenuLoader
        active: !root.startupLazyToolMenus
        sourceComponent: viewMenuComponent
    }

    Component {
        id: panelOptionsMenuComponent

        FmMenu {
            FmMenu {
                title: "Panel Appearance"
                width: 270
                icon.source: "../assets/icons-classic/columns-2.svg"
                itemIconColor: Theme.actionIconColor("view-grid")

                FmMenuToggleItem {
                    text: root.showActionBar ? "Hide Action Bar" : "Show Action Bar"
                    active: root.showActionBar
                    icon.source: "../assets/icons-classic/operation-drawer-compact.svg"
                    iconColor: Theme.actionIconColor("view-details")
                    onTriggered: root.actionBarVisibilityRequested(!root.showActionBar)
                }
                FmMenuToggleItem {
                    text: root.showSelectionBadges ? "Hide Selection Badges" : "Show Selection Badges"
                    active: root.showSelectionBadges
                    icon.source: "../assets/icons-classic/select-all.svg"
                    iconColor: Theme.actionIconColor("primary")
                    onTriggered: root.selectionBadgesVisibilityRequested(!root.showSelectionBadges)
                }
            }

            FmMenu {
                title: "Hover & Peek"
                width: 270
                icon.source: "../assets/icons-classic/duplicate.svg"
                itemIconColor: Theme.actionIconColor("info")

                FmMenuToggleItem {
                    text: root.showMediaHoverPreviews ? "Hide Media Hover Previews" : "Show Media Hover Previews"
                    active: root.showMediaHoverPreviews
                    icon.source: "../assets/icons-classic/image.svg"
                    iconColor: Theme.categoryInfo
                    onTriggered: root.mediaHoverPreviewsVisibilityRequested(!root.showMediaHoverPreviews)
                }
                FmMenuToggleItem {
                    text: root.showFolderHoverPreviews ? "Hide Folder Hover Previews" : "Show Folder Hover Previews"
                    active: root.showFolderHoverPreviews
                    icon.source: "../assets/icons-classic/folder-open.svg"
                    iconColor: Theme.categoryNavigation
                    onTriggered: root.folderHoverPreviewsVisibilityRequested(!root.showFolderHoverPreviews)
                }
                FmMenuToggleItem {
                    text: root.folderPeekEnabled ? "Hide Folder Peek" : "Show Folder Peek"
                    active: root.folderPeekEnabled
                    icon.source: "../assets/icons-classic/panel-open.svg"
                    iconColor: Theme.categoryAction
                    onTriggered: root.folderPeekEnabledRequested(!root.folderPeekEnabled)
                }
            }
        }
    }

    Component {
        id: sortMenuComponent

        FmMenu {
            FmMenuItem {
                text: "Default Sort"
                shortcut: "Name A-Z"
                active: root.isDefaultSort()
                icon.source: "../assets/icons-classic/sort-default.svg"
                iconColor: Theme.actionIconColor("primary")
                onTriggered: root.setDefaultSort()
            }

            FmMenuSeparator {}

            FmMenuItem {
                text: "Name"
                shortcut: root.sortShortcut(0)
                active: root.isSortRole(0)
                icon.source: root.sortRoleIcon(0)
                iconColor: Theme.actionIconColor(root.sortRoleTone(0))
                onTriggered: root.setSort(0)
            }
            FmMenuItem {
                text: "Date Modified"
                shortcut: root.sortShortcut(3)
                active: root.isSortRole(3)
                icon.source: root.sortRoleIcon(3)
                iconColor: Theme.actionIconColor(root.sortRoleTone(3))
                onTriggered: root.setSort(3)
            }
            FmMenuItem {
                text: "Date Created"
                shortcut: root.sortShortcut(4)
                active: root.isSortRole(4)
                icon.source: root.sortRoleIcon(4)
                iconColor: Theme.actionIconColor(root.sortRoleTone(4))
                onTriggered: root.setSort(4)
            }
            FmMenuItem {
                text: "Size"
                shortcut: root.sortShortcut(1)
                active: root.isSortRole(1)
                icon.source: root.sortRoleIcon(1)
                iconColor: Theme.actionIconColor(root.sortRoleTone(1))
                onTriggered: root.setSort(1)
            }
            FmMenuItem {
                text: "Type"
                shortcut: root.sortShortcut(2)
                active: root.isSortRole(2)
                icon.source: root.sortRoleIcon(2)
                iconColor: Theme.actionIconColor(root.sortRoleTone(2))
                onTriggered: root.setSort(2)
            }
            FmMenuItem {
                text: "Extension"
                shortcut: root.sortShortcut(5)
                active: root.isSortRole(5)
                icon.source: root.sortRoleIcon(5)
                iconColor: Theme.actionIconColor(root.sortRoleTone(5))
                onTriggered: root.setSort(5)
            }

            FmMenuSeparator {}

            FmMenuItem {
                text: "Ascending"
                shortcut: root.directionLabel(Qt.AscendingOrder)
                active: root.activeSortOrder() === Qt.AscendingOrder
                icon.source: "../assets/icons-classic/sort-ascending.svg"
                iconColor: Theme.actionIconColor("sort")
                onTriggered: root.setSortOrder(Qt.AscendingOrder)
            }
            FmMenuItem {
                text: "Descending"
                shortcut: root.directionLabel(Qt.DescendingOrder)
                active: root.activeSortOrder() === Qt.DescendingOrder
                icon.source: "../assets/icons-classic/sort-descending.svg"
                iconColor: Theme.actionIconColor("sort")
                onTriggered: root.setSortOrder(Qt.DescendingOrder)
            }

            FmMenuSeparator {}

            FmMenuToggleItem {
                text: root.directoryModel && root.directoryModel.mixFilesAndFolders
                      ? "Folders First"
                      : "Mixed Files & Folders"
                icon.source: root.directoryModel && root.directoryModel.mixFilesAndFolders
                             ? "../assets/icons-classic/sort-folders-first.svg"
                             : "../assets/icons-classic/sort-mixed.svg"
                iconColor: Theme.actionIconColor(root.directoryModel && root.directoryModel.mixFilesAndFolders
                                                  ? "folder"
                                                  : "sort")
                onTriggered: {
                    root.directoryModel.mixFilesAndFolders = !root.directoryModel.mixFilesAndFolders
                }
            }
        }
    }

    Loader {
        id: sortMenuLoader
        active: !root.startupLazyToolMenus
        sourceComponent: sortMenuComponent
    }

    Row {
        anchors.fill: parent
        spacing: 6

        FmIconButton {
            id: panelViewToggle
            width: 32
            height: 32
            visible: root.controller ? !root.controller.isDeviceRoot && !root.controller.isFavoritesRoot : false
            iconSource: root.controller && root.controller.viewMode === 0
                        ? "../assets/icons-classic/list.svg"
                        : (root.controller && root.controller.viewMode === 1
                           ? "../assets/icons-classic/layout-grid.svg"
                           : "../assets/icons-classic/layout-list.svg")
            iconTone: root.controller && root.controller.viewMode === 0
                      ? "view-details"
                      : (root.controller && root.controller.viewMode === 1
                         ? "view-grid"
                         : "view-brief")
            onClicked: root.openViewMenu(panelViewToggle)
            ToolTip.visible: hovered
            ToolTip.text: "Change View Mode"
        }

        FmIconButton {
            id: sortButton
            width: 32
            height: 32
            iconSource: root.sortRoleIcon(root.activeSortRole())
            iconTone: root.sortRoleTone(root.activeSortRole())
            isHighlighted: root.activeSortMenu ? root.activeSortMenu.opened : false
            onClicked: root.openSortMenu(sortButton)
            ToolTip.visible: hovered
            ToolTip.text: "Sort: " + root.sortRoleLabel(root.activeSortRole())
                          + " (" + root.directionLabel(root.activeSortOrder()) + ")"
        }

        FmIconButton {
            id: filterButton
            width: 32
            height: 32
            iconSource: "../assets/icons-classic/funnel.svg"
            iconTone: "filter"
            isHighlighted: root.controller && root.controller.categoryFilterActive
            onClicked: root.openFilterPopover(filterButton)
            ToolTip.visible: hovered
            ToolTip.text: root.controller && root.controller.categoryFilterActive
                          ? "Filters - " + root.controller.categoryFilterSummary
                            + (root.controller.categoryFilterSuspended ? " (Suspended)" : "")
                          : "Open Filters"

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 2
                anchors.topMargin: 2
                width: 8
                height: 8
                radius: 4
                visible: root.controller && root.controller.categoryFilterActive
                color: root.controller && root.controller.categoryFilterSuspended
                       ? Theme.warning
                       : Theme.categoryUtility
                border.color: Theme.panelSurface
                border.width: 1
            }
        }

        FmIconButton {
            id: panelOptionsButton
            width: 32
            height: 32
            iconSource: "../assets/icons-classic/attributes.svg"
            iconTone: "attributes"
            isHighlighted: root.panelOptionsMenuItem ? root.panelOptionsMenuItem.opened : false
            onClicked: root.openPanelOptionsMenu(panelOptionsButton)
            ToolTip.visible: hovered
            ToolTip.text: "Panel Appearance and Hover Options"
        }
    }
}
