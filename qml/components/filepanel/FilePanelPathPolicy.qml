import QtQuick

QtObject {
    id: root

    function normalizedPath(path) {
        let value = String(path || "").replace(/\\/g, "/")
        if (value === "devices://" || value === "favorites://") {
            return value
        }
        while (value.length > 1
               && value.endsWith("/")
               && !/^[A-Za-z]:\/$/.test(value)
               && !value.endsWith("|/")) {
            value = value.slice(0, -1)
        }
        return Qt.platform.os === "windows" ? value.toLowerCase() : value
    }

    function samePath(left, right) {
        return root.normalizedPath(left) === root.normalizedPath(right)
    }

    function directChildPath(parentPath, childPath) {
        if (!parentPath || !childPath) return ""

        let p = String(parentPath).replace(/\\/g, "/")
        let c = String(childPath).replace(/\\/g, "/")
        if (c.startsWith("archive://") && !p.startsWith("archive://")) {
            let archiveFile = c.substring(10).split("|")[0]
            if (archiveFile.startsWith(p.endsWith("/") ? p : p + "/")) {
                return archiveFile
            }
        }
        if (p !== "devices://" && p !== "favorites://" && !p.endsWith("/")) {
            p = p + "/"
        }
        if (!c.startsWith(p)) {
            return ""
        }

        let sub = c.substring(p.length)
        let parts = sub.split("/")
        if (parts.length > 0 && parts[0].length > 0) {
            let slash = parentPath.endsWith("/") || parentPath.endsWith("\\") ? "" : "/"
            return parentPath + slash + parts[0]
        }
        return ""
    }
}
