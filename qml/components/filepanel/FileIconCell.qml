import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string iconSource: ""
    property string iconName: ""
    property string path: ""
    property string suffix: ""
    property string mimeType: ""
    property string name: ""
    property bool isDirectory: false
    property bool useNativeIcons: true
    property string thumbnailSource: ""
    property bool showThumbnail: false
    property int iconSize: 16
    property real thumbCornerRadius: Math.max(2, iconSize / 8)
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true
    readonly property string explicitIconSource: root.iconSource.length > 0
        ? root.iconSource
        : root.iconSourceForName(root.iconName)
    readonly property string bundledIconSource: root.explicitIconSource.length > 0
        ? root.explicitIconSource
        : root.bundledIconForPath(root.path, root.isDirectory, root.suffix)
    readonly property string nativeIconSource: root.iconSourceFor(root.path, root.isDirectory, root.suffix, root.mimeType, root.name, root.useNativeIcons)
    readonly property bool pdfThumbnail: !root.isDirectory && String(root.suffix || "").toLowerCase() === "pdf"
    readonly property bool thumbnailReady: root.showThumbnail && thumbImg.status === Image.Ready
    readonly property bool nativeIconRequested: root.useNativeIcons && root.nativeIconSource.length > 0
    readonly property bool nativeIconReady: root.nativeIconRequested && nativeIconImg.status === Image.Ready
    readonly property bool nativeIconFailed: root.nativeIconRequested && nativeIconImg.status === Image.Error
    readonly property bool showBundledIcon: !root.thumbnailReady
                                           && (!root.nativeIconRequested || !root.useNativeIcons || root.nativeIconFailed)
    readonly property bool showNativeIcon: !root.thumbnailReady && root.nativeIconReady

    function bundledIconForSuffix(isDirectory, suffix) {
        return fileTypeIconResolver.iconForSuffix(String(suffix || ""), isDirectory)
    }

    function iconSourceForName(name) {
        const value = String(name || "").trim()
        return value.length > 0
            ? "qrc:/qt/qml/FM/qml/assets/filetypes-next/" + value + ".svg"
            : ""
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
            return bundledIconForSuffix(isDirectory, suffix)
        }
        if (value.length > 0) {
            return fileTypeIconResolver.iconForPathHint(value, isDirectory)
        }
        return bundledIconForSuffix(isDirectory, suffix)
    }

    function nativeIconOverrideForPath(path, isDirectory) {
        const value = String(path || "")
        if (value.length === 0) {
            return ""
        }
        return fileTypeIconResolver.nativeIconOverrideForPathHint(value, isDirectory)
    }

    function nativeIconOverrideForIdentity(path, isDirectory, suffix, name) {
        const nameValue = String(name || "")
        const suffixValue = String(suffix || "")
        if (nameValue.length > 0) {
            let hint = nameValue
            if (suffixValue.length > 0 && hint.toLowerCase().indexOf("." + suffixValue.toLowerCase()) < 0) {
                hint += "." + suffixValue
            }
            const nameIcon = nativeIconOverrideForPath(hint, isDirectory)
            if (nameIcon.length > 0) {
                return nameIcon
            }
        }
        return nativeIconOverrideForPath(path, isDirectory)
    }

    function isVirtualRootPath(path) {
        const value = String(path || "")
        return value === "devices://" || value === "favorites://" || value === "selection://"
    }

    function isProviderVirtualIconPath(path) {
        const value = String(path || "").toLowerCase()
        return value === "gdrive://"
               || value === "gdrive://my-drive"
               || value === "gdrive://shared-with-me"
               || value === "gdrive://shortcuts"
               || value === "gdrive://trash"
    }

    function iconQuery(isDirectory, suffix, mimeType, name, providerPath) {
        let query = isDirectory
            ? ("directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        if (providerPath) {
            query += "&provider=true"
        }
        const suffixValue = String(suffix || "")
        if (suffixValue.length > 0) {
            query += "&suffix=" + encodeURIComponent(suffixValue)
        }
        const mimeValue = String(mimeType || "")
        if (mimeValue.length > 0) {
            query += "&mime=" + encodeURIComponent(mimeValue)
        }
        const nameValue = String(name || "")
        if (nameValue.length > 0) {
            query += "&name=" + encodeURIComponent(nameValue)
        }
        return query
    }

    function iconSourceFor(path, isDirectory, suffix, mimeType, name, useNativeIcons) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        const providerPath = value.indexOf("://") > 0
                             && lower.indexOf("archive://") !== 0
                             && lower.indexOf("file://") !== 0
        if (!useNativeIcons) {
            return bundledIconForPath(path, isDirectory, suffix)
        }
        if (!path || String(path).length === 0) {
            return bundledIconForSuffix(isDirectory, suffix)
        }
        if ((!providerPath || isProviderVirtualIconPath(path)) && root.explicitIconSource.length > 0) {
            return root.explicitIconSource
        }
        const overrideIcon = nativeIconOverrideForIdentity(path, isDirectory, suffix, name)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        if (isVirtualRootPath(path)) {
            return bundledIconForPath(path, isDirectory, suffix)
        }
        const query = "?" + iconQuery(isDirectory, suffix, mimeType, name, providerPath)
        return "image://icon/" + encodeURIComponent(path + query)
    }

    implicitWidth: iconSize
    implicitHeight: iconSize

    Image {
        id: fallbackIconImg
        anchors.fill: parent
        source: root.bundledIconSource
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        asynchronous: false
        cache: true
        smooth: true
        mipmap: false
        visible: root.showBundledIcon
    }

    Image {
        id: nativeIconImg
        anchors.fill: parent
        source: root.nativeIconSource
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        asynchronous: root.useNativeIcons
        cache: true
        smooth: true
        mipmap: false
        visible: root.showNativeIcon
    }

    Rectangle {
        anchors.fill: parent
        radius: root.thumbCornerRadius
        color: "#ffffff"
        visible: root.pdfThumbnail && root.thumbnailReady
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
        visible: root.thumbnailReady

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
