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

    Label { text: "Progress and Activity"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "progress/matrix · Indeterminate animations run only while UI Lab is open."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "DETERMINATE BARS"
        FmProgressBar { Layout.fillWidth: true; value: 0 }
        FmProgressBar { Layout.fillWidth: true; value: 0.5 }
        FmProgressBar { Layout.fillWidth: true; value: 1 }
        FmProgressBar { visible: stateMatrix; Layout.fillWidth: true; value: 0.72; fillColor: Theme.danger }
        FmProgressBar { visible: stateMatrix; Layout.fillWidth: true; value: 0.38; enabled: false }
    }

    DialogSection {
        title: "ACTIVITY"
        FmProgressBar { Layout.fillWidth: true; indeterminate: labVisible }
        RowLayout {
            spacing: 18
            FmProgressRing { value: 0 }
            FmProgressRing { value: 0.5; implicitWidth: 28; implicitHeight: 28 }
            FmProgressRing { value: 1; implicitWidth: 38; implicitHeight: 38 }
            FmProgressRing { running: labVisible; implicitWidth: 28; implicitHeight: 28 }
            FmProgressRing { value: 0.66; accentColor: Theme.danger; implicitWidth: 28; implicitHeight: 28 }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "SLIDER STATES"
        FmSlider { Layout.fillWidth: true; value: 0.35 }
        FmSlider { Layout.fillWidth: true; value: 0.75; enabled: false }
    }
}
