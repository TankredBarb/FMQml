import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string mode: "pane"
    property string path: ""
    property string type: ""
    property string name: ""
    property string mimeName: ""
    property string extension: ""
    property bool directory: false
    property string sizeText: ""
    property string modifiedText: ""
    property string absolutePath: ""
    property bool hidden: false
    property bool symlink: false
    property string permissionsText: ""
    property string content: ""
    property int lineCount: 0
    property bool loading: false
    property var extraProperties: []
    property bool hasPdfSupport: false
    property int sourceSizeWidth: mode === "quicklook" ? 2048 : 512
    property int sourceSizeHeight: mode === "quicklook" ? 2048 : 512

    readonly property bool compactLayout: width < 620 || mode === "pane"
    readonly property bool mediaType: ["image", "video", "svg", "pdf", "font"].includes(type)
    readonly property bool iconType: ["info", "archive", "executable", "shortcut"].includes(type)
    readonly property int previewHeight: type === "text" ? 220 : (compactLayout ? 224 : 0)

    clip: true

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function fileName() {
        if (root.name.length > 0) {
            return root.name
        }
        if (root.path.length === 0 || root.path === "devices://") {
            return root.type === "info" ? "Devices and Drives" : "Preview"
        }
        const parts = root.path.split(/[/\\]/)
        return parts.length > 0 ? parts[parts.length - 1] : root.path
    }

    function typeLabel() {
        if (root.mimeName === "drive") {
            return root.extension.length > 0 ? root.extension.toUpperCase() : "Local Disk"
        }
        if (root.directory) return "Folder"
        if (root.type === "archive") return "Archive File"
        if (root.type === "executable") return "Application"
        if (root.type === "shortcut") return "Shortcut"
        if (root.type === "audio") return "Audio File"
        if (root.type === "video") return "Video File"
        if (root.type === "image") return "Image File"
        if (root.type === "pdf") return "PDF Document"
        if (root.type === "svg") return "SVG Image"
        if (root.type === "font") return "Font File"
        if (root.type === "text") return "Text File"
        return root.mimeName.length > 0 ? root.mimeName : "File"
    }

    function iconSource() {
        if (root.path.length > 0 && root.path !== "devices://") {
            return "image://icon/" + encodeURIComponent(root.path + (root.directory ? "?directory=true" : ""))
        }
        return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
    }

    function extraValue(label) {
        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        for (let i = 0; i < extras.length; i++) {
            if (safeText(extras[i].label) === label) {
                return safeText(extras[i].value)
            }
        }
        return ""
    }

    function detailProperties() {
        const props = [
            { label: "Name", value: fileName() },
            { label: "Type", value: typeLabel() }
        ]

        if (root.path.length > 0 && root.path !== "devices://") {
            props.push({ label: "Location", value: root.absolutePath.length > 0 ? root.absolutePath : root.path })
        }

        if (root.sizeText.length > 0) {
            props.push({ label: "Size", value: root.sizeText })
        }

        if (root.modifiedText.length > 0) {
            props.push({ label: "Modified", value: root.modifiedText })
        }

        if (root.permissionsText.length > 0) {
            props.push({ label: "Permissions", value: root.permissionsText })
        }

        if (root.hidden) {
            props.push({ label: "Hidden", value: "Yes" })
        }

        if (root.symlink) {
            props.push({ label: "Symlink", value: "Yes" })
        }

        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        for (let i = 0; i < extras.length; i++) {
            const label = safeText(extras[i].label)
            if (label.length > 0 && !["Name", "Type", "Size", "Modified", "Location", "Permissions"].includes(label)) {
                props.push(extras[i])
            }
        }

        return props
    }

    Component {
        id: imagePreviewComponent

        ImagePreview {
            anchors.fill: parent
            sourcePath: root.path
            fillMode: Image.PreserveAspectFit
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
        }
    }

    Component {
        id: zoomableImagePreviewComponent

        ZoomableImagePreview {
            anchors.fill: parent
            sourcePath: root.path
            fillMode: Image.PreserveAspectFit
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
        }
    }

    Component {
        id: audioPreviewComponent

        AudioPreview {
            anchors.fill: parent
            path: root.path
        }
    }

    Component {
        id: previewCardComponent

        Rectangle {
            radius: 16
            color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)
            border.color: Theme.border
            border.width: 1
            clip: true

            Item {
                anchors.fill: parent
                anchors.margins: 14

                TextPreview {
                    anchors.fill: parent
                    visible: root.type === "text"
                    text: root.content
                    lineCount: root.lineCount
                    loading: root.loading
                    wrapText: root.mode === "pane"
                    showLineNumbers: true
                    lineHeightFollowsContent: root.mode === "quicklook"
                    fixedLineHeight: 18
                    lineNumberWidth: root.mode === "quicklook" ? 45 : 48
                    textPadding: root.mode === "quicklook" ? 24 : 18
                    maximumLineNumbers: root.mode === "pane" ? 100 : 0
                    loadingTitle: "Loading preview..."
                    loadingSubtitle: "Large files are loaded asynchronously."
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "image"
                    active: root.type === "image"
                    sourceComponent: root.mode === "quicklook"
                                     ? zoomableImagePreviewComponent
                                     : imagePreviewComponent
                }

                MediaPreview {
                    anchors.fill: parent
                    visible: ["video", "svg", "pdf", "font"].includes(root.type)
                    sourcePath: root.path
                    type: root.type
                    hasPdfSupport: root.hasPdfSupport
                    sourceSizeWidth: root.sourceSizeWidth
                    sourceSizeHeight: root.sourceSizeHeight
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "audio"
                    active: root.type === "audio"
                    sourceComponent: audioPreviewComponent
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    visible: root.iconType
                    spacing: 12
                    width: Math.min(parent.width - 24, 260)

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: root.compactLayout ? 84 : 112
                        height: width
                        radius: root.type === "audio" ? width / 2 : 18
                        color: themeController.isDark ? Theme.withAlpha(Theme.textPrimary, 0.05)
                                                      : Theme.withAlpha(Theme.textPrimary, 0.03)
                        border.color: Theme.border
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            source: root.iconSource()
                            sourceSize: Qt.size(root.compactLayout ? 42 : 56, root.compactLayout ? 42 : 56)
                            smooth: true
                            opacity: 0.9
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.fileName()
                        font.bold: true
                        font.pixelSize: root.compactLayout ? 14 : 18
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.typeLabel()
                        font.pixelSize: root.compactLayout ? 11 : 13
                        color: Theme.accent
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.mode === "quicklook" ? 24 : 14
        spacing: 12
        visible: root.compactLayout

        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: root.previewHeight
            sourceComponent: previewCardComponent
        }

        PreviewPropertiesList {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Details"
            properties: root.detailProperties()
            rowRadius: 10
            rowPadding: 12
            labelPixelSize: 10
            valuePixelSize: 12
            rowSpacing: 8
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24
        visible: !root.compactLayout

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 260
            sourceComponent: previewCardComponent
        }

        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.panelBorder
            opacity: 0.15
        }

        PreviewPropertiesList {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            title: "Details"
            properties: root.detailProperties()
            rowRadius: 10
            rowPadding: 12
            labelPixelSize: 10
            valuePixelSize: 12
            rowSpacing: 10
        }
    }
}
