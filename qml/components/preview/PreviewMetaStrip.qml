import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../../style"

Rectangle {
    id: root

    property var items: []
    property bool compact: true
    property color accentColor: Theme.accent
    property int columnCount: 0
    property real backgroundOpacity: root.compact ? 0.88 : 0.92
    property real borderOpacity: themeController.isDark ? 0.70 : 0.82
    property int cornerRadius: Theme.radiusMd
    property int labelWeight: Font.Normal
    property bool showHideButton: false
    property bool wrapItems: false

    signal hideRequested()

    readonly property var visibleItems: {
        const source = Array.isArray(root.items) ? root.items : []
        const result = []
        for (let i = 0; i < source.length; i++) {
            const value = source[i] === undefined || source[i] === null ? "" : String(source[i])
            if (value.length > 0) {
                result.push(value)
            }
        }
        return result
    }
    readonly property int effectiveColumns: root.columnCount > 0
                                            ? Math.min(root.columnCount, Math.max(1, root.visibleItems.length))
                                            : Math.max(1, root.visibleItems.length)
    readonly property int effectiveRows: Math.max(1, Math.ceil(root.visibleItems.length / root.effectiveColumns))
    readonly property int lineHeight: root.compact ? 12 : (root.wrapItems ? 18 : 15)
    readonly property int verticalPadding: root.compact ? 7 : 9
    readonly property int rowGap: root.compact ? 0 : 4

    visible: visibleItems.length > 0
    height: root.wrapItems
            ? root.verticalPadding * 2 + Math.max(root.lineHeight, wrappedFlow.childrenRect.height)
            : root.verticalPadding * 2 + root.effectiveRows * root.lineHeight + (root.effectiveRows - 1) * root.rowGap
    radius: root.cornerRadius
    color: Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg, root.backgroundOpacity)
    border.color: Theme.withAlpha(Theme.border, root.borderOpacity)
    border.width: 1
    clip: true

    GridLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 8 : 12
        anchors.rightMargin: (root.compact ? 8 : 12) + (root.showHideButton ? 28 : 0)
        anchors.topMargin: root.verticalPadding
        anchors.bottomMargin: root.verticalPadding
        columns: root.effectiveColumns
        columnSpacing: root.compact ? 6 : 8
        rowSpacing: root.rowGap
        visible: !root.wrapItems

        Repeater {
            model: root.visibleItems

            Label {
                required property string modelData
                required property int index

                Layout.fillWidth: true
                Layout.maximumWidth: root.compact ? 96 : (root.columnCount > 0 ? 180 : 220)
                text: modelData
                color: index === 0 ? root.accentColor : Theme.textSecondary
                font.pixelSize: root.compact ? 9 : 11
                font.weight: index === 0 ? Font.Bold : root.labelWeight
                elide: Text.ElideRight
            }
        }
    }

    Flow {
        id: wrappedFlow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.compact ? 8 : 12
        anchors.rightMargin: (root.compact ? 8 : 12) + (root.showHideButton ? 28 : 0)
        anchors.topMargin: root.verticalPadding
        spacing: root.compact ? 6 : 14
        visible: root.wrapItems

        Repeater {
            model: root.visibleItems

            Label {
                required property string modelData
                required property int index

                width: Math.min(implicitWidth, root.compact ? 96 : 180)
                height: root.lineHeight
                text: modelData
                color: index === 0 ? root.accentColor : Theme.textSecondary
                font.pixelSize: root.compact ? 9 : 11
                font.weight: index === 0 ? Font.Bold : root.labelWeight
                elide: Text.ElideRight
            }
        }
    }

    ToolButton {
        id: hideButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: root.compact ? 4 : 6
        anchors.topMargin: root.compact ? 4 : 6
        width: root.compact ? 20 : 22
        height: width
        visible: root.showHideButton
        hoverEnabled: true
        padding: 4
        opacity: hovered ? 1.0 : 0.78
        display: AbstractButton.IconOnly
        ToolTip.visible: hovered
        ToolTip.text: "Hide metadata"
        onClicked: root.hideRequested()

        contentItem: Item {
            implicitWidth: root.compact ? 12 : 13
            implicitHeight: implicitWidth

            RecolorSvgIcon {
                anchors.centerIn: parent
                width: parent.implicitWidth
                height: parent.implicitHeight
                sourcePath: "qrc:/qt/qml/FM/qml/assets/toolbar-next/eye-off.svg"
                recolorColor: hideButton.hovered ? Theme.actionIconColor("hidden") : Theme.actionIconColor("muted")
                sourceSize: Qt.size(32, 32)
                opacity: hideButton.enabled ? 1.0 : 0.42
            }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: hideButton.hovered ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.12 : 0.08) : "transparent"
            border.color: hideButton.hovered ? Theme.withAlpha(Theme.border, 0.45) : "transparent"
            border.width: 1
        }
    }
}
