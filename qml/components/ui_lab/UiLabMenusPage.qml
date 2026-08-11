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
        title: "COMBO POPUPS"
        RowLayout {
            Layout.fillWidth: true
            FmComboBox { Layout.preferredWidth: 220; model: ["Short", "Medium label", "A long constrained choice"]; currentIndex: 1 }
            FmComboBox { Layout.preferredWidth: 240; model: ["Row 01", "Row 02", "Row 03", "Row 04", "Row 05", "Row 06", "Row 07", "Row 08", "Row 09", "Row 10", "Row 11", "Row 12"] }
            FmComboBox { visible: stateMatrix; Layout.preferredWidth: 180; model: ["Disabled"]; enabled: false }
        }
    }
}
