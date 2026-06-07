import QtQuick

QtObject {
    id: root

    property var directoryModel: null

    function validIndex(index) {
        return root.directoryModel
                && index >= 0
                && index < root.directoryModel.count
    }

    function rangeOriginIndex(fallbackIndex) {
        if (!root.directoryModel || root.directoryModel.count <= 0) {
            return -1
        }

        const firstSelected = root.directoryModel.firstSelectedRow()
        if (firstSelected >= 0) {
            return firstSelected
        }

        if (root.validIndex(fallbackIndex)) {
            return fallbackIndex
        }

        return 0
    }

    function selectClickedRow(index, modifiers, fallbackIndex) {
        if (!root.directoryModel || !root.validIndex(index)) {
            return
        }

        if (modifiers & Qt.ShiftModifier) {
            const originIdx = root.rangeOriginIndex(fallbackIndex)
            if (originIdx >= 0) {
                root.directoryModel.extendOrTrimRange(originIdx, index)
            }
        } else if (modifiers & Qt.ControlModifier) {
            root.directoryModel.toggleSelected(index)
        } else {
            root.directoryModel.selectOnly(index)
        }
    }

    function selectRightClickedRow(index, path) {
        if (!root.directoryModel || !root.validIndex(index)) {
            return false
        }
        if (!root.directoryModel.selectedCount || root.directoryModel.selectedPaths().indexOf(path) < 0) {
            root.directoryModel.selectOnly(index)
            return true
        }
        return false
    }
}
