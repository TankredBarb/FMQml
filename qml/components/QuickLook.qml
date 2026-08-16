import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "dialogs"
import "framework"
import "preview"

Popup {
    id: root
    z: 100

    property string previewPath: ""
    property bool restorePreviewOnClose: false
    property string restorePreviewPath: ""
    property var restorePreviewSelection: []
    property bool imageMetadataHidden: true
    property bool playbackControlsReady: false
    property var backdropSource: null
    property var navigationController: null
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true
    readonly property bool translucentSurface: typeof appSettings !== "undefined" && appSettings
                                                ? appSettings.quickLookTransparency
                                                : false
    readonly property real transparencyStrength: typeof appSettings !== "undefined" && appSettings
                                                 ? appSettings.commandPaletteTransparencyStrength / 100.0
                                                 : 0.6
    readonly property real surfaceAlpha: themeController.isDark
                                         ? 1.0 - transparencyStrength * 0.32
                                         : 1.0 - transparencyStrength * 0.26
    readonly property bool blurSurface: root.translucentSurface
                                        && typeof appSettings !== "undefined" && appSettings
                                        && appSettings.surfaceBlur && root.backdropSource
    readonly property string displayPath: root.previewPath.length > 0 ? root.previewPath : quickLookController.path
    readonly property real navigationFooterHeight: navigationFooter.visible
                                                   ? navigationFooter.implicitHeight : 0

    function updateImageMetadataDemand() {
        if (typeof quickLookController === "undefined" || !quickLookController || !quickLookController.setImageMetadataRequested) return
        quickLookController.setImageMetadataRequested("quicklook", root.opened)
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
        if (root.displayPath === "mega:///" || root.displayPath === "mega://") {
            return "MEGA"
        }
        if (root.displayPath === "telegram://" || root.displayPath === "telegram:///") {
            return "Telegram"
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
            return "qrc:/qt/qml/FM/qml/assets/icons-classic/computer.svg"
        }
        return quickLookController.presentationIconSourceForPath(
            root.displayPath, quickLookController.directory, quickLookController.extension,
            quickLookController.mimeName, root.useNativeIcons)
    }

    function displayFallbackIconSource() {
        if (root.displayPath.length === 0) return "qrc:/qt/qml/FM/qml/assets/icons-classic/computer.svg"
        return quickLookController.presentationIconSourceForPath(
            root.displayPath, quickLookController.directory, quickLookController.extension,
            quickLookController.mimeName, false)
    }

    function displaySubtitle() {
        if (quickLookController.mimeName === "drive") {
            return quickLookController.extension.length > 0 ? quickLookController.extension.toUpperCase() : "Drive Preview"
        }
        if (quickLookController.type === "provider") {
            const account = root.extraValue("Account")
            return account.length > 0 ? account : "Provider Location"
        }
        if (root.displayPath === "selection://") {
            return "Multiple Selection"
        }
        if (quickLookController.type.length === 0) {
            return "Preview"
        }
        return quickLookController.type.toUpperCase() + " Preview"
    }

    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.round(Math.min(parent.width * 0.84, 960))
    height: Math.round(Math.min(parent.height - 24,
                                Math.min(parent.height * 0.84, 720)
                                + root.navigationFooterHeight
                                + (navigationFooter.visible ? 18 : 0)))
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: root.blurSurface ? 1.0 : 0.95; to: 1.0; duration: 250; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: root.blurSurface ? 1.0 : 0.95; duration: 150; easing.type: Easing.InCubic }
    }

    background: TranslucentSurface {
        translucent: root.translucentSurface
        active: root.visible
        backgroundBlurEnabled: root.blurSurface
        backdropSource: root.backdropSource
        backdropTransformItem: root
        cornerRadius: Theme.radiusLg
        baseColor: root.translucentSurface
                   ? Theme.withAlpha(Theme.panelSurfaceStrong, root.surfaceAlpha)
                   : Theme.panelSurface
        startColor: Theme.chromeGradientStart
        midColor: Theme.chromeGradientMid
        endColor: Theme.panelSurface
        gradientStrength: 0.5
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 20
        shadowVerticalOffset: 8

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 1
            height: 1
            radius: 0.5
            color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.42 : 0.30)
        }
    }

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
                textHasPreviousPage: quickLookController.textHasPreviousPage
                textHasNextPage: quickLookController.textHasNextPage
                textFirstLine: quickLookController.textFirstLine
                textLanguageLabel: quickLookController.textLanguageLabel
                textDefaultWrap: quickLookController.textDefaultWrap
                textDefaultLineNumbers: quickLookController.textDefaultLineNumbers
                textFontFamily: quickLookController.textFontFamily
                textStyleRanges: quickLookController.textStyleRanges
                textTokenColor1: quickLookController.textTokenColor1
                textTokenColor2: quickLookController.textTokenColor2
                textTokenColor3: quickLookController.textTokenColor3
                textTokenColor4: quickLookController.textTokenColor4
                loading: quickLookController.loading
                previewTransferActive: quickLookController.previewTransferActive
                previewTransferBytes: quickLookController.previewTransferBytes
                previewTransferTotal: quickLookController.previewTransferTotal
                previewTransferPreparing: quickLookController.previewTransferPreparing
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

        ColumnLayout {
            id: navigationFooter
            Layout.fillWidth: true
            spacing: 0
            visible: root.navigationController && root.navigationController.navigationActive

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.panelBorder
                opacity: themeController.isDark ? 0.34 : 0.26
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6
                Layout.bottomMargin: 6
                implicitWidth: 84
                implicitHeight: 38
                radius: 12
                color: Theme.mixColors(
                           Theme.withAlpha(Theme.controlSurface,
                                           themeController.isDark ? 0.54 : 0.42),
                           Theme.withAlpha(Theme.categoryNavigation, 1.0),
                           themeController.isDark ? 0.12 : 0.08)
                border.width: 1
                border.color: Theme.withAlpha(Theme.categoryNavigation,
                                              themeController.isDark ? 0.30 : 0.24)

                Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: 18
                    color: Theme.withAlpha(Theme.panelBorder,
                                           themeController.isDark ? 0.28 : 0.22)
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    FmIconButton {
                        enabled: root.navigationController && root.navigationController.canGoPrevious
                        implicitWidth: 34
                        implicitHeight: 32
                        iconSize: 18
                        iconTone: "navigation"
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-left.svg"
                        Accessible.name: "Previous item"
                        ToolTip.visible: hovered
                        ToolTip.text: "Previous item"
                        ToolTip.delay: 350
                        onClicked: root.navigationController.navigate(-1)
                    }

                    FmIconButton {
                        enabled: root.navigationController && root.navigationController.canGoNext
                        implicitWidth: 34
                        implicitHeight: 32
                        iconSize: 18
                        iconTone: "navigation"
                        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-right.svg"
                        Accessible.name: "Next item"
                        ToolTip.visible: hovered
                        ToolTip.text: "Next item"
                        ToolTip.delay: 350
                        onClicked: root.navigationController.navigate(1)
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Left"
        enabled: root.opened && root.navigationController
                 && root.navigationController.navigationActive
                 && root.navigationController.canGoPrevious
        autoRepeat: true
        onActivated: root.navigationController.navigate(-1)
    }

    Shortcut {
        sequence: "Right"
        enabled: root.opened && root.navigationController
                 && root.navigationController.navigationActive
                 && root.navigationController.canGoNext
        autoRepeat: true
        onActivated: root.navigationController.navigate(1)
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
        if (root.navigationController) root.navigationController.endNavigation()
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
