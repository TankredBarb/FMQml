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

    Label { text: "Scrollbars and Scrolling"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "scrollbars/interactive · Compare full and flat production scrollbars using fixed local content."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "VERTICAL SCROLLING"
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Frame {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                padding: 0
                background: Rectangle { color: Theme.surface; border.color: Theme.panelBorder; radius: Theme.radiusSm }
                ListView {
                    id: fullList
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: 18
                    delegate: Label { required property int index; width: ListView.view.width; height: 34; leftPadding: 10; text: "Full scrollbar row " + (index + 1); color: Theme.textPrimary; verticalAlignment: Text.AlignVCenter }
                    ScrollBar.vertical: FmScrollBar { wheelTarget: fullList }
                }
            }
            Frame {
                visible: stateMatrix
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                padding: 0
                background: Rectangle { color: Theme.surface; border.color: Theme.panelBorder; radius: Theme.radiusSm }
                ListView {
                    id: flatList
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: 18
                    delegate: Label { required property int index; width: ListView.view.width; height: 34; leftPadding: 10; text: "Flat scrollbar row " + (index + 1); color: Theme.textPrimary; verticalAlignment: Text.AlignVCenter }
                    ScrollBar.vertical: FmScrollBar { flat: true; wheelTarget: flatList }
                }
            }
        }
    }

    DialogSection {
        title: "HORIZONTAL SCROLLING"
        Flickable {
            id: horizontalScene
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            contentWidth: 1100
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Row {
                spacing: 8
                Repeater { model: 12; Rectangle { required property int index; width: 82; height: 58; radius: Theme.radiusSm; color: Theme.withAlpha(Theme.accent, 0.12 + (index % 3) * 0.05); border.color: Theme.withAlpha(Theme.accent, 0.35); Label { anchors.centerIn: parent; text: "Card " + (index + 1); color: Theme.textPrimary } } }
            }
            ScrollBar.horizontal: FmScrollBar { wheelTarget: horizontalScene }
        }
    }
}
