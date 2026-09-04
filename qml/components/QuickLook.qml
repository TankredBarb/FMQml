import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
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
    property bool fullscreenActive: false
    property bool standaloneHost: false
    property bool managesPreviewSession: true
    property var imageViewState: null
    property int windowVisibilityBeforeFullscreen: Window.Windowed
    property var hostWindow: null
    property var fullscreenHost: null
    property var backdropSource: null
    property var navigationController: null
    readonly property bool fullscreenTraceEnabled: Qt.application.arguments.includes("--quicklook-trace")
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
    readonly property bool fullscreenAvailable: ["image", "svg", "pdf", "book", "video", "text"].includes(
                                                   quickLookController.type)

    function traceFullscreen(stage) {
        if (!root.fullscreenTraceEnabled) {
            return
        }
        const window = root.hostWindow
        console.log("[QuickLookFullscreen]",
                    "t=" + Date.now(),
                    "stage=" + stage,
                    "opened=" + root.opened,
                    "visible=" + root.visible,
                    "fullscreen=" + root.fullscreenActive,
                    "popup=" + Math.round(root.x) + "," + Math.round(root.y)
                               + " " + Math.round(root.width) + "x" + Math.round(root.height),
                    "windowVisibility=" + (window ? window.visibility : "none"),
                    "window=" + (window ? Math.round(window.width) + "x" + Math.round(window.height) : "none"))
    }

    function enterFullscreen() {
        if (!root.standaloneHost && root.fullscreenHost) {
            root.fullscreenHost.enterFullscreen()
            return
        }
        const window = root.hostWindow
        if (root.fullscreenActive || !root.fullscreenAvailable || !window) {
            return
        }
        root.windowVisibilityBeforeFullscreen = window.visibility
        root.fullscreenActive = true
        root.traceFullscreen("enter-before-window-fullscreen")
        window.visibility = Window.FullScreen
        Qt.callLater(() => root.traceFullscreen("enter-next-frame"))
    }

    function leaveFullscreen() {
        if (root.standaloneHost && root.fullscreenHost) {
            root.fullscreenHost.leaveFullscreen()
            return
        }
        if (!root.fullscreenActive) {
            return
        }
        root.traceFullscreen("leave-before")
        const window = root.hostWindow
        root.fullscreenActive = false
        if (window) {
            window.visibility = root.windowVisibilityBeforeFullscreen
        }
        Qt.callLater(() => root.traceFullscreen("leave-next-frame"))
    }

    function toggleFullscreen() {
        if (root.fullscreenActive) {
            root.leaveFullscreen()
        } else {
            root.enterFullscreen()
        }
    }

    function requestClose() {
        root.traceFullscreen("request-close")
        if (root.standaloneHost && root.fullscreenHost) {
            root.fullscreenHost.closeAll()
            return
        }
        root.close()
    }

    function updateImageMetadataDemand() {
        if (!root.managesPreviewSession) return
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

    x: root.fullscreenActive || root.standaloneHost ? 0 : Math.round((parent.width - width) / 2)
    y: root.fullscreenActive || root.standaloneHost ? 0 : Math.round((parent.height - height) / 2)
    width: root.fullscreenActive || root.standaloneHost
           ? parent.width : Math.round(Math.min(parent.width * 0.84, 960))
    height: root.fullscreenActive || root.standaloneHost
            ? parent.height
            : Math.round(Math.min(parent.height - 24,
                                  Math.min(parent.height * 0.84, 720)
                                  + root.navigationFooterHeight
                                  + (navigationFooter.visible ? 18 : 0)))

    modal: true
    focus: true
    closePolicy: (root.fullscreenActive ? Popup.NoAutoClose : Popup.CloseOnEscape)
                 | Popup.CloseOnPressOutside

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
        cornerRadius: root.fullscreenActive ? 0 : Theme.radiusLg
        baseColor: root.translucentSurface
                   ? Theme.withAlpha(Theme.panelSurfaceStrong, root.surfaceAlpha)
                   : Theme.panelSurface
        startColor: Theme.chromeGradientStart
        midColor: Theme.chromeGradientMid
        endColor: Theme.panelSurface
        gradientStrength: 0.5
        borderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: root.fullscreenActive ? 0 : 20
        shadowVerticalOffset: root.fullscreenActive ? 0 : 8

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
            if (event.key === Qt.Key_Escape && root.fullscreenActive) {
                root.leaveFullscreen()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.requestClose()
                event.accepted = true
            }
        }

        PreviewHeader {
            Layout.fillWidth: true
            backgroundBaseColor: root.translucentSurface ? "transparent" : Theme.opaque(
                Theme.mixColors(Theme.panelSurface,
                                Theme.panelSurfaceStrong,
                                themeController.isDark ? 0.82 : 0.68))
            iconSource: root.displayIconSource()
            fallbackIconSource: root.displayFallbackIconSource()
            title: root.displayTitle()
            subtitle: root.displaySubtitle()
            secondaryActionVisible: root.fullscreenAvailable || root.fullscreenActive
            secondaryActionIconSource: root.fullscreenActive
                                       ? "qrc:/qt/qml/FM/qml/assets/icons-classic/fullscreen-exit.svg"
                                       : "qrc:/qt/qml/FM/qml/assets/icons-classic/fullscreen.svg"
            secondaryActionAccessibleName: root.fullscreenActive ? "Exit full screen" : "Enter full screen"
            secondaryActionToolTip: secondaryActionAccessibleName + " (F11)"
            onSecondaryActionRequested: root.toggleFullscreen()
            onCloseRequested: root.requestClose()
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
                adaptiveImageLayout: root.fullscreenActive && quickLookController.type === "image"
                imageBaseViewMode: root.imageViewState ? root.imageViewState.baseViewMode : "adaptive"
                imageBackgroundMode: appSettings ? appSettings.quickLookImageBackground : 0
                onImageBaseViewModeChangedByUser: (mode) => {
                    if (root.imageViewState) root.imageViewState.baseViewMode = mode
                }
                onImageBackgroundModeChangedByUser: (mode) => {
                    if (appSettings) appSettings.quickLookImageBackground = mode
                }
                sourceSizeWidth: root.fullscreenActive && quickLookController.type === "image"
                                 ? Math.min(4096, Math.max(2048, Math.ceil(width * Screen.devicePixelRatio)))
                                 : 2048
                sourceSizeHeight: root.fullscreenActive && quickLookController.type === "image"
                                  ? Math.min(4096, Math.max(2048, Math.ceil(height * Screen.devicePixelRatio)))
                                  : 2048
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
        sequence: "F11"
        enabled: root.enabled && root.opened
                 && (root.fullscreenAvailable || root.fullscreenActive)
        autoRepeat: false
        onActivated: root.toggleFullscreen()
    }

    Shortcut {
        sequence: "Left"
        enabled: root.enabled && root.opened && root.navigationController
                 && root.navigationController.navigationActive
                 && root.navigationController.canGoPrevious
        autoRepeat: true
        onActivated: root.navigationController.navigate(-1)
    }

    Shortcut {
        sequence: "Right"
        enabled: root.enabled && root.opened && root.navigationController
                 && root.navigationController.navigationActive
                 && root.navigationController.canGoNext
        autoRepeat: true
        onActivated: root.navigationController.navigate(1)
    }

    onImageMetadataHiddenChanged: root.updateImageMetadataDemand()
    onAboutToShow: {
        root.traceFullscreen("about-to-show")
        root.playbackControlsReady = true
    }
    onAboutToHide: {
        root.traceFullscreen("about-to-hide")
        root.playbackControlsReady = false
        if (root.managesPreviewSession && typeof quickLookController !== "undefined" && quickLookController) {
            quickLookController.unloadBookContent()
        }
    }
    onOpened: {
        root.traceFullscreen("opened")
        root.updateImageMetadataDemand()
        root.ensureBookContent()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }
    onClosed: {
        root.traceFullscreen("closed-before-leave")
        if (!root.standaloneHost) {
            root.leaveFullscreen()
        }
        root.traceFullscreen("closed-after-leave")
        root.updateImageMetadataDemand()
        if (root.managesPreviewSession && root.navigationController) root.navigationController.endNavigation()
        if (root.managesPreviewSession && root.restorePreviewOnClose
                && typeof quickLookController !== "undefined" && quickLookController) {
            if (root.restorePreviewPath === "selection://" && root.restorePreviewSelection.length > 1)
                quickLookController.previewSelection(root.restorePreviewSelection)
            else
                quickLookController.preview(root.restorePreviewPath)
        }
        root.restorePreviewOnClose = false
        root.restorePreviewPath = ""
        root.restorePreviewSelection = []
        if (root.managesPreviewSession && root.imageViewState)
            root.imageViewState.baseViewMode = "adaptive"
    }

    Connections {
        target: typeof quickLookController !== "undefined" ? quickLookController : null
        function onPathChanged() { root.ensureBookContent() }
        function onTypeChanged() {
            root.ensureBookContent()
        }
        function onLoadingChanged() {
            root.ensureBookContent()
        }
    }

    Connections {
        target: root.hostWindow
        enabled: root.fullscreenTraceEnabled
        function onVisibilityChanged() { root.traceFullscreen("window-visibility-changed") }
        function onWidthChanged() { root.traceFullscreen("window-width-changed") }
        function onHeightChanged() { root.traceFullscreen("window-height-changed") }
    }


    Connections {
        target: root
        enabled: root.fullscreenTraceEnabled && (root.opened || root.visible)
        function onXChanged() { root.traceFullscreen("popup-x-changed") }
        function onYChanged() { root.traceFullscreen("popup-y-changed") }
        function onWidthChanged() { root.traceFullscreen("popup-width-changed") }
        function onHeightChanged() { root.traceFullscreen("popup-height-changed") }
        function onScaleChanged() { root.traceFullscreen("popup-scale-changed") }
        function onOpacityChanged() { root.traceFullscreen("popup-opacity-changed") }
    }
}
