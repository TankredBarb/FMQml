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

    Label { text: "Icons"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "icons/classic · Production SVG loading and semantic recoloring at common sizes."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "SEMANTIC ICONS"
        GridLayout {
            Layout.fillWidth: true
            columns: width > 620 ? 4 : 2
            rowSpacing: 12
            columnSpacing: 12
            Repeater {
                model: [
                    { name: "Search", icon: "search.svg", tone: Theme.textPrimary },
                    { name: "Folder", icon: "folder-open.svg", tone: Theme.accent },
                    { name: "Refresh", icon: "refresh.svg", tone: Theme.activeAccent },
                    { name: "Settings", icon: "settings.svg", tone: Theme.textSecondary },
                    { name: "Favorite", icon: "star.svg", tone: Theme.warning },
                    { name: "Delete", icon: "delete.svg", tone: Theme.danger },
                    { name: "Close", icon: "close.svg", tone: Theme.textPrimary },
                    { name: "Information", icon: "info.svg", tone: Theme.categoryInfo }
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    IconTile { source: "qrc:/qt/qml/FM/qml/assets/icons-classic/" + modelData.icon; iconColor: modelData.tone }
                    Label { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; elide: Text.ElideRight }
                }
            }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "SIZE MATRIX"
        RowLayout {
            spacing: 16
            Repeater {
                model: [16, 20, 24, 32]
                delegate: ColumnLayout {
                    required property int modelData
                    IconTile { tileSize: modelData + 18; iconSize: modelData; source: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder-open.svg" }
                    Label { Layout.alignment: Qt.AlignHCenter; text: modelData + " px"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro }
                }
            }
        }
    }
}
