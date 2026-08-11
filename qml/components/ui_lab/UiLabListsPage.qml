import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

ColumnLayout {
    property bool stateMatrix: true
    property bool labVisible: false
    width: parent ? parent.width : 700
    spacing: 12

    Label { text: "Lists, Rows, and Delegates"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "lists/rows · Production information rows with deterministic labels, values, icons, and status."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "DIALOG LIST ROWS"
        DialogListRow { label: "Name"; value: "Example document.txt"; iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/document.svg" }
        DialogListRow { label: "Status"; value: "Ready"; emphasizeValue: true; valueColor: Theme.success }
        DialogListRow { label: "Location"; value: "/example/a/deliberately/long/path/that/exercises/middle/elision/document.txt"; isLink: true; valueMaximumLineCount: 1 }
        DialogListRow { visible: stateMatrix; label: "Unavailable"; value: "Not available in this environment"; valueColor: Theme.textSecondary }
        DialogListRow { visible: stateMatrix; label: "Activity"; value: "Calculating deterministic result"; showBusy: labVisible }
    }

    DialogSection {
        title: "BADGES AND ROW DENSITY"
        Flow {
            Layout.fillWidth: true
            spacing: 8
            InlineBadge { text: "Default" }
            InlineBadge { text: "Active"; textColor: Theme.accent; fillColor: Theme.withAlpha(Theme.accent, 0.12); strokeColor: Theme.withAlpha(Theme.accent, 0.42) }
            InlineBadge { text: "Success"; textColor: Theme.success; fillColor: Theme.withAlpha(Theme.success, 0.12); strokeColor: Theme.withAlpha(Theme.success, 0.42) }
            InlineBadge { text: "Warning"; textColor: Theme.warning; fillColor: Theme.withAlpha(Theme.warning, 0.12); strokeColor: Theme.withAlpha(Theme.warning, 0.42) }
            InlineBadge { text: "Error"; textColor: Theme.danger; fillColor: Theme.withAlpha(Theme.danger, 0.12); strokeColor: Theme.withAlpha(Theme.danger, 0.42) }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "EMPTY STATE"
        Item {
            Layout.fillWidth: true
            implicitHeight: 170
            EmptyState {
                anchors.centerIn: parent
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/search.svg"
                colorizeIcon: true
                title: "No deterministic results"
                subtitle: "Empty-state text remains centered and readable at every viewport width."
                hint: "Try another scenario"
            }
        }
    }
}
