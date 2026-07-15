import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

ColumnLayout {
    id: storageDriveGrid

    required property var storageRoot
    required property var contextMenu
    spacing: 0

    readonly property int driveColumns: flowLayout.cols
    readonly property int portableColumns: portableFlow.cols
    readonly property real driveFlowImplicitHeight: flowLayout.implicitHeight
    readonly property real portableFlowImplicitHeight: portableFlow.implicitHeight
    function refreshPositioners() { flowLayout.forceLayout(); portableFlow.forceLayout() }
    function driveItemAt(position) { return drivesRepeater.itemAt(position) }
    function portableItemAt(position) { return portableRepeater.itemAt(position) }
// ── Drives Section Header ─────────────────────────────────────────
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
            radius: 2
            color: Theme.accent
        }

        Label {
            font.family: Theme.fontFamily
            text: "Devices and Drives"
            font.pixelSize: Theme.fontSizeBody
            font.bold: true
            color: Theme.textPrimary
        }
    }
}

// ── Drives Flow Layout ────────────────────────────────────────────
Flow {
    id: flowLayout
    Layout.fillWidth: true
    Layout.leftMargin: 16
    Layout.rightMargin: 16
    Layout.topMargin: 8
    Layout.bottomMargin: (16) + storageDriveGrid.storageRoot.gapAmount
    spacing: 12

    readonly property int minCardW: 280
    readonly property int cols: Math.max(1, Math.floor((width + spacing) / (minCardW + spacing)))
    readonly property real cardW: Math.floor((width - (cols - 1) * spacing) / cols)

    Repeater {
        id: drivesRepeater
        model: storageDriveGrid.storageRoot.driveIndexes
        delegate: Item {
            required property int index
            required property var modelData
            width: flowLayout.cardW
            height: card.height

            StorageDriveCard {
                id: card
                storageRoot: storageDriveGrid.storageRoot
                contextMenu: storageDriveGrid.contextMenu
                sourceIndex: parent.modelData
                cardWidth: flowLayout.cardW
                animationIndex: parent.index
            }
        } // end delegate
    } // end Repeater
} // end Flow

Item {
    Layout.fillWidth: true
    visible: storageDriveGrid.storageRoot.portableCount > 0
    implicitHeight: visible ? 32 : 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 8

        Rectangle {
            width: 4
            height: 14
            radius: Theme.radiusSm
            color: Theme.actionIconColor("media")
        }

        Label {
            font.family: Theme.fontFamily
            text: "Portable Media Devices"
            font.pixelSize: Theme.fontSizeBody
            font.bold: true
            color: Theme.textPrimary
        }
    }
}

Flow {
    id: portableFlow
    Layout.fillWidth: true
    Layout.leftMargin: 16
    Layout.rightMargin: 16
    Layout.topMargin: storageDriveGrid.storageRoot.portableCount > 0 ? 8 : 0
    Layout.bottomMargin: storageDriveGrid.storageRoot.portableCount > 0 ? 16 + storageDriveGrid.storageRoot.gapAmount : 0
    spacing: 12
    visible: storageDriveGrid.storageRoot.portableCount > 0

    readonly property int minCardW: 250
    readonly property int cols: Math.max(1, Math.floor((width + spacing) / (minCardW + spacing)))
    readonly property real cardW: Math.floor((width - (cols - 1) * spacing) / cols)

    Repeater {
        id: portableRepeater
        model: storageDriveGrid.storageRoot.portableIndexes
        delegate: Item {
            id: portableCardWrapper
            readonly property int sourceIndex: modelData
            readonly property string devicePath: storageDriveGrid.storageRoot.modelValue(sourceIndex, storageDriveGrid.storageRoot.pathRole, "")
            readonly property string deviceName: storageDriveGrid.storageRoot.modelValue(sourceIndex, storageDriveGrid.storageRoot.nameRole, "")
            readonly property string deviceType: storageDriveGrid.storageRoot.modelValue(sourceIndex, storageDriveGrid.storageRoot.driveTypeRole, "")
            readonly property string subtitle: storageDriveGrid.storageRoot.modelValue(sourceIndex, storageDriveGrid.storageRoot.subtitleRole, "")
            readonly property bool isReady: storageDriveGrid.storageRoot.modelValue(sourceIndex, storageDriveGrid.storageRoot.isReadyRole, true)
            property real appearOffsetY: 10
            width: portableFlow.cardW
            height: 76
            visible: true
            property bool isSelected: storageDriveGrid.storageRoot.currentPortableIndex === sourceIndex
            transform: Translate { y: portableCardWrapper.appearOffsetY }

            Rectangle {
                id: portableCardVisual
                x: 0
                y: !storageDriveGrid.storageRoot.effectsReduced && portableMouse.containsMouse
                    ? -2
                    : (portableCardWrapper.isSelected ? -1 : 0)
                width: parent.width
                height: parent.height
                radius: Theme.radiusSm
                scale: !storageDriveGrid.storageRoot.effectsReduced && portableMouse.containsMouse
                    ? 1.02
                    : (portableCardWrapper.isSelected ? 1.01 : 1.0)

                color: {
                    if (portableCardWrapper.isSelected) {
                        return themeController.isDark
                            ? Theme.withAlpha(Theme.panelSurface, 0.90)
                            : Theme.withAlpha(Theme.panelSurface, 0.97)
                    }
                    if (themeController.isDark) {
                        if (!storageDriveGrid.storageRoot.effectsReduced && portableMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.84)
                        return Theme.withAlpha(Theme.panelSurface, 0.62)
                    } else {
                        if (!storageDriveGrid.storageRoot.effectsReduced && portableMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.92)
                        return Theme.withAlpha(Theme.panelSurface, 0.74)
                    }
                }

                border.color: portableCardWrapper.isSelected
                    ? Theme.accent
                    : (!storageDriveGrid.storageRoot.effectsReduced && portableMouse.containsMouse
                        ? (themeController.isDark ? Theme.withAlpha(Theme.accent, 0.46) : Theme.withAlpha(Theme.accent, 0.36))
                        : Theme.panelBorder)
                border.width: portableCardWrapper.isSelected ? 1.5 : 1

                Behavior on color { enabled: !storageDriveGrid.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                Behavior on border.color { enabled: !storageDriveGrid.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
                Behavior on scale { enabled: !storageDriveGrid.storageRoot.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                Behavior on y { enabled: !storageDriveGrid.storageRoot.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            } // end portableCardVisual

            RowLayout {
                anchors.fill: portableCardVisual
                    anchors.margins: 10
                    spacing: 10

                    IconTile {
                        tileSize: 36
                        iconSize: 18
                        cornerRadius: Theme.radiusSm
                        source: storageDriveGrid.storageRoot.portableIconSource(portableCardWrapper.deviceType)
                        iconColor: storageDriveGrid.storageRoot.portableIconColor(portableCardWrapper.deviceType)
                        tileColor: Theme.withAlpha(
                            storageDriveGrid.storageRoot.portableIconColor(portableCardWrapper.deviceType),
                            (themeController.isDark ? 0.15 : 0.10)
                                + ((!storageDriveGrid.storageRoot.effectsReduced && portableMouse.containsMouse) || portableCardWrapper.isSelected ? 0.10 : 0))
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Label {
                                font.family: Theme.fontFamily
                                text: portableCardWrapper.deviceName || portableCardWrapper.devicePath
                                font.pixelSize: Theme.fontSizeLabel
                                font.bold: true
                                color: TextColors.thisPcText
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            InlineBadge {
                                text: "READ ONLY"
                                fillColor: Theme.withAlpha(storageDriveGrid.storageRoot.portableIconColor(portableCardWrapper.deviceType), themeController.isDark ? 0.18 : 0.12)
                                strokeColor: "transparent"
                                textColor: storageDriveGrid.storageRoot.portableIconColor(portableCardWrapper.deviceType)
                                horizontalPadding: 7
                                badgeHeight: 17
                                fontSize: 8
                                fontWeight: Font.Bold
                            }
                        }

                        Label {
                            font.family: Theme.fontFamily
                            text: portableCardWrapper.subtitle || "Portable media device"
                            font.pixelSize: Theme.fontSizeMicro
                            color: TextColors.thisPcText
                            opacity: 0.72
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                MouseArea {
                    id: portableMouse
                    anchors.fill: parent
                    hoverEnabled: !storageDriveGrid.storageRoot.effectsReduced
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton

                    onClicked: function(mouse) {
                        if (storageDriveGrid.storageRoot.panel) storageDriveGrid.storageRoot.panel.activated()
                        storageDriveGrid.storageRoot.forceActiveFocus()
                        storageDriveGrid.storageRoot.currentDriveIndex = -1
                        storageDriveGrid.storageRoot.currentPortableIndex = portableCardWrapper.sourceIndex
                        storageDriveGrid.storageRoot.currentFolderIndex = -1
                        quickLookController.preview(portableCardWrapper.devicePath)
                    }

                    onDoubleClicked: function(mouse) {
                        if (!portableCardWrapper.isReady) return
                        storageDriveGrid.storageRoot.controller.openPath(portableCardWrapper.devicePath)
                    }
                }

            opacity: 0
            Component.onCompleted: {
                if (storageDriveGrid.storageRoot.effectsReduced) {
                    opacity = 1
                    appearOffsetY = 0
                } else {
                    portableAppearAnim.start()
                }
            }

            ParallelAnimation {
                id: portableAppearAnim
                NumberAnimation {
                    target: portableCardWrapper
                    property: "opacity"
                    from: 0; to: 1
                    duration: 260 + (index % 6) * 40
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: portableCardWrapper
                    property: "appearOffsetY"
                    from: 10; to: 0
                    duration: 300 + (index % 6) * 40
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}

}
