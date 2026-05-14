import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 400
    height: 500
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
            shadowBlur: 0.5
            shadowVerticalOffset: 5
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Image {
                    source: "image://icon/" + propertiesController.path
                    sourceSize: Qt.size(48, 48)
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                }

                Label {
                    text: propertiesController.name
                    font.bold: true
                    font.pixelSize: 18
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
            }
        }

        // Details
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Pane {
                width: parent.width
                padding: 24
                background: null

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 24

                    // Basic Info Section
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: "DETAILS"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                        
                        PropertyRow { label: "Type:"; value: propertiesController.typeText }
                        PropertyRow { label: "Location:"; value: propertiesController.path; elideMode: true }
                        PropertyRow { label: "Size:"; value: propertiesController.sizeText; visible: !propertiesController.isDirectory }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border; opacity: 0.4 }

                    // Dates Section
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: "DATES"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                        
                        PropertyRow { label: "Created:"; value: propertiesController.created }
                        PropertyRow { label: "Modified:"; value: propertiesController.modified }
                        PropertyRow { label: "Accessed:"; value: propertiesController.accessed }
                    }
                }
            }
        }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "transparent"

            Button {
                anchors.centerIn: parent
                text: "Close"
                onClicked: root.close()
                
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 36
                    radius: 8
                    color: parent.hovered ? Theme.accent : Theme.surface
                    border.color: Theme.accent
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                contentItem: Label {
                    text: parent.text
                    font.bold: true
                    font.pixelSize: 13
                    color: parent.hovered ? Theme.accentText : Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // Helper component for rows
    component PropertyRow : RowLayout {
        property string label: ""
        property string value: ""
        property bool elideMode: false
        spacing: 10
        Label {
            text: label
            color: Theme.textSecondary
            Layout.preferredWidth: 80
        }
        Label {
            text: value
            color: Theme.textPrimary
            Layout.fillWidth: true
            elide: elideMode ? Text.ElideMiddle : Text.ElideNone
            wrapMode: elideMode ? Text.NoWrap : Text.Wrap
            font.pixelSize: 12
        }
    }

    Connections {
        target: propertiesController
        function onVisibleChanged() {
            if (propertiesController.visible) root.open()
            else root.close()
        }
    }

    onClosed: propertiesController.visible = false
}
