import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"

Item {
    id: root

    implicitWidth: 360
    implicitHeight: mainContainer.height

    property bool active: workspaceController.operationQueue.busy || workspaceController.operationQueue.error.length > 0

    visible: opacity > 0
    opacity: active ? 1.0 : 0.0
    y: active ? 0 : 20

    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

    Rectangle {
        id: mainContainer
        width: parent.width
        height: content.implicitHeight + 28
        radius: 18
        color: Theme.panelSurfaceStrong
        border.color: workspaceController.operationQueue.error.length > 0
                      ? Theme.withAlpha(Theme.danger, 0.25)
                      : Theme.panelBorder
        border.width: 1

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.75
            shadowVerticalOffset: 10
            shadowOpacity: 0.35
            shadowColor: Theme.glassShadow
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            radius: 3
            color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.accent
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                    Rectangle {
                        width: 40
                        height: 40
                        radius: 12
                        color: workspaceController.operationQueue.error.length > 0
                               ? Theme.withAlpha(Theme.danger, 0.14)
                               : Theme.withAlpha(Theme.accent, 0.14)
                        border.color: workspaceController.operationQueue.error.length > 0
                                      ? Theme.withAlpha(Theme.danger, 0.26)
                                      : Theme.withAlpha(Theme.accent, 0.26)
                        border.width: 1

                    Image {
                        anchors.centerIn: parent
                        source: workspaceController.operationQueue.error.length > 0
                                ? "../assets/icons/info.svg"
                                : "../assets/icons/refresh.svg"
                        sourceSize: Qt.size(20, 20)

                        RotationAnimation on rotation {
                            from: 0; to: 360; duration: 1800
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
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: workspaceController.operationQueue.error.length > 0 ? "Operation failed" : "Operations"
                        font.bold: true
                        font.pixelSize: 15
                        color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textPrimary
                    }

                    RowLayout {
                        spacing: 6
                        visible: workspaceController.operationQueue.busy

                        Rectangle {
                            radius: 9
                            height: 20
                            implicitWidth: itemsLabel.implicitWidth + 14
                            color: Theme.panelSurface
                            border.color: Theme.panelBorder
                            border.width: 1

                            Label {
                                id: itemsLabel
                                anchors.centerIn: parent
                                text: workspaceController.operationQueue.completedItems + "/" + workspaceController.operationQueue.totalItems
                                color: Theme.textPrimary
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Rectangle {
                            visible: workspaceController.operationQueue.speedText !== ""
                            radius: 9
                            height: 20
                            implicitWidth: speedLabel.implicitWidth + 14
                            color: Theme.withAlpha(Theme.accent, 0.10)
                            border.color: Theme.withAlpha(Theme.accent, 0.18)
                            border.width: 1

                            Label {
                                id: speedLabel
                                anchors.centerIn: parent
                                text: workspaceController.operationQueue.speedText
                                color: Theme.accent
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Rectangle {
                            visible: workspaceController.operationQueue.remainingTimeText !== ""
                            radius: 9
                            height: 20
                            implicitWidth: etaLabel.implicitWidth + 14
                            color: Theme.panelSurface
                            border.color: Theme.panelBorder
                            border.width: 1

                            Label {
                                id: etaLabel
                                anchors.centerIn: parent
                                text: workspaceController.operationQueue.remainingTimeText
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: workspaceController.operationQueue.busy

                ProgressBar {
                    id: pBar
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: workspaceController.operationQueue.progress

                    background: Rectangle {
                        implicitHeight: 10
                        color: Theme.panelBorder
                        radius: 5
                        opacity: 0.35
                    }

                    contentItem: Item {
                        Rectangle {
                            width: pBar.visualPosition * parent.width
                            height: parent.height
                            radius: 5
                            color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.accent

                            Rectangle {
                                anchors.right: parent.right
                                width: 18
                                height: parent.height
                                radius: 5
                                color: "white"
                                opacity: 0.22
                                visible: pBar.visualPosition > 0.05
                            }
                        }
                    }

                    Behavior on value {
                        NumberAnimation { duration: 180 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Rectangle {
                        radius: 9
                        height: 20
                        implicitWidth: pctLabel.implicitWidth + 14
                        color: Theme.withAlpha(Theme.accent, 0.10)
                        border.color: Theme.withAlpha(Theme.accent, 0.16)
                        border.width: 1

                        Label {
                            id: pctLabel
                            anchors.centerIn: parent
                            text: Math.round(workspaceController.operationQueue.progress * 100) + "%"
                            color: Theme.accent
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Item { Layout.preferredWidth: 8 }

                    Label {
                        text: workspaceController.operationQueue.remainingTimeText
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        visible: workspaceController.operationQueue.remainingTimeText !== ""
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: workspaceController.operationQueue.currentLabel || "Preparing..."
                        color: Theme.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: msgLabel.implicitHeight + 24
                color: workspaceController.operationQueue.error.length > 0
                       ? Theme.withAlpha(Theme.danger, 0.06)
                       : Theme.panelSurfaceSoft
                radius: 14
                border.color: workspaceController.operationQueue.error.length > 0
                              ? Theme.withAlpha(Theme.danger, 0.2)
                              : Theme.panelBorder
                border.width: 1

                Label {
                    id: msgLabel
                    anchors.fill: parent
                    anchors.margins: 10
                    text: workspaceController.operationQueue.error.length > 0
                          ? workspaceController.operationQueue.error
                          : (workspaceController.operationQueue.currentLabel || "Initializing...")
                    color: workspaceController.operationQueue.error.length > 0 ? Theme.danger : Theme.textPrimary
                    font.pixelSize: 11
                    font.family: "Segoe UI Semibold, Arial"
                    wrapMode: Text.Wrap
                    elide: Text.ElideMiddle
                    maximumLineCount: 2
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    id: cancelBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    text: workspaceController.operationQueue.error.length > 0 ? "Dismiss" : "Cancel operation"

                    background: Rectangle {
                        radius: 9
                        color: cancelBtn.pressed
                               ? Theme.withAlpha(Theme.danger, 0.14)
                               : (cancelBtn.hovered ? Theme.withAlpha(Theme.danger, 0.08) : "transparent")
                        border.color: workspaceController.operationQueue.error.length > 0
                                      ? Theme.panelBorder
                                      : Theme.withAlpha(Theme.danger, 0.3)
                        border.width: 1
                    }

                    contentItem: Label {
                        text: cancelBtn.text
                        color: workspaceController.operationQueue.error.length > 0 ? Theme.textPrimary : Theme.danger
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
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
