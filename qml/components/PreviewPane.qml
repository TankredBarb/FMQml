import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "preview"

Pane {
    id: root

    property bool liveResizeActive: false
    readonly property bool simplifyVisualsForPerformance: typeof appSettings !== "undefined" && appSettings
                                                          ? appSettings.simplifyVisualsForPerformance
                                                          : true
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true
    readonly property bool simplifiedForResize: root.liveResizeActive && root.simplifyVisualsForPerformance

    readonly property bool hasPreviewContent: quickLookController.path.length > 0
                                              || quickLookController.path === "devices://"
                                              || quickLookController.type === "info"

    function displayTitle() {
        if (quickLookController.name.length > 0) {
            return quickLookController.name
        }
        if (quickLookController.path.length === 0) {
            return "Preview"
        }
        if (quickLookController.path === "devices://") {
            return "Devices and Drives"
        }

        const parts = quickLookController.path.split(/[/\\]/)
        const tail = parts.length > 0 ? parts[parts.length - 1] : quickLookController.path
        return tail.length > 0 ? tail : quickLookController.path
    }

    function displayIconSource() {
        if (quickLookController.path.length === 0) {
            return quickLookController.type === "info"
                   ? "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
                   : "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/panel-right.svg"
        }
        if (quickLookController.path === "devices://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
        }
        const query = quickLookController.directory
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        return "image://icon/" + encodeURIComponent(quickLookController.path + query)
    }

    function displaySubtitle() {
        if (!root.hasPreviewContent) {
            return "Select a file or folder to inspect it here"
        }
        if (quickLookController.mimeName === "drive") {
            return quickLookController.extension.length > 0 ? quickLookController.extension.toUpperCase() : "Drive Preview"
        }
        if (quickLookController.type === "info") {
            return "System Overview"
        }
        return quickLookController.type.length > 0 ? quickLookController.type.toUpperCase() + " Preview" : "Preview"
    }

    padding: 0
    clip: true

    implicitWidth: 320
    implicitHeight: 480

    background: SurfaceCard {
        surfaceColor: themeController.isDark ? Theme.surface : Theme.bg
        strokeColor: Theme.border
        cornerRadius: 0

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          themeController.isDark ? 0.045 : 0.065)
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        PreviewHeader {
            Layout.fillWidth: true
            liveResizeActive: root.simplifiedForResize
            iconSource: root.displayIconSource()
            title: root.displayTitle()
            subtitle: root.displaySubtitle()
            closeIconSource: "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/eye-off.svg"
            onCloseRequested: quickLookController.visible = false
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
                visible: !root.hasPreviewContent
                z: 1

                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 32, 260)
                    iconSource: "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/panel-right.svg"
                    title: "No file selected"
                    subtitle: "Select a file or folder in the active panel to see preview and metadata here."
                    hint: "Preview follows the active panel"
                }
            }

            Item {
                anchors.fill: parent
                visible: root.hasPreviewContent && root.simplifiedForResize
                z: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: 10
                        color: themeController.isDark
                               ? Qt.rgba(1, 1, 1, 0.045)
                               : Qt.rgba(0, 0, 0, 0.03)
                        border.color: Theme.border
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: Theme.accent
                                opacity: 0.9
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Preview paused while resizing"
                                font.pixelSize: 11
                                font.bold: true
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 112
                        radius: 14
                        color: themeController.isDark
                               ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                               : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                        border.color: Theme.border
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Image {
                                source: root.displayIconSource()
                                sourceSize: Qt.size(40, 40)
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                smooth: true
                                mipmap: false
                                asynchronous: true
                                opacity: 0.92
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: root.displayTitle()
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: Theme.textPrimary
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }

                                Label {
                                    text: root.displaySubtitle()
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }

                                Label {
                                    text: ((quickLookController.sizeText.length > 0 ? quickLookController.sizeText : "")
                                           + (quickLookController.sizeText.length > 0 && quickLookController.modifiedText.length > 0 ? "  |  " : "")
                                           + (quickLookController.modifiedText.length > 0 ? quickLookController.modifiedText : ""))
                                    visible: text.length > 0
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: "Heavy preview content will resume after drag"
                                    font.pixelSize: 10
                                    color: Theme.textSecondary
                                    opacity: 0.82
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ColumnLayout {
                            width: parent.width
                            spacing: 10

                            function addRow(label, value) {
                                if (!value || String(value).length === 0) {
                                    return ""
                                }
                                return label + "\n" + String(value)
                            }

                            Repeater {
                                model: [
                                    { label: "Location", value: quickLookController.absolutePath.length > 0 ? quickLookController.absolutePath : quickLookController.path },
                                    { label: "Size", value: quickLookController.sizeText },
                                    { label: "Modified", value: quickLookController.modifiedText },
                                    { label: "Access", value: quickLookController.permissionsText },
                                    { label: "Attributes", value: quickLookController.attributesText },
                                    { label: "Symlink", value: quickLookController.symlink ? "Yes" : "" }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    visible: modelData.value && String(modelData.value).length > 0
                                    Layout.fillWidth: true
                                    implicitHeight: visible ? 54 : 0
                                    radius: 10
                                    color: Theme.panelSurfaceSoft
                                    border.color: Theme.border
                                    border.width: 1

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 4

                                        Label {
                                            text: parent.parent.modelData.label
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: Theme.textSecondary
                                        }

                                        Label {
                                            text: String(parent.parent.modelData.value)
                                            font.pixelSize: 12
                                            color: Theme.textPrimary
                                            width: parent.width
                                            wrapMode: Text.Wrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            PreviewRenderer {
                anchors.fill: parent
                visible: root.hasPreviewContent && !root.simplifiedForResize
                mode: "pane"
                path: quickLookController.path
                type: quickLookController.type
                name: quickLookController.name
                mimeName: quickLookController.mimeName
                extension: quickLookController.extension
                directory: quickLookController.directory
                sizeText: quickLookController.sizeText
                modifiedText: quickLookController.modifiedText
                absolutePath: quickLookController.absolutePath
                hidden: quickLookController.hidden
                symlink: quickLookController.symlink
                permissionsText: quickLookController.permissionsText
                attributesText: quickLookController.attributesText
                content: quickLookController.content
                lineCount: quickLookController.lines
                loading: quickLookController.loading
                extraProperties: quickLookController.extraProperties
                hasPdfSupport: quickLookController.hasPdfSupport
                sourceSizeWidth: 512
                sourceSizeHeight: 512
            }
        }
    }
}
