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
    readonly property int iconSize: Math.max(Theme.scaledSize(16),
                                             Math.min(Theme.scaledSize(24),
                                                      Math.round(Math.max(Theme.scaledSize(16), height - Theme.scaledSize(10)))))
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
            font.family: Theme.fontFamily
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
