import QtQuick

QtObject {
    id: root

    property var activeViewProvider: null
    property var overlayBlockedProvider: null
    property var directoryModel: null
    property bool virtualRootMode: false
    property bool fileViewsModelEnabled: true
    property bool loadingDirectory: false
    property bool resizeOptimized: false
    property bool isRenaming: false
    property bool rubberBandPressed: false
    property bool rubberBandActive: false
    property bool pendingCurrentIndexInit: false
    property bool pendingScrollRestoreEnabled: false
    property string pendingScrollRestorePath: ""
    property bool reuseArmedByUserScroll: false
    property var reuseArmedView: null
    property bool reuseScrollbarPressed: false

    function activeView() {
        return root.activeViewProvider ? root.activeViewProvider() : null
    }

    function overlayBlocked() {
        return root.overlayBlockedProvider ? root.overlayBlockedProvider() : false
    }

    function userScrollReasonAllowed(reason) {
        return reason === "movement-start"
            || reason === "flick-start"
            || reason === "scrollbar-press"
    }

    function commonGateAllows(view) {
        if (!view || view !== root.activeView()) return false
        if (root.virtualRootMode || !root.fileViewsModelEnabled) return false
        if (root.loadingDirectory || root.resizeOptimized) return false
        if (root.isRenaming || root.rubberBandPressed || root.rubberBandActive) return false
        if (root.pendingCurrentIndexInit
                || root.pendingScrollRestoreEnabled
                || root.pendingScrollRestorePath.length > 0) return false
        if (root.overlayBlocked()) return false
        if (!root.directoryModel) return false
        if (root.directoryModel.loading || root.directoryModel.count <= 0) return false
        return view.count > 0
    }

    function canArm(view, reason) {
        return root.userScrollReasonAllowed(reason) && root.commonGateAllows(view)
    }

    function canEnable(view) {
        if (!root.reuseArmedByUserScroll || root.reuseArmedView !== view) return false
        if (!root.commonGateAllows(view)) return false
        const userScrollActive = view.moving || view.flicking || root.reuseScrollbarPressed
        return userScrollActive
    }
}
