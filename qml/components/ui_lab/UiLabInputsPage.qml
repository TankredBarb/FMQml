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

    Label { text: "Text Inputs"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "inputs/matrix · All values are local to this scene and reset when Reset Scene is pressed."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "TEXT FIELDS"
        GridLayout {
            Layout.fillWidth: true
            columns: width > 620 ? 2 : 1
            columnSpacing: 10
            rowSpacing: 8
            FmTextField { Layout.fillWidth: true; placeholderText: "Placeholder" }
            FmTextField { Layout.fillWidth: true; text: "Editable value" }
            FmTextField { Layout.fillWidth: true; text: "Invalid value"; error: true }
            FmTextField { Layout.fillWidth: true; text: "Disabled value"; enabled: false }
        }
        FmTextField { visible: stateMatrix; Layout.fillWidth: true; text: "A long value that should remain clipped inside the production text field without changing surrounding geometry" }
    }

    DialogSection {
        title: "TEXT AREAS"
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            FmTextArea { Layout.fillWidth: true; text: "Short editable note" }
            FmTextArea { visible: stateMatrix; Layout.fillWidth: true; error: true; text: "Wrapped error text. This deterministic paragraph is long enough to exercise line wrapping and selection." }
        }
    }

    DialogSection {
        title: "NUMERIC AND CHOICE INPUTS"
        Flow {
            Layout.fillWidth: true
            spacing: 12
            FmSpinBox { from: 0; to: 100; value: 42 }
            FmSpinBox { from: 0; to: 10; value: 10; enabled: false }
            FmComboBox { model: ["Alpha", "Beta", "Gamma"]; currentIndex: 1 }
            FmComboBox { editable: true; model: ["Editable choice", "Second choice"]; editText: "Custom value" }
        }
    }
}
