import QtQuick

QtObject {
    id: root

    property var directoryModel: null
    property string targetSelectPath: ""
    property real pendingScrollRestoreY: -1

    function readiness(view) {
        if (!view) {
            return { ready: false, reason: "no-view", minY: 0, maxY: 0 }
        }

        if (view.forceLayout) {
            view.forceLayout()
        }

        if (view.contentHeight <= 0) {
            return { ready: false, reason: "empty", minY: 0, maxY: 0 }
        }

        const minY = view.originY || 0
        const maxY = Math.max(minY, minY + view.contentHeight - view.height + (view.bottomMargin || 0))
        if (root.directoryModel && root.directoryModel.loading) {
            return { ready: false, reason: "loading", minY: minY, maxY: maxY }
        }
        return { ready: true, reason: "", minY: minY, maxY: maxY }
    }
}
