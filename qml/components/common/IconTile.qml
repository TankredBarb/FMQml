import QtQuick
import QtQuick.Effects
import "../../style"

Rectangle {
    id: root

    property string source: ""
    property color tileColor: Theme.withAlpha(Theme.accent, 0.12)
    property color iconColor: Theme.accent
    property int tileSize: 32
    property int iconSize: 16
    property int cornerRadius: Theme.radiusSm
    property bool colorize: true
    property bool asynchronous: true
    property bool imageCache: true

    implicitWidth: root.tileSize
    implicitHeight: root.tileSize
    radius: root.cornerRadius
    color: root.tileColor

    Image {
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        source: root.source
        sourceSize: Qt.size(root.iconSize, root.iconSize)
        asynchronous: root.asynchronous
        cache: root.imageCache
        layer.enabled: root.colorize && root.source.length > 0
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: root.iconColor
        }
    }
}
