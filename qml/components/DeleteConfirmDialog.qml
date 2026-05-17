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
    width: Math.min(parent.width * 0.82, 740)
    height: compactMode
            ? 360
            : Math.min(parent.height * 0.84, 620)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property int itemCount: Array.isArray(paths) ? paths.length : 0
    readonly property int previewCount: Math.min(itemCount, 16)
    readonly property bool compactMode: itemCount <= 1

    function openFor(targetPaths, label) {
        root.paths = targetPaths || []
        root.panelLabel = label || ""
        if (root.itemCount > 0) {
            root.open()
        }
    }

    function fileNameFor(path) {
        if (!path) {
            return ""
        }
        const normalized = String(path)
        const parts = normalized.split(/[/\\\\]/).filter(part => part.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : normalized
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.98; duration: 120; easing.type: Easing.InCubic }
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
            Layout.preferredHeight: compactMode ? 104 : 120

            Rectangle {
                anchors.fill: parent
                radius: 18
                color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, themeController.isDark ? 0.14 : 0.07)
                border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.18)
                border.width: 1
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 6
                radius: 3
                color: Theme.danger
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Rectangle {
                    width: 52
                    height: 52
                    radius: 16
                    color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, themeController.isDark ? 0.18 : 0.12)
                    border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.25)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/delete.svg"
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
                            text: root.itemCount === 1 ? "Delete 1 item?" : "Delete " + root.itemCount + " items?"
                            font.pixelSize: 22
                            font.bold: true
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            visible: root.itemCount > 0
                            radius: 10
                            height: 24
                            implicitWidth: countLabel.implicitWidth + 18
                            color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, themeController.isDark ? 0.18 : 0.12)
                            border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.22)
                            border.width: 1

                            Label {
                                id: countLabel
                                anchors.centerIn: parent
                                text: root.itemCount + " selected"
                                color: Theme.danger
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Label {
                        text: root.panelLabel.length > 0
                              ? root.panelLabel
                              : "This action is immediate and cannot be undone."
                        font.pixelSize: 12
                        color: Theme.textSecondary
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        maximumLineCount: 2
                        elide: Text.ElideMiddle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            radius: 10
                            height: 24
                            implicitWidth: chipText1.implicitWidth + 18
                            color: Theme.glassSurface
                            border.color: Theme.glassBorder
                            border.width: 1

                            Label {
                                id: chipText1
                                anchors.centerIn: parent
                                text: compactMode ? "1 item" : root.previewCount + " shown"
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Rectangle {
                            radius: 10
                            height: 24
                            implicitWidth: chipText2.implicitWidth + 18
                            color: Theme.glassSurface
                            border.color: Theme.glassBorder
                            border.width: 1

                            Label {
                                id: chipText2
                                anchors.centerIn: parent
                                text: compactMode ? "Single delete" : root.previewCount + " names previewed"
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
            Layout.margins: compactMode ? 14 : 18
            spacing: compactMode ? 10 : 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: compactMode ? 48 : 56
                radius: 14
                color: Theme.glassSurface
                border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.16)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: compactMode ? 10 : 14
                    spacing: compactMode ? 10 : 12

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.18)
                        border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.25)
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "!"
                            color: Theme.danger
                            font.pixelSize: 15
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            text: "Permanent deletion"
                            color: Theme.textPrimary
                            font.pixelSize: compactMode ? 11 : 12
                            font.bold: true
                        }
                        Label {
                            text: "Files will be removed immediately from disk."
                            color: Theme.textSecondary
                            font.pixelSize: compactMode ? 10 : 11
                        }
                    }
                }
            }

            Rectangle {
                visible: !compactMode
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 220
                radius: 14
                color: Theme.glassSurfaceSoft
                border.color: Theme.glassBorder
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label {
                            text: "Preview"
                            font.bold: true
                            font.pixelSize: 11
                            color: Theme.textSecondary
                            opacity: 0.8
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            visible: root.itemCount > root.previewCount
                            text: "+" + (root.itemCount - root.previewCount) + " more"
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        background: null

                        ListView {
                            width: parent.width
                            model: root.previewCount
                            spacing: 6
                            clip: true

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 32
                                radius: 8
                                color: index % 2 === 0
                                       ? Theme.glassSurface
                                       : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.06 : 0.04)
                                border.color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.7)
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12)
                                        border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.18)
                                        border.width: 1

                                        Label {
                                            anchors.centerIn: parent
                                            text: "x"
                                            color: Theme.danger
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.fileNameFor(root.paths[index])
                                        color: Theme.textPrimary
                                        font.pixelSize: 12
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: compactMode ? 66 : 76
            color: Qt.rgba(0, 0, 0, themeController.isDark ? 0.1 : 0.03)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Button {
                    text: "Cancel"
                    Layout.preferredWidth: 126
                    onClicked: root.close()

                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? Theme.surfaceHover : Theme.glassSurface
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
                    text: "Delete"
                    Layout.preferredWidth: 150
                    onClicked: {
                        workspaceController.operationQueue.deletePaths(root.paths)
                        root.close()
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.pressed ? Qt.darker(Theme.danger, 1.08)
                                             : (parent.hovered ? Qt.lighter(Theme.danger, 1.06) : Theme.danger)
                        border.color: Theme.danger
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
            }
        }
    }
}
