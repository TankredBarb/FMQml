import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    property var controller: null
    property string archivePath: ""
    property string displayName: ""
    property string sizeText: ""
    readonly property color dialogAccent: Theme.accent

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.9, 480)
    padding: 20

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(targetController, path, name, size) {
        root.controller = targetController
        root.archivePath = path || ""
        root.displayName = name || fileNameFor(path)
        root.sizeText = size || ""
        if (root.controller && root.archivePath.length > 0) {
            root.open()
        }
    }

    function fileNameFor(path) {
        if (!path) return ""
        const text = String(path)
        const parts = text.split(/[|/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : text
    }

    function prepareArchive() {
        if (root.controller && root.archivePath.length > 0) {
            root.controller.openNestedArchivePath(root.archivePath)
        }
        root.close()
    }

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    background: DialogShell {
        accentColor: root.dialogAccent
        shellBorderColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.30 : 0.22)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.prepareArchive()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/archive.svg"
            iconTint: root.dialogAccent
            accentColor: root.dialogAccent
            title: "Prepare Nested Archive"
            subtitle: root.displayName
            showCloseButton: false
        }

        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: infoLayout.implicitHeight + 18
            surfaceColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.08 : 0.045)
            strokeColor: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.24 : 0.18)

            ColumnLayout {
                id: infoLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: "This archive is inside another archive. It will be prepared in a temporary workspace before browsing."
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    visible: root.sizeText.length > 0
                    Layout.preferredWidth: sizeLabel.implicitWidth + 18
                    Layout.preferredHeight: 24
                    radius: Theme.radiusSm
                    color: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.16 : 0.10)
                    border.color: Theme.withAlpha(root.dialogAccent, themeController.isDark ? 0.38 : 0.26)
                    border.width: 1

                    Label {
                        id: sizeLabel
                        anchors.centerIn: parent
                        text: "Archive size: " + root.sizeText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeCaption
                        font.weight: Font.DemiBold
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "The panel path and breadcrumbs will stay unchanged."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.Wrap
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
                text: "Prepare"
                Layout.fillWidth: true
                highlighted: true
                primaryColor: root.dialogAccent
                primaryHoverColor: root.dialogAccent
                primaryPressedColor: root.dialogAccent
                onClicked: root.prepareArchive()
            }
        }
    }
}
