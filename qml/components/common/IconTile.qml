import QtQuick
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

    RecolorSvgIcon {
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        sourcePath: root.source
        recolorEnabled: root.colorize
        recolorColor: root.iconColor
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        asynchronous: root.asynchronous
        cache: root.imageCache
    }
}
