import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../framework"
import "../dialogs"

ColumnLayout {
    id: pageRoot

    property bool stateMatrix: true
    property bool labVisible: false
    width: parent ? parent.width : 700
    spacing: 12

    function openMenuBelow(menu, anchor) {
        const position = anchor.mapToItem(Overlay.overlay, 0, anchor.height + 4)
        menu.x = position.x
        menu.y = position.y
        menu.open()
    }

    Label { text: "Combo, Menu, and Popup"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "menus/interactive · Open menus, navigate with the keyboard, then verify Escape and click-outside closing."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "MENU SURFACE"
        RowLayout {
            FmButton {
                id: actionMenuButton
                text: "Open menu"
                highlighted: true
                onClicked: pageRoot.openMenuBelow(actionMenu, actionMenuButton)
            }
            FmButton {
                id: longMenuButton
                text: "Open long menu"
                onClicked: pageRoot.openMenuBelow(longMenu, longMenuButton)
            }
            Item { Layout.fillWidth: true }
            Label { text: "No action changes application state"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
        }
        FmMenu {
            id: actionMenu
            parent: Overlay.overlay
            z: 10002
            FmMenuItem { text: "Open"; shortcut: "Enter"; icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg" }
            FmMenuItem { text: "Selected action"; active: true; icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/star.svg" }
            FmMenuItem { text: "Unavailable between actions"; enabled: false; icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg" }
            FmMenuSeparator {}
            FmMenuItem { text: "A deliberately long item label constrained by menu geometry"; icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/document.svg" }
            FmMenuItem { text: "Delete"; destructive: true; icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/delete.svg" }
        }
        FmMenu {
            id: longMenu
            parent: Overlay.overlay
            z: 10002
            width: 300
            height: 360
            Repeater {
                model: 14
                FmMenuItem {
                    required property int index
                    text: "Deterministic menu item " + (index + 1)
                    active: index === 4
                    icon.source: index % 3 === 0
                                 ? "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg"
                                 : (index % 3 === 1
                                    ? "qrc:/qt/qml/FM/qml/assets/icons-classic/document.svg"
                                    : "qrc:/qt/qml/FM/qml/assets/icons-classic/star.svg")
                }
            }
        }
    }

    DialogSection {
        title: "PROPOSED APP MAIN MENU"
        Label {
            Layout.fillWidth: true
            text: "Phase 0 prototype · Inspect grouping, submenu discoverability, active states, keyboard navigation, and long-label headroom."
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeCaption
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            FmButton {
                id: proposedMenuButton
                text: "Open proposed main menu"
                highlighted: true
                onClicked: pageRoot.openMenuBelow(proposedMainMenu, proposedMenuButton)
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "290 px · production FmMenu"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMicro
            }
        }

        FmMenu {
            id: proposedMainMenu
            parent: Overlay.overlay
            z: 10002
            width: 290

            FmMenuItem {
                text: "Command Palette"
                shortcut: "Ctrl+K"
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
                iconColor: Theme.categoryAction
            }
            FmMenuItem {
                text: "Favorites"
                shortcut: "Ctrl+B"
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/star.svg"
                iconColor: Theme.categoryNavigation
            }
            FmMenuSeparator {}

            FmMenu {
                title: "View"
                z: 10003
                width: 280
                itemIconColor: Theme.categoryNavigation
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/layout-grid.svg"
                FmMenuItem {
                    text: "Split Panels"
                    shortcut: "F3"
                    active: true
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/panel-open.svg"
                    iconColor: Theme.categoryNavigation
                }
                FmMenuItem {
                    text: "Preview Pane"
                    active: true
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/panel-right.svg"
                    iconColor: Theme.categoryNavigation
                }
                FmMenuItem {
                    text: "Show Hidden Files"
                    shortcut: "Ctrl+H"
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/eye.svg"
                    iconColor: Theme.categoryUtility
                }
                FmMenuSeparator {}
                FmMenuItem {
                    text: "Theme: " + themeController.schemeName
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/theme.svg"
                    iconColor: Theme.categoryUtility
                }
            }

            FmMenu {
                title: "Tools"
                z: 10003
                width: 280
                itemIconColor: Theme.categoryAction
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/disk-usage.svg"
                FmMenuItem {
                    text: "File Search"
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
                    iconColor: Theme.categoryAction
                }
                FmMenuItem {
                    text: "Disk Usage"
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/disk-usage.svg"
                    iconColor: Theme.categoryInfo
                }
                FmMenuItem {
                    text: "Compare Folders"
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-compare.svg"
                    iconColor: Theme.categoryAction
                }
                FmMenuItem {
                    text: "Unavailable tool for this location"
                    enabled: false
                    icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/terminal.svg"
                }
            }

            FmMenuSeparator {}
            FmMenuItem {
                text: "Plugins"
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/plugin.svg"
                iconColor: Theme.categoryUtility
            }
            FmMenuItem {
                text: "Settings"
                shortcut: "Ctrl+,"
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg"
                iconColor: Theme.accent
            }
            FmMenuItem {
                text: "Help and Shortcuts"
                shortcut: "F1"
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
                iconColor: Theme.categoryInfo
            }
            FmMenuSeparator {}
            FmMenuItem {
                text: "Quit FM"
                shortcut: "Ctrl+Q"
                destructive: true
                icon.source: "qrc:/qt/qml/FM/qml/assets/icons-classic/exit.svg"
            }
        }
    }

    DialogSection {
        title: "COMBO POPUPS"
        RowLayout {
            Layout.fillWidth: true
            FmComboBox { Layout.preferredWidth: 220; model: ["Short", "Medium label", "A long constrained choice"]; currentIndex: 1 }
            FmComboBox { Layout.preferredWidth: 240; model: ["Row 01", "Row 02", "Row 03", "Row 04", "Row 05", "Row 06", "Row 07", "Row 08", "Row 09", "Row 10", "Row 11", "Row 12"] }
            FmComboBox { visible: stateMatrix; Layout.preferredWidth: 180; model: ["Disabled"]; enabled: false }
        }
    }
}
