import QtQuick
import QtQuick.Controls
import "../../style"

Item {
    id: root

    property var dragCoordinator
    property bool useNativeIcons: true
    readonly property bool active: dragCoordinator && dragCoordinator.active
    readonly property int itemCount: active ? dragCoordinator.itemCount : 0
    readonly property var previewItems: active ? dragCoordinator.dragItems.slice(0, 3) : []
    readonly property real targetX: active ? dragCoordinator.pointerX : 0
    readonly property real targetY: active ? dragCoordinator.pointerY : 0

    width: 68
    height: 52
    x: Math.min(Math.max(8, targetX + 14), parent ? Math.max(8, parent.width - width - 8) : targetX + 14)
    y: Math.min(Math.max(8, targetY + 14), parent ? Math.max(8, parent.height - height - 8) : targetY + 14)
    visible: active
    opacity: active ? 1 : 0
    z: 40

    Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }

    Repeater {
        model: root.previewItems

        Rectangle {
            width: 38
            height: 38
            x: index * 7
            y: index * 4
            radius: Theme.radiusSm
            color: Theme.panelSurfaceStrong
            border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.52 : 0.42)
            border.width: 1
            antialiasing: true
            opacity: 1.0 - index * 0.10
            z: 3 - index

            FileIconCell {
                anchors.centerIn: parent
                iconSize: 26
                path: modelData.path || ""
                isDirectory: modelData.isDirectory === true
                suffix: modelData.suffix || ""
                useNativeIcons: root.useNativeIcons
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        width: Math.max(24, countLabel.implicitWidth + 12)
        height: 22
        radius: 11
        color: Theme.accent
        border.color: Theme.withAlpha(Theme.accentText, 0.28)
        border.width: 1
        z: 6

        Label {
            id: countLabel
            anchors.centerIn: parent
            text: String(root.itemCount)
            color: Theme.accentText
            font.pixelSize: Theme.fontSizeCaption
            font.weight: Font.DemiBold
        }
    }
}
