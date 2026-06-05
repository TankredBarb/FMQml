import QtQuick

QtObject {
    id: root

    property var directoryModel: null
    property string currentItemPath: ""
    property var listView: null
    property real listRowHeight: 1
    property var itemRectProvider: null

    function itemRect(view, index) {
        return root.itemRectProvider ? root.itemRectProvider(view, index) : null
    }

    function pathAtModelIndex(index) {
        if (!root.directoryModel) {
            return ""
        }
        if (index < 0 || index >= root.directoryModel.count) {
            return ""
        }
        return root.directoryModel.pathAt(index)
    }

    function itemOffsetForIndex(view, index) {
        if (!view || !root.directoryModel || index < 0 || index >= root.directoryModel.count) {
            return undefined
        }
        const rect = root.itemRect(view, index)
        return rect ? rect.y - view.contentY : undefined
    }

    function itemVisibleInView(view, index) {
        if (!view || !root.directoryModel || index < 0 || index >= root.directoryModel.count) {
            return false
        }
        const rect = root.itemRect(view, index)
        if (!rect) {
            return false
        }
        return rect.y + rect.height > view.contentY
                && rect.y < view.contentY + view.height
    }

    function anchorForIndex(view, index, setsCurrent, source) {
        const path = root.pathAtModelIndex(index)
        if (!path) {
            return null
        }
        const offsetY = root.itemOffsetForIndex(view, index)
        return {
            path: path,
            offsetY: offsetY !== undefined ? offsetY : 0,
            setsCurrent: setsCurrent,
            source: source
        }
    }

    function selectedAnchorForView(view) {
        const model = root.directoryModel
        if (!view || !model || model.selectedCount <= 0) {
            return null
        }

        const selectedPaths = model.selectedPaths()
        const selected = []
        for (let i = 0; i < selectedPaths.length; ++i) {
            const idx = model.indexOfPath(selectedPaths[i])
            if (idx >= 0) {
                selected.push({ path: selectedPaths[i], index: idx })
            }
        }
        if (selected.length === 0) {
            return null
        }
        selected.sort((a, b) => a.index - b.index)

        if (selected.length === 1) {
            return root.anchorForIndex(view, selected[0].index, true, "single-selection")
        }

        let nearestVisible = null
        let nearestDistance = Number.MAX_VALUE
        for (let j = 0; j < selected.length; ++j) {
            const idx = selected[j].index
            if (!root.itemVisibleInView(view, idx)) {
                continue
            }
            const offsetY = root.itemOffsetForIndex(view, idx)
            const distance = Math.abs(offsetY === undefined ? 0 : offsetY)
            if (distance < nearestDistance) {
                nearestDistance = distance
                nearestVisible = root.anchorForIndex(view, idx, true, "visible-selection")
            }
        }

        return nearestVisible || root.anchorForIndex(view, selected[0].index, true, "first-selection")
    }

    function firstVisibleAnchorForView(view) {
        if (!view || view.count <= 0) {
            return null
        }

        let index = 0
        if (view === root.listView) {
            index = Math.floor(view.contentY / Math.max(1, root.listRowHeight))
        } else {
            const columns = Math.max(1, Math.floor(view.width / Math.max(1, view.cellWidth)))
            const row = Math.floor(view.contentY / Math.max(1, view.cellHeight))
            index = row * columns
        }
        index = Math.max(0, Math.min(view.count - 1, index))
        return root.anchorForIndex(view, index, false, "first-visible")
    }

    function viewAnchor(view) {
        if (!view || !root.directoryModel) {
            return null
        }

        if (view.currentIndex >= 0 && view.currentIndex < root.directoryModel.count) {
            return root.anchorForIndex(view, view.currentIndex, true, "current")
        }

        if (root.currentItemPath.length > 0) {
            const currentPathIndex = root.directoryModel.indexOfPath(root.currentItemPath)
            if (currentPathIndex >= 0) {
                return root.anchorForIndex(view, currentPathIndex, true, "current-path")
            }
        }

        const selectedAnchor = root.selectedAnchorForView(view)
        if (selectedAnchor) {
            return selectedAnchor
        }

        return root.firstVisibleAnchorForView(view)
    }
}
