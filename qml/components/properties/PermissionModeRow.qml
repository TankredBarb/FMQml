import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../framework"

RowLayout {
    id: modeRow

    required property string title
    required property int readBit
    required property int writeBit
    required property int executeBit
    required property var modeEnabled
    required property var setModeBit

    Layout.fillWidth: true
    spacing: 8

    Label {
        text: modeRow.title
        Layout.preferredWidth: 72
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.DemiBold
        color: Theme.textSecondary
    }

    FmCheckBox {
        text: "Read"
        Layout.preferredWidth: 96
        checked: modeRow.modeEnabled(modeRow.readBit)
        onToggled: modeRow.setModeBit(modeRow.readBit, checked)
    }

    FmCheckBox {
        text: "Write"
        Layout.preferredWidth: 96
        checked: modeRow.modeEnabled(modeRow.writeBit)
        onToggled: modeRow.setModeBit(modeRow.writeBit, checked)
    }

    FmCheckBox {
        text: "Execute"
        Layout.preferredWidth: 96
        checked: modeRow.modeEnabled(modeRow.executeBit)
        onToggled: modeRow.setModeBit(modeRow.executeBit, checked)
    }
}
