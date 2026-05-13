import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Control {
    id: root

    required property var controller
    property bool isEditing: false

    implicitHeight: 36

    function focusPath() {
        isEditing = true
        field.forceActiveFocus()
        field.selectAll()
    }

    background: Rectangle {
        color: Theme.surface // White background
        radius: Theme.radius
        border.color: root.isEditing && field.activeFocus ? Theme.accent : Theme.border
        
        MouseArea {
            anchors.fill: parent
            visible: !root.isEditing
            onClicked: root.focusPath()
        }
    }

    contentItem: Item {
        clip: true

        RowLayout {
            id: breadcrumbs
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 0
            visible: !root.isEditing

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
                        font.bold: true // Make it more visible
                        padding: 6
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: Theme.textPrimary // Use primary instead of default
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? Theme.surfaceHover : "transparent"
                            radius: 4
                        }
                    }
                    Label {
                        text: ">"
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        visible: index < pathRepeater.count - 1
                        opacity: 0.5
                        width: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        TextField {
            id: field
            anchors.fill: parent
            anchors.leftMargin: 4
            visible: root.isEditing
            text: root.controller.currentPath
            selectByMouse: true
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
            onAccepted: {
                root.controller.openPath(text)
                root.isEditing = false
            }
            onActiveFocusChanged: if (!activeFocus) root.isEditing = false

            background: null
        }
    }
}

