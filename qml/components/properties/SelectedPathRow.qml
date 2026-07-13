import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

Rectangle {
        id: selectedRow

        required property string filePath
        required property string fileName
        required property string parentPath
        required property bool isDirectory
        required property string suffix
        property bool useNativeIcons: true

        width: ListView.view ? ListView.view.width : 0
        height: 42
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: Theme.panelBorder
        border.width: 1

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 9

            FileIconCell {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                iconSize: 18
                path: selectedRow.filePath
                suffix: selectedRow.suffix
                isDirectory: selectedRow.isDirectory
                useNativeIcons: selectedRow.useNativeIcons
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    clip: true

                    Label {
                        text: {
                            if (selectedRow.isDirectory || !selectedRow.suffix) return selectedRow.fileName
                            const extLen = selectedRow.suffix.length
                            if (extLen > 0 && selectedRow.fileName.endsWith("." + selectedRow.suffix)) {
                                return selectedRow.fileName.substring(0, selectedRow.fileName.length - extLen - 1)
                            }
                            return selectedRow.fileName
                        }
                        color: selectedRow.isDirectory ? TextColors.folderNameText : TextColors.fileNameText
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }

                    Label {
                        visible: !selectedRow.isDirectory && !!selectedRow.suffix && selectedRow.fileName.endsWith("." + selectedRow.suffix)
                        text: "." + selectedRow.suffix
                        color: TextColors.fileExtensionText
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: false
                    }
                }

                Label {
                    text: selectedRow.parentPath
                    Layout.fillWidth: true
                    color: TextColors.filePathText
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideMiddle
                }
            }
        }
    }

