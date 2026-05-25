import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    property string imagePath: ""

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.9, 460)
    padding: 20

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(path) {
        root.imagePath = path || ""
        if (root.imagePath.length > 0) {
            root.open()
        }
    }

    function fileNameFor(path) {
        if (!path) return ""
        const parts = String(path).split(/[/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : path
    }

    function mountDescription() {
        return "Windows will handle assignment automatically."
    }

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    background: DialogShell {
        accentColor: Theme.categoryAction
        shellBorderColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.30 : 0.22)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                workspaceController.mountIsoAutomatically(root.imagePath)
                root.close()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/hard-drive.svg"
            iconTint: Theme.categoryAction
            accentColor: Theme.categoryAction
            title: "Mount ISO"
            subtitle: root.fileNameFor(root.imagePath)
            showCloseButton: false
        }

        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: infoLayout.implicitHeight + 18
            surfaceColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.07 : 0.04)
            strokeColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.22 : 0.16)

            ColumnLayout {
                id: infoLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: "The image will be mounted read-only."
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: Theme.categoryAction
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.mountDescription()
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }

            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Cancel"
                Layout.fillWidth: true
                highlighted: false
                onClicked: root.close()
            }

            DialogActionButton {
                text: "Mount"
                Layout.fillWidth: true
                highlighted: true
                onClicked: {
                    workspaceController.mountIsoAutomatically(root.imagePath)
                    root.close()
                }
            }
        }
    }
}
