import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "filepanel"

Item {
    id: root

    required property var controller
    required property var panel

    required property int index
    required property string name
    required property string path
    required property string iconName
    property string mimeType: ""
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string primaryBadgeKind
    required property bool isPinned
    required property string sizeText
    required property string modifiedText
    required property string suffix

    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false
    property bool isRenaming: false

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()
    signal emptySpaceRightClicked()

    implicitHeight: Theme.rowHeight
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
        leftMargin: 4
        rightMargin: 4
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 8

        FileIconCell {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.alignment: Qt.AlignVCenter
            path: root.path
            name: root.name
            iconName: root.iconName
            mimeType: root.mimeType
            isDirectory: root.isDirectory
            primaryBadgeKind: root.primaryBadgeKind
            isPinned: root.isPinned
            suffix: root.suffix
            useNativeIcons: root.panel ? root.panel.effectiveUseNativeIcons : (typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true)
            iconSize: 16
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            clip: true

            Label {
                text: {
                    if (root.isDirectory || !root.suffix) return root.name
                    const extLen = root.suffix.length
                    if (extLen > 0 && root.name.endsWith("." + root.suffix)) {
                        return root.name.substring(0, root.name.length - extLen - 1)
                    }
                    return root.name
                }
                color: root.isDirectory ? TextColors.folderNameText : TextColors.fileNameText
                elide: Text.ElideRight
                font.pixelSize: Theme.fontSizeBody
                font.weight: root.isSelected ? Font.Medium : Font.Normal
                Layout.fillWidth: true
                Layout.maximumWidth: root.isDirectory ? Infinity : Math.ceil(implicitWidth)
            }

            Label {
                visible: !root.isDirectory && !!root.suffix && root.name.endsWith("." + root.suffix)
                text: "." + root.suffix
                color: TextColors.fileExtensionText
                font.pixelSize: Theme.fontSizeBody
                font.weight: root.isSelected ? Font.Medium : Font.Normal
                elide: Text.ElideRight
                Layout.fillWidth: false
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false
        onPressed: root.cancelRenameOnPress("table-resize-item-press")

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
