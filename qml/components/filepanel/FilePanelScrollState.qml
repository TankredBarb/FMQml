import QtQuick

QtObject {
    id: root

    property var positions: ({})
    property int currentMode: 0
    property var normalizePathProvider: null

    function normalizedPath(path) {
        return root.normalizePathProvider ? root.normalizePathProvider(path) : String(path || "")
    }

    function canStorePath(path) {
        return !!path && path !== "devices://" && path !== "favorites://"
    }

    function keyForPath(path, mode) {
        const keyMode = mode === undefined ? root.currentMode : mode
        return root.normalizedPath(path) + "|" + keyMode
    }

    function state(path, mode) {
        if (!root.canStorePath(path)) {
            return null
        }
        const stored = root.positions[root.keyForPath(path, mode)]
        return stored ? stored : null
    }

    function save(path, mode, y, x, focusedPath, focusedOffsetY, anchorPath, anchorOffsetY, anchorSetsCurrent, anchorSource) {
        if (!root.canStorePath(path)) {
            return false
        }
        root.positions[root.keyForPath(path, mode)] = {
            y: y,
            x: x,
            focusedPath: focusedPath || "",
            focusedOffsetY: focusedOffsetY,
            anchorPath: anchorPath || focusedPath || "",
            anchorOffsetY: anchorOffsetY,
            anchorSetsCurrent: anchorSetsCurrent === true,
            anchorSource: anchorSource || ""
        }
        return true
    }

    function viewModeAnchor(path, oldMode) {
        const oldState = root.state(path, oldMode)
        if (!oldState) {
            return { path: "", offsetY: 0, setsCurrent: false, source: "" }
        }
        const anchorPath = oldState.anchorPath || oldState.focusedPath || ""
        const anchorOffsetY = oldState.anchorOffsetY !== undefined
                              ? oldState.anchorOffsetY
                              : (oldState.focusedOffsetY !== undefined ? oldState.focusedOffsetY : 0)
        return {
            path: anchorPath,
            offsetY: anchorOffsetY,
            setsCurrent: oldState.anchorSetsCurrent === true || !!oldState.focusedPath,
            source: oldState.anchorSource || (oldState.focusedPath ? "focused" : "")
        }
    }
}
