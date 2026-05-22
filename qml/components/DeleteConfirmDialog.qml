import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

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

    background: Rectangle {
        color: Theme.glassSurfaceStrong
        radius: 12
        border.color: Theme.border
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glassShadow
            shadowBlur: 20
            shadowVerticalOffset: 8
        }
    }

    contentItem: ColumnLayout {
        spacing: 16

        // HEADER
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Image {
                source: "../assets/icons/delete.svg"
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignVCenter
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: Theme.danger
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: root.itemCount === 1 ? "Delete item?" : "Delete " + root.itemCount + " items?"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }

                Label {
                    text: "This action cannot be undone."
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                }
            }
        }

        // FILE LIST BOX
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: listLayout.implicitHeight + 16
            radius: 8
            color: Theme.surfaceHover
            border.color: Theme.border
            border.width: 1
            clip: true

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
                        radius: 4
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

        // FOOTER BUTTONS
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10

            Button {
                id: cancelBtn
                text: "Cancel"
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                onClicked: root.close()

                contentItem: Label {
                    text: cancelBtn.text
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 8
                    color: cancelBtn.pressed ? Theme.surfaceActive : (cancelBtn.hovered ? Theme.itemHoverFill : "transparent")
                    border.color: Theme.border
                    border.width: 1
                }
            }

            Button {
                id: deleteBtn
                text: "Delete Forever"
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                onClicked: {
                    workspaceController.operationQueue.deletePaths(root.paths)
                    root.close()
                }

                contentItem: Label {
                    text: deleteBtn.text
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 8
                    color: deleteBtn.pressed ? Qt.darker(Theme.danger, 1.1) : (deleteBtn.hovered ? Qt.lighter(Theme.danger, 1.1) : Theme.danger)
                }
            }
        }
    }
}
