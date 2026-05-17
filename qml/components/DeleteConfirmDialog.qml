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
    
    // Динамическая ширина в зависимости от контента, но не более 500
    width: Math.min(parent.width * 0.9, 480)
    // Высота подстраивается под список
    height: contentColumn.implicitHeight + 48

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property int itemCount: Array.isArray(paths) ? paths.length : 0
    readonly property int maxVisibleItems: 5
    readonly property bool hasMore: itemCount > maxVisibleItems
    
    // Цвет опасности: яркий коралловый для темной темы, классический красный для светлой
    readonly property color destructiveColor: themeController.isDark ? "#ff5261" : "#e11d48"
    readonly property color destructiveBg: Qt.rgba(destructiveColor.r, destructiveColor.g, destructiveColor.b, 0.1)

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
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.92; to: 1.0; duration: 200; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.95; duration: 150; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        color: Theme.glassSurfaceStrong
        radius: 20
        border.color: Theme.glassBorder
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.8
            shadowVerticalOffset: 12
            shadowColor: Theme.glassShadow
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // HEADER
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Rectangle {
                width: 56
                height: 56
                radius: 16
                color: root.destructiveBg
                border.color: Qt.rgba(root.destructiveColor.r, root.destructiveColor.g, root.destructiveColor.b, 0.2)
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    source: "../assets/icons/delete.svg"
                    sourceSize: Qt.size(28, 28)
                    smooth: true
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: root.destructiveColor
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: root.itemCount === 1 ? "Delete item?" : "Delete " + root.itemCount + " items?"
                    font.pixelSize: 22
                    font.bold: true
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }

                Label {
                    text: "This action cannot be undone."
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    opacity: 0.8
                    Layout.fillWidth: true
                }
            }
        }

        // FILE LIST BOX
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: listLayout.implicitHeight + 16
            radius: 12
            color: Qt.rgba(0, 0, 0, themeController.isDark ? 0.2 : 0.05)
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
                        height: 32
                        radius: 8
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 10

                            Image {
                                source: "image://icon/" + root.paths[index]
                                sourceSize: Qt.size(18, 18)
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                            }

                            Label {
                                text: root.fileNameFor(root.paths[index])
                                color: Theme.textPrimary
                                font.pixelSize: 13
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
                    height: 32
                    radius: 8
                    color: "transparent"

                    Label {
                        anchors.centerIn: parent
                        text: "... and " + (root.itemCount - root.maxVisibleItems) + " more items"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        font.italic: true
                    }
                }
            }
        }

        // FOOTER BUTTONS
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 12

            Button {
                id: cancelBtn
                text: "Cancel"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                onClicked: root.close()

                background: Rectangle {
                    radius: 10
                    color: cancelBtn.hovered ? Theme.surfaceHover : "transparent"
                    border.color: Theme.border
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: deleteBtn
                text: "Delete Forever"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                onClicked: {
                    workspaceController.operationQueue.deletePaths(root.paths)
                    root.close()
                }

                background: Rectangle {
                    radius: 10
                    color: deleteBtn.pressed ? Qt.darker(root.destructiveColor, 1.1)
                                            : (deleteBtn.hovered ? Qt.lighter(root.destructiveColor, 1.1) : root.destructiveColor)
                }
                contentItem: Label {
                    text: parent.text
                    color: "white"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
