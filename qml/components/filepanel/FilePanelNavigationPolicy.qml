import QtQuick

QtObject {
    id: root

    property string pendingNavigationCommitPath: ""
    property string currentPath: ""
    property string pendingScrollRestorePath: ""
    property bool pendingScrollRestoreEnabled: false
    property bool resizeOptimized: false
    property var samePanelPathProvider: null

    function samePanelPath(a, b) {
        return root.samePanelPathProvider ? root.samePanelPathProvider(a, b) : a === b
    }

    function navigationCommitPending() {
        return root.pendingNavigationCommitPath.length > 0
                && !root.samePanelPath(root.currentPath, root.pendingNavigationCommitPath)
    }

    function navigationCommitArrived() {
        return root.pendingNavigationCommitPath.length > 0
                && root.samePanelPath(root.currentPath, root.pendingNavigationCommitPath)
    }

    function scrollRestorePending() {
        return root.pendingScrollRestorePath.length > 0 || root.pendingScrollRestoreEnabled
    }

    function canAutoPositionCurrentIndex() {
        return !root.resizeOptimized
                && !root.navigationCommitPending()
                && !root.scrollRestorePending()
    }
}
