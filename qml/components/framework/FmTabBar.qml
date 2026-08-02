import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../../style"

Control {
    id: root

    property var model: []
    property int currentIndex: 0
    property string textRole: "text"
    property string valueRole: "value"
    property color accentColor: Theme.accent
    property var hoveredTab: null
    property int delegateRevision: 0
    readonly property var activeTab: {
        root.delegateRevision
        return tabRepeater.itemAt(currentIndex)
    }
    signal activated(int index, var value)

    function itemText(item) {
        if (typeof item === "string" || typeof item === "number")
            return String(item)
        return item && item[textRole] !== undefined ? String(item[textRole]) : ""
    }

    function itemValue(item, index) {
        return item && typeof item === "object" && item[valueRole] !== undefined
               ? item[valueRole] : index
    }

    implicitWidth: 320
    implicitHeight: 40
    padding: 4

    background: FmTabBarVisual {
        id: tabVisual
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        highlightX: root.activeTab ? root.activeTab.x + root.leftPadding : root.leftPadding
        highlightWidth: root.activeTab ? root.activeTab.width : 0
        hoverX: root.hoveredTab ? root.hoveredTab.x + root.leftPadding : root.leftPadding
        hoverWidth: root.hoveredTab ? root.hoveredTab.width : 0
        hoverAmount: root.hoveredTab && root.hoveredTab !== root.activeTab ? 1 : 0
        surfaceColor: Theme.panelSurface
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.90 : 0.78)
        accentColor: root.accentColor

        Behavior on highlightX { NumberAnimation { duration: 280; easing.type: Easing.InOutCubic } }
        Behavior on highlightWidth { NumberAnimation { duration: 280; easing.type: Easing.InOutCubic } }
        Behavior on hoverAmount { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }

    contentItem: RowLayout {
        id: tabRow
        spacing: 4

        Repeater {
            id: tabRepeater
            model: root.model

            delegate: Button {
                id: tabButton
                required property int index
                required property var modelData
                readonly property bool active: root.currentIndex === index

                Layout.fillWidth: true
                implicitHeight: 32
                padding: 0
                hoverEnabled: true
                background: null
                Component.onCompleted: root.delegateRevision++
                contentItem: Label {
                    text: root.itemText(tabButton.modelData)
                    color: tabButton.active ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                onHoveredChanged: {
                    if (hovered)
                        root.hoveredTab = tabButton
                    else if (root.hoveredTab === tabButton)
                        root.hoveredTab = null
                }
            onClicked: {
                root.activated(index, root.itemValue(modelData, index))
                if (root.currentIndex !== index)
                    root.currentIndex = index
            }
            }
        }
    }
}
