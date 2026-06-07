import QtQuick

QtObject {
    id: root

    property bool useNativeIcons: true
    property bool useHighQualitySystemIcons: true

    function bundledIconForSuffix(isDirectory, suffix) {
        return fileTypeIconResolver.iconForSuffix(String(suffix || ""), isDirectory)
    }

    function shouldUseSuffixForPath(path, suffix) {
        const value = String(path || "")
        const ext = String(suffix || "")
        return ext.length > 0 && value.indexOf("://") > 0 && value.indexOf("archive://") !== 0
               && value !== "devices://" && value !== "favorites://" && value !== "selection://"
    }

    function bundledIconForPath(path, isDirectory, suffix) {
        const value = String(path || "")
        if (shouldUseSuffixForPath(value, suffix)) {
            return root.bundledIconForSuffix(isDirectory, suffix)
        }
        if (value.length > 0) {
            return fileTypeIconResolver.iconForPathHint(value, isDirectory)
        }
        return root.bundledIconForSuffix(isDirectory, suffix)
    }

    function nativeIconOverrideForPath(path, isDirectory) {
        const value = String(path || "")
        if (value.length === 0) {
            return ""
        }
        return fileTypeIconResolver.nativeIconOverrideForPathHint(value, isDirectory)
    }

    function supportsNativeIcon(path) {
        const value = String(path || "")
        return value.indexOf("://") < 0 || value.indexOf("archive://") === 0
    }

    function panelIconSource(path, isDirectory, suffix) {
        if (!root.useNativeIcons) {
            return root.bundledIconForPath(path, isDirectory, suffix)
        }
        const overrideIcon = root.nativeIconOverrideForPath(path, isDirectory)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        if (!root.supportsNativeIcon(path)) {
            return root.bundledIconForPath(path, isDirectory, suffix)
        }
        const query = isDirectory
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        return "image://icon/" + encodeURIComponent(path + query)
    }
}
