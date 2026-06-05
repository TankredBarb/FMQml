import QtQuick

QtObject {
    id: root

    property bool useNativeIcons: true
    property bool useHighQualitySystemIcons: true

    function bundledIconForSuffix(isDirectory, suffix) {
        return fileTypeIconResolver.iconForSuffix(String(suffix || ""), isDirectory)
    }

    function panelIconSource(path, isDirectory, suffix) {
        if (!root.useNativeIcons) {
            return root.bundledIconForSuffix(isDirectory, suffix)
        }
        const query = isDirectory
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        return "image://icon/" + encodeURIComponent(path + query)
    }
}
