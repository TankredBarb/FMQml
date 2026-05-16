import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"

Item {
    id: root
    
    implicitWidth: 380
    implicitHeight: mainContainer.height
    
    property bool active: workspaceController.operationQueue.busy || workspaceController.operationQueue.error.length > 0
    
    visible: opacity > 0
    opacity: active ? 1.0 : 0.0
    y: active ? 0 : 20
    
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }

    Rectangle {
        id: mainContainer
        width: parent.width
        height: content.implicitHeight + 32
        color: Theme.surface
        radius: 16
        border.color: Theme.border
        border.width: 1
        
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.6
            shadowVerticalOffset: 6
            shadowOpacity: 0.3
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 40
                    height: 40
                    radius: 12
                    color: workspaceController.operationQueue.error.length > 0 
                           ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.1)
                           : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)

                    Image {
                        anchors.centerIn: parent
                        source: workspaceController.operationQueue.error.length > 0 
                                ? "../assets/icons/info.svg" 
                                : "../assets/icons/refresh.svg"
                        sourceSize: Qt.size(22, 22)
                        
                        RotationAnimation on rotation {
                            from: 0; to: 360; duration: 2000
                            loops: Animation.Infinite
                            running: workspaceController.operationQueue.busy && workspaceController.operationQueue.error.length === 0
                        }

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            colorization: 1.0
                            colorizationColor: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.accent
                        }
                    }
                }

                ColumnLayout {
                    spacing: 0
                    Label {
                        text: workspaceController.operationQueue.error.length > 0 ? "Operation Failed" : "File Operation"
                        font.bold: true
                        font.pixelSize: 16
                        color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textPrimary
                    }
                    RowLayout {
                        spacing: 8
                        Label {
                            visible: workspaceController.operationQueue.busy
                            text: workspaceController.operationQueue.completedItems + " / " + workspaceController.operationQueue.totalItems + " items"
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }
                        Rectangle {
                            width: 3; height: 3; radius: 1.5
                            color: Theme.textSecondary; opacity: 0.5
                            visible: workspaceController.operationQueue.busy && workspaceController.operationQueue.speedText !== ""
                        }
                        Label {
                            text: workspaceController.operationQueue.speedText
                            color: Theme.accent
                            font.pixelSize: 11
                            font.bold: true
                            visible: workspaceController.operationQueue.busy && workspaceController.operationQueue.speedText !== ""
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // Progress Section
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: workspaceController.operationQueue.busy

                ProgressBar {
                    id: pBar
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: workspaceController.operationQueue.progress
                    
                    background: Rectangle {
                        implicitHeight: 10
                        color: Theme.border
                        radius: 5
                        opacity: 0.3
                    }

                    contentItem: Item {
                        Rectangle {
                            width: pBar.visualPosition * parent.width
                            height: parent.height
                            radius: 5
                            color: Theme.accent
                            
                            Rectangle {
                                anchors.right: parent.right
                                width: 20
                                height: parent.height
                                radius: 5
                                color: "white"
                                opacity: 0.3
                                visible: pBar.visualPosition > 0.05
                            }
                        }
                    }
                    
                    Behavior on value { 
                        NumberAnimation { duration: 200 } 
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: workspaceController.operationQueue.remainingTimeText
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        visible: workspaceController.operationQueue.remainingTimeText !== ""
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: Math.round(workspaceController.operationQueue.progress * 100) + "%"
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            // Current File Status Area
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: msgLabel.implicitHeight + 24
                color: workspaceController.operationQueue.error.length > 0 
                       ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.05)
                       : Theme.bg
                radius: 12
                border.color: workspaceController.operationQueue.error.length > 0 
                              ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.2)
                              : Theme.border
                border.width: 1

                Label {
                    id: msgLabel
                    anchors.fill: parent
                    anchors.margins: 12
                    text: workspaceController.operationQueue.error.length > 0
                          ? workspaceController.operationQueue.error
                          : (workspaceController.operationQueue.currentLabel || "Initializing...")
                    color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textPrimary
                    font.pixelSize: 12
                    font.family: "Segoe UI Semibold, Arial"
                    wrapMode: Text.Wrap
                    elide: Text.ElideMiddle
                    maximumLineCount: 2
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                
                Button {
                    id: cancelBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: workspaceController.operationQueue.error.length > 0 ? "Dismiss" : "Cancel Operation"
                    
                    contentItem: Label {
                        text: cancelBtn.text
                        color: workspaceController.operationQueue.error.length > 0 ? Theme.textPrimary : Theme.danger
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 8
                        color: cancelBtn.pressed 
                               ? Qt.rgba(1, 0, 0, 0.15) 
                               : (cancelBtn.hovered ? Qt.rgba(1, 0, 0, 0.08) : "transparent")
                        border.color: workspaceController.operationQueue.error.length > 0 ? Theme.border : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.3)
                        border.width: 1
                    }

                    onClicked: {
                        if (workspaceController.operationQueue.error.length > 0) {
                            workspaceController.operationQueue.error = ""
                        } else {
                            workspaceController.operationQueue.cancel()
                        }
                    }
                }
            }
        }
    }
}
