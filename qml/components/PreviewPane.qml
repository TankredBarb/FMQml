import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "preview"

Pane {
    id: root

    property bool liveResizeActive: false
    property bool scrollPauseActive: false
    property bool imageMetadataHidden: false
    property bool previewPending: false
    property string pendingPreviewPath: ""
    readonly property bool detailsPanelRaised: typeof appSettings !== "undefined" && appSettings
                                                ? appSettings.previewDetailsRaised
                                                : false
    readonly property bool ultraLightMode: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.ultraLightMode
                                           : false
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true
    readonly property bool resizeOptimized: root.liveResizeActive
    readonly property bool lightweightPreviewActive: root.resizeOptimized || root.ultraLightMode

    readonly property string effectivePreviewPath: root.previewPending && root.pendingPreviewPath.length > 0
                                                   ? root.pendingPreviewPath
                                                   : quickLookController.path
    readonly property bool effectiveLoading: root.previewPending || quickLookController.loading
    readonly property bool hasPreviewContent: root.effectivePreviewPath.length > 0
                                              || root.effectivePreviewPath === "devices://"
                                              || root.effectivePreviewPath === "favorites://"
                                              || quickLookController.type === "info"

    function updateImageMetadataDemand() {
        if (typeof quickLookController === "undefined" || !quickLookController || !quickLookController.setImageMetadataRequested) return
        quickLookController.setImageMetadataRequested("pane", root.visible && !root.imageMetadataHidden && !root.lightweightPreviewActive)
    }

    function toggleDetailsPanelPlacement() {
        if (typeof appSettings !== "undefined" && appSettings) {
            appSettings.previewDetailsRaised = !appSettings.previewDetailsRaised
        }
    }

    function extraValue(label) {
        const extras = Array.isArray(quickLookController.extraProperties) ? quickLookController.extraProperties : []
        for (let i = 0; i < extras.length; ++i) {
            if (String(extras[i].label || "") === label) {
                return String(extras[i].value || "")
            }
        }
        return ""
    }

    function titleForPath(path) {
        if (path.length === 0) {
            return "Preview"
        }
        if (path === "devices://") {
            return "Devices and Drives"
        }
        if (path === "favorites://") {
            return "Favorites"
        }
        if (path === "selection://") {
            return "Multiple selection"
        }

        const parts = path.split(/[/\\]/)
        const tail = parts.length > 0 ? parts[parts.length - 1] : path
        return tail.length > 0 ? tail : path
    }

    function extensionForPath(path) {
        const name = root.titleForPath(path)
        const dot = name.lastIndexOf(".")
        return dot > 0 && dot < name.length - 1 ? name.slice(dot + 1).toLowerCase() : ""
    }

    function displayTitle() {
        if (root.previewPending) {
            return root.titleForPath(root.effectivePreviewPath)
        }
        if (quickLookController.type === "book") {
            const bookTitle = quickLookController.bookTitle.length > 0
                            ? quickLookController.bookTitle
                            : root.extraValue("Title")
            if (bookTitle.length > 0) {
                return bookTitle
            }
            const author = quickLookController.bookAuthor.length > 0
                         ? quickLookController.bookAuthor
                         : root.extraValue("Author")
            if (author.length > 0) {
                return author
            }
        }
        if (quickLookController.name.length > 0) {
            return quickLookController.name
        }
        return root.titleForPath(root.effectivePreviewPath)
    }

    function displayIconSource() {
        const path = root.effectivePreviewPath
        if (path === "selection://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/grid.svg"
        }
        if (path.length === 0) {
            return quickLookController.type === "info"
                   ? "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
                   : "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/panel-right.svg"
        }
        if (path === "devices://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
        }
        if (path === "favorites://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
        }
        if (!root.useNativeIcons) {
            return fileTypeIconResolver.iconForSuffix(root.previewPending ? root.extensionForPath(path) : quickLookController.extension,
                                                      root.previewPending ? false : quickLookController.directory)
        }
        const query = (!root.previewPending && quickLookController.directory)
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        return "image://icon/" + encodeURIComponent(path + query)
    }

    function displaySubtitle() {
        if (!root.hasPreviewContent) {
            return "Select a file or folder to inspect it here"
        }
        if (root.previewPending) {
            return "Loading Preview"
        }
        if (quickLookController.mimeName === "drive") {
            return quickLookController.extension.length > 0 ? quickLookController.extension.toUpperCase() : "Drive Preview"
        }
        if (quickLookController.directory) {
            return "Folder Preview"
        }
        if (quickLookController.type === "info") {
            if (quickLookController.path === "selection://") {
                return "Multiple Selection"
            }
            return quickLookController.path === "favorites://" ? "Virtual Location" : "System Overview"
        }
        return quickLookController.type.length > 0 ? quickLookController.type.toUpperCase() + " Preview" : "Preview"
    }

    function lightweightProperties() {
        const deferredText = root.ultraLightMode && !root.resizeOptimized
                           ? "Available in full preview"
                           : (root.scrollPauseActive ? "Resumes after scroll" : "Resumes after drag")
        const props = [
            { label: "Name", value: root.displayTitle() },
            { label: "Type", value: root.displaySubtitle() }
        ]

        if (root.effectivePreviewPath.length > 0 && root.effectivePreviewPath !== "devices://" && root.effectivePreviewPath !== "selection://") {
            props.push({ label: "Location", value: deferredText })
        }

        if (quickLookController.sizeText.length > 0) {
            props.push({ label: "Size", value: quickLookController.sizeText })
        }

        if (quickLookController.modifiedText.length > 0) {
            props.push({ label: "Modified", value: quickLookController.modifiedText })
        }

        if (root.effectivePreviewPath.length > 0 && root.effectivePreviewPath !== "devices://" && root.effectivePreviewPath !== "selection://") {
            props.push({ label: "Access", value: deferredText })
            props.push({ label: "Attributes", value: deferredText })
        }

        return props
    }

    padding: 0
    clip: true

    onVisibleChanged: root.updateImageMetadataDemand()
    onImageMetadataHiddenChanged: root.updateImageMetadataDemand()
    onLightweightPreviewActiveChanged: root.updateImageMetadataDemand()
    Component.onCompleted: root.updateImageMetadataDemand()

    implicitWidth: 320
    implicitHeight: 480

    background: SurfaceCard {
        surfaceColor: Theme.panelSurface
        strokeColor: Theme.panelStroke
        cornerRadius: Theme.panelRadius

        Rectangle {
            anchors.fill: parent
            radius: Theme.innerRadius(parent.cornerRadius, 1)
            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.045 : 0.065)
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        PreviewHeader {
            Layout.fillWidth: true
            liveResizeActive: root.lightweightPreviewActive
            iconSource: root.displayIconSource()
            title: root.displayTitle()
            subtitle: root.displaySubtitle()
            closeIconSource: "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/eye-off.svg"
            onCloseRequested: quickLookController.visible = false
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.panelStrokeSubtle
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
                visible: root.hasPreviewContent && root.lightweightPreviewActive
                z: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 224
                        radius: Theme.panelRadius
                        color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)
                        border.color: Theme.panelStroke
                        border.width: 1
                        clip: true

                        Rectangle {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 12
                            anchors.rightMargin: 12
                            width: statusLabel.implicitWidth + 18
                            height: 26
                            radius: Theme.radiusForSide(height)
                            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.16 : 0.10)
                            border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.32 : 0.22)
                            border.width: 1

                            Label {
                                id: statusLabel
                                anchors.centerIn: parent
                                text: root.ultraLightMode && !root.resizeOptimized ? "Ultra light" : (root.scrollPauseActive ? "Scrolling" : "Resizing")
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
                                radius: Theme.radiusLg
                                color: themeController.isDark ? Theme.withAlpha(Theme.textPrimary, 0.05)
                                                              : Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: Theme.panelStroke
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
                                radius: Theme.radiusForSide(height)
                                color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
                                border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.38)
                                border.width: 1

                                Label {
                                    id: pausedLabel
                                    anchors.centerIn: parent
                                    text: root.ultraLightMode && !root.resizeOptimized ? "Full preview disabled" : (root.scrollPauseActive ? "Preview resumes after scroll" : "Preview resumes after drag")
                                    font.pixelSize: 10
                                    color: Theme.textSecondary
                                }
                            }
                        }
                    }

                    PreviewFactsPanel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        verticalPlacement: root.detailsPanelRaised ? "top" : "bottom"
                        placementToggleVisible: true
                        title: "Details"
                        properties: root.lightweightProperties()
                        onPlacementToggleRequested: root.toggleDetailsPanelPlacement()
                    }
                }
            }

            Loader {
                id: fullPreviewHost
                anchors.fill: parent
                active: root.hasPreviewContent && !root.lightweightPreviewActive
                visible: active
                asynchronous: false
                sourceComponent: fullPreviewComponent
            }

            Component {
                id: fullPreviewComponent
                PreviewRenderer {
                    anchors.fill: parent
                    mode: "pane"
                    path: root.effectivePreviewPath
                    type: root.previewPending ? "info" : quickLookController.type
                    name: root.previewPending ? root.displayTitle() : quickLookController.name
                    mimeName: root.previewPending ? "" : quickLookController.mimeName
                    extension: root.previewPending ? root.extensionForPath(root.effectivePreviewPath) : quickLookController.extension
                    directory: root.previewPending ? false : quickLookController.directory
                    sizeText: root.previewPending ? "Loading preview..." : quickLookController.sizeText
                    modifiedText: root.previewPending ? "" : quickLookController.modifiedText
                    absolutePath: root.previewPending ? root.effectivePreviewPath : quickLookController.absolutePath
                    hidden: root.previewPending ? false : quickLookController.hidden
                    symlink: root.previewPending ? false : quickLookController.symlink
                    permissionsText: root.previewPending ? "" : quickLookController.permissionsText
                    attributesText: root.previewPending ? "" : quickLookController.attributesText
                    content: root.previewPending ? "" : quickLookController.content
                    lineCount: root.previewPending ? 0 : quickLookController.lines
                    textTruncated: root.previewPending ? false : quickLookController.textTruncated
                    fullTextAvailable: root.previewPending ? false : quickLookController.fullTextAvailable
                    textChunked: root.previewPending ? false : quickLookController.textChunked
                    textChunkIndex: root.previewPending ? 0 : quickLookController.textChunkIndex
                    textChunkCount: root.previewPending ? 0 : quickLookController.textChunkCount
                    loading: root.effectiveLoading
                    extraProperties: root.previewPending ? [] : quickLookController.extraProperties
                    audioTitle: root.previewPending ? "" : quickLookController.audioTitle
                    audioArtist: root.previewPending ? "" : quickLookController.audioArtist
                    audioAlbum: root.previewPending ? "" : quickLookController.audioAlbum
                    audioYear: root.previewPending ? "" : quickLookController.audioYear
                    audioTrack: root.previewPending ? "" : quickLookController.audioTrack
                    audioGenre: root.previewPending ? "" : quickLookController.audioGenre
                    audioComment: root.previewPending ? "" : quickLookController.audioComment
                    audioDuration: root.previewPending ? "" : quickLookController.audioDuration
                    audioBitrate: root.previewPending ? "" : quickLookController.audioBitrate
                    audioSampleRate: root.previewPending ? "" : quickLookController.audioSampleRate
                    audioChannels: root.previewPending ? "" : quickLookController.audioChannels
                    mediaSourceUrl: root.previewPending ? "" : quickLookController.mediaSourceUrl
                    hasPdfSupport: quickLookController.hasPdfSupport
                    hasMultimediaSupport: quickLookController.hasMultimediaSupport
                    imageWidth: root.previewPending ? 0 : quickLookController.imageWidth
                    imageHeight: root.previewPending ? 0 : quickLookController.imageHeight
                    imageFormatText: root.previewPending ? "" : quickLookController.imageFormatText
                    imageColorDepthText: root.previewPending ? "" : quickLookController.imageColorDepthText
                    imageAlphaChannelText: root.previewPending ? "" : quickLookController.imageAlphaChannelText
                    imageDpiText: root.previewPending ? "" : quickLookController.imageDpiText
                    imageColorSpaceText: root.previewPending ? "" : quickLookController.imageColorSpaceText
                    imagePixelFormatText: root.previewPending ? "" : quickLookController.imagePixelFormatText
                    bookPageIndex: root.previewPending ? 0 : quickLookController.bookPageIndex
                    bookPageCount: root.previewPending ? 0 : quickLookController.bookPageCount
                    bookCoverSource: root.previewPending ? "" : quickLookController.bookCoverSource
                    bookTitle: root.previewPending ? "" : quickLookController.bookTitle
                    bookAuthor: root.previewPending ? "" : quickLookController.bookAuthor
                    imageMetadataHidden: root.imageMetadataHidden
                    detailsPanelRaised: root.detailsPanelRaised
                    sourceSizeWidth: 512
                    sourceSizeHeight: 512
                    useNativeIcons: root.useNativeIcons
                    onHideImageMetadataRequested: root.imageMetadataHidden = true
                    onShowImageMetadataRequested: root.imageMetadataHidden = false
                    onDetailsPanelPlacementToggleRequested: root.toggleDetailsPanelPlacement()
                    onBookPageRequested: (pageIndex) => quickLookController.loadBookPage(pageIndex)
                    onBookReaderSizeChanged: (pixelSize) => quickLookController.setBookReaderPixelSize(pixelSize)
                }
            }
        }
    }
}
