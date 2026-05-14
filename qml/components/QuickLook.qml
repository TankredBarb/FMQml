import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.85, 900)
    height: Math.min(parent.height * 0.85, 650)
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Enter/Exit Animations
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 250; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.95; duration: 150; easing.type: Easing.InCubic }
    }

    background: Item {
        Rectangle {
            id: bgRect
            anchors.fill: parent
            color: themeController.isDark ? "#222222" : "#FFFFFF"
            radius: 16
            opacity: 0.95 // Slight transparency for modern look
            border.color: Theme.border
            border.width: 1
        }

        // Shadow effect
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowBlur: 0.8
            shadowVerticalOffset: 10
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header with blurred background feel
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                spacing: 12
                
                Image {
                    source: "image://icon/" + quickLookController.path
                    sourceSize: Qt.size(24, 24)
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: -2
                    Label {
                        text: quickLookController.path.split(/[/\\]/).pop()
                        font.bold: true
                        font.pixelSize: 15
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }
                    Label {
                        text: quickLookController.type.toUpperCase() + " Preview"
                        font.pixelSize: 10
                        font.capitalization: Font.AllUppercase
                        color: Theme.textSecondary
                        opacity: 0.7
                    }
                }

                ToolButton {
                    id: closeBtn
                    onClicked: root.close()
                    padding: 8
                    contentItem: Image {
                        source: "../assets/icons/eye-off.svg"
                        sourceSize: Qt.size(18, 18)
                        opacity: closeBtn.hovered ? 1.0 : 0.6
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }
                    background: Rectangle {
                        implicitWidth: 36
                        implicitHeight: 36
                        color: parent.hovered ? Theme.surfaceHover : "transparent"
                        radius: 18
                    }
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            opacity: 0.5
        }

        // Content Area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Enhanced Text Preview
            Item {
                anchors.fill: parent
                visible: quickLookController.type === "text" || quickLookController.type === "info"

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Line Numbers Sidebar
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 45
                        color: themeController.isDark ? "#2a2a2a" : "#f5f5f5"
                        visible: quickLookController.type === "text"

                        Column {
                            anchors.fill: parent
                            anchors.topMargin: 24
                            spacing: 0
                            Repeater {
                                model: Math.min(quickLookController.lines, 100) // Show up to 100 line numbers for perf
                                Label {
                                    width: parent.width
                                    text: index + 1
                                    font.family: "Cascadia Code, Consolas, Monospace"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                    opacity: 0.5
                                    horizontalAlignment: Text.AlignHCenter
                                    height: 18.2 // Match TextArea line height approx
                                }
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.border
                            opacity: 0.3
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        TextArea {
                            id: textPreview
                            text: quickLookController.content
                            readOnly: true
                            color: Theme.textPrimary
                            font.family: "Cascadia Code, Consolas, Monospace"
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            padding: 24
                            topPadding: 24
                            background: null
                            
                            selectionColor: Theme.accent
                            selectedTextColor: Theme.accentText

                            // Subtle code-like feel
                            textFormat: Text.PlainText
                        }
                    }
                }
                
                // Text Metadata Overlay (Bottom Right)
                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 16
                    width: metaLayout.implicitWidth + 24
                    height: 28
                    radius: 14
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                    border.color: Theme.accent
                    border.width: 1
                    visible: quickLookController.type === "text"

                    RowLayout {
                        id: metaLayout
                        anchors.centerIn: parent
                        spacing: 8
                        Label {
                            text: quickLookController.extension.toUpperCase()
                            font.bold: true
                            font.pixelSize: 10
                            color: Theme.accent
                        }
                        Rectangle { width: 1; height: 10; color: Theme.accent; opacity: 0.5 }
                        Label {
                            text: quickLookController.lines + " lines"
                            font.pixelSize: 10
                            color: Theme.textPrimary
                        }
                    }
                }
            }

            // Image Preview with Fade-in and Loading State
            Item {
                anchors.fill: parent
                visible: quickLookController.type === "image"
                
                // Optimized Image Loading
                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: 20
                    source: (quickLookController.type === "image" && root.opened) ? ("image://thumbnail/" + quickLookController.path) : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    sourceSize.width: 1280
                    sourceSize.height: 1280
                    smooth: true
                    opacity: status === Image.Ready ? 1.0 : 0.0
                    
                    Behavior on opacity { 
                        NumberAnimation { duration: 400; easing.type: Easing.OutCubic } 
                    }
                }

                // Stylized "Please Wait" Overlay
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    visible: previewImage.status !== Image.Ready
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 16
                        
                        BusyIndicator {
                            id: busyIndicator
                            Layout.alignment: Qt.AlignHCenter
                            running: previewImage.status === Image.Loading
                            
                            contentItem: Item {
                                implicitWidth: 48
                                implicitHeight: 48
                                
                                // Custom rotating indicator for modern look
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 24
                                    color: "transparent"
                                    border.color: Theme.accent
                                    border.width: 3
                                    opacity: 0.2
                                }
                                
                                Canvas {
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.beginPath();
                                        ctx.strokeStyle = Theme.accent;
                                        ctx.lineWidth = 3;
                                        ctx.arc(24, 24, 21, 0, Math.PI * 0.5);
                                        ctx.stroke();
                                    }
                                    
                                    RotationAnimation on rotation {
                                        from: 0; to: 360; duration: 800; loops: Animation.Infinite
                                        running: busyIndicator.running
                                    }
                                }
                            }
                        }
                        
                        Label {
                            text: "Preparing preview..."
                            font.pixelSize: 13
                            color: Theme.textSecondary
                            Layout.alignment: Qt.AlignHCenter
                            
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { from: 0.4; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 1.0; to: 0.4; duration: 800; easing.type: Easing.InOutQuad }
                            }
                        }
                    }
                }
            }
        }
    }

    onOpened: forceActiveFocus()
    
    Connections {
        target: quickLookController
        function onVisibleChanged() {
            if (quickLookController.visible) {
                if (!root.opened) root.open()
            } else {
                if (root.opened) root.close()
            }
        }
    }
    
    onClosed: quickLookController.visible = false
}
