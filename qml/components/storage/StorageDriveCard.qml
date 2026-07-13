import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Item {
id: cardWrapper
    required property var storageRoot
    required property var contextMenu
    required property int sourceIndex
    required property real cardWidth
    required property int animationIndex
readonly property string drivePath: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.pathRole, "")
readonly property string driveType: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.driveTypeRole, "")
readonly property bool isReady: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.isReadyRole, false)
readonly property bool isCritical: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.isCriticalRole, false)
readonly property bool canEject: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.canEjectRole, false)
readonly property bool canUnmount: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.canUnmountRole, false)
readonly property bool canSafelyRemove: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.canSafelyRemoveRole, false)
readonly property bool canMount: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.canMountRole, false)
readonly property string mountId: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.mountIdRole, "")
readonly property bool actionPending: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.actionPendingRole, false)
readonly property string deviceDescription: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.deviceDescriptionRole, "")
readonly property real usagePercent: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.usagePercentRole, 0)
readonly property real freeSpace: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.freeSpaceRole, 0)
readonly property real totalSpace: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.totalSpaceRole, 0)
readonly property string driveName: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.nameRole, "")
readonly property string fileSystem: cardWrapper.storageRoot.modelValue(sourceIndex, cardWrapper.storageRoot.fileSystemRole, "")
width: cardWrapper.cardWidth
height: cardWrapper.storageRoot.ultraLightMode ? 82 : 108
visible: true

// Shadow underlay (no children)
Rectangle {
    id: cardShadow
    anchors.fill: cardVisual
    radius: cardVisual.radius
    scale: cardVisual.scale
    color: "transparent"

    layer.enabled: !cardWrapper.storageRoot.effectsReduced
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: themeController.isDark
            ? Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.34 : (cardWrapper.isSelected ? 0.30 : 0.12))
            : Qt.rgba(0, 0, 0, cardMouse.containsMouse ? 0.10 : (cardWrapper.isSelected ? 0.08 : 0.03))
        shadowBlur: cardMouse.containsMouse ? 0.8 : (cardWrapper.isSelected ? 0.7 : 0.4)
        shadowVerticalOffset: cardMouse.containsMouse ? 5 : (cardWrapper.isSelected ? 3 : 2)
        shadowHorizontalOffset: 0
    }
}

Rectangle {
    id: cardVisual
    x: 0
    y: !cardWrapper.storageRoot.effectsReduced && cardMouse.containsMouse ? -2 : 0
    width: parent.width
    height: parent.height
    radius: Theme.radiusMd
    scale: !cardWrapper.storageRoot.effectsReduced && cardMouse.containsMouse
        ? 1.02
        : (cardWrapper.isSelected ? 1.01 : 1.0)

    color: {
        if (themeController.isDark) {
            if (!cardWrapper.storageRoot.effectsReduced && cardMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.86)
            return Theme.withAlpha(Theme.panelSurface, 0.62)
        } else {
            if (!cardWrapper.storageRoot.effectsReduced && cardMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.94)
            return Theme.withAlpha(Theme.panelSurface, 0.74)
        }
    }

    border.color: {
        if (cardWrapper.isSelected) {
            return Theme.accent
        }
        if (!cardWrapper.storageRoot.effectsReduced && cardMouse.containsMouse) {
            return themeController.isDark
                ? Theme.withAlpha(Theme.accent, 0.46)
                : Theme.withAlpha(Theme.accent, 0.36)
        }
        return Theme.panelBorder
    }
    border.width: cardWrapper.isSelected ? 1.5 : 1

    Behavior on color { enabled: !cardWrapper.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
    Behavior on border.color { enabled: !cardWrapper.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
    Behavior on scale { enabled: !cardWrapper.storageRoot.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: !cardWrapper.storageRoot.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
} // end cardVisual

RowLayout {
    anchors.fill: cardVisual
        anchors.margins: cardWrapper.storageRoot.ultraLightMode ? 10 : 14
        spacing: cardWrapper.storageRoot.ultraLightMode ? 10 : 14

        // Drive icon column
        Item {
            Layout.preferredWidth: cardWrapper.storageRoot.ultraLightMode ? 38 : 48
            Layout.alignment: Qt.AlignVCenter

            IconTile {
                anchors.centerIn: parent
                tileSize: cardWrapper.storageRoot.ultraLightMode ? 34 : 44
                iconSize: cardWrapper.storageRoot.ultraLightMode ? 19 : 24
                cornerRadius: Theme.radiusMd
                source: cardWrapper.storageRoot.driveIconSource(cardWrapper.driveType)
                iconColor: cardWrapper.storageRoot.driveIconColor(cardWrapper.driveType)
                tileColor: Theme.withAlpha(
                    cardWrapper.storageRoot.driveIconColor(cardWrapper.driveType),
                    (themeController.isDark ? 0.18 : 0.12)
                        + (!cardWrapper.storageRoot.effectsReduced && cardMouse.containsMouse ? 0.08 : 0))

                Behavior on tileColor { enabled: !cardWrapper.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
            }
        }

        // Info column
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: cardWrapper.storageRoot.ultraLightMode ? 4 : 5

            // Drive name + FS badge row
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    font.family: Theme.fontFamily
                    text: cardWrapper.driveName || cardWrapper.drivePath
                    font.pixelSize: Theme.fontSizeBody
                    font.bold: true
                    color: TextColors.thisPcText
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // FS badge
                InlineBadge {
                    visible: !cardWrapper.storageRoot.ultraLightMode && cardWrapper.fileSystem && cardWrapper.fileSystem.length > 0
                    text: cardWrapper.fileSystem || ""
                    fillColor: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.12)
                    strokeColor: "transparent"
                    textColor: Theme.accent
                    horizontalPadding: 8
                    badgeHeight: 17
                    fontSize: 9
                    fontWeight: Font.Bold
                    letterSpacing: 0.5
                }
            }

            // Free space text
            Label {
                font.family: Theme.fontFamily
                text: cardWrapper.isReady
                    ? (cardWrapper.storageRoot.formatBytes(cardWrapper.freeSpace) + " free of " + cardWrapper.storageRoot.formatBytes(cardWrapper.totalSpace)
                       + (cardWrapper.drivePath.length > 0 ? " • " + cardWrapper.storageRoot.displayPath(cardWrapper.drivePath) : ""))
                    : (cardWrapper.canMount ? "Not mounted" : "Not ready")
                font.pixelSize: Theme.fontSizeCaption
                color: cardWrapper.isCritical ? Theme.danger : TextColors.thisPcText
                opacity: 0.88
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Progress bar
            LinearProgress {
                Layout.fillWidth: true
                value: cardWrapper.isReady ? cardWrapper.usagePercent : 0
                trackColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.55)
                fillColor: cardWrapper.storageRoot.progressColor(cardWrapper.usagePercent, cardWrapper.isCritical)
                preserveMinimumFill: true
            }

            // Drive type tag + percent row
            RowLayout {
                Layout.fillWidth: true
                visible: !cardWrapper.storageRoot.ultraLightMode
                spacing: 4

                Label {
                    font.family: Theme.fontFamily
                    text: cardWrapper.storageRoot.driveTypeLabel(cardWrapper.driveType)
                        + (cardWrapper.deviceDescription.length > 0
                           ? " • " + cardWrapper.deviceDescription : "")
                    font.pixelSize: Theme.fontSizeMicro
                    font.bold: true
                    font.letterSpacing: 0.8
                    color: cardWrapper.storageRoot.driveIconColor(cardWrapper.driveType)
                    opacity: 0.82
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // Warning icon for critical
                Label {
                    font.family: Theme.fontFamily
                    text: "⚠"
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.danger
                    visible: cardWrapper.isCritical
                }

                Label {
                    font.family: Theme.fontFamily
                    text: cardWrapper.isReady
                        ? (Math.round(cardWrapper.usagePercent * 100) + "% used")
                        : "—"
                    font.pixelSize: Theme.fontSizeMicro
                    color: cardWrapper.isCritical ? Theme.danger : TextColors.thisPcText
                    opacity: 0.75
                }
            }
        }
    }

    // Mouse interaction
    MouseArea {
        id: cardMouse
        anchors.fill: parent
        hoverEnabled: !cardWrapper.storageRoot.effectsReduced
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor

        onClicked: function(mouse) {
            if (cardWrapper.storageRoot.panel) cardWrapper.storageRoot.panel.activated()
            cardWrapper.storageRoot.forceActiveFocus()
            cardWrapper.storageRoot.currentDriveIndex = cardWrapper.sourceIndex
            cardWrapper.storageRoot.currentPortableIndex = -1
            cardWrapper.storageRoot.currentFolderIndex = -1
            if (mouse.button === Qt.RightButton) {
                cardWrapper.contextMenu.driveIndex = cardWrapper.sourceIndex
                cardWrapper.contextMenu.drivePath  = cardWrapper.drivePath
                cardWrapper.contextMenu.driveType  = cardWrapper.driveType
                cardWrapper.contextMenu.canEject = cardWrapper.canEject
                cardWrapper.contextMenu.canUnmount = cardWrapper.canUnmount
                cardWrapper.contextMenu.canSafelyRemove = cardWrapper.canSafelyRemove
                cardWrapper.contextMenu.canMount = cardWrapper.canMount
                cardWrapper.contextMenu.mountId = cardWrapper.mountId
                cardWrapper.contextMenu.actionPending = cardWrapper.actionPending
                cardWrapper.contextMenu.managedIsoMount = workspaceController.isManagedIsoMountRoot(cardWrapper.drivePath)
                cardWrapper.contextMenu.popup()
            } else if (cardWrapper.canMount) {
                workspaceController.requestMountVolume(cardWrapper.mountId)
            } else {
                cardWrapper.storageRoot.previewDrive(cardWrapper.sourceIndex)
            }
        }

        onDoubleClicked: function(mouse) {
            if (!cardWrapper.isReady) return
            cardWrapper.storageRoot.controller.openPath(cardWrapper.drivePath)
        }
    }

// Card appear animation
opacity: 0
Component.onCompleted: {
    if (cardWrapper.storageRoot.effectsReduced) {
        opacity = 1
    } else {
        appearAnim.start()
    }
}

NumberAnimation {
    id: appearAnim
    target: cardWrapper
    property: "opacity"
    from: 0; to: 1
    duration: 250 + (animationIndex % 6) * 40
    easing.type: Easing.OutCubic
}

property bool isSelected: cardWrapper.storageRoot.currentDriveIndex === sourceIndex
        }
