import QtQuick

QtObject {
    id: root

    property bool useNativeIcons: true
    property bool useHighQualitySystemIcons: true

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

    function bundledIconForPath(path, isDirectory, suffix, iconName) {
        const explicitIcon = root.iconSourceForName(iconName)
        if (explicitIcon.length > 0) {
            return explicitIcon
        }
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

    function nativeIconOverrideForIdentity(path, isDirectory, suffix, name) {
        const nameValue = String(name || "")
        const suffixValue = String(suffix || "")
        if (nameValue.length > 0) {
            let hint = nameValue
            if (suffixValue.length > 0 && hint.toLowerCase().indexOf("." + suffixValue.toLowerCase()) < 0) {
                hint += "." + suffixValue
            }
            const nameIcon = root.nativeIconOverrideForPath(hint, isDirectory)
            if (nameIcon.length > 0) {
                return nameIcon
            }
        }
        return root.nativeIconOverrideForPath(path, isDirectory)
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

    function shouldUseNativeFolderOverlay(path, isDirectory, iconName) {
        return root.useNativeIcons
               && isDirectory
               && root.providerFolderOverlayName(path, iconName).length > 0
    }

    function nativeProviderFolderBaseSource(name) {
        const query = "?" + root.iconQuery(true, "", "", name, true)
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

    function panelIconSource(path, isDirectory, suffix, iconName, mimeType, name) {
        const value = String(path || "")
        const lower = value.toLowerCase()
        const providerPath = value.indexOf("://") > 0
                             && lower.indexOf("archive://") !== 0
                             && lower.indexOf("file://") !== 0
        if (!root.useNativeIcons) {
            return root.bundledIconForPath(path, isDirectory, suffix, iconName)
        }
        if (root.shouldUseNativeFolderOverlay(path, isDirectory, iconName)) {
            return root.nativeProviderFolderBaseSource(name)
        }
        const explicitIcon = root.iconSourceForName(iconName)
        if ((!providerPath || root.isProviderVirtualIconPath(path)) && explicitIcon.length > 0) {
            return explicitIcon
        }
        const overrideIcon = root.nativeIconOverrideForIdentity(path, isDirectory, suffix, name)
        if (overrideIcon.length > 0) {
            return overrideIcon
        }
        if (root.isVirtualRootPath(path)) {
            return root.bundledIconForPath(path, isDirectory, suffix, iconName)
        }
        const query = "?" + root.iconQuery(isDirectory, suffix, mimeType, name, providerPath)
        return "image://icon/" + encodeURIComponent(path + query)
    }
}
