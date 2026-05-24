import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property bool active: false
    property bool showLoadingRail: false
    property string statusMessage: ""
    property bool isCurrentPathArchive: false
    property var loadingFolderNameProvider

    implicitHeight: showLoadingRail ? 52 : 36
    visible: statusMessage.length > 0 || showLoadingRail
    opacity: visible ? 1.0 : 0.0

    Behavior on implicitHeight {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutQuad
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.panelSurfaceStrong
        border.color: root.active ? Theme.withAlpha(Theme.activeAccent, 0.4) : Theme.panelBorder
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: Theme.activeAccent
            visible: root.active
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            BusyIndicator {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                running: root.showLoadingRail
                visible: running
            }

            Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                radius: Theme.radiusSm
                visible: !root.showLoadingRail && root.statusMessage.length > 0
                color: Theme.accent
                opacity: 0.9

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.5; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1.0; to: 0.5; duration: 1000; easing.type: Easing.InOutSine }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    Layout.fillWidth: true
                    text: root.showLoadingRail ? (root.isCurrentPathArchive ? "Loading archive..." : "Scanning folder") : root.statusMessage
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    font.weight: root.showLoadingRail ? Font.Medium : Font.Normal
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.showLoadingRail
                    text: {
                        if (root.loadingFolderNameProvider) {
                            return "Reading items from " + root.loadingFolderNameProvider()
                        }
                        return "Reading items"
                    }
                    color: Theme.textSecondary
                    opacity: 0.8
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
