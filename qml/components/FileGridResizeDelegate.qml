import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "filepanel"

Item {
    id: root

    required property int index
    required property string name
    required property string path
    required property string iconName
    required property string suffix
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile

    property var panel
    property bool currentItem: false
    property bool panelActive: true
    property int gridIconSize: 48
    readonly property int displayedIconSize: Math.max(28, Math.round(root.gridIconSize * 0.8))
    readonly property color selectedStateFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark
            ? (root.panelActive ? 0.34 : 0.20)
            : (root.panelActive ? 0.28 : 0.16))
    readonly property color currentStateFill: Theme.withAlpha(
        Theme.activeAccent,
        themeController.isDark
            ? (root.panelActive ? 0.18 : 0.11)
            : (root.panelActive ? 0.14 : 0.09))

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    opacity: isHidden ? 0.55 : 1.0

    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: Theme.radiusMd
        color: root.isSelected
               ? root.selectedStateFill
               : (root.currentItem ? root.currentStateFill : "transparent")
        border.color: "transparent"
        border.width: 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.gridIconSize
            Layout.preferredHeight: root.gridIconSize

            FileIconCell {
                anchors.centerIn: parent
                width: root.displayedIconSize
                height: root.displayedIconSize
                path: root.path
                iconName: root.iconName
                isDirectory: root.isDirectory
                suffix: root.suffix
                useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
                iconSize: root.displayedIconSize
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.name
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.pixelSize: 12
            font.weight: root.isSelected ? Font.Medium : Font.Normal
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            maximumLineCount: 2
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.rightClicked()
            } else {
                root.clicked(mouse)
            }
        }
        onDoubleClicked: root.doubleClicked()
    }
}
