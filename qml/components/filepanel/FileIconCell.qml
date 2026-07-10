import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string iconSource: ""
    property string iconName: ""
    property string path: ""
    property string suffix: ""
    property string mimeType: ""
    property string name: ""
    property string primaryBadgeKind: ""
    property bool isPinned: false
    property bool isDirectory: false
    property bool useNativeIcons: true
    property string thumbnailSource: ""
    property bool showThumbnail: false
    property bool hasThumbnail: false
    property int iconSize: 16
    property bool thumbnailDisplayed: false
    property real thumbCornerRadius: Math.max(2, iconSize / 8)
    signal thumbnailError()
    signal thumbnailSoftMiss()
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
    readonly property string providerOverlaySource: root.providerFolderOverlaySource(root.path, root.iconName)
    readonly property string providerAvatarSource: root.shouldUseProviderAvatar(root.path)
                                                    ? "image://thumbnail/" + encodeURIComponent(root.path)
                                                    : ""
    readonly property bool nativeFolderOverlay: root.shouldUseNativeFolderOverlay(root.path, root.isDirectory, root.iconName, root.useNativeIcons)
    readonly property bool pdfThumbnail: !root.isDirectory && String(root.suffix || "").toLowerCase() === "pdf"
    readonly property bool thumbnailReady: root.showThumbnail && root.thumbnailDisplayed
    readonly property bool thumbnailDebugOverlay: typeof thumbnailDebugOverlayEnabled !== "undefined"
                                                  && thumbnailDebugOverlayEnabled
    readonly property bool providerAvatarReady: providerAvatarImg.status === Image.Ready
                                             && providerAvatarImg.implicitWidth > 1
                                             && providerAvatarImg.implicitHeight > 1
    readonly property bool nativeIconRequested: root.useNativeIcons && root.nativeIconSource.length > 0
    readonly property bool nativeIconReady: root.nativeIconRequested && nativeIconImg.status === Image.Ready
    readonly property bool nativeIconFailed: root.nativeIconRequested && nativeIconImg.status === Image.Error
    readonly property bool showBundledIcon: !root.thumbnailReady
                                           && (!root.nativeIconRequested || !root.useNativeIcons || root.nativeIconFailed)
    readonly property bool showNativeIcon: !root.thumbnailReady && root.nativeIconReady
    readonly property bool showProviderOverlay: root.nativeFolderOverlay
                                                && root.showNativeIcon
                                                && root.providerOverlaySource.length > 0
    readonly property string primaryBadgeSource: root.primaryBadgeSourceFor(root.primaryBadgeKind)
    readonly property bool primaryBadgeIsWarning: root.primaryBadgeKind === "broken-link"
    readonly property bool showPrimaryBadge: !root.showProviderOverlay
                                             && root.primaryBadgeSource.length > 0
    readonly property int providerOverlayGlyphSize: Math.max(8, Math.round(root.iconSize * 0.38))
    readonly property int providerOverlaySize: Math.max(14, Math.round(root.iconSize * 0.50))
    readonly property int providerOverlayInset: Math.max(2, Math.round(root.providerOverlaySize * 0.10))
    readonly property int pinnedBadgeSize: Math.max(12, Math.round(root.iconSize * 0.42))
    readonly property int pinnedBadgeGlyphSize: Math.max(7, Math.round(root.pinnedBadgeSize * 0.62))

    onPathChanged: thumbnailDisplayed = false
    onThumbnailSourceChanged: {
        if (typeof thumbnailTraceEnabled !== "undefined" && thumbnailTraceEnabled) {
            console.log("[ThumbnailTrace] cell-source path=" + path + " source=" + thumbnailSource
                        + " displayed=" + thumbnailDisplayed)
        }
        if (thumbnailSource.length === 0) {
            thumbnailDisplayed = false
        }
    }

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

    function shouldUseProviderAvatar(path) {
        if (!root.isDirectory || !root.useNativeIcons || !root.hasThumbnail) {
            return false
        }
        if (typeof appSettings !== "undefined" && appSettings && !appSettings.showThumbnails) {
            return false
        }
        const value = String(path || "").toLowerCase()
        if (value.endsWith("/__load_more__") || value.endsWith("/__load_more__/")) {
            return false
        }
        return value.indexOf("telegram://chat/") === 0 || value.indexOf("telegram://channel/") === 0
    }

    function primaryBadgeSourceFor(kind) {
        switch (String(kind || "")) {
        case "broken-link":
            return "qrc:/qt/qml/FM/qml/assets/icons/badge-link-broken.svg"
        case "link":
            return "qrc:/qt/qml/FM/qml/assets/icons/badge-link.svg"
        case "locked":
            return "qrc:/qt/qml/FM/qml/assets/icons/badge-lock.svg"
        case "mount-point":
            return "qrc:/qt/qml/FM/qml/assets/icons/badge-mount.svg"
        case "archive":
            return "qrc:/qt/qml/FM/qml/assets/icons/badge-archive.svg"
        default:
            return ""
        }
    }

    function isProviderVirtualIconPath(path) {
        const value = String(path || "").toLowerCase()
        return value === "gdrive://"
               || value === "gdrive://my-drive"
               || value === "gdrive://shared-with-me"
               || value === "gdrive://shortcuts"
               || value === "gdrive://trash"
               || value === "mega:///"
               || value === "mega:///cloud drive"
               || (value.indexOf("mega://link/") === 0 && value.substring(12).indexOf("/") < 0)
               || value === "telegram://"
               || value === "telegram:///"
               || value === "telegram://saved"
               || value === "telegram://chats"
               || value === "telegram://downloads"
    }

    function providerFolderOverlayName(path, iconName) {
        const iconValue = String(iconName || "").trim()
        if (iconValue === "gdrive-shortcut" || iconValue === "gdrive-file-shortcut") {
            return "gdrive-badge-shortcut"
        }

        const value = String(path || "").toLowerCase()
        if (value === "gdrive://" || value === "gdrive://my-drive") {
            return "gdrive"
        }
        if (value === "gdrive://shared-with-me") {
            return "gdrive-badge-shared"
        }
        if (value === "gdrive://shortcuts") {
            return "gdrive-badge-shortcut"
        }
        if (value === "gdrive://trash") {
            return "gdrive-badge-trash"
        }
        if (value === "mega:///" || value === "mega:///cloud drive"
                || (value.indexOf("mega://link/") === 0 && value.substring(12).indexOf("/") < 0)) {
            return "mega"
        }
        if (iconValue === "mega" || iconValue === "mega-clouddrive") {
            return "mega"
        }
        if (iconValue === "instagram-stories" || iconValue === "instagram-badge-stories") {
            return "instagram-badge-stories"
        }
        if (iconValue === "instagram-load-more" || iconValue === "instagram-badge-load-more") {
            return "instagram-badge-load-more"
        }
        if (value.indexOf("instagram://") === 0 && (value.endsWith("/stories") || value.endsWith("/stories/"))) {
            return "instagram-badge-stories"
        }
        if (value.indexOf("instagram://") === 0 && (value.endsWith("/__load_more__") || value.endsWith("/__load_more__/"))) {
            return "instagram-badge-load-more"
        }
        if (iconValue === "telegram-saved") {
            return "telegram"
        }
        if (iconValue === "telegram-chats") {
            return "telegram-badge-chat"
        }
        if (iconValue === "telegram-downloads") {
            return "telegram-badge-downloads"
        }
        if (iconValue === "telegram-badge-load-more") {
            return "telegram-badge-load-more"
        }
        if (value === "telegram://" || value === "telegram:///") {
            return "telegram"
        }
        if (value === "telegram://saved") {
            return "telegram"
        }
        if (value === "telegram://chats") {
            return "telegram-badge-chat"
        }
        if (value === "telegram://downloads") {
            return "telegram-badge-downloads"
        }
        if (value.indexOf("telegram://") === 0 && (value.endsWith("/__load_more__") || value.endsWith("/__load_more__/"))) {
            return "telegram-badge-load-more"
        }
        if (value.indexOf("telegram://chat/") === 0) {
            return "telegram-badge-chat"
        }
        if (value.indexOf("telegram://channel/") === 0) {
            return "telegram-badge-channel"
        }
        return ""
    }

    function providerFolderOverlaySource(path, iconName) {
        const overlayName = root.providerFolderOverlayName(path, iconName)
        return overlayName.length > 0 ? root.iconSourceForName(overlayName) : ""
    }

    function shouldUseNativeFolderOverlay(path, isDirectory, iconName, useNativeIcons) {
        return useNativeIcons
               && isDirectory
               && root.providerFolderOverlayName(path, iconName).length > 0
    }

    function nativeProviderFolderBaseSource(name) {
        const query = "?" + iconQuery(true, "", "", name, true)
        return "image://icon/" + encodeURIComponent("provider-folder" + query)
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
        if (root.shouldUseNativeFolderOverlay(path, isDirectory, root.iconName, useNativeIcons)) {
            return root.nativeProviderFolderBaseSource(name)
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
        id: providerOverlayBackground
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.providerOverlaySize
        height: width
        radius: width / 2
        color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.90 : 0.96)
        border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.24 : 0.16)
        border.width: 1
        clip: true
        visible: root.showProviderOverlay
    }

    Image {
        id: providerOverlayImg
        anchors.centerIn: providerOverlayBackground
        width: root.providerOverlayGlyphSize
        height: width
        source: root.nativeFolderOverlay && root.showNativeIcon && !root.providerAvatarReady ? root.providerOverlaySource : ""
        sourceSize: Qt.size(width * 2, height * 2)
        asynchronous: false
        cache: true
        smooth: true
        mipmap: false
        visible: root.showProviderOverlay && !root.providerAvatarReady
    }

    Image {
        id: providerAvatarImg
        anchors.fill: providerOverlayBackground
        anchors.margins: root.providerOverlayInset
        source: root.showProviderOverlay ? root.providerAvatarSource : ""
        sourceSize: Qt.size(width * 2, height * 2)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        mipmap: false
        visible: root.showProviderOverlay && root.providerAvatarReady

        layer.enabled: visible
        layer.effect: MultiEffect {
            maskEnabled: true
            maskThresholdMin: 0.5
            maskThresholdMax: 1.0
            maskSpreadAtMin: 1.0
            maskSpreadAtMax: 0.0
            maskSource: ShaderEffectSource {
                sourceItem: Rectangle {
                    width: providerAvatarImg.width
                    height: providerAvatarImg.height
                    radius: width / 2
                }
            }
        }
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

        onStatusChanged: {
            if (typeof thumbnailTraceEnabled !== "undefined" && thumbnailTraceEnabled) {
                console.log("[ThumbnailTrace] cell-status path=" + root.path + " status=" + status
                            + " size=" + implicitWidth + "x" + implicitHeight
                            + " displayed=" + root.thumbnailDisplayed)
            }
            if (status === Image.Error) {
                root.thumbnailDisplayed = false
                root.thumbnailError()
            } else if (status === Image.Ready && implicitWidth <= 1 && implicitHeight <= 1) {
                root.thumbnailSoftMiss()
            } else if (status === Image.Ready) {
                root.thumbnailDisplayed = true
            }
        }

        layer.enabled: visible
        layer.effect: null
    }

    Rectangle {
        id: primaryBadgeBackground
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.providerOverlaySize
        height: width
        radius: width / 2
        color: root.primaryBadgeIsWarning
               ? Theme.withAlpha(Theme.danger, themeController.isDark ? 0.96 : 0.92)
               : Theme.withAlpha(Theme.textSecondary, themeController.isDark ? 0.90 : 0.82)
        border.color: root.primaryBadgeIsWarning
                      ? Theme.withAlpha(Theme.danger, themeController.isDark ? 0.55 : 0.42)
                      : Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.30 : 0.22)
        border.width: 1
        visible: root.showPrimaryBadge
    }

    Image {
        anchors.centerIn: primaryBadgeBackground
        width: root.providerOverlayGlyphSize
        height: width
        source: root.showPrimaryBadge ? root.primaryBadgeSource : ""
        sourceSize: Qt.size(width * 2, height * 2)
        asynchronous: false
        cache: true
        smooth: true
        mipmap: false
        visible: root.showPrimaryBadge
    }

    Rectangle {
        id: pinnedBadgeBackground
        anchors.top: parent.top
        anchors.right: parent.right
        width: root.pinnedBadgeSize
        height: width
        radius: width / 2
        color: Theme.withAlpha(Theme.activeAccent, themeController.isDark ? 0.96 : 0.90)
        border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.30 : 0.22)
        border.width: 1
        visible: root.isPinned
    }

    Image {
        anchors.centerIn: pinnedBadgeBackground
        width: root.pinnedBadgeGlyphSize
        height: width
        source: root.isPinned ? "qrc:/qt/qml/FM/qml/assets/icons/badge-pinned.svg" : ""
        sourceSize: Qt.size(width * 2, height * 2)
        asynchronous: false
        cache: true
        smooth: true
        mipmap: false
        visible: root.isPinned
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        width: Math.max(12, root.iconSize * 0.45)
        height: width
        radius: width / 2
        color: "#99000000"
        visible: root.thumbnailDebugOverlay && root.showThumbnail

        Text {
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: Math.max(8, parent.height - 3)
            text: root.thumbnailReady ? "✓" : (thumbImg.status === Image.Error ? "×" : "…")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: root.thumbCornerRadius
        clip: true
        visible: thumbImg.visible
    }
}
