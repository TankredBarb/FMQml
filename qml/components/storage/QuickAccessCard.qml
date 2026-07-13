import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Item {
id: folderCardWrapper
    required property var storageRoot
    required property int sourceIndex
    required property real cardWidth
    required property int animationIndex
readonly property string folderPath: folderCardWrapper.storageRoot.modelValue(sourceIndex, folderCardWrapper.storageRoot.pathRole, "")
readonly property string folderName: folderCardWrapper.storageRoot.modelValue(sourceIndex, folderCardWrapper.storageRoot.nameRole, "")
readonly property string folderIcon: folderCardWrapper.storageRoot.modelValue(sourceIndex, folderCardWrapper.storageRoot.iconRole, "")
property real appearOffsetY: 10
width: folderCardWrapper.cardWidth
height: folderCardWrapper.storageRoot.ultraLightMode ? 52 : 68
visible: true
property bool isSelected: folderCardWrapper.storageRoot.currentFolderIndex === sourceIndex
transform: Translate { y: folderCardWrapper.appearOffsetY }

// Shadow underlay (no children)
Rectangle {
    id: folderCardShadow
    anchors.fill: folderCardVisual
    radius: folderCardVisual.radius
    scale: folderCardVisual.scale
    color: "transparent"

    layer.enabled: !folderCardWrapper.storageRoot.effectsReduced
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: themeController.isDark
            ? Qt.rgba(0, 0, 0, (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.32 : 0.10)
            : Qt.rgba(0, 0, 0, (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.08 : 0.02)
        shadowBlur: (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 0.6 : 0.3
        shadowVerticalOffset: (folderMouse.containsMouse || folderCardWrapper.isSelected) ? 4 : 2
    }
}

Rectangle {
    id: folderCardVisual
    x: 0
    y: !folderCardWrapper.storageRoot.effectsReduced && folderMouse.containsMouse
        ? -2
        : (folderCardWrapper.isSelected ? -1 : 0)
    width: parent.width
    height: parent.height
    radius: Theme.radiusSm
    scale: !folderCardWrapper.storageRoot.effectsReduced && folderMouse.containsMouse
        ? 1.02
        : (folderCardWrapper.isSelected ? 1.01 : 1.0)

    color: {
        if (folderCardWrapper.isSelected) {
            return themeController.isDark
                ? Theme.withAlpha(Theme.panelSurface, 0.90)
                : Theme.withAlpha(Theme.panelSurface, 0.97)
        }
        if (themeController.isDark) {
            if (!folderCardWrapper.storageRoot.effectsReduced && folderMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.84)
            return Theme.withAlpha(Theme.panelSurface, 0.62)
        } else {
            if (!folderCardWrapper.storageRoot.effectsReduced && folderMouse.containsMouse) return Theme.withAlpha(Theme.panelSurface, 0.92)
            return Theme.withAlpha(Theme.panelSurface, 0.74)
        }
    }

    border.color: folderCardWrapper.isSelected
        ? Theme.accent
        : (!folderCardWrapper.storageRoot.effectsReduced && folderMouse.containsMouse
            ? (themeController.isDark ? Theme.withAlpha(Theme.accent, 0.46) : Theme.withAlpha(Theme.accent, 0.36))
            : Theme.panelBorder)
    border.width: folderCardWrapper.isSelected ? 1.5 : 1

    Behavior on color { enabled: !folderCardWrapper.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
    Behavior on border.color { enabled: !folderCardWrapper.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
    Behavior on scale { enabled: !folderCardWrapper.storageRoot.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: !folderCardWrapper.storageRoot.effectsReduced; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
} // end folderCardVisual

RowLayout {
    anchors.fill: folderCardVisual
        anchors.margins: folderCardWrapper.storageRoot.ultraLightMode ? 8 : 10
        spacing: folderCardWrapper.storageRoot.ultraLightMode ? 8 : 10

        IconTile {
            tileSize: folderCardWrapper.storageRoot.ultraLightMode ? 28 : 32
            iconSize: folderCardWrapper.storageRoot.ultraLightMode ? 14 : 16
            cornerRadius: Theme.radiusSm
            source: folderCardWrapper.storageRoot.folderIconSource(folderCardWrapper.folderIcon)
            useOriginalColor: folderCardWrapper.folderIcon === "gdrive" || folderCardWrapper.folderIcon === "mega"
            iconColor: folderCardWrapper.storageRoot.folderIconColor(folderCardWrapper.folderIcon)
            tileColor: Theme.withAlpha(
                folderCardWrapper.storageRoot.folderIconColor(folderCardWrapper.folderIcon),
                (themeController.isDark ? 0.15 : 0.10)
                    + ((!folderCardWrapper.storageRoot.effectsReduced && folderMouse.containsMouse) || folderCardWrapper.isSelected ? 0.10 : 0))

            Behavior on tileColor { enabled: !folderCardWrapper.storageRoot.effectsReduced; ColorAnimation { duration: Theme.motionFast } }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Label {
                font.family: Theme.fontFamily
                text: folderCardWrapper.folderName
                font.pixelSize: Theme.fontSizeLabel
                font.bold: true
                color: TextColors.thisPcText
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label {
                font.family: Theme.fontFamily
                visible: !folderCardWrapper.storageRoot.ultraLightMode
                text: "System Folder"
                font.pixelSize: Theme.fontSizeMicro
                color: TextColors.thisPcText
                opacity: 0.6
            }
        }
    }

    MouseArea {
        id: folderMouse
        anchors.fill: parent
        hoverEnabled: !folderCardWrapper.storageRoot.effectsReduced
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: function(mouse) {
            if (folderCardWrapper.storageRoot.panel) folderCardWrapper.storageRoot.panel.activated()
            folderCardWrapper.storageRoot.forceActiveFocus()
            if (mouse.button === Qt.RightButton) return
            folderCardWrapper.storageRoot.currentDriveIndex = -1
            folderCardWrapper.storageRoot.currentPortableIndex = -1
            folderCardWrapper.storageRoot.currentFolderIndex = folderCardWrapper.sourceIndex
            quickLookController.preview(folderCardWrapper.folderPath)
        }

        onDoubleClicked: function(mouse) {
            folderCardWrapper.storageRoot.controller.openPath(folderCardWrapper.folderPath)
        }
    }

// Staggered fade-in/slide-up animation
opacity: 0
Component.onCompleted: {
    if (folderCardWrapper.storageRoot.effectsReduced) {
        opacity = 1
        appearOffsetY = 0
    } else {
        folderAppearAnim.start()
    }
}

ParallelAnimation {
    id: folderAppearAnim
    NumberAnimation {
        target: folderCardWrapper
        property: "opacity"
        from: 0; to: 1
        duration: 300 + (animationIndex % 6) * 40
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        target: folderCardWrapper
        property: "appearOffsetY"
        from: 10; to: 0
        duration: 350 + (animationIndex % 6) * 40
        easing.type: Easing.OutCubic
    }
}
        }
