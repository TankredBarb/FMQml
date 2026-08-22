import QtQuick
import QtQuick.Controls
import "../framework"
import "../../style"

FmMenu {
    id: root

    property var appRoot
    property var workspaceController
    property bool previewVisible: false

    readonly property var activeController: !root.workspaceController ? null
                                            : (root.workspaceController.activePanel === 0
                                               ? root.workspaceController.leftPanel
                                               : root.workspaceController.rightPanel)
    readonly property bool hiddenFilesVisible: !!root.activeController
                                                && !!root.activeController.directoryModel
                                                && root.activeController.directoryModel.showHidden
    readonly property bool searchableLocation: !!root.activeController
                                                && !!root.activeController.currentPath
                                                && !root.activeController.isVirtualRoot
                                                && root.activeController.pathCanUseLocalShellAction(root.activeController.currentPath)

    width: 290

    FmMenuItem {
        text: "Command Palette"
        shortcut: "Ctrl+K"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
        iconColor: Theme.categoryAction
        onTriggered: if (root.appRoot) root.appRoot.openCommandPalette()
    }
    FmMenuItem {
        text: "Favorites"
        shortcut: "Ctrl+B"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/star.svg"
        iconColor: Theme.categoryNavigation
        onTriggered: if (root.appRoot) root.appRoot.navigateActivePanel("favorites://")
    }
    FmMenuSeparator {}

    FmMenu {
        title: "View"
        width: 280
        itemIconColor: Theme.categoryNavigation
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/layout-grid.svg"

        FmMenuToggleItem {
            text: "Split Panels"
            shortcut: "F3"
            active: !!root.workspaceController && root.workspaceController.splitEnabled
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/panel-open.svg"
            iconColor: Theme.categoryNavigation
            onTriggered: if (root.appRoot) root.appRoot.toggleSplitView()
        }
        FmMenuToggleItem {
            text: "Preview Pane"
            shortcut: "Ctrl+P"
            active: root.previewVisible
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/panel-right.svg"
            iconColor: Theme.categoryNavigation
            onTriggered: if (root.appRoot) root.appRoot.togglePreviewPane()
        }
        FmMenu {
            title: "Sidebar Panels"
            width: 250
            itemIconColor: Theme.categoryNavigation
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/panel-open.svg"

            FmMenuToggleItem {
                text: "Places"
                active: !!root.appRoot && root.appRoot.sidebarPlacesEnabled
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/home.svg"
                iconColor: Theme.categoryNavigation
                onTriggered: if (root.appRoot) root.appRoot.toggleSidebarPanel("places")
            }
            FmMenuToggleItem {
                text: "Recent Folders"
                active: !!root.appRoot && root.appRoot.sidebarRecentEnabled
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/calendar-clock.svg"
                iconColor: Theme.categoryUtility
                onTriggered: if (root.appRoot) root.appRoot.toggleSidebarPanel("recent")
            }
            FmMenuToggleItem {
                text: "Folder Tree"
                active: !!root.appRoot && root.appRoot.sidebarFoldersEnabled
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
                iconColor: Theme.categoryNavigation
                onTriggered: if (root.appRoot) root.appRoot.toggleSidebarPanel("folders")
            }
            FmMenuSeparator {}
            FmMenuItem {
                text: "Customize..."
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg"
                iconColor: Theme.categoryUtility
                onTriggered: if (root.appRoot) root.appRoot.openSidebarSettings()
            }
        }
        FmMenuToggleItem {
            text: "Show Hidden Files"
            shortcut: "Ctrl+H"
            active: root.hiddenFilesVisible
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/eye.svg"
            iconColor: Theme.categoryUtility
            onTriggered: if (root.appRoot) root.appRoot.toggleHiddenFiles()
        }
        FmMenuSeparator {}
        FmMenuItem {
            text: "Theme: " + themeController.schemeName
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/theme.svg"
            iconColor: Theme.categoryUtility
            onTriggered: if (root.appRoot) root.appRoot.openThemeSelector()
        }
    }

    FmMenu {
        title: "Tools"
        width: 280
        itemIconColor: Theme.categoryAction
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/disk-usage.svg"

        FmMenuItem {
            text: "File Search"
            shortcut: "Ctrl+Shift+F"
            enabled: root.searchableLocation
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
            iconColor: Theme.categoryAction
            onTriggered: if (root.appRoot) root.appRoot.openFileSearch()
        }
        FmMenuItem {
            text: "Disk Usage"
            enabled: root.searchableLocation
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/disk-usage.svg"
            iconColor: Theme.categoryInfo
            onTriggered: if (root.appRoot) root.appRoot.openDiskUsage("")
        }
        FmMenuItem {
            text: "Compare Folders"
            enabled: !!root.workspaceController && root.workspaceController.splitEnabled
            icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-compare.svg"
            iconColor: Theme.categoryAction
            onTriggered: if (root.appRoot) root.appRoot.openFolderCompare()
        }
    }

    FmMenuSeparator {}
    FmMenuItem {
        text: "Plugins"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/plugin.svg"
        iconColor: Theme.categoryUtility
        onTriggered: if (root.appRoot) root.appRoot.openPluginManagerDialog()
    }
    FmMenuItem {
        text: "Settings"
        shortcut: "Ctrl+,"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg"
        iconColor: Theme.accent
        onTriggered: if (root.appRoot) root.appRoot.openSettingsDialog()
    }
    FmMenuItem {
        text: "Help and Shortcuts"
        shortcut: "F1"
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
        iconColor: Theme.categoryInfo
        onTriggered: if (root.appRoot) root.appRoot.openHelpDialog()
    }
    FmMenuSeparator {}
    FmMenuItem {
        text: "Quit FM"
        shortcut: "Ctrl+Q"
        destructive: true
        icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/exit.svg"
        onTriggered: if (root.appRoot) root.appRoot.quitApplication()
    }
}
