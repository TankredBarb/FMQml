import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Control {
    id: root

    required property var controller

    implicitHeight: 36

    function focusPath() {
        root.forceActiveFocus()
    }

    background: Rectangle {
        color: themeController.isDark ? Theme.surface : Theme.bg
        radius: Theme.radius
        border.color: root.activeFocus ? Theme.accent : Theme.border
        border.width: root.activeFocus ? 2 : 1
    }

    contentItem: Item {
        clip: true

        RowLayout {
            id: breadcrumbs
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 2

            Repeater {
                id: pathRepeater
                model: {
                    let path = root.controller.currentPath
                    if (!path) return []
                    
                    // Simple path splitting for breadcrumbs
                    let parts = path.split(/[/\\]/).filter(p => p.length > 0)
                    let result = []
                    let current = ""
                    
                    // Handle Windows drive
                    if (path.includes(":") && parts.length > 0) {
                        current = parts[0] + "\\"
                        result.push({ name: parts[0], path: current })
                        parts.shift()
                    }
                    
                    for (let p of parts) {
                        current += (current.endsWith("\\") || current.endsWith("/") ? "" : "/") + p
                        result.push({ name: p, path: current })
                    }
                    return result
                }

                delegate: Row {
                    spacing: 0
                    ToolButton {
                        text: modelData.name
                        flat: true
                        onClicked: root.controller.openPath(modelData.path)
                        font.pixelSize: 12
                        font.bold: true
                        padding: 5
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: Theme.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.down ? Theme.surfaceActive : (parent.hovered ? Theme.surfaceHover : "transparent")
                            radius: 5
                        }
                    }
                    Label {
                        text: ">"
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        visible: index < pathRepeater.count - 1
                        opacity: 0.65
                        width: 14
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.LeftButton
            onClicked: root.focusPath()
        }
    }
}

