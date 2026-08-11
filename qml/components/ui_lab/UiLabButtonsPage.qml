import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../framework"
import "../dialogs"

ColumnLayout {
    property bool stateMatrix: true
    property bool labVisible: false
    width: parent ? parent.width : 700
    spacing: 12

    Label { text: "Buttons and Actions"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "buttons/matrix · Hover and press the interactive examples; use Tab and Space/Enter to inspect keyboard focus."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "FM BUTTON"
        Flow {
            Layout.fillWidth: true
            spacing: 10
            FmButton { text: "Standard" }
            FmButton { text: "Highlighted"; highlighted: true }
            FmButton { text: "Destructive"; highlighted: true; primaryColor: Theme.danger }
            FmButton { text: "Flat"; flat: true }
            FmButton { text: "Disabled"; enabled: false }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "GEOMETRY AND LABELS"
        FmButton { Layout.fillWidth: true; text: "A deliberately long action label at the available scene width" }
        RowLayout {
            Layout.fillWidth: true
            FmButton { text: "Minimum" }
            Item { Layout.fillWidth: true }
            FmButton { Layout.preferredWidth: 240; text: "Wide action"; highlighted: true }
        }
    }

    DialogSection {
        title: "ICON ACTIONS"
        RowLayout {
            spacing: 12
            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"; showIdleSurface: true; Accessible.name: "Search"; ToolTip.text: "Search"; ToolTip.visible: hovered }
            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/refresh.svg"; isHighlighted: true; Accessible.name: "Refresh" }
            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/delete.svg"; iconTone: "danger"; showIdleSurface: true; Accessible.name: "Delete" }
            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg"; enabled: false; Accessible.name: "Disabled settings" }
            FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/star.svg"; iconSize: 24; implicitWidth: 42; implicitHeight: 42; showIdleSurface: true; Accessible.name: "Large favorite" }
        }
    }
}
