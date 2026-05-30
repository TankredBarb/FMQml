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
    required property string suffix
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile

    property bool currentItem: false
    property bool panelActive: true
    property int gridIconSize: 48
    readonly property int displayedIconSize: Math.max(28, Math.round(root.gridIconSize * 0.8))

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    opacity: isHidden ? 0.55 : 1.0

    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: Theme.radiusSm
        color: root.isSelected
               ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
               : (root.currentItem ? Theme.itemCurrentFill : "transparent")
        border.color: root.isSelected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.itemCurrentBorder : "transparent")
        border.width: root.isSelected || root.currentItem ? 1 : 0
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
                isDirectory: root.isDirectory
                suffix: root.suffix
                useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
                iconSize: root.displayedIconSize
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: root.name
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            elide: Text.ElideRight
            font.pixelSize: 12
            font.weight: root.isSelected || root.currentItem ? Font.Medium : Font.Normal
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
