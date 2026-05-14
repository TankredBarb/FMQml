import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Pane {
    id: root

    padding: 0

    background: Rectangle {
        color: themeController.isDark
                ? Theme.surface
                : Theme.bg

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          themeController.isDark ? 0.04 : 0.06)
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: themeController.isDark ? Theme.bg : Qt.darker(Theme.bg, 1.04)
        }

        border.color: Theme.border
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 8
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 6
            spacing: 8

            Rectangle {
                width: 3
                height: 14
                radius: 1.5
                color: Theme.accent
            }

            Label {
                text: "Places"
                font.pixelSize: 11
                font.bold: true
                color: Theme.textSecondary
            }
        }

        ListView {
            id: placesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: workspaceController.placesModel
            clip: true
            interactive: contentHeight > height

            delegate: ItemDelegate {
                id: placeDelegate
                width: placesList.width
                height: 40
                padding: 0

                readonly property bool isActive: {
                    let panel = workspaceController.activePanel === 0
                        ? workspaceController.leftPanel
                        : workspaceController.rightPanel
                    return panel.currentPath === model.path
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        source: model.icon === "drive"
                                ? "../assets/icons/hard-drive.svg"
                                : "../assets/icons/" + model.icon + ".svg"
                        sourceSize: Qt.size(20, 20)
                        asynchronous: true
                        cache: true
                    }

                    Label {
                        text: model.name
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        font.weight: isActive ? Font.Medium : Font.Normal
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: 6
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6

                    color: {
                        if (isActive || placeDelegate.down || placeDelegate.hovered)
                            return Theme.surfaceHover
                        return "transparent"
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        width: 3
                        radius: 1.5
                        visible: isActive
                        color: Theme.accent
                    }

                    Behavior on color {
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                onClicked: {
                    let panel = workspaceController.activePanel === 0
                        ? workspaceController.leftPanel
                        : workspaceController.rightPanel
                    panel.openPath(model.path)
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
}
