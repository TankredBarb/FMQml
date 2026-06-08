import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "filepanel"

Item {
    id: root

    required property var controller
    property var panel
    required property int index
    required property string name
    required property string path
    required property string iconName
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string suffix

    property bool currentItem: false
    property bool panelActive: true
    property bool isRenaming: false
    readonly property int baseHeight: 28
    readonly property real scaleFactor: height / baseHeight
    readonly property int iconSize: Math.max(16, Math.min(48, Math.round(16 * scaleFactor)))
    readonly property int fontSize: Math.max(11, Math.min(16, Math.round(12 * (1.0 + (scaleFactor - 1.0) * 0.5))))

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: 28
    opacity: isHidden ? 0.55 : 1.0

    function startRename() {
        root.isRenaming = false
    }

    function cancelRenameOnPress(reason) {
        if (root.panel && root.panel.cancelInlineRenameForNavigation) {
            root.panel.cancelInlineRenameForNavigation(reason)
        }
    }

    FileItemStateLayer {
        anchors.fill: parent
        selected: root.isSelected
        panelActive: root.panelActive
        currentItem: root.currentItem
        hovered: false
        scrolling: true
        resizeOptimized: true
        leftMargin: 6
        rightMargin: 6
        topMargin: 2
        bottomMargin: 2
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        spacing: 8

        FileIconCell {
            Layout.preferredWidth: root.iconSize
            Layout.preferredHeight: root.iconSize
            Layout.alignment: Qt.AlignVCenter
            path: root.path
            iconName: root.iconName
            isDirectory: root.isDirectory
            suffix: root.suffix
            useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
            iconSize: root.iconSize
        }

        Label {
            Layout.fillWidth: true
            text: root.name
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.pixelSize: root.fontSize
            font.weight: root.isSelected ? Font.Medium : Font.Normal
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false
        onPressed: root.cancelRenameOnPress("brief-resize-item-press")

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
