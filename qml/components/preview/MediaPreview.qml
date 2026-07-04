import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property string name: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string mimeName: ""
    property string extension: ""
    property string type: ""
    property bool hasPdfSupport: false
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048
    property bool interactiveImage: false
    property bool compactControls: true
    property int imageWidth: 0
    property int imageHeight: 0
    property var extraProperties: []
    property bool metadataHidden: false
    property string mediaSourceUrl: ""
    property bool multimediaControlsAvailable: false
    property bool playbackControlsActive: true
    property bool requestVideoThumbnail: true

    readonly property bool useVideoPlayback: root.type === "video"
                                             && !root.compactControls
                                             && root.multimediaControlsAvailable
                                             && root.playbackControlsActive
                                             && root.mediaSourceUrl.length > 0

    clip: true

    signal hideMetadataRequested()
    signal showMetadataRequested()

    VideoPreview {
        anchors.fill: parent
        visible: root.type === "video" && !root.useVideoPlayback
        sourcePath: root.sourcePath
        name: root.name
        sizeText: root.sizeText
        modifiedText: root.modifiedText
        mimeName: root.mimeName
        extension: root.extension
        sourceSizeWidth: root.sourceSizeWidth
        sourceSizeHeight: root.sourceSizeHeight
        loadingText: "Loading video preview..."
        compact: root.compactControls
        extraProperties: root.extraProperties
        metadataHidden: root.metadataHidden
        requestThumbnail: root.requestVideoThumbnail
        onHideMetadataRequested: root.hideMetadataRequested()
        onShowMetadataRequested: root.showMetadataRequested()
    }

    VideoPlaybackPreview {
        anchors.fill: parent
        visible: root.useVideoPlayback
        sourcePath: root.sourcePath
        mediaSourceUrl: root.mediaSourceUrl
        name: root.name
        sizeText: root.sizeText
        modifiedText: root.modifiedText
        mimeName: root.mimeName
        extension: root.extension
        sourceSizeWidth: root.sourceSizeWidth
        sourceSizeHeight: root.sourceSizeHeight
        compact: root.compactControls
        extraProperties: root.extraProperties
        metadataHidden: root.metadataHidden
        requestThumbnail: root.requestVideoThumbnail
        playbackActive: root.useVideoPlayback
        onHideMetadataRequested: root.hideMetadataRequested()
        onShowMetadataRequested: root.showMetadataRequested()
    }

    Loader {
        id: previewImageLoader
        anchors.fill: parent
        visible: root.type !== "video" && (root.type !== "pdf" || !root.hasPdfSupport)
        active: visible
        sourceComponent: root.interactiveImage && root.type === "svg"
                         ? zoomablePreviewComponent
                         : staticPreviewComponent
    }

    Component {
        id: staticPreviewComponent

        ImagePreview {
            requestThumbnail: root.type === "svg"
                              || root.type === "font"
                              || (root.type === "pdf" && !root.hasPdfSupport)
            sourcePath: root.sourcePath
            extension: root.extension
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            imageWidth: root.imageWidth
            imageHeight: root.imageHeight
            extraProperties: root.extraProperties
            compactMeta: root.compactControls
            metadataHidden: root.metadataHidden
            fillMode: Image.PreserveAspectFit
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
            onHideMetadataRequested: root.hideMetadataRequested()
            onShowMetadataRequested: root.showMetadataRequested()
        }
    }

    Component {
        id: zoomablePreviewComponent

        ZoomableImagePreview {
            requestThumbnail: true
            sourcePath: root.sourcePath
            extension: root.extension
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            imageWidth: root.imageWidth
            imageHeight: root.imageHeight
            extraProperties: root.extraProperties
            compactMeta: root.compactControls
            metadataHidden: root.metadataHidden
            fillMode: Image.PreserveAspectFit
            sourceSizeWidth: root.sourceSizeWidth
            sourceSizeHeight: root.sourceSizeHeight
            onHideMetadataRequested: root.hideMetadataRequested()
            onShowMetadataRequested: root.showMetadataRequested()
        }
    }

    Loader {
        id: pdfPreviewerLoader
        anchors.fill: parent
        visible: root.type === "pdf" && root.hasPdfSupport
        source: visible ? "../PdfPreviewer.qml" : ""
    }

    Binding {
        target: pdfPreviewerLoader.item
        property: "sourcePath"
        value: root.sourcePath
        when: pdfPreviewerLoader.status === Loader.Ready
    }

    Binding {
        target: pdfPreviewerLoader.item
        property: "compactControls"
        value: root.compactControls
        when: pdfPreviewerLoader.status === Loader.Ready
    }

    PdfPreviewFallback {
        anchors.centerIn: parent
        visible: root.type === "pdf"
                 && previewImageLoader.item
                 && previewImageLoader.item.imageStatus === Image.Error
    }
}
