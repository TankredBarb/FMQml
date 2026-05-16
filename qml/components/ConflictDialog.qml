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
    width: 500
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    property string sourcePath: ""
    property string destinationPath: ""
    property real sourceSize: 0
    property var sourceModified: new Date()
    property real destSize: 0
    property var destModified: new Date()

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200 }
        NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
    }

    background: Rectangle {
        color: Theme.surface
        radius: 16
        border.color: Theme.border
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.6
            shadowVerticalOffset: 8
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12
                
                Image {
                    source: "../assets/icons/info.svg"
                    sourceSize: Qt.size(24, 24)
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: Theme.accent
                    }
                }
                
                Label {
                    text: "File Name Conflict"
                    font.bold: true
                    font.pixelSize: 18
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }
            }
            
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
                opacity: 0.3
            }
        }

        ColumnLayout {
            Layout.margins: 20
            spacing: 20
            
            Label {
                text: "A file with the same name already exists in the destination folder. What would you like to do?"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                color: Theme.textSecondary
                font.pixelSize: 13
            }

            // Comparison Grid
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: 15
                rowSpacing: 15

                // Source Info
                FileConflictCard {
                    title: "Source File"
                    path: root.sourcePath
                    size: root.sourceSize
                    modified: root.sourceModified
                    accentColor: Theme.accent
                    Layout.fillWidth: true
                }

                // Destination Info
                FileConflictCard {
                    title: "Existing File"
                    path: root.destinationPath
                    size: root.destSize
                    modified: root.destModified
                    accentColor: Theme.danger
                    Layout.fillWidth: true
                }
            }

            CheckBox {
                id: applyToAll
                text: "Apply to all remaining conflicts"
                Layout.topMargin: 5
                
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    radius: 4
                    border.color: applyToAll.checked ? Theme.accent : Theme.textSecondary
                    color: applyToAll.checked ? Theme.accent : "transparent"
                    
                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/refresh.svg"
                        sourceSize: Qt.size(12, 12)
                        visible: applyToAll.checked
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            colorization: 1.0
                            colorizationColor: "white"
                        }
                    }
                }
                
                contentItem: Label {
                    text: applyToAll.text
                    font.pixelSize: 13
                    color: Theme.textPrimary
                    leftPadding: 26
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Footer Actions
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            color: Qt.rgba(0,0,0, themeController.isDark ? 0.1 : 0.02)
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12
                
                ConflictButton {
                    text: "Replace"
                    highlighted: true
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, applyToAll.checked)
                        root.close()
                    }
                }

                ConflictButton {
                    text: "Keep Both"
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.KeepBoth, applyToAll.checked)
                        root.close()
                    }
                }

                ConflictButton {
                    text: "Skip"
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.Skip, applyToAll.checked)
                        root.close()
                    }
                }

                Item { Layout.fillWidth: true }

                ConflictButton {
                    text: "Cancel"
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.Cancel, false)
                        root.close()
                    }
                }
            }
        }
    }

    // Helper components
    component FileConflictCard : Rectangle {
        property string title: ""
        property string path: ""
        property real size: 0
        property var modified
        property color accentColor: Theme.accent
        
        Layout.preferredHeight: 120
        radius: 12
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.05)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Label {
                text: title
                font.bold: true
                font.pixelSize: 11
                color: accentColor
                opacity: 0.8
            }

            RowLayout {
                spacing: 12
                Image {
                    source: "image://icon/" + path
                    sourceSize: Qt.size(32, 32)
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                }
                ColumnLayout {
                    spacing: 2
                    Label {
                        text: formatSize(size)
                        color: Theme.textPrimary
                        font.bold: true
                        font.pixelSize: 13
                    }
                    Label {
                        text: Qt.formatDateTime(modified, "dd MMM yyyy, hh:mm")
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            Label {
                text: path
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                color: Theme.textSecondary
                font.pixelSize: 10
                opacity: 0.7
            }
        }
        
        function formatSize(bytes) {
            if (bytes < 1024) return bytes + " B";
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
            if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB";
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB";
        }
    }

    component ConflictButton : Button {
        id: cBtn
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        
        background: Rectangle {
            radius: 8
            color: cBtn.pressed ? Qt.darker(baseColor, 1.1) : (cBtn.hovered ? Qt.lighter(baseColor, 1.1) : baseColor)
            border.color: highlighted ? "transparent" : Theme.border
            border.width: 1
            
            property color baseColor: highlighted ? Theme.accent : "transparent"
            Behavior on color { ColorAnimation { duration: 150 } }
        }
        
        contentItem: Label {
            text: cBtn.text
            font.bold: true
            font.pixelSize: 13
            color: highlighted ? "white" : Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
