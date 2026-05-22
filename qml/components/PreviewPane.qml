import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Pane {
    id: root

    padding: 0
    clip: true

    implicitWidth: 320
    implicitHeight: 480

    background: Rectangle {
        color: themeController.isDark ? Theme.surface : Theme.bg
        border.color: Theme.border
        border.width: 1

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          themeController.isDark ? 0.045 : 0.065)
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 12

                    Image {
                        source: quickLookController.path.length > 0
                            && quickLookController.path !== "devices://"
                            ? "image://icon/" + encodeURIComponent(quickLookController.path)
                            : quickLookController.type === "info"
                              ? "../assets/icons/computer.svg"
                              : "../assets/lucide-toolbar/panel-right.svg"
                    sourceSize: Qt.size(24, 24)
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    smooth: true
                    mipmap: false
                    opacity: quickLookController.path.length > 0 ? 1.0 : 0.92
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        text: quickLookController.path.length > 0
                              && quickLookController.path !== "devices://"
                              ? quickLookController.path.split(/[/\\]/).pop()
                              : quickLookController.type === "info"
                                ? quickLookController.name
                                : "Preview"
                        font.bold: true
                        font.pixelSize: 14
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }

                    Label {
                        text: quickLookController.path.length > 0
                              && quickLookController.path !== "devices://"
                              ? quickLookController.type.toUpperCase() + " Preview"
                              : quickLookController.type === "info"
                                ? "System Overview"
                                : "Select a file or folder to inspect it here"
                        font.pixelSize: 10
                        color: Theme.textSecondary
                        opacity: 0.80
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                ToolButton {
                    id: closeBtn
                    onClicked: quickLookController.visible = false
                    padding: 8
                    contentItem: Image {
                        source: "../assets/lucide-toolbar/eye-off.svg"
                        sourceSize: Qt.size(18, 18)
                        smooth: true
                        mipmap: false
                        opacity: closeBtn.hovered ? 1.0 : 0.72
                    }
                    background: Rectangle {
                        implicitWidth: 36
                        implicitHeight: 36
                        color: closeBtn.hovered ? Theme.surfaceHover : "transparent"
                        radius: 18
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            opacity: themeController.isDark ? 0.34 : 0.24
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Item {
                anchors.fill: parent
                visible: quickLookController.path.length === 0
                         && quickLookController.path !== "devices://"
                         && quickLookController.type !== "info"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    width: Math.min(parent.width - 32, 260)

                    Image {
                        source: "../assets/lucide-toolbar/panel-right.svg"
                        sourceSize: Qt.size(42, 42)
                        Layout.alignment: Qt.AlignHCenter
                        opacity: 0.42
                    }

                    Label {
                        text: "No file selected"
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Label {
                        text: "Select a file or folder in the active panel to see preview and metadata here."
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        radius: 10
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, themeController.isDark ? 0.10 : 0.08)
                        border.color: Theme.border
                        border.width: 1
                        implicitHeight: hintText.implicitHeight + 14
                        implicitWidth: hintText.implicitWidth + 18

                        Label {
                            id: hintText
                            anchors.centerIn: parent
                            text: "Preview follows the active panel"
                            color: Theme.textSecondary
                            font.pixelSize: 10
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: (["image", "video", "audio", "archive", "svg", "pdf", "font", "executable", "shortcut"].includes(quickLookController.type))
                                               ? 224
                                               : (quickLookController.type === "text" ? 220 : 190)
                    radius: 16
                    color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)
                    border.color: Theme.border
                    border.width: 1
                    clip: true

                    Item {
                        anchors.fill: parent
                        anchors.margins: 14

                        Item {
                            anchors.fill: parent
                            visible: quickLookController.type === "archive"

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 16

                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 80
                                    height: 80
                                    radius: 16
                                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                    
                                    Image {
                                        anchors.centerIn: parent
                                        source: "../assets/icons/folder.svg" // Using folder as archive base
                                        sourceSize: Qt.size(40, 40)
                                        opacity: 0.8
                                    }
                                }

                                Label {
                                    text: "Archive File"
                                    Layout.alignment: Qt.AlignHCenter
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }
                        }

                        Item {
                            anchors.fill: parent
                            visible: ["image", "video", "svg", "pdf", "font", "audio"].includes(quickLookController.type)

                            Image {
                                id: previewImage
                                anchors.fill: parent
                                source: (quickLookController.path.length > 0 && 
                                         (["image", "video", "svg", "font", "audio"].includes(quickLookController.type) || 
                                          (quickLookController.type === "pdf" && !quickLookController.hasPdfSupport)))
                                        ? ("image://thumbnail/" + encodeURIComponent(quickLookController.path))
                                        : ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: false
                                sourceSize.width: 512
                                sourceSize.height: 512
                                smooth: true
                                opacity: status === Image.Ready ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 250 } }
                            }

                            Loader {
                                id: pdfPreviewerLoader
                                anchors.fill: parent
                                visible: quickLookController.type === "pdf" && quickLookController.hasPdfSupport
                                source: visible ? "PdfPreviewer.qml" : ""
                            }
                            
                            Binding {
                                target: pdfPreviewerLoader.item
                                property: "sourcePath"
                                value: quickLookController.path
                                when: pdfPreviewerLoader.status === Loader.Ready
                            }

                            // Overlay icon for video
                            Image {
                                anchors.centerIn: parent
                                source: "../assets/icons/video.svg"
                                sourceSize: Qt.size(48, 48)
                                visible: quickLookController.type === "video" && previewImage.status === Image.Ready
                                opacity: 0.7
                            }

                            BusyIndicator {
                                anchors.centerIn: parent
                                running: previewImage.status === Image.Loading
                            }

                            Rectangle {
                                anchors.fill: parent
                                visible: previewImage.status === Image.Loading
                                color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.58 : 0.64)

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 10
                                    width: Math.min(parent.width - 24, 240)

                                    BusyIndicator {
                                        running: true
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }

                                    Label {
                                        text: quickLookController.type === "video" ? "Loading video preview..." : (quickLookController.type === "pdf" ? "Loading PDF preview..." : "Loading image preview...")
                                        color: Theme.textSecondary
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignHCenter
                                        width: parent.width
                                    }
                                }
                            }
                        }

                        // Fallback for PDF when no system thumbnail is available
                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: quickLookController.type === "pdf" && previewImage.status === Image.Error
                            spacing: 16

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 80
                                height: 80
                                radius: 16
                                color: Qt.rgba(219/255, 68/255, 55/255, 0.15)
                                
                                Image {
                                    anchors.centerIn: parent
                                    source: "../assets/icons/document.svg"
                                    sourceSize: Qt.size(40, 40)
                                    opacity: 0.8
                                }
                            }

                            Label {
                                text: "PDF Document"
                                Layout.alignment: Qt.AlignHCenter
                                font.bold: true
                                font.pixelSize: 13
                                color: Theme.textPrimary
                            }
                            
                            Label {
                                text: "No system preview available"
                                Layout.alignment: Qt.AlignHCenter
                                font.pixelSize: 11
                                color: Theme.textSecondary
                                opacity: 0.7
                            }
                        }

                        Item {
                            anchors.fill: parent
                            visible: quickLookController.type === "audio" && previewImage.status !== Image.Ready

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 16

                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 80
                                    height: 80
                                    radius: 40
                                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                                    
                                    Image {
                                        anchors.centerIn: parent
                                        source: "../assets/icons/music.svg"
                                        sourceSize: Qt.size(40, 40)
                                        opacity: 0.8
                                    }
                                }

                                Label {
                                    text: "Audio File"
                                    Layout.alignment: Qt.AlignHCenter
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }
                        }

                        Item {
                            anchors.fill: parent
                            visible: quickLookController.type === "executable" || quickLookController.type === "shortcut"

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 16

                                Image {
                                    Layout.alignment: Qt.AlignHCenter
                                    source: quickLookController.path.length > 0 ? "image://icon/" + encodeURIComponent(quickLookController.path) : ""
                                    sourceSize: Qt.size(64, 64)
                                    smooth: true
                                }

                                Label {
                                    text: quickLookController.type === "shortcut" ? "Shortcut" : "Executable"
                                    Layout.alignment: Qt.AlignHCenter
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                            }
                        }

                        Item {
                            anchors.fill: parent
                            visible: quickLookController.type === "text"

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 48
                                    color: Theme.glassSurfaceSoft

                                    Column {
                                        anchors.fill: parent
                                        anchors.topMargin: 18
                                        spacing: 0

                                        Repeater {
                                            model: Math.min(quickLookController.lines, 100)

                                            Label {
                                                width: parent.width
                                                text: index + 1
                                                font.family: "Cascadia Code, Consolas, Monospace"
                                                font.pixelSize: 11
                                                color: Theme.textSecondary
                                                opacity: 0.55
                                                horizontalAlignment: Text.AlignHCenter
                                                height: 18
                                            }
                                        }
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true

                                    ScrollView {
                                        anchors.fill: parent
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        background: null
                                        clip: true

                                        TextArea {
                                            text: quickLookController.content
                                            readOnly: true
                                            color: Theme.textPrimary
                                            font.family: "Cascadia Code, Consolas, Monospace"
                                            font.pixelSize: 13
                                            wrapMode: Text.Wrap
                                            padding: 18
                                            topPadding: 18
                                            background: null
                                            selectByMouse: true
                                            selectionColor: Theme.accent
                                            selectedTextColor: Theme.accentText
                                            opacity: quickLookController.loading ? 0.35 : 1.0
                                        }
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        visible: quickLookController.loading
                                        color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.72 : 0.78)

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 10
                                            width: Math.min(parent.width - 24, 220)

                                            BusyIndicator {
                                                running: true
                                                anchors.horizontalCenter: parent.horizontalCenter
                                            }

                                            Label {
                                                text: "Loading preview..."
                                                color: Theme.textSecondary
                                                font.pixelSize: 11
                                                horizontalAlignment: Text.AlignHCenter
                                                width: parent.width
                                            }

                                            Label {
                                                text: "Large files are loaded asynchronously."
                                                color: Theme.textSecondary
                                                opacity: 0.75
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignHCenter
                                                width: parent.width
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            anchors.fill: parent
                            visible: quickLookController.type === "info"

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 112
                                    radius: 14
                                    color: themeController.isDark ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                                                                 : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                                    border.color: Theme.border
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 12

                                        Image {
                                            source: quickLookController.path.length > 0
                                                    && quickLookController.path !== "devices://"
                                                    ? "image://icon/" + encodeURIComponent(quickLookController.path)
                                                    : "../assets/icons/computer.svg"
                                            sourceSize: Qt.size(40, 40)
                                            Layout.preferredWidth: 40
                                            Layout.preferredHeight: 40
                                            smooth: true
                                            mipmap: false
                                            opacity: 0.92
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Label {
                                                text: quickLookController.name.length > 0 ? quickLookController.name : "Item"
                                                font.pixelSize: 14
                                                font.bold: true
                                                color: Theme.textPrimary
                                                Layout.fillWidth: true
                                                elide: Text.ElideMiddle
                                            }

                                            Label {
                                                text: quickLookController.mimeName === "drive"
                                                      ? quickLookController.extension.toUpperCase()
                                                      : quickLookController.directory ? "Folder" : "File"
                                                font.pixelSize: 11
                                                color: Theme.textSecondary
                                            }

                                            Label {
                                                text: quickLookController.sizeText + "  |  " + quickLookController.modifiedText
                                                font.pixelSize: 11
                                                color: Theme.textSecondary
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }

                                            RowLayout {
                                                visible: quickLookController.path.length > 0
                                                         && quickLookController.path !== "devices://"
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Rectangle {
                                                    visible: quickLookController.hidden
                                                    radius: 8
                                                    color: Theme.surfaceHover
                                                    border.color: Theme.border
                                                    border.width: 1
                                                    implicitHeight: 20
                                                    implicitWidth: hiddenTag.implicitWidth + 14

                                                    Label {
                                                        id: hiddenTag
                                                        anchors.centerIn: parent
                                                        text: "Hidden"
                                                        font.pixelSize: 9
                                                        color: Theme.textSecondary
                                                    }
                                                }

                                                Rectangle {
                                                    visible: quickLookController.symlink
                                                    radius: 8
                                                    color: Theme.surfaceHover
                                                    border.color: Theme.border
                                                    border.width: 1
                                                    implicitHeight: 20
                                                    implicitWidth: linkTag.implicitWidth + 14

                                                    Label {
                                                        id: linkTag
                                                        anchors.centerIn: parent
                                                        text: "Symlink"
                                                        font.pixelSize: 9
                                                        color: Theme.textSecondary
                                                    }
                                                }

                                                Rectangle {
                                                    radius: 8
                                                    color: Theme.surfaceHover
                                                    border.color: Theme.border
                                                    border.width: 1
                                                    implicitHeight: 20
                                                    implicitWidth: accessTag.implicitWidth + 14

                                                    Label {
                                                        id: accessTag
                                                        anchors.centerIn: parent
                                                        text: quickLookController.permissionsText
                                                        font.pixelSize: 9
                                                        color: Theme.textSecondary
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }

                                Item {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    background: null

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: {
                                let base = [
                                    { label: "Name", value: quickLookController.name },
                                    { label: "Type", value: quickLookController.directory ? "Folder" : "File" },
                                    { label: "Size", value: quickLookController.sizeText },
                                    { label: "Modified", value: quickLookController.modifiedText }
                                ];
                                
                                // Add extra properties from controller
                                for (let i = 0; i < quickLookController.extraProperties.length; i++) {
                                    base.push(quickLookController.extraProperties[i]);
                                }
                                
                                return base;
                            }

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                radius: 12
                                color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
                                border.color: Theme.border
                                border.width: 1
                                implicitHeight: valueColumn.implicitHeight + 26

                                Item {
                                    anchors.fill: parent
                                    anchors.margins: 14

                                    ColumnLayout {
                                        id: valueColumn
                                        width: parent.width
                                        spacing: 4

                                        Label {
                                            text: modelData.label
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: Theme.textSecondary
                                            opacity: 0.88
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.value.length > 0 ? modelData.value : "-"
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignLeft
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
