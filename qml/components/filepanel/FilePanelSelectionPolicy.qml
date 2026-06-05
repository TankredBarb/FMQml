import QtQuick

QtObject {
    id: root

    property var directoryModel: null
    property string anchorPath: ""

    function rowPath(index) {
        if (!root.directoryModel || index < 0 || index >= root.directoryModel.count) {
            return ""
        }
        return root.directoryModel.pathAt(index)
    }

    function clearAnchor() {
        root.anchorPath = ""
    }

    function setAnchorFromIndex(index) {
        root.anchorPath = root.rowPath(index)
    }

    function anchorIndex(fallbackIndex) {
        if (!root.directoryModel) {
            return -1
        }

        if (root.anchorPath.length > 0) {
            const idx = root.directoryModel.indexOfPath(root.anchorPath)
            if (idx >= 0) {
                return idx
            }
        }

        return fallbackIndex >= 0 && fallbackIndex < root.directoryModel.count ? fallbackIndex : -1
    }

    function rowSelected(index) {
        const path = root.rowPath(index)
        if (path.length === 0 || !root.directoryModel) {
            return false
        }
        return root.directoryModel.selectedPaths().indexOf(path) >= 0
    }

    function selectClickedRow(index, modifiers, fallbackIndex) {
        if (!root.directoryModel) {
            return
        }

        if (modifiers & Qt.ShiftModifier) {
            const anchorIdx = root.anchorIndex(fallbackIndex)
            if (anchorIdx >= 0) {
                if (!root.rowSelected(index)
                        || !root.directoryModel.trimSelectedRangeTo(anchorIdx, index)) {
                    root.directoryModel.selectRange(anchorIdx, index)
                }
                if (root.anchorPath.length === 0) {
                    root.setAnchorFromIndex(anchorIdx)
                }
            } else {
                root.directoryModel.selectOnly(index)
                root.setAnchorFromIndex(index)
            }
        } else if (modifiers & Qt.ControlModifier) {
            root.directoryModel.toggleSelected(index)
            root.setAnchorFromIndex(index)
        } else {
            root.directoryModel.selectOnly(index)
            root.setAnchorFromIndex(index)
        }
    }

    function selectRightClickedRow(index, path) {
        if (!root.directoryModel) {
            return false
        }
        if (!root.directoryModel.selectedCount || root.directoryModel.selectedPaths().indexOf(path) < 0) {
            root.directoryModel.selectOnly(index)
            root.setAnchorFromIndex(index)
            return true
        }
        return false
    }
}
