import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.9, 620)
    height: contentColumn.implicitHeight + 48

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    property string sourcePath: ""
    property string destinationPath: ""
    property real sourceSize: 0
    property var sourceModified: new Date()
    property real destSize: 0
    property var destModified: new Date()
    property bool applyToAll: false

    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.92; to: 1.0; duration: 200; easing.type: Easing.OutBack }
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
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    source: "../assets/icons/info.svg"
                    sourceSize: Qt.size(28, 28)
                    smooth: true
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: Theme.accent
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: "File Conflict"
                    font.pixelSize: 22
                    font.bold: true
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }

                Label {
                    text: "A file with this name already exists. How do you want to proceed?"
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    opacity: 0.8
                    Layout.fillWidth: true
                }
            }
        }

        // CARDS CONTAINER
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            FileConflictCard {
                title: "Existing File"
                path: root.destinationPath
                size: root.destSize
                modified: root.destModified
                isDest: true
                Layout.fillWidth: true
            }

            FileConflictCard {
                title: "New File"
                path: root.sourcePath
                size: root.sourceSize
                modified: root.sourceModified
                isDest: false
                Layout.fillWidth: true
            }
        }

        // APPLY TO ALL
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            CheckBox {
                id: applyAllCheck
                text: "Apply to all remaining conflicts"
                checked: root.applyToAll
                onCheckedChanged: root.applyToAll = checked
                
                indicator: Rectangle {
                    implicitWidth: 20
                    implicitHeight: 20
                    radius: 6
                    border.color: applyAllCheck.checked ? Theme.accent : Theme.border
                    color: applyAllCheck.checked ? Theme.accent : "transparent"
                    
                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/select-all.svg"
                        sourceSize: Qt.size(12, 12)
                        visible: applyAllCheck.checked
                        layer.enabled: true
                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" }
                    }
                }

                contentItem: Label {
                    text: applyAllCheck.text
                    font.pixelSize: 13
                    color: Theme.textPrimary
                    leftPadding: 28
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // ACTIONS
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10

            ThemedButton {
                text: "Replace"
                isPrimary: true
                Layout.fillWidth: true
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, root.applyToAll)
                    root.close()
                }
            }

            ThemedButton {
                text: "Keep Both"
                Layout.fillWidth: true
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.KeepBoth, root.applyToAll)
                    root.close()
                }
            }

            ThemedButton {
                text: "Cancel"
                isDanger: true
                Layout.preferredWidth: 100
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Cancel, false)
                    root.close()
                }
            }
        }
    }

    // INTERNAL COMPONENTS
    component ThemedButton : Button {
        id: btn
        property bool isPrimary: false
        property bool isDanger: false
        
        height: 40
        
        background: Rectangle {
            radius: 10
            color: {
                if (isPrimary) return btn.pressed ? Qt.darker(Theme.accent, 1.1) : (btn.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
                if (isDanger) return btn.pressed ? Qt.darker(Theme.danger, 1.1) : (btn.hovered ? Qt.lighter(Theme.danger, 1.1) : "transparent")
                return btn.pressed ? Theme.surfaceActive : (btn.hovered ? Theme.surfaceHover : "transparent")
            }
            border.color: {
                if (isPrimary) return "transparent"
                if (isDanger) return Theme.danger
                return Theme.border
            }
            border.width: isPrimary ? 0 : 1
        }

        contentItem: Label {
            text: btn.text
            color: isPrimary ? "white" : (isDanger ? Theme.danger : Theme.textPrimary)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component FileConflictCard : Rectangle {
        property string title: ""
        property string path: ""
        property real size: 0
        property var modified
        property bool isDest: false

        height: 72
        radius: 12
        color: Qt.rgba(0, 0, 0, themeController.isDark ? 0.2 : 0.05)
        border.color: Theme.border
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 14

            Rectangle {
                width: 44
                height: 44
                radius: 10
                color: Theme.glassSurface
                border.color: Theme.glassBorder
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    source: "image://icon/" + path
                    sourceSize: Qt.size(28, 28)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    spacing: 8
                    Label {
                        text: title
                        font.bold: true
                        font.pixelSize: 11
                        color: isDest ? Theme.danger : Theme.accent
                        opacity: 0.9
                    }
                    Label {
                        text: root.formatSize(size)
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }

                Label {
                    text: root.fileNameFor(path)
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label {
                    text: "Modified: " + Qt.formatDateTime(modified, "dd MMM yyyy, hh:mm")
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
            }
        }
    }

    function fileNameFor(path) {
        if (!path) return ""
        const parts = String(path).split(/[/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : path
    }
}
