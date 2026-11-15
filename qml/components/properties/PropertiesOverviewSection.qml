import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

Rectangle {
    id: root

    required property var section
    property var menuBackdropSource: null
    property string identityIconName: "document"
    readonly property bool identity: section.layout === "identity"
    readonly property bool statistics: section.layout === "statistics"

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + 20
    radius: Theme.radiusMd
    color: root.statistics ? Qt.tint(Theme.panelSurface, Theme.withAlpha(Theme.accent, themeController.isDark ? 0.055 : 0.035))
                           : Theme.panelSurface
    border.width: 1
    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.22 : 0.18)

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4

        RowLayout {
            visible: root.section.title.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.bottomMargin: 4
            spacing: 8

            RecolorSvgIcon {
                visible: root.section.icon.length > 0
                sourcePath: root.section.icon.length > 0
                            ? "qrc:/qt/qml/FM/qml/assets/icons-classic/" + root.section.icon + ".svg" : ""
                recolorColor: Theme.accent
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
            }
            Label {
                Layout.fillWidth: true
                text: root.section.title.toUpperCase()
                color: Theme.accent
                font.pixelSize: Theme.fontSizeCaption
                font.weight: Font.DemiBold
                font.letterSpacing: 0.8
            }
        }

        GridLayout {
            id: fields
            Layout.fillWidth: true
            columns: width >= 320 && (root.identity || (root.statistics && root.section.rows.length > 1)) ? 2 : 1
            uniformCellWidths: root.statistics
            columnSpacing: 12
            rowSpacing: root.statistics ? 8 : 2

            Repeater {
                model: root.section.rows
                DialogListRow {
                    required property var modelData
                    required property int index
                    readonly property string rowKey: modelData.key || ""
                    readonly property bool identityName: rowKey === "general.name" || rowKey === "selection.count"
                    readonly property bool hero: identityName || rowKey === "details.title"
                    readonly property bool typeRow: rowKey === "general.type" || rowKey === "selection.contains"
                    readonly property bool contentsRow: rowKey === "general.contents" || rowKey === "selection.contents"
                    readonly property bool noteRow: modelData.note === true
                    readonly property bool compactRow: modelData.compact === true || noteRow

                    Layout.columnSpan: noteRow || (root.identity && !identityName && !typeRow)
                                       || (root.statistics && index === root.section.rows.length - 1
                                           && root.section.rows.length % 2 === 1) ? fields.columns : 1
                    Layout.fillWidth: !typeRow || fields.columns === 1
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: root.identity && typeRow && fields.columns === 2
                                           ? Math.min(90, fields.width * 0.24) : 1
                    Layout.alignment: typeRow ? Qt.AlignVCenter : Qt.AlignTop
                    menuBackdropSource: root.menuBackdropSource
                    grouped: true
                    stacked: root.statistics
                    label: modelData.label || ""
                    value: modelData.value || ""
                    showLabel: !identityName && !typeRow
                    isLink: rowKey === "general.location" || rowKey === "general.fullPath"
                            || rowKey === "general.root" || rowKey === "selection.location"
                    valueMaximumLineCount: noteRow ? 12 : (isLink ? 4 : 3)
                    showBusy: modelData.busy === true
                    emphasizeValue: modelData.emphasize === true || hero || (root.statistics && !contentsRow && !compactRow)
                    valuePixelSize: noteRow ? Theme.fontSizeCaption
                                    : hero || (root.statistics && !contentsRow && !compactRow) ? Theme.fontSizeTitle : Theme.fontSizeLabel
                    valueColor: typeRow || noteRow ? TextColors.dialogSecondaryText : Theme.textPrimary
                    iconSource: identityName
                                ? "qrc:/qt/qml/FM/qml/assets/icons-classic/" + root.identityIconName + ".svg" : ""
                    iconPixelSize: identityName ? 28 : 14

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        height: 1
                        visible: root.section.layout === "rows" && index < root.section.rows.length - 1
                        color: Theme.withAlpha(Theme.accent, 0.16)
                    }
                }
            }
        }
    }
}
