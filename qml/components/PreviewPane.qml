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
                                              || quickLookController.path === "favorites://"
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
        if (quickLookController.path === "favorites://") {
            return "Favorites"
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
        if (quickLookController.path === "favorites://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
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
            return quickLookController.path === "favorites://" ? "Virtual Location" : "System Overview"
        }
        return quickLookController.type.length > 0 ? quickLookController.type.toUpperCase() + " Preview" : "Preview"
    }

    function lightweightProperties() {
        const props = [
            { label: "Name", value: root.displayTitle() },
            { label: "Type", value: root.displaySubtitle() }
        ]

        if (quickLookController.sizeText.length > 0) {
            props.push({ label: "Size", value: quickLookController.sizeText })
        }

        if (quickLookController.modifiedText.length > 0) {
            props.push({ label: "Modified", value: quickLookController.modifiedText })
        }

        return props
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
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 224
                        radius: 16
                        color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)
                        border.color: Theme.border
                        border.width: 1
                        clip: true

                        Rectangle {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 12
                            anchors.rightMargin: 12
                            width: statusLabel.implicitWidth + 18
                            height: 26
                            radius: 13
                            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.10)
                            border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.32 : 0.22)
                            border.width: 1

                            Label {
                                id: statusLabel
                                anchors.centerIn: parent
                                text: "Resizing"
                                font.pixelSize: 10
                                font.bold: true
                                color: Theme.accent
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 32, 260)
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 84
                                height: 84
                                radius: 18
                                color: themeController.isDark ? Theme.withAlpha(Theme.textPrimary, 0.05)
                                                              : Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: Theme.border
                                border.width: 1

                                Image {
                                    anchors.centerIn: parent
                                    source: root.displayIconSource()
                                    sourceSize: Qt.size(42, 42)
                                    smooth: true
                                    mipmap: false
                                    asynchronous: true
                                    opacity: 0.9
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayTitle()
                                font.bold: true
                                font.pixelSize: 14
                                color: Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideMiddle
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displaySubtitle()
                                font.pixelSize: 11
                                color: Theme.accent
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 2
                                width: pausedLabel.implicitWidth + 22
                                height: 28
                                radius: 14
                                color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
                                border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.55 : 0.38)
                                border.width: 1

                                Label {
                                    id: pausedLabel
                                    anchors.centerIn: parent
                                    text: "Preview resumes after drag"
                                    font.pixelSize: 10
                                    color: Theme.textSecondary
                                }
                            }
                        }
                    }

                    PreviewPropertiesList {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "Details"
                        properties: root.lightweightProperties()
                        rowRadius: 10
                        rowPadding: 12
                        labelPixelSize: 10
                        valuePixelSize: 12
                        rowSpacing: 8
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
