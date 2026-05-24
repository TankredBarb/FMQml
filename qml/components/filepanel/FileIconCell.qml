import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string iconSource: ""
    property string thumbnailSource: ""
    property bool showThumbnail: false
    property int iconSize: 16
    property real thumbCornerRadius: Math.max(2, iconSize / 8)

    implicitWidth: iconSize
    implicitHeight: iconSize

    Image {
        id: iconImg
        anchors.fill: parent
        source: root.iconSource
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        asynchronous: true
        cache: true
        smooth: true
        mipmap: false
        visible: !root.showThumbnail || thumbImg.status !== Image.Ready
    }

    Image {
        id: thumbImg
        anchors.fill: parent
        source: root.showThumbnail ? root.thumbnailSource : ""
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        visible: root.showThumbnail && status === Image.Ready

        layer.enabled: visible
        layer.effect: null
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: root.thumbCornerRadius
        clip: true
        visible: thumbImg.visible
    }
}
