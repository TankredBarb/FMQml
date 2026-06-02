import QtQuick
import "../../style"

Image {
    id: root

    property string sourcePath: ""
    property color recolorColor: Theme.accent
    property bool recolorEnabled: true
    property bool recolorStroke: true
    property bool recolorFill: true
    property string cacheKey: ""

    function colorText(value) {
        return String(value)
    }

    source: {
        if (root.sourcePath.length === 0) {
            return ""
        }
        if (!root.recolorEnabled) {
            return root.sourcePath
        }

        const payload = [
            root.sourcePath,
            root.colorText(root.recolorColor),
            root.recolorStroke ? "1" : "0",
            root.recolorFill ? "1" : "0",
            root.cacheKey
        ].join("\n")
        return "image://svgrecolor/" + encodeURIComponent(payload)
    }

    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: false
}
