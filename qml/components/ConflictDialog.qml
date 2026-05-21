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
    width: Math.min(parent.width * 0.9, 520)
    padding: 20

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
        color: Theme.surface
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
                source: "../assets/icons/info.svg"
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignVCenter
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: Theme.accent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "File Conflict"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }

                Label {
                    text: "A file with this name already exists. How do you want to proceed?"
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                }
            }
        }

        // CARDS CONTAINER
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

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
            Layout.topMargin: 4
            
            CheckBox {
                id: applyAllCheck
                text: "Apply to all remaining conflicts"
                checked: root.applyToAll
                onCheckedChanged: root.applyToAll = checked
                
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    radius: 4
                    border.color: applyAllCheck.checked ? Theme.accent : Theme.border
                    border.width: applyAllCheck.checked ? 0 : 1
                    color: applyAllCheck.checked ? Theme.accent : "transparent"
                    
                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/select-all.svg"
                        sourceSize: Qt.size(10, 10)
                        visible: applyAllCheck.checked
                        layer.enabled: true
                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" }
                    }
                }

                contentItem: Label {
                    text: applyAllCheck.text
                    font.pixelSize: 12
                    color: Theme.textPrimary
                    leftPadding: 26
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // ACTIONS
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10

            Button {
                id: replaceBtn
                text: "Replace"
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, root.applyToAll)
                    root.close()
                }
                
                contentItem: Label {
                    text: replaceBtn.text
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: replaceBtn.pressed ? Qt.darker(Theme.accent, 1.1) : (replaceBtn.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
                }
            }

            Button {
                id: keepBothBtn
                text: "Keep Both"
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.KeepBoth, root.applyToAll)
                    root.close()
                }
                
                contentItem: Label {
                    text: keepBothBtn.text
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: keepBothBtn.pressed ? Theme.surfaceActive : (keepBothBtn.hovered ? Theme.surfaceHover : "transparent")
                    border.color: Theme.border
                    border.width: 1
                }
            }

            Button {
                id: cancelBtn
                text: "Cancel"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 34
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Cancel, false)
                    root.close()
                }
                
                contentItem: Label {
                    text: cancelBtn.text
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.danger
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: cancelBtn.pressed ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.15) : (cancelBtn.hovered ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.08) : "transparent")
                    border.color: Theme.danger
                    border.width: 1
                }
            }
        }
    }

    // INTERNAL COMPONENTS
    component FileConflictCard : Rectangle {
        property string title: ""
        property string path: ""
        property real size: 0
        property var modified
        property bool isDest: false

        height: 64
        radius: 8
        color: Theme.surfaceHover
        border.color: Theme.border
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            Rectangle {
                width: 36
                height: 36
                radius: 6
                color: Theme.surface
                border.color: Theme.border
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    source: path !== "" ? "image://icon/" + path : ""
                    sourceSize: Qt.size(24, 24)
                    smooth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                RowLayout {
                    spacing: 8
                    Label {
                        text: title
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: isDest ? Theme.danger : Theme.accent
                    }
                    Label {
                        text: root.formatSize(size)
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }
                }

                Label {
                    text: root.fileNameFor(path)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label {
                    text: "Modified: " + Qt.formatDateTime(modified, "dd MMM yyyy, hh:mm")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                }
            }
        }
    }
}
