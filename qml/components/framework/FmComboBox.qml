import QtQuick
import QtQuick.Controls
import FM
import "../../style"

ComboBox {
    id: root

    property color accentColor: Theme.accent
    property real paintActivation: root.opened ? 1 : (root.down ? 0.72 : (root.hovered ? 0.38 : 0))
    property real paintArrowPosition: root.opened ? 1 : 0

    hoverEnabled: true
    implicitWidth: Math.max(120, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 36
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeLabel
    font.weight: Font.Medium

    delegate: FmMenuItem {
        required property int index

        width: ListView.view ? ListView.view.width : root.width
        height: Math.max(34, Theme.controlHeight - 4)
        useHighlightedState: true
        highlighted: root.highlightedIndex === index
        active: root.currentIndex === index
        text: root.textAt(index)
        font: root.font
    }

    indicator: Item {
        x: root.mirrored ? root.leftPadding : root.width - width
        y: 0
        implicitWidth: 30
        implicitHeight: root.height
    }

    contentItem: TextField {
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        text: root.editable ? root.editText : root.displayText
        enabled: root.editable
        autoScroll: root.editable
        readOnly: root.down
        inputMethodHints: root.inputMethodHints
        validator: root.validator
        selectByMouse: root.selectTextByMouse
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        selectionColor: root.accentColor
        selectedTextColor: Theme.readableOn(root.accentColor, Theme.textPrimary)
        font: root.font
        verticalAlignment: Text.AlignVCenter
        background: null
    }

    background: FmComboBoxVisual {
        implicitWidth: 120
        implicitHeight: Theme.controlHeight
        textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        enabled: root.enabled
        activation: root.paintActivation
        arrowPosition: root.paintArrowPosition
        active: root.opened || root.activeFocus
        surfaceColor: Theme.mixColors(Theme.panelSurfaceStrong, Theme.panelSurface, 0.52)
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.68 : 0.78)
        accentColor: root.accentColor
        indicatorColor: root.enabled ? Theme.textPrimary : Theme.textSecondary
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 300)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ListView {
            id: popupList
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            highlightMoveDuration: 0
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: Math.max(0, height * 2)
            pixelAligned: false

            WheelHandler {
                target: null
                blocking: true
                onWheel: (event) => popupScrollBar.routeWheel(event)
            }

            ScrollBar.vertical: FmScrollBar {
                id: popupScrollBar
                flat: true
                wheelTarget: popupList
            }
        }

        background: FmMenuVisual {
            textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
            surfaceColor: Theme.menuSurface
            borderColor: Theme.mixColors(Theme.menuBorder, root.accentColor, 0.18)
            accentColor: root.accentColor
            shadowColor: Theme.shadow
            dark: themeController.isDark
        }
    }

    Behavior on paintActivation {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    Behavior on paintArrowPosition {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }
}
