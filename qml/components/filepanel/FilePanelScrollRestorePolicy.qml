import QtQuick

QtObject {
    id: root

    property var directoryModel: null
    property string targetSelectPath: ""
    property real pendingScrollRestoreY: -1

    function readiness(view) {
        if (!view) {
            return { ready: false, reason: "no-view", maxY: 0 }
        }

        if (view.forceLayout) {
            view.forceLayout()
        }

        if (view.contentHeight <= 0) {
            return { ready: false, reason: "empty", maxY: 0 }
        }

        const maxY = Math.max(0, view.contentHeight - view.height)
        if (root.directoryModel && root.directoryModel.loading) {
            return { ready: false, reason: "loading", maxY: maxY }
        }
        return { ready: true, reason: "", maxY: maxY }
    }
}
