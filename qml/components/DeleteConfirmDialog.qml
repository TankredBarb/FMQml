import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    property var paths: []
    property string panelLabel: ""

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    
    width: Math.min(parent.width * 0.9, 400)
    padding: 20

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    readonly property int itemCount: Array.isArray(paths) ? paths.length : 0
    readonly property int maxVisibleItems: 5
    readonly property bool hasMore: itemCount > maxVisibleItems

    function openFor(targetPaths, label) {
        root.paths = targetPaths || []
        root.panelLabel = label || ""
        if (root.itemCount > 0) {
            root.open()
        }
    }

    function fileNameFor(path) {
        if (!path) return ""
        const parts = String(path).split(/[/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : path
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: DialogShell {
        accentColor: Theme.danger
        shellBorderColor: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.34 : 0.24)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                workspaceController.operationQueue.deletePaths(root.paths)
                root.close()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/delete.svg"
            iconTint: Theme.danger
            accentColor: Theme.danger
            title: root.itemCount === 1 ? "Delete item?" : "Delete " + root.itemCount + " items?"
            subtitle: "This action cannot be undone."
            showCloseButton: false
        }

        // FILE LIST BOX
        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: listLayout.implicitHeight + 16
            surfaceColor: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.07 : 0.04)
            strokeColor: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.24 : 0.18)

            ColumnLayout {
                id: listLayout
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Repeater {
                    model: Math.min(root.itemCount, root.maxVisibleItems)
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 28
                        radius: Theme.radiusSm
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 8

                            Image {
                                source: "image://icon/" + encodeURIComponent(root.paths[index])
                                sourceSize: Qt.size(16, 16)
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }

                            Label {
                                text: root.fileNameFor(root.paths[index])
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }

                // "And more" indicator
                Rectangle {
                    visible: root.hasMore
                    Layout.fillWidth: true
                    height: 28
                    color: "transparent"

                    Label {
                        anchors.centerIn: parent
                        text: "... and " + (root.itemCount - root.maxVisibleItems) + " more items"
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        font.italic: true
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
                text: "Delete Forever"
                Layout.fillWidth: true
                highlighted: true
                primaryColor: Theme.danger
                primaryHoverColor: Qt.lighter(Theme.danger, 1.1)
                primaryPressedColor: Qt.darker(Theme.danger, 1.1)
                onClicked: {
                    workspaceController.operationQueue.deletePaths(root.paths)
                    root.close()
                }
            }
        }
    }
}
