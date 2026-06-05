import QtQuick

QtObject {
    id: root

    property int currentMode: 0
    property var listView: null
    property var gridView: null
    property var briefView: null

    function viewForMode(mode) {
        if (mode === 2) return root.briefView
        if (mode === 0) return root.listView
        return root.gridView
    }

    function activeView() {
        return root.viewForMode(root.currentMode)
    }

    function currentIndex() {
        const view = root.activeView()
        return view ? view.currentIndex : -1
    }

    function setCurrentIndex(index) {
        const view = root.activeView()
        if (!view) {
            return -1
        }
        const previousIndex = view.currentIndex
        view.currentIndex = index
        return previousIndex
    }

    function startCurrentItemRename() {
        const view = root.activeView()
        if (!view || !view.currentItem) {
            return false
        }
        view.currentItem.startRename()
        return true
    }

    function forceActiveFocus() {
        const view = root.activeView()
        if (!view) {
            return false
        }
        view.forceActiveFocus()
        return true
    }

    function motionActive(scrollbarPressed) {
        const view = root.activeView()
        return Boolean(view && (view.moving || view.flicking || scrollbarPressed))
    }
}
