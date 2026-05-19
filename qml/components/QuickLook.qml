import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Popup {
    id: root

    property string previewPath: ""

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.85, 900)
    height: Math.min(parent.height * 0.85, 650)
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
            color: Theme.glassSurfaceStrong
            radius: 16
            border.color: Theme.glassBorder
            border.width: 1
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

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
                    source: "image://icon/" + root.previewPath
                    sourceSize: Qt.size(24, 24)
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: -2
                    Label {
                        text: root.previewPath.split(/[/\\]/).pop()
                        font.bold: true
                        font.pixelSize: 15
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }
                    Label {
                        text: quickLookController.type.toUpperCase() + " Preview"
                        font.pixelSize: 10
                        color: Theme.textSecondary
                        opacity: 0.7
                    }
                }

                Button {
                    id: closeBtn
                    onClicked: root.close()
                    hoverEnabled: true
                    
                    background: Rectangle {
                        implicitWidth: 32
                        implicitHeight: 32
                        radius: 16
                        color: closeBtn.hovered ? (themeController.isDark ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.06)) : "transparent"
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                        
                        scale: closeBtn.hovered ? 1.08 : 1.0
                        Behavior on scale {
                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                        }
                    }

                    contentItem: Item {
                        implicitWidth: 32
                        implicitHeight: 32
                        
                        Item {
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            
                            Rectangle {
                                anchors.centerIn: parent
                                width: 14
                                height: 1.5
                                rotation: 45
                                color: closeBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                                opacity: closeBtn.hovered ? 1.0 : 0.7
                                radius: 0.75
                                antialiasing: true
                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 14
                                height: 1.5
                                rotation: -45
                                color: closeBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                                opacity: closeBtn.hovered ? 1.0 : 0.7
                                radius: 0.75
                                antialiasing: true
                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            opacity: themeController.isDark ? 0.34 : 0.26
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Item {
                anchors.fill: parent
                visible: quickLookController.type === "text" || quickLookController.type === "info"

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Line Numbers Sidebar with transparency
                    Rectangle {
                        id: lineNumbersSidebar
                        Layout.fillHeight: true
                        Layout.preferredWidth: 45
                        color: Theme.glassSurfaceSoft
                        visible: quickLookController.type === "text"
                        clip: true

                        readonly property real lineSpacing: textPreview.lineCount > 0 ? textPreview.contentHeight / textPreview.lineCount : 18.2

                        Column {
                            id: lineNumbersColumn
                            x: 0
                            y: 24 - (textPreviewScrollView.contentItem ? textPreviewScrollView.contentItem.contentY : 0)
                            width: parent.width
                            spacing: 0
                            
                            Repeater {
                                model: textPreview.lineCount
                                Label {
                                    width: parent.width
                                    text: index + 1
                                    font.family: "Cascadia Code, Consolas, Monospace"
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                    opacity: 0.5
                                    horizontalAlignment: Text.AlignHCenter
                                    height: lineNumbersSidebar.lineSpacing
                                }
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.border
                            opacity: 0.2
                        }
                    }

                    ScrollView {
                        id: textPreviewScrollView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                        background: null
                        clip: true

                        TextArea {
                            id: textPreview
                            text: quickLookController.content
                            readOnly: true
                            color: Theme.textPrimary
                            font.family: "Cascadia Code, Consolas, Monospace"
                            font.pixelSize: 13
                            wrapMode: Text.NoWrap
                            padding: 24
                            topPadding: 24
                            bottomPadding: 24
                            background: null
                            selectByMouse: true
                            selectionColor: Theme.accent
                            selectedTextColor: Theme.accentText
                        }
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: ["image", "video", "svg", "pdf", "font", "audio"].includes(quickLookController.type)
                
                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: 20
                    source: ((["image", "video", "svg", "font", "audio"].includes(quickLookController.type) || 
                              (quickLookController.type === "pdf" && !quickLookController.hasPdfSupport)) && 
                             root.opened && root.previewPath.length > 0) ? ("image://thumbnail/" + root.previewPath) : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    sourceSize.width: 2048
                    sourceSize.height: 2048
                    smooth: true
                    opacity: status === Image.Ready ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                }

                Loader {
                    id: pdfPreviewerLoader
                    anchors.fill: parent
                    anchors.margins: 20
                    visible: quickLookController.type === "pdf" && quickLookController.hasPdfSupport
                    source: visible ? "PdfPreviewer.qml" : ""
                }
                
                Binding {
                    target: pdfPreviewerLoader.item
                    property: "sourcePath"
                    value: root.previewPath
                    when: pdfPreviewerLoader.status === Loader.Ready
                }

                // Overlay icon for video
                Image {
                    anchors.centerIn: parent
                    source: "../assets/icons/video.svg"
                    sourceSize: Qt.size(64, 64)
                    visible: quickLookController.type === "video" && previewImage.status === Image.Ready
                    opacity: 0.6
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: previewImage.status === Image.Loading
                }
                
                // Fallback for PDF when no system thumbnail is available
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: quickLookController.type === "pdf" && previewImage.status === Image.Error
                    spacing: 24

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 120
                        height: 120
                        radius: 24
                        color: Qt.rgba(219/255, 68/255, 55/255, 0.15)
                        
                        Image {
                            anchors.centerIn: parent
                            source: "../assets/icons/document.svg"
                            sourceSize: Qt.size(60, 60)
                            opacity: 0.8
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        
                        Label {
                            text: "PDF Document"
                            font.bold: true
                            font.pixelSize: 18
                            color: Theme.textPrimary
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: "No system preview available"
                            font.pixelSize: 12
                            color: Theme.textSecondary
                            Layout.alignment: Qt.AlignHCenter
                            opacity: 0.7
                        }
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: quickLookController.type === "audio" && previewImage.status !== Image.Ready

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 24

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 120
                        height: 120
                        radius: 60
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                        
                        Image {
                            anchors.centerIn: parent
                            source: "../assets/icons/music.svg"
                            sourceSize: Qt.size(60, 60)
                            opacity: 0.8
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        Label {
                            text: "Audio File"
                            font.bold: true
                            font.pixelSize: 18
                            color: Theme.textPrimary
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: quickLookController.mimeName
                            font.pixelSize: 12
                            color: Theme.textSecondary
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }

            // Shortcut stub
            Item {
                anchors.fill: parent
                visible: quickLookController.type === "shortcut"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 24

                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: root.opened && root.previewPath.length > 0 ? "image://icon/" + root.previewPath : ""
                        sourceSize: Qt.size(128, 128)
                        smooth: true
                    }

                    Label {
                        text: "Shortcut"
                        Layout.alignment: Qt.AlignHCenter
                        font.bold: true
                        font.pixelSize: 18
                        color: Theme.textPrimary
                    }
                }
            }

            // Executable metadata preview card
            Item {
                anchors.fill: parent
                visible: quickLookController.type === "executable"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 40
                    spacing: 40

                    // Left Column: Big Icon and Primary info
                    ColumnLayout {
                        Layout.preferredWidth: 220
                        Layout.fillHeight: true
                        spacing: 16
                        Layout.alignment: Qt.AlignVCenter

                        Item { Layout.fillHeight: true }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 140
                            height: 140
                            radius: 28
                            color: themeController.isDark ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                            border.color: Theme.glassBorder
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                source: (quickLookController.type === "executable" && root.opened && root.previewPath.length > 0) ? ("image://icon/" + root.previewPath) : ""
                                sourceSize: Qt.size(96, 96)
                                smooth: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            
                            Label {
                                text: quickLookController.name
                                font.bold: true
                                font.pixelSize: 18
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideMiddle
                            }
                            Label {
                                text: quickLookController.sizeText
                                font.pixelSize: 13
                                color: Theme.textSecondary
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                opacity: 0.8
                            }
                            Label {
                                text: "Application"
                                font.pixelSize: 11
                                font.capitalization: Font.AllUppercase
                                font.weight: Font.Bold
                                color: Theme.accent
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                opacity: 0.9
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // Vertical Separator
                    Rectangle {
                        Layout.fillHeight: true
                        width: 1
                        color: Theme.border
                        opacity: 0.2
                    }

                    // Right Column: Metadata List
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        ColumnLayout {
                            width: parent.width
                            spacing: 14
                            
                            Label {
                                text: "Version Information"
                                font.bold: true
                                font.pixelSize: 15
                                color: Theme.textPrimary
                                Layout.bottomMargin: 8
                            }

                            Repeater {
                                model: quickLookController.extraProperties
                                
                                delegate: ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Label {
                                        text: modelData.label
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: Theme.textSecondary
                                        opacity: 0.6
                                    }
                                    Label {
                                        text: modelData.value
                                        font.pixelSize: 13
                                        color: Theme.textPrimary
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                    }

                                    // Inner separator line
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 1
                                        color: Theme.border
                                        opacity: 0.1
                                        Layout.topMargin: 8
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    onOpened: forceActiveFocus()
}
