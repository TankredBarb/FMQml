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
    property string mimeType: ""
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string primaryBadgeKind
    required property bool isPinned
    required property string suffix

    property bool currentItem: false
    property bool panelActive: true
    property bool isRenaming: false
    readonly property int iconSize: Math.max(Theme.scaledSize(16),
                                             Math.min(Theme.scaledSize(40),
                                                      Math.round(Math.max(Theme.scaledSize(16), height - Theme.scaledSize(8)))))
    readonly property int fontSize: Theme.fontSizeBody

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: root.panel ? root.panel.briefRowHeight : Math.max(Theme.controlHeight - 10, Theme.fontSizeLabel + 16)
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
        leftMargin: Theme.scaledSize(6)
        rightMargin: Theme.scaledSize(6)
        topMargin: 2
        bottomMargin: 2
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.scaledSize(14)
        anchors.rightMargin: Theme.scaledSize(8)
        spacing: Theme.scaledSize(8)

        FileIconCell {
            Layout.preferredWidth: root.iconSize
            Layout.preferredHeight: root.iconSize
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
            iconSize: root.iconSize
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
                font.family: Theme.fontFamily
                font.pixelSize: root.fontSize
                font.weight: root.isSelected ? Font.Medium : Font.Normal
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.maximumWidth: root.isDirectory ? Infinity : Math.ceil(implicitWidth)
            }

            Label {
                visible: !root.isDirectory && !!root.suffix && root.name.endsWith("." + root.suffix)
                text: "." + root.suffix
                color: TextColors.fileExtensionText
                font.family: Theme.fontFamily
                font.pixelSize: root.fontSize
                font.weight: root.isSelected ? Font.Medium : Font.Normal
                verticalAlignment: Text.AlignVCenter
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
