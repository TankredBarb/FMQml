import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 640
    height: 720
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 250; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.92; to: 1.0; duration: 250; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; from: 1.0; to: 0.92; duration: 200; easing.type: Easing.InQuad }
    }

    background: Rectangle {
        color: Theme.surface
        radius: 20
        border.color: Theme.border
        border.width: 1
        
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.8
            shadowVerticalOffset: 8
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
        }

        // Subtle gradient overlay for premium feel
        Rectangle {
            anchors.fill: parent
            radius: 20
            opacity: 0.03
            gradient: Gradient {
                GradientStop { position: 0.0; color: "white" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header Section
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 24
                spacing: 20

                Rectangle {
                    width: 48
                    height: 48
                    radius: 12
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)
                    
                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/info.svg"
                        sourceSize: Qt.size(24, 24)
                        smooth: true
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    Label {
                        text: "Shortcuts Guide"
                        font.bold: true
                        font.pixelSize: 22
                        color: Theme.textPrimary
                    }
                    Label {
                        text: "Master FM with keyboard efficiency"
                        font.pixelSize: 13
                        color: Theme.textSecondary
                        opacity: 0.8
                    }
                }

                ToolButton {
                    onClicked: root.close()
                    background: Rectangle {
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: parent.hovered ? Theme.surfaceHover : "transparent"
                        border.color: parent.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : "transparent"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✕"
                        font.pixelSize: 18
                        color: parent.hovered ? Theme.accent : Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 64
                height: 1
                color: Theme.border
                opacity: 0.5
            }
        }

        // Content Scroll Area
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Pane {
                width: parent.width
                padding: 32
                background: null

                ColumnLayout {
                    width: parent.width
                    spacing: 40

                    HelpSection {
                        title: "CORE NAVIGATION"
                        accentColor: Theme.accent
                        items: [
                            { key: "F1", desc: "Show this reference guide" },
                            { key: "F3", desc: "Toggle dual-pane split view" },
                            { key: "Tab", desc: "Cycle focus between active panels" },
                            { key: "F5", desc: "Refresh file list & directory state" },
                            { key: "Space", desc: "Preview file or view folder properties" },
                            { key: "Esc", desc: "Clear selection or dismiss dialogs" }
                        ]
                    }

                    HelpSection {
                        title: "MOVING AROUND"
                        accentColor: "#3498db"
                        items: [
                            { key: "Alt + ↑", desc: "Navigate to parent directory" },
                            { key: "Alt + ←", desc: "Go back in navigation history" },
                            { key: "Alt + →", desc: "Go forward in navigation history" },
                            { key: "Ctrl + L", desc: "Jump to path bar for manual entry" },
                            { key: "/", desc: "Instant search in current folder" }
                        ]
                    }

                    HelpSection {
                        title: "FILE MANAGEMENT"
                        accentColor: "#e67e22"
                        items: [
                            { key: "F2", desc: "Rename the currently focused item" },
                            { key: "Ctrl + C", desc: "Copy selection to clipboard" },
                            { key: "Ctrl + X", desc: "Cut selection to clipboard" },
                            { key: "Ctrl + V", desc: "Paste items from clipboard" },
                            { key: "Del", desc: "Permanently delete selected items" },
                            { key: "Ctrl + Z", desc: "Undo the last file operation" },
                            { key: "Ctrl + Y", desc: "Redo the previously undone action" }
                        ]
                    }

                    HelpSection {
                        title: "SELECTING"
                        accentColor: "#9b59b6"
                        items: [
                            { key: "Ctrl + A", desc: "Select all items in current view" },
                            { key: "Ctrl + Click", desc: "Add or remove individual items" }
                        ]
                    }
                }
            }
        }
        
        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"
            
            Label {
                anchors.centerIn: parent
                text: "Built with passion for speed and aesthetics"
                font.pixelSize: 11
                color: Theme.textSecondary
                opacity: 0.5
                font.italic: true
            }
        }
    }

    component HelpSection: ColumnLayout {
        property string title
        property color accentColor: Theme.accent
        property var items: []
        Layout.fillWidth: true
        spacing: 16

        RowLayout {
            spacing: 12
            Rectangle {
                width: 4
                height: 16
                radius: 2
                color: accentColor
            }
            Label {
                text: title
                font.bold: true
                font.pixelSize: 12
                font.letterSpacing: 1.2
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
        }

        GridLayout {
            columns: 1
            rowSpacing: 14
            Layout.fillWidth: true

            Repeater {
                model: items
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 20
                    
                    // Modern "Keycap" look
                    Rectangle {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 28
                        color: themeController.isDark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.03)
                        radius: 6
                        border.color: Theme.border
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: modelData.key
                            font.family: "Segoe UI", "Inter", "sans-serif"
                            font.pixelSize: 11
                            font.bold: true
                            color: Theme.textPrimary
                        }
                        
                        // Subtle 3D effect for the keycap
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 2
                            radius: 2
                            color: Theme.border
                            opacity: 0.3
                        }
                    }

                    Label {
                        text: modelData.desc
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.WordWrap
                        opacity: 0.9
                    }
                }
            }
        }
    }
}
