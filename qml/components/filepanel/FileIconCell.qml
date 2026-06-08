import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string iconSource: ""
    property string iconName: ""
    property string path: ""
    property string suffix: ""
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
    readonly property string nativeIconSource: root.explicitIconSource.length > 0
        ? root.explicitIconSource
        : root.iconSourceFor(root.path, root.isDirectory, root.suffix, root.useNativeIcons)
    readonly property bool pdfThumbnail: !root.isDirectory && String(root.suffix || "").toLowerCase() === "pdf"

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

    function supportsNativeIcon(path) {
        const value = String(path || "")
        return value.indexOf("://") < 0 || value.indexOf("archive://") === 0
    }

    function iconSourceFor(path, isDirectory, suffix, useNativeIcons) {
        if (!useNativeIcons) {
            return bundledIconForPath(path, isDirectory, suffix)
        }
        if (!path || String(path).length === 0) {
            return bundledIconForSuffix(isDirectory, suffix)
        }
        const overrideIcon = nativeIconOverrideForPath(path, isDirectory)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        if (!supportsNativeIcon(path)) {
            return bundledIconForPath(path, isDirectory, suffix)
        }
        const query = isDirectory
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
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
        visible: (!root.showThumbnail || thumbImg.status !== Image.Ready)
                 && (!root.useNativeIcons || nativeIconImg.status !== Image.Ready)
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
        visible: root.useNativeIcons
                 && (!root.showThumbnail || thumbImg.status !== Image.Ready)
                 && status === Image.Ready
    }

    Rectangle {
        anchors.fill: parent
        radius: root.thumbCornerRadius
        color: "#ffffff"
        visible: root.pdfThumbnail && root.showThumbnail && thumbImg.status === Image.Ready
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
