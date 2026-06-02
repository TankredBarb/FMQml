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
    property string attributesText: ""
    property string content: ""
    property int lineCount: 0
    property bool textTruncated: false
    property bool fullTextAvailable: false
    property bool textChunked: false
    property int textChunkIndex: 0
    property int textChunkCount: 0
    property bool loading: false
    property var extraProperties: []
    property string audioTitle: ""
    property string audioArtist: ""
    property string audioAlbum: ""
    property string audioYear: ""
    property string audioTrack: ""
    property string audioGenre: ""
    property string audioComment: ""
    property string audioDuration: ""
    property string audioBitrate: ""
    property string audioSampleRate: ""
    property string audioChannels: ""
    property string mediaSourceUrl: ""
    property bool hasPdfSupport: false
    property bool hasMultimediaSupport: false
    property bool playbackControlsActive: true
    property bool useNativeIcons: true
    property int imageWidth: 0
    property int imageHeight: 0
    property string imageFormatText: ""
    property string imageColorDepthText: ""
    property string imageAlphaChannelText: ""
    property string imageDpiText: ""
    property string imageColorSpaceText: ""
    property string imagePixelFormatText: ""
    property bool imageMetadataHidden: false
    property bool detailsPanelRaised: false
    property int bookPageIndex: 0
    property int bookPageCount: 0
    property string bookCoverSource: ""
    property string bookTitle: ""
    property string bookAuthor: ""
    property int sourceSizeWidth: mode === "quicklook" ? 2048 : 512
    property int sourceSizeHeight: mode === "quicklook" ? 2048 : 512
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true

    readonly property bool compactLayout: width < 620 || mode === "pane"
    readonly property bool archiveInnerPath: path.indexOf("archive://") === 0
    readonly property bool archiveLimitedType: archiveInnerPath && type === "info" && !directory
    readonly property bool folderType: type === "info" && directory
    readonly property bool mediaType: ["image", "video", "svg", "pdf", "font"].includes(type)
    readonly property bool iconType: type === "info" && !archiveLimitedType && !folderType
    readonly property int previewHeight: type === "text" ? 220 : (compactLayout ? 224 : 0)

    signal loadFullTextRequested()
    signal previousTextChunkRequested()
    signal nextTextChunkRequested()
    signal hideImageMetadataRequested()
    signal showImageMetadataRequested()
    signal detailsPanelPlacementToggleRequested()
    signal bookPageRequested(int pageIndex)
    signal bookReaderSizeChanged(int pixelSize)

    clip: true

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function displayPath(path) {
        const value = safeText(path)
        if (value.length === 0 || value.indexOf("archive://") === 0 || value.indexOf("devices://") === 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function displayLocation() {
        const value = root.absolutePath.length > 0 ? root.absolutePath : root.path
        if (value.length === 0 || value === "devices://" || value === "selection://") {
            return ""
        }
        return displayPath(value)
    }

    function fileName() {
        if (root.name.length > 0) {
            return root.name
        }
        if (root.path === "selection://") {
            return "Multiple selection"
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
        if (root.path === "selection://" || root.mimeName === "selection") {
            return "Multiple Selection"
        }
        if (root.directory) return "Folder"
        if (root.type === "archive") return "Archive File"
        if (root.type === "executable") return "Application"
        if (root.type === "shortcut") return "Shortcut"
        if (root.type === "audio") return "Audio File"
        if (root.type === "video") return "Video File"
        if (root.type === "book") return "Book"
        if (root.type === "image") return "Image File"
        if (root.type === "pdf") return "PDF Document"
        if (root.type === "svg") return "SVG Image"
        if (root.type === "font") return "Font File"
        if (root.type === "text") return "Text File"
        return root.mimeName.length > 0 ? root.mimeName : "File"
    }

    function codeLanguageLabel() {
        const ext = root.extension.toLowerCase()
        const labels = {
            "c": "C",
            "cc": "C++",
            "cpp": "C++",
            "cxx": "C++",
            "h": "Header",
            "hh": "C++ Header",
            "hpp": "C++ Header",
            "cs": "C#",
            "java": "Java",
            "js": "JavaScript",
            "jsx": "JavaScript",
            "ts": "TypeScript",
            "tsx": "TypeScript",
            "qml": "QML",
            "py": "Python",
            "rs": "Rust",
            "go": "Go",
            "php": "PHP",
            "rb": "Ruby",
            "swift": "Swift",
            "kt": "Kotlin",
            "kts": "Kotlin",
            "css": "CSS",
            "scss": "SCSS",
            "sass": "Sass",
            "html": "HTML",
            "htm": "HTML",
            "xml": "XML",
            "json": "JSON",
            "jsonc": "JSON",
            "yaml": "YAML",
            "yml": "YAML",
            "toml": "TOML",
            "ini": "INI",
            "cmake": "CMake",
            "md": "Markdown",
            "sh": "Shell",
            "bat": "Batch",
            "ps1": "PowerShell"
        }
        if (labels[ext]) return labels[ext]
        const lowerName = fileName().toLowerCase()
        if (lowerName === "cmakelists.txt") return "CMake"
        if (lowerName === "makefile") return "Makefile"
        if (lowerName === "dockerfile") return "Dockerfile"
        return ""
    }

    function isCodeLikeText() {
        return root.type === "text" && codeLanguageLabel().length > 0
    }

    function iconSource() {
        if (!root.useNativeIcons) {
            return fileTypeIconResolver.iconForSuffix(root.extension, root.directory)
        }
        if (root.path === "selection://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/grid.svg"
        }
        if (root.path.length > 0 && root.path !== "devices://") {
            const query = root.directory
                ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            return "image://icon/" + encodeURIComponent(root.path + query)
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
        if (root.mode === "quicklook" && ["svg", "pdf", "font"].includes(root.type)) {
            const specificProps = typeSpecificProperties()
            if (specificProps.length > 0) {
                return specificProps
            }
        }

        const props = [
            { label: "Name", value: fileName() },
            { label: "Type", value: typeLabel() }
        ]

        if (root.path.length > 0 && root.path !== "devices://" && root.path !== "selection://") {
            props.push({ label: "Location", value: displayPath(root.absolutePath.length > 0 ? root.absolutePath : root.path) })
        }

        if (root.sizeText.length > 0) {
            props.push({ label: "Size", value: root.sizeText })
        }

        if (root.modifiedText.length > 0) {
            props.push({ label: "Modified", value: root.modifiedText })
        }

        if (root.permissionsText.length > 0) {
            props.push({ label: "Access", value: root.permissionsText })
        }

        if (root.attributesText.length > 0) {
            props.push({ label: "Attributes", value: root.attributesText })
        }

        if (root.symlink) {
            props.push({ label: "Symlink", value: "Yes" })
        }

        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        if (!(root.mode === "quicklook" && root.type === "image")) {
            for (let i = 0; i < extras.length; i++) {
                const label = safeText(extras[i].label)
                if (label.length > 0 && !["Name", "Type", "Size", "Modified", "Location", "Access", "Attributes"].includes(label)) {
                    props.push(extras[i])
                }
            }
        }

        return props
    }

    function basicProperties() {
        const props = [
            { label: "Name", value: fileName() },
            { label: "Type", value: typeLabel() }
        ]

        if (root.path.length > 0 && root.path !== "devices://" && root.path !== "selection://") {
            props.push({ label: "Location", value: displayPath(root.absolutePath.length > 0 ? root.absolutePath : root.path) })
        }

        if (root.sizeText.length > 0) {
            props.push({ label: "Size", value: root.sizeText })
        }

        if (root.modifiedText.length > 0) {
            props.push({ label: "Modified", value: root.modifiedText })
        }

        if (root.permissionsText.length > 0) {
            props.push({ label: "Access", value: root.permissionsText })
        }

        let attributeText = root.attributesText
        if (root.symlink) {
            attributeText = attributeText.length > 0 ? attributeText + ", Symlink" : "Symlink"
        }
        if (attributeText.length > 0) {
            props.push({ label: "Attributes", value: attributeText })
        }

        return props
    }

    function typeSpecificProperties() {
        const props = []
        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        const seen = {}
        function push(label, value) {
            const text = safeText(value)
            if (label.length > 0 && text.length > 0 && !seen[label]) {
                props.push({ label: label, value: text })
                seen[label] = true
            }
        }

        if (root.type === "image") {
            push("Format", root.imageFormatText.length > 0 ? root.imageFormatText : extraValue("Format"))
            push("Dimensions", root.imageWidth > 0 && root.imageHeight > 0 ? root.imageWidth + " x " + root.imageHeight : extraValue("Dimensions"))
            push("Megapixels", extraValue("Megapixels"))
            push("Color Depth", root.imageColorDepthText.length > 0 ? root.imageColorDepthText : extraValue("Color Depth"))
            push("Alpha Channel", root.imageAlphaChannelText.length > 0 ? root.imageAlphaChannelText : extraValue("Alpha Channel"))
            push("Frames", extraValue("Frames"))
            for (let i = 0; i < extras.length; i++) {
                push(safeText(extras[i].label), extras[i].value)
            }
            return props
        }

        const allowed = {
            "svg": ["Format", "Dimensions", "viewBox", "Lines"],
            "pdf": ["Format", "PDF Version", "Pages", "Title", "Author", "Subject", "Keywords", "Creator", "Producer", "Creation Date", "Modification Date"],
            "font": ["Family", "Style", "Weight", "Units per Em", "Ascent", "Descent"]
        }
        const order = allowed[root.type] || []
        for (let i = 0; i < order.length; i++) {
            const label = order[i]
            for (let j = 0; j < extras.length; j++) {
                if (safeText(extras[j].label) === label) {
                    push(label, extras[j].value)
                    break
                }
            }
        }
        return props
    }

    Component {
        id: imagePreviewComponent

        ImagePreview {
            anchors.fill: parent
            sourcePath: root.path
            extension: root.extension
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            imageWidth: root.imageWidth
            imageHeight: root.imageHeight
            imageFormatText: root.imageFormatText
            imageColorDepthText: root.imageColorDepthText
            imageAlphaChannelText: root.imageAlphaChannelText
            imageDpiText: root.imageDpiText
            imageColorSpaceText: root.imageColorSpaceText
            imagePixelFormatText: root.imagePixelFormatText
            extraProperties: root.extraProperties
            compactMeta: root.mode === "pane"
            metadataHidden: root.imageMetadataHidden
            fillMode: Image.PreserveAspectFit
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
            onHideMetadataRequested: root.hideImageMetadataRequested()
            onShowMetadataRequested: root.showImageMetadataRequested()
        }
    }

    Component {
        id: zoomableImagePreviewComponent

        ZoomableImagePreview {
            anchors.fill: parent
            sourcePath: root.path
            extension: root.extension
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            imageWidth: root.imageWidth
            imageHeight: root.imageHeight
            imageFormatText: root.imageFormatText
            imageColorDepthText: root.imageColorDepthText
            imageAlphaChannelText: root.imageAlphaChannelText
            imageDpiText: root.imageDpiText
            imageColorSpaceText: root.imageColorSpaceText
            imagePixelFormatText: root.imagePixelFormatText
            extraProperties: root.extraProperties
            compactMeta: root.mode === "pane"
            metadataHidden: root.imageMetadataHidden
            fillMode: Image.PreserveAspectFit
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
            controlsVisible: root.mode === "quicklook"
            onHideMetadataRequested: root.hideImageMetadataRequested()
            onShowMetadataRequested: root.showImageMetadataRequested()
        }
    }

    Component {
        id: audioPreviewComponent

        AudioPreview {
            anchors.fill: parent
            path: root.path
            name: root.fileName()
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            mimeName: root.mimeName
            extension: root.extension
            audioTitle: root.audioTitle
            audioArtist: root.audioArtist
            audioAlbum: root.audioAlbum
            audioYear: root.audioYear
            audioTrack: root.audioTrack
            audioGenre: root.audioGenre
            audioComment: root.audioComment
            audioDuration: root.audioDuration.length > 0 ? root.audioDuration : root.extraValue("Duration")
            audioBitrate: root.audioBitrate.length > 0 ? root.audioBitrate : root.extraValue("Bitrate")
            audioSampleRate: root.audioSampleRate.length > 0 ? root.audioSampleRate : root.extraValue("Sample Rate")
            audioChannels: root.audioChannels.length > 0 ? root.audioChannels : root.extraValue("Channels")
            mediaSourceUrl: root.mediaSourceUrl
            multimediaControlsAvailable: root.hasMultimediaSupport
            playbackControlsActive: root.playbackControlsActive
            compact: root.compactLayout
            showDetails: root.mode === "quicklook"
        }
    }

    Component {
        id: archivePreviewComponent

        ArchivePreview {
            anchors.fill: parent
            path: root.path
            name: root.fileName()
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            mimeName: root.mimeName
            extension: root.extension
            extraProperties: root.extraProperties
            compact: root.compactLayout
            showDetails: root.mode === "quicklook"
            useNativeIcons: root.useNativeIcons
            useHighQualitySystemIcons: root.useHighQualitySystemIcons
        }
    }

    Component {
        id: bookPreviewComponent

        BookPreview {
            anchors.fill: parent
            name: root.fileName()
            content: root.content
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            extension: root.extension
            extraProperties: root.extraProperties
            coverSource: root.bookCoverSource
            bookTitle: root.bookTitle
            bookAuthor: root.bookAuthor
            pageIndex: root.bookPageIndex
            pageCount: root.bookPageCount
            compact: root.compactLayout
            showDetails: root.mode === "quicklook"
            loading: root.loading
            onPageRequested: (pageIndex) => root.bookPageRequested(pageIndex)
            onReaderSizeChanged: (pixelSize) => root.bookReaderSizeChanged(pixelSize)
        }
    }

    Component {
        id: executablePreviewComponent

        ExecutablePreview {
            anchors.fill: parent
            type: root.type
            path: root.path
            name: root.fileName()
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            mimeName: root.mimeName
            extension: root.extension
            extraProperties: root.extraProperties
            compact: root.compactLayout
            showDetails: root.mode === "quicklook"
            useNativeIcons: root.useNativeIcons
            useHighQualitySystemIcons: root.useHighQualitySystemIcons
        }
    }

    Component {
        id: fontPreviewComponent

        FontPreview {
            anchors.fill: parent
            path: root.path
            name: root.fileName()
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            extension: root.extension
            extraProperties: root.extraProperties
            compact: root.compactLayout
            showDetails: root.mode === "quicklook"
        }
    }

    Component {
        id: archiveLimitedComponent

        ArchiveLimitedPreview {
            anchors.fill: parent
            iconSource: root.iconSource()
            compact: root.compactLayout
        }
    }

    Component {
        id: folderPreviewComponent

        FolderPreview {
            anchors.fill: parent
            iconSource: root.iconSource()
            title: root.fileName()
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            locationText: root.displayLocation()
            compact: root.compactLayout
        }
    }

    Component {
        id: unsupportedPreviewComponent

        UnsupportedPreview {
            anchors.fill: parent
            iconSource: root.iconSource()
            title: root.fileName()
            typeText: root.typeLabel()
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            locationText: root.displayLocation()
            extension: root.extension
            compact: root.compactLayout
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
                    allowLoadFullText: root.mode === "quicklook"
                    textTruncated: root.textTruncated
                    fullTextAvailable: root.fullTextAvailable
                    textChunked: root.textChunked
                    textChunkIndex: root.textChunkIndex
                    textChunkCount: root.textChunkCount
                    previewKey: root.path
                    wrapText: root.mode === "pane"
                    defaultWrapText: root.mode === "pane"
                    codeMode: root.isCodeLikeText()
                    languageLabel: root.codeLanguageLabel()
                    showLineNumbers: true
                    lineHeightFollowsContent: root.mode === "quicklook"
                    fixedLineHeight: 18
                    fontPixelSize: root.mode === "pane" ? 10 : 13
                    defaultFontPixelSize: root.mode === "pane" ? 10 : 13
                    lineNumberWidth: root.mode === "quicklook" ? 45 : 48
                    textPadding: root.mode === "quicklook" ? 24 : 18
                    maximumLineNumbers: root.mode === "pane" ? 100 : 2000
                    loadingTitle: "Loading preview..."
                    loadingSubtitle: "Large files are loaded asynchronously."
                    onLoadFullTextRequested: root.loadFullTextRequested()
                    onPreviousTextChunkRequested: root.previousTextChunkRequested()
                    onNextTextChunkRequested: root.nextTextChunkRequested()
                }

                Loader {
                    anchors.fill: parent
                    visible: root.archiveLimitedType
                    active: root.archiveLimitedType
                    sourceComponent: archiveLimitedComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: root.folderType
                    active: root.folderType
                    sourceComponent: folderPreviewComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "image"
                    active: root.type === "image"
                    sourceComponent: zoomableImagePreviewComponent
                }

                MediaPreview {
                    anchors.fill: parent
                    visible: ["video", "svg", "pdf"].includes(root.type)
                    sourcePath: root.path
                    name: root.fileName()
                    sizeText: root.sizeText
                    modifiedText: root.modifiedText
                    mimeName: root.mimeName
                    extension: root.extension
                    type: root.type
                    hasPdfSupport: root.hasPdfSupport
                    sourceSizeWidth: root.sourceSizeWidth
                    sourceSizeHeight: root.sourceSizeHeight
                    imageWidth: root.imageWidth
                    imageHeight: root.imageHeight
                    extraProperties: root.extraProperties
                    interactiveImage: root.mode === "quicklook"
                    compactControls: root.mode === "pane"
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "audio"
                    active: root.type === "audio"
                    sourceComponent: audioPreviewComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "archive"
                    active: root.type === "archive"
                    sourceComponent: archivePreviewComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "book"
                    active: root.type === "book"
                    sourceComponent: bookPreviewComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: ["executable", "shortcut"].includes(root.type)
                    active: ["executable", "shortcut"].includes(root.type)
                    sourceComponent: executablePreviewComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: root.type === "font"
                    active: root.type === "font"
                    sourceComponent: fontPreviewComponent
                }

                Loader {
                    anchors.fill: parent
                    visible: root.iconType
                    active: root.iconType
                    sourceComponent: unsupportedPreviewComponent
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
            visible: root.mode === "pane"
            active: root.mode === "pane"
            sourceComponent: previewCardComponent
        }

        PreviewFactsPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.mode === "pane"
            verticalPlacement: root.detailsPanelRaised ? "top" : "bottom"
            placementToggleVisible: true
            title: "Details"
            properties: root.detailProperties()
            onPlacementToggleRequested: root.detailsPanelPlacementToggleRequested()
        }

        PreviewFactsPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            visible: root.mode !== "pane"
            title: "Details"
            properties: root.basicProperties()
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.mode === "quicklook"
            active: root.mode === "quicklook"
            sourceComponent: previewCardComponent
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

        PreviewFactsPanel {
            Layout.preferredWidth: 320
            Layout.minimumWidth: 300
            Layout.maximumWidth: 360
            Layout.preferredHeight: implicitHeight
            Layout.alignment: Qt.AlignTop
            title: "Details"
            properties: root.basicProperties()
        }
    }
}
