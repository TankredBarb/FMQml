import QtQuick
import QtQuick.Controls
import "../../style"
import "../common"

ComboBox {
    id: combo

    delegate: ItemDelegate {
        width: combo.width
        height: Math.max(34, Theme.controlHeight - 4)
        highlighted: combo.highlightedIndex === index
        contentItem: Label {
            text: modelData.label
            color: highlighted ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: highlighted ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            leftPadding: 8
        }
        background: Rectangle {
            radius: Theme.radiusSm
            color: highlighted || hovered ? Theme.menuItemHover : "transparent"
        }
    }

    indicator: RecolorSvgIcon {
        x: combo.width - width - 10
        y: Math.round((combo.height - height) / 2)
        width: 10
        height: 10
        sourcePath: "../../assets/icons/arrow-up.svg"
        sourceSize: Qt.size(16, 16)
        recolorEnabled: true
        recolorColor: Theme.textSecondary
        rotation: combo.opened ? 0 : 180
        opacity: 0.72
    }

    contentItem: Label {
        leftPadding: 10
        rightPadding: 24
        text: combo.displayText
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
        font.weight: Font.Medium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSm
        color: Theme.panelSurfaceSoft
        border.color: combo.opened ? Theme.accent : Theme.panelBorder
        border.width: combo.opened ? 2 : 1
    }

    popup: Popup {
        y: combo.height + 4
        width: combo.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: combo.popup.visible ? combo.delegateModel : null
            currentIndex: combo.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            color: Theme.menuSurface
            radius: Theme.radiusSm
            border.color: Theme.menuBorder
        }
    }
}
