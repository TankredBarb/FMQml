import QtQuick

QtObject {
    id: root

    property var directoryModel: null

    function validIndex(index) {
        return root.directoryModel
                && index >= 0
                && index < root.directoryModel.count
    }

    function selectClickedRow(index, modifiers) {
        if (!root.directoryModel || !root.validIndex(index)) {
            return
        }

        if (modifiers & Qt.ShiftModifier) {
            root.directoryModel.extendOrTrimRange(0, index)
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
