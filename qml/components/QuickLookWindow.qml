import QtQuick
import QtQuick.Window

Window {
    id: root

    property var normalPopup: null
    property var hostWindow: null
    property var navigationController: null
    property var imageViewState: null

    transientParent: root.hostWindow
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    color: "transparent"
    visible: false

    function enterFullscreen() {
        if (!root.normalPopup || quickLook.opened) return
        root.normalPopup.fullscreenActive = true
        root.normalPopup.opacity = 0
        root.normalPopup.enabled = false
        root.showFullScreen()
        quickLook.open()
        root.requestActivate()
    }

    function leaveFullscreen() {
        quickLook.close()
        root.hide()
        if (root.normalPopup) {
            root.normalPopup.fullscreenActive = false
            root.normalPopup.opacity = 1
            root.normalPopup.enabled = true
            Qt.callLater(() => root.normalPopup.contentItem.forceActiveFocus())
        }
    }

    function closeAll() {
        quickLook.close()
        root.hide()
        if (root.normalPopup) {
            root.normalPopup.fullscreenActive = false
            root.normalPopup.opacity = 1
            root.normalPopup.enabled = true
            root.normalPopup.requestClose()
        }
    }

    onClosing: (close) => {
        if (quickLook.opened) {
            close.accepted = false
            root.closeAll()
        }
    }

    QuickLook {
        id: quickLook
        parent: root.contentItem
        hostWindow: root
        navigationController: root.navigationController
        standaloneHost: true
        managesPreviewSession: false
        fullscreenHost: root
        backdropSource: null
        previewPath: root.normalPopup ? root.normalPopup.previewPath : ""
        imageViewState: root.imageViewState
        fullscreenActive: true

        onClosed: {
            root.hide()
        }
    }
}
