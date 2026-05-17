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
    width: Math.min(parent.width * 0.82, 820)
    height: Math.min(parent.height * 0.86, 660)

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    property string sourcePath: ""
    property string destinationPath: ""
    property real sourceSize: 0
    property var sourceModified: new Date()
    property real destSize: 0
    property var destModified: new Date()

    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
    }

    background: Rectangle {
        color: Theme.glassSurfaceStrong
        radius: 18
        border.color: Theme.glassBorder
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.7
            shadowVerticalOffset: 14
            shadowColor: Theme.glassShadow
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 128

            Rectangle {
                anchors.fill: parent
                radius: 18
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.10 : 0.06)
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                border.width: 1
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 6
                radius: 3
                color: Theme.accent
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Rectangle {
                    width: 52
                    height: 52
                    radius: 16
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.18 : 0.12)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/info.svg"
                        sourceSize: Qt.size(24, 24)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "File name conflict"
                            font.pixelSize: 22
                            font.bold: true
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            radius: 10
                            height: 24
                            implicitWidth: badgeLabel.implicitWidth + 18
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.16 : 0.10)
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                            border.width: 1

                            Label {
                                id: badgeLabel
                                anchors.centerIn: parent
                                text: "Apply to all available"
                                color: Theme.accent
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Label {
                        text: "A file with the same name already exists in the destination folder. Pick the resolution for this item."
                        wrapMode: Text.Wrap
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                    Rectangle {
                        radius: 10
                        height: 24
                        implicitWidth: sourceChip.implicitWidth + 18
                        color: Theme.glassSurface
                        border.color: Theme.glassBorder
                        border.width: 1
                            Label {
                                id: sourceChip
                                anchors.centerIn: parent
                                text: "Source: " + root.formatSize(root.sourceSize)
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                    Rectangle {
                        radius: 10
                        height: 24
                        implicitWidth: destChip.implicitWidth + 18
                        color: Theme.glassSurface
                        border.color: Theme.glassBorder
                        border.width: 1
                            Label {
                                id: destChip
                                anchors.centerIn: parent
                                text: "Existing: " + root.formatSize(root.destSize)
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            opacity: 0.35
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 18
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                radius: 14
                color: Theme.glassSurface
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                        border.width: 1
                        Label {
                            anchors.centerIn: parent
                            text: "="
                            color: Theme.accent
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            text: "Compare the files"
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Label {
                            text: "Use Replace, Keep Both, Skip, or Cancel."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: null
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    GridLayout {
                        columns: width < 700 ? 1 : 2
                        columnSpacing: 14
                        rowSpacing: 14
                        Layout.fillWidth: true

                        FileConflictCard {
                            title: "Source File"
                            roleHint: "Incoming"
                            path: root.sourcePath
                            size: root.sourceSize
                            modified: root.sourceModified
                            accentColor: Theme.accent
                            Layout.fillWidth: true
                        }

                        FileConflictCard {
                            title: "Existing File"
                            roleHint: "Already in destination"
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
                        Layout.topMargin: 4

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
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
                color: Qt.rgba(0, 0, 0, themeController.isDark ? 0.1 : 0.03)
            radius: 18

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Button {
                    text: "Replace"
                    Layout.preferredWidth: 132
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, applyToAll.checked)
                        root.close()
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.pressed ? Qt.darker(Theme.accent, 1.08)
                                             : (parent.hovered ? Qt.lighter(Theme.accent, 1.06) : Theme.accent)
                        border.color: Theme.accent
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "white"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "Keep Both"
                    Layout.preferredWidth: 132
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.KeepBoth, applyToAll.checked)
                        root.close()
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.pressed ? Qt.darker(Theme.surfaceHover, 1.06)
                                             : (parent.hovered ? Theme.surfaceHover : Theme.glassSurface)
                        border.color: Theme.glassBorder
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
                    text: "Skip"
                    Layout.preferredWidth: 96
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.Skip, applyToAll.checked)
                        root.close()
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.pressed ? Qt.darker(Theme.surfaceHover, 1.06)
                                             : (parent.hovered ? Theme.surfaceHover : Theme.glassSurface)
                        border.color: Theme.glassBorder
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

                Item { Layout.fillWidth: true }

                Button {
                    text: "Cancel"
                    Layout.preferredWidth: 100
                    onClicked: {
                        workspaceController.operationQueue.resolveConflict(OperationQueue.Cancel, false)
                        root.close()
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.pressed ? Qt.darker(Theme.danger, 1.08)
                                             : (parent.hovered ? Qt.lighter(Theme.danger, 1.06) : Theme.glassSurface)
                        border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.45)
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.danger
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    component FileConflictCard : Rectangle {
        property string title: ""
        property string roleHint: ""
        property string path: ""
        property real size: 0
        property var modified
        property color accentColor: Theme.accent

        radius: 14
        color: Theme.glassSurface
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.18)
        border.width: 1
        Layout.preferredHeight: 162

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: accentColor
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        text: title
                        font.bold: true
                        font.pixelSize: 12
                        color: accentColor
                    }

                    Label {
                        text: roleHint
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 40
                    height: 40
                    radius: 12
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

                    Label {
                        text: root.formatSize(size)
                        color: Theme.textPrimary
                        font.bold: true
                        font.pixelSize: 16
                    }

                    Label {
                        text: Qt.formatDateTime(modified, "dd MMM yyyy, hh:mm")
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.border
                opacity: 0.25
            }

            Label {
                text: path
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                color: Theme.textSecondary
                font.pixelSize: 10
                wrapMode: Text.NoWrap
            }
        }
    }
}
