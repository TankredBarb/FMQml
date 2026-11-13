import QtQuick
import QtQuick.Controls
import "../../style"
import "../preview"

Rectangle {
    id: page

    required property var controller
    property bool active: false
    readonly property real contentImplicitHeight: 320

    anchors.fill: parent
    anchors.margins: 16
    visible: page.active
    color: Theme.panelSurface
    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.22 : 0.18)
    border.width: 1
    radius: Theme.radiusMd
    clip: true

    Repeater {
        model: page.active && page.controller.available ? [page.controller.request] : []
        ImagePreview {
            required property var modelData
            anchors.fill: parent
            anchors.margins: 8
            explicitSource: modelData.source
            sourceSizeWidth: 1024
            sourceSizeHeight: 1024
            requestThumbnail: false
            metadataControlsEnabled: false
            metadataHidden: true
            onImageStatusChanged: {
                if (imageStatus === Image.Ready || imageStatus === Image.Error)
                    page.controller.complete(modelData.id, imageStatus === Image.Ready,
                                             imageImplicitWidth, imageImplicitHeight)
            }
        }
    }
}
