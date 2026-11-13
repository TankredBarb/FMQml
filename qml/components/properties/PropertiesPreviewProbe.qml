import QtQuick

Item {
    id: root
    required property var controller
    visible: false

    // Replacing the generation destroys its Image and cancels the old Qt request.
    Repeater {
        model: root.controller.request.id ? [root.controller.request] : []
        Image {
            required property var modelData
            source: modelData.source
            sourceSize: Qt.size(128, 128)
            asynchronous: true
            cache: false
            visible: false
            onStatusChanged: {
                if (status === Image.Ready || status === Image.Error)
                    root.controller.complete(modelData.id, status === Image.Ready, implicitWidth, implicitHeight)
            }
        }
    }
}
