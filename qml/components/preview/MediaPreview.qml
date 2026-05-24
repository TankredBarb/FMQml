import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string sourcePath: ""
    property string type: ""
    property bool hasPdfSupport: false
    property int sourceSizeWidth: 2048
    property int sourceSizeHeight: 2048

    clip: true

    VideoPreview {
        anchors.fill: parent
        visible: root.type === "video"
        sourcePath: root.sourcePath
        sourceSizeWidth: root.sourceSizeWidth
        sourceSizeHeight: root.sourceSizeHeight
        loadingText: "Loading video preview..."
    }

    ImagePreview {
        id: previewImage
        anchors.fill: parent
        requestThumbnail: root.type === "svg"
                          || root.type === "font"
                          || (root.type === "pdf" && !root.hasPdfSupport)
        sourcePath: root.sourcePath
        fillMode: Image.PreserveAspectFit
        sourceSizeWidth: root.sourceSizeWidth
        sourceSizeHeight: root.sourceSizeHeight
        visible: root.type !== "video" && (root.type !== "pdf" || !root.hasPdfSupport)
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

    PdfPreviewFallback {
        anchors.centerIn: parent
        visible: root.type === "pdf" && previewImage.imageStatus === Image.Error
    }
}
