import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

ColumnLayout {
    id: quickAccessGrid

    required property var storageRoot
    spacing: 0

    readonly property int columns: quickAccessFlow.cols
    readonly property real flowImplicitHeight: quickAccessFlow.implicitHeight
    function refreshPositioner() { quickAccessFlow.forceLayout() }
    function itemAt(position) { return foldersRepeater.itemAt(position) }
// ── Quick Access Section Header ───────────────────────────────────
Item {
    Layout.fillWidth: true
    implicitHeight: 32

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 8

        Rectangle {
            width: 4
            height: 14
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.accent, 0.92)
        }

        Label {
            font.family: Theme.fontFamily
            text: "Quick Access"
            font.pixelSize: Theme.fontSizeBody
            font.bold: true
            color: Theme.textPrimary
        }
    }
}

// ── Quick Access Flow Layout ──────────────────────────────────────
Flow {
    id: quickAccessFlow
    Layout.fillWidth: true
    Layout.leftMargin: 16
    Layout.rightMargin: 16
    Layout.topMargin: 8
    Layout.bottomMargin: 16 + quickAccessGrid.storageRoot.gapAmount
    spacing: 12

    readonly property int minCardW: 180
    readonly property int cols: Math.max(1, Math.floor((width + spacing) / (minCardW + spacing)))
    readonly property real cardW: Math.floor((width - (cols - 1) * spacing) / cols)

    Repeater {
        id: foldersRepeater
        model: quickAccessGrid.storageRoot.folderIndexes
        delegate: Item {
            required property int index
            required property var modelData
            width: quickAccessFlow.cardW
            height: card.height

            QuickAccessCard {
                id: card
                storageRoot: quickAccessGrid.storageRoot
                sourceIndex: parent.modelData
                cardWidth: quickAccessFlow.cardW
                animationIndex: parent.index
            }
        }
    }
}
}
