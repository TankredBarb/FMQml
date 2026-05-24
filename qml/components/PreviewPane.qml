import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "preview"

Pane {
    id: root

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
        return "image://icon/" + encodeURIComponent(quickLookController.path + (quickLookController.directory ? "?directory=true" : ""))
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

            PreviewRenderer {
                anchors.fill: parent
                visible: root.hasPreviewContent
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
