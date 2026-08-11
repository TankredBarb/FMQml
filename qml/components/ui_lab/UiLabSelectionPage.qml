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

    Label { text: "Selection Controls"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "selection/matrix · Interactive values do not affect application settings."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "CHECK BOXES"
        Flow {
            Layout.fillWidth: true
            spacing: 14
            FmCheckBox { text: "Unchecked" }
            FmCheckBox { text: "Checked"; checked: true }
            FmCheckBox { text: "Partial"; tristate: true; checkState: Qt.PartiallyChecked }
            FmCheckBox { text: "Disabled"; checked: true; enabled: false }
        }
    }

    DialogSection {
        title: "SWITCHES"
        Flow {
            Layout.fillWidth: true
            spacing: 14
            FmSwitch { text: "Off" }
            FmSwitch { text: "On"; checked: true }
            FmSwitch { text: "Disabled"; checked: true; enabled: false }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "TOGGLE ROWS"
        FmToggleRow { title: "Enabled feature"; subtitle: "A normal two-line production settings row."; checked: true; onToggled: newChecked => checked = newChecked }
        FmToggleRow { title: "Long title that must elide without moving the switch outside the row"; subtitle: "Long supporting text wraps to at most two lines while preserving a useful click target."; onToggled: newChecked => checked = newChecked }
        FmToggleRow { title: "Unavailable feature"; subtitle: "Disabled state"; checked: true; toggleEnabled: false }
    }
}
