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

    Label { text: "Dialog Surfaces"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "dialogs/standard · Non-modal previews of the production dialog shell, header, section, and footer composition."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: stateMatrix ? 390 : 330
        color: "transparent"

        DialogShell {
            anchors.fill: parent
            accentColor: Theme.accent
            shellColor: Theme.panelSurface
            shellBorderColor: Theme.panelBorder
            shadowBlur: 12
            shadowVerticalOffset: 3
        }

        DialogHeader {
            id: previewHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
            title: "Example dialog"
            subtitle: "Production surface composition"
        }

        ColumnLayout {
            anchors.top: previewHeader.bottom
            anchors.bottom: previewFooter.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 14
            spacing: 10

            DialogSection {
                title: "CONTENT"
                Label { Layout.fillWidth: true; text: "Dialog content uses the same typography, spacing, borders, and semantic colors as application dialogs."; color: Theme.textPrimary; wrapMode: Text.WordWrap }
                FmTextField { Layout.fillWidth: true; placeholderText: "Example field" }
            }
            DialogSection {
                visible: stateMatrix
                title: "SECONDARY SECTION"
                Label { Layout.fillWidth: true; text: "State Matrix reveals optional and secondary dialog structures."; color: Theme.textSecondary; wrapMode: Text.WordWrap }
            }
        }

        DialogFooter {
            id: previewFooter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            Label { text: "No application state is changed"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
            Item { Layout.fillWidth: true }
            FmButton { text: "Cancel" }
            FmButton { text: "Apply"; highlighted: true }
        }
    }

    Label { Layout.fillWidth: true; text: "The close and footer actions are visual test targets only; this preview is intentionally not another nested modal."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; wrapMode: Text.WordWrap }
}
