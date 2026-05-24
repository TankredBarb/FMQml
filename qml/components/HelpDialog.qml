import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 600
    height: 680
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        color: Theme.panelSurface
        radius: Theme.radiusLg
        border.color: Theme.panelBorder
        border.width: 1
        
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 20
            shadowVerticalOffset: 8
            shadowColor: Theme.glassShadow
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.close()
                event.accepted = true
            }
        }

        // Header Section
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
                    spacing: 2
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    Label {
                        text: "Shortcuts Guide"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Label {
                        text: "Master FM with keyboard efficiency"
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }

                Button {
                    id: closeBtn
                    flat: true
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: root.close()
                    
                    contentItem: Label {
                        text: "✕"
                        font.pixelSize: 14
                        color: closeBtn.hovered ? Theme.accent : Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: closeBtn.pressed ? Theme.surfaceActive : (closeBtn.hovered ? Theme.panelSurfaceSoft : "transparent")
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.panelBorder
                opacity: 0.4
            }
        }

        // Content Scroll Area
        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ScrollBar.vertical: ScrollBar { 
                policy: ScrollBar.AsNeeded 
            }

            Pane {
                width: scrollView.availableWidth
                padding: 20
                background: null

                ColumnLayout {
                    width: parent.width
                    spacing: 28

                    HelpSection {
                        title: "CORE NAVIGATION"
                        accentColor: Theme.accent
                        items: [
                            { key: "F1", desc: "Show this reference guide" },
                            { key: "Enter", desc: "Open selected file, folder, or drive" },
                            { key: "Space", desc: "Preview file or view folder properties" },
                            { key: "Tab", desc: "Cycle focus between active panels" },
                            { key: "F3", desc: "Toggle dual-pane split view" },
                            { key: "F5", desc: "Refresh file list & directory state" },
                            { key: "Esc", desc: "Clear selection or dismiss dialogs" }
                        ]
                    }

                    HelpSection {
                        title: "MOVING AROUND"
                        accentColor: "#3498db"
                        items: [
                            { key: "Alt + ↑", desc: "Navigate to parent directory" },
                            { key: "Backspace", desc: "Navigate to parent directory" },
                            { key: "Alt + ←", desc: "Go back in navigation history" },
                            { key: "Alt + →", desc: "Go forward in navigation history" },
                            { key: "Ctrl + L", desc: "Jump to path bar for manual entry" },
                            { key: "A-Z / /", desc: "Start typing name to search" }
                        ]
                    }

                    HelpSection {
                        title: "FILE MANAGEMENT"
                        accentColor: "#e67e22"
                        items: [
                            { key: "F2", desc: "Rename focused item (Batch rename if multiple)" },
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
                            { key: "Ctrl + Click", desc: "Add or remove individual items" },
                            { key: "Shift + Click", desc: "Select range of items (inclusive)" }
                        ]
                    }
                }
            }
        }
        
        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"
            
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.panelBorder
                opacity: 0.4
            }

            Label {
                anchors.centerIn: parent
                text: "Built with passion for speed and aesthetics"
                font.pixelSize: 10
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
        spacing: 12

        RowLayout {
            spacing: 8
            Rectangle {
                width: 3
                height: 12
                radius: 1.5
                color: accentColor
            }
            Label {
                text: title
                font.bold: true
                font.pixelSize: 11
                font.letterSpacing: 1.0
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: items
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    
                    // Modern "Keycap" look
                    Rectangle {
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 24
                        color: Theme.panelSurfaceSoft
                        radius: Theme.radiusSm
                        border.color: Theme.panelBorder
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: modelData.key
                            font.family: "Segoe UI", "Inter", "sans-serif"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                    }

                    Label {
                        text: modelData.desc
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
