import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../framework"
import "../dialogs"

ColumnLayout {
    property bool stateMatrix: true
    property bool labVisible: false
    width: parent ? parent.width : 700
    spacing: 12

    Label { text: "Composite Patterns"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "composite/cards · Small production-style compositions assembled from existing primitives."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "SETTINGS CARD"
        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: settingsLayout.implicitHeight + 24
            ColumnLayout {
                id: settingsLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    IconTile { source: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg" }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { Layout.fillWidth: true; text: "Deterministic feature"; color: Theme.textPrimary; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        Label { Layout.fillWidth: true; text: "A compact title, description, status, and action composition."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; wrapMode: Text.WordWrap }
                    }
                    InlineBadge { text: "Ready"; textColor: Theme.success }
                }
                FmToggleRow { title: "Enable example"; subtitle: "Local state inside this scene only"; onToggled: newChecked => checked = newChecked }
            }
        }
    }

    DialogSection {
        title: "ACTION CARD"
        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: actionLayout.implicitHeight + 24
            RowLayout {
                id: actionLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                IconTile { tileSize: 42; iconSize: 22; source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { Layout.fillWidth: true; text: "Open deterministic workspace"; color: Theme.textPrimary; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Label { Layout.fillWidth: true; text: "No filesystem access is performed."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight }
                }
                FmButton { text: "Open"; highlighted: true }
            }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "INLINE VALIDATION"
        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: validationLayout.implicitHeight + 24
            ColumnLayout {
                id: validationLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 7
                Label { text: "Destination label"; color: Theme.textPrimary; font.weight: Font.Medium }
                FmTextField { Layout.fillWidth: true; text: "Invalid deterministic value"; error: true }
                Label { Layout.fillWidth: true; text: "Explain the problem next to the affected production control."; color: Theme.danger; font.pixelSize: Theme.fontSizeCaption; wrapMode: Text.WordWrap }
            }
        }
    }
}
