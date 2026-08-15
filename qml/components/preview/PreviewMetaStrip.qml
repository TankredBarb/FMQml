import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../framework"
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
    property bool floatingCard: false

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
    readonly property var floatingItems: {
        const source = Array.isArray(root.items) ? root.items : []
        const result = []
        for (let i = 0; i < source.length; ++i) {
            const item = source[i]
            const label = item && typeof item === "object" ? String(item.label || "") : ""
            const value = item && typeof item === "object" ? String(item.value || "") : String(item || "")
            if (value.length > 0) result.push({ label: label, value: value })
        }
        return result
    }
    readonly property int floatingColumns: root.compact
                                           ? Math.max(1, Math.min(root.floatingItems.length, 4))
                                           : Math.max(1, Math.min(
                                                          root.floatingItems.length,
                                                          10,
                                                          Math.floor(Math.max(84, root.width - 40) / 88)))
    readonly property int floatingRows: Math.max(1, Math.ceil(root.floatingItems.length / root.floatingColumns))

    visible: visibleItems.length > 0
    height: root.floatingCard
            ? (root.compact ? 40 : root.floatingRows * 29 + 20)
            : root.wrapItems
            ? root.verticalPadding * 2 + Math.max(root.lineHeight, wrappedFlow.childrenRect.height)
            : root.verticalPadding * 2 + root.effectiveRows * root.lineHeight + (root.effectiveRows - 1) * root.rowGap
    radius: root.cornerRadius
    color: root.floatingCard ? "transparent"
                             : Theme.withAlpha(themeController.isDark ? Theme.surface : Theme.bg, root.backgroundOpacity)
    border.color: root.floatingCard ? "transparent" : Theme.withAlpha(Theme.border, root.borderOpacity)
    border.width: root.floatingCard ? 0 : 1
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
        visible: !root.floatingCard && !root.wrapItems

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
        anchors.right: root.floatingCard ? floatingSurface.right : parent.right
        anchors.top: root.floatingCard ? floatingSurface.top : parent.top
        anchors.leftMargin: root.compact ? 8 : 12
        anchors.rightMargin: (root.compact ? 8 : 12) + (root.showHideButton ? 28 : 0)
        anchors.topMargin: root.verticalPadding
        spacing: root.compact ? 6 : 14
        visible: !root.floatingCard && root.wrapItems

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

    Rectangle {
        id: floatingSurface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 8
        width: Math.min(parent.width - 16,
                        root.floatingColumns * (root.compact ? 88 : 94)
                        + (root.showHideButton ? 30 : 0) + 16)
        height: parent.height - 8
        visible: root.floatingCard
        radius: Theme.radiusMd
        color: Theme.mixColors(
                   Theme.withAlpha(Theme.panelSurfaceStrong,
                                   themeController.isDark ? 0.70 : 0.76),
                   Theme.withAlpha(root.accentColor, 1.0),
                   themeController.isDark ? 0.055 : 0.035)
        border.width: 1
        border.color: Theme.withAlpha(root.accentColor,
                                      themeController.isDark ? 0.25 : 0.18)

        GridLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 10
            anchors.rightMargin: root.showHideButton ? 34 : 10
            anchors.topMargin: root.compact ? 9 : 7
            columns: root.floatingColumns
            columnSpacing: root.compact ? 8 : 10
            rowSpacing: 2

            Repeater {
                model: root.floatingItems

                ColumnLayout {
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: modelData.label
                        visible: !root.compact && text.length > 0
                        color: Theme.withAlpha(Theme.textSecondary, 0.84)
                        font.pixelSize: root.compact ? 8 : 9
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: modelData.value
                        color: index === 0 ? root.accentColor : Theme.textPrimary
                        font.pixelSize: root.compact ? 10 : 10
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    FmIconButton {
        id: hideButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: root.compact ? 4 : 6
        anchors.topMargin: root.compact ? 4 : 6
        width: root.compact ? 20 : 22
        height: width
        visible: root.showHideButton
        hoverEnabled: true
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/eye-off.svg"
        iconSize: root.compact ? 12 : 13
        svgRecolorColor: hideButton.hovered ? Theme.chromeIconColor("hidden") : Theme.chromeIconColor("muted")
        opacity: hovered ? 1.0 : 0.78
        display: AbstractButton.IconOnly
        ToolTip.visible: hovered
        ToolTip.text: "Hide metadata"
        onClicked: root.hideRequested()

        z: 2

    }
}
