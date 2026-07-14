import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"
import "preview"

Popup {
    id: root

    property string previewPath: ""
    property bool restorePreviewOnClose: false
    property string restorePreviewPath: ""
    property var restorePreviewSelection: []
    property bool imageMetadataHidden: false
    property bool playbackControlsReady: false
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true
    readonly property string displayPath: root.previewPath.length > 0 ? root.previewPath : quickLookController.path

    function updateImageMetadataDemand() {
        if (typeof quickLookController === "undefined" || !quickLookController || !quickLookController.setImageMetadataRequested) return
        quickLookController.setImageMetadataRequested("quicklook", root.opened && !root.imageMetadataHidden)
    }

    function ensureBookContent() {
        if (!root.opened || typeof quickLookController === "undefined" || !quickLookController) {
            return
        }
        if (quickLookController.type === "book") {
            quickLookController.loadBookContent()
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

    function displayTitle() {
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
        if (root.displayPath.length === 0) {
            return "Preview"
        }
        if (root.displayPath === "devices://") {
            return "Devices and Drives"
        }
        if (root.displayPath === "favorites://") {
            return "Favorites"
        }
        if (root.displayPath === "gdrive://") {
            return "Google Drive"
        }
        if (root.displayPath === "selection://") {
            return "Multiple selection"
        }

        const parts = root.displayPath.split(/[/\\]/)
        const tail = parts.length > 0 ? parts[parts.length - 1] : root.displayPath
        return tail.length > 0 ? tail : root.displayPath
    }

    function displayIconSource() {
        if (root.displayPath.length === 0) {
            return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
        }
        return quickLookController.presentationIconSourceForPath(
            root.displayPath, quickLookController.directory, quickLookController.extension,
            quickLookController.mimeName, root.useNativeIcons, root.useHighQualitySystemIcons)
    }

    function displayFallbackIconSource() {
        if (root.displayPath.length === 0) return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
        return quickLookController.presentationIconSourceForPath(
            root.displayPath, quickLookController.directory, quickLookController.extension,
            quickLookController.mimeName, false, root.useHighQualitySystemIcons)
    }

    function displaySubtitle() {
        if (quickLookController.mimeName === "drive") {
            return quickLookController.extension.length > 0 ? quickLookController.extension.toUpperCase() : "Drive Preview"
        }
        if (root.displayPath === "gdrive://" && quickLookController.sizeText.length > 0) {
            return quickLookController.sizeText
        }
        if (root.displayPath === "selection://") {
            return "Multiple Selection"
        }
        if (quickLookController.type.length === 0) {
            return "Preview"
        }
        return quickLookController.type.toUpperCase() + " Preview"
    }

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.84, 960)
    height: Math.min(parent.height * 0.84, 720)
    
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

    background: DialogShell {}

    contentItem: ColumnLayout {
        spacing: 0
        focus: true

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => {
            if (event.accepted) {
                return
            }
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.close()
                event.accepted = true
            }
        }

        PreviewHeader {
            Layout.fillWidth: true
            iconSource: root.displayIconSource()
            fallbackIconSource: root.displayFallbackIconSource()
            title: root.displayTitle()
            subtitle: root.displaySubtitle()
            closeIconSource: "qrc:/qt/qml/FM/qml/assets/toolbar-next/eye-off.svg"
            closeIconTint: Theme.textSecondary
            closeIconTintHover: Theme.textPrimary
            onCloseRequested: root.close()
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.panelBorder
            opacity: themeController.isDark ? 0.34 : 0.26
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            PreviewRenderer {
                anchors.fill: parent
                mode: "quicklook"
                path: root.displayPath
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
                textTruncated: quickLookController.textTruncated
                fullTextAvailable: quickLookController.fullTextAvailable
                textChunked: quickLookController.textChunked
                textChunkIndex: quickLookController.textChunkIndex
                textChunkCount: quickLookController.textChunkCount
                loading: quickLookController.loading
                extraProperties: quickLookController.extraProperties
                audioTitle: quickLookController.audioTitle
                audioArtist: quickLookController.audioArtist
                audioAlbum: quickLookController.audioAlbum
                audioYear: quickLookController.audioYear
                audioTrack: quickLookController.audioTrack
                audioGenre: quickLookController.audioGenre
                audioComment: quickLookController.audioComment
                audioDuration: quickLookController.audioDuration
                audioBitrate: quickLookController.audioBitrate
                audioSampleRate: quickLookController.audioSampleRate
                audioChannels: quickLookController.audioChannels
                audioCoverSource: quickLookController.audioCoverSource
                mediaSourceUrl: quickLookController.mediaSourceUrl
                hasPdfSupport: quickLookController.hasPdfSupport
                hasMultimediaSupport: quickLookController.hasMultimediaSupport
                playbackControlsActive: root.playbackControlsReady
                imageWidth: quickLookController.imageWidth
                imageHeight: quickLookController.imageHeight
                imageFormatText: quickLookController.imageFormatText
                imageColorDepthText: quickLookController.imageColorDepthText
                imageAlphaChannelText: quickLookController.imageAlphaChannelText
                imageDpiText: quickLookController.imageDpiText
                imageColorSpaceText: quickLookController.imageColorSpaceText
                imagePixelFormatText: quickLookController.imagePixelFormatText
                bookPageIndex: quickLookController.bookPageIndex
                bookPageCount: quickLookController.bookPageCount
                bookCoverSource: quickLookController.bookCoverSource
                bookTitle: quickLookController.bookTitle
                bookAuthor: quickLookController.bookAuthor
                imageMetadataHidden: root.imageMetadataHidden
                sourceSizeWidth: 2048
                sourceSizeHeight: 2048
                useNativeIcons: root.useNativeIcons
                onHideImageMetadataRequested: root.imageMetadataHidden = true
                onShowImageMetadataRequested: root.imageMetadataHidden = false
                onLoadFullTextRequested: quickLookController.loadFullText()
                onPreviousTextChunkRequested: quickLookController.loadTextChunk(quickLookController.textChunkIndex - 1)
                onNextTextChunkRequested: quickLookController.loadTextChunk(quickLookController.textChunkIndex + 1)
                onBookPageRequested: (pageIndex) => quickLookController.loadBookPage(pageIndex)
                onBookReaderSizeChanged: (pixelSize) => quickLookController.setBookReaderPixelSize(pixelSize)
            }
        }
    }

    onImageMetadataHiddenChanged: root.updateImageMetadataDemand()
    onAboutToShow: root.playbackControlsReady = true
    onAboutToHide: {
        root.playbackControlsReady = false
        if (typeof quickLookController !== "undefined" && quickLookController) {
            quickLookController.unloadBookContent()
        }
    }
    onOpened: {
        root.updateImageMetadataDemand()
        root.ensureBookContent()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }
    onClosed: {
        root.updateImageMetadataDemand()
        if (root.restorePreviewOnClose && typeof quickLookController !== "undefined" && quickLookController) {
            if (root.restorePreviewPath === "selection://" && root.restorePreviewSelection.length > 1)
                quickLookController.previewSelection(root.restorePreviewSelection)
            else
                quickLookController.preview(root.restorePreviewPath)
        }
        root.restorePreviewOnClose = false
        root.restorePreviewPath = ""
        root.restorePreviewSelection = []
    }

    Connections {
        target: typeof quickLookController !== "undefined" ? quickLookController : null
        function onPathChanged() { root.ensureBookContent() }
        function onTypeChanged() { root.ensureBookContent() }
        function onLoadingChanged() { root.ensureBookContent() }
    }
}
