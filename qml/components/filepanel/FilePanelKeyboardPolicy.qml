import QtQuick

QtObject {
    id: root

    property var panel: null
    property var directoryModel: null

    function isSelectionSuppressingNavigationKey(key) {
        return key === Qt.Key_Up || key === Qt.Key_Down
                || key === Qt.Key_Left || key === Qt.Key_Right
                || key === Qt.Key_PageUp || key === Qt.Key_PageDown
                || key === Qt.Key_Home || key === Qt.Key_End
    }

    function isInitialCurrentIndexKey(key) {
        return key === Qt.Key_Up || key === Qt.Key_Down
                || key === Qt.Key_Left || key === Qt.Key_Right
    }

    function verticalStepSize(view) {
        if (view && view.cellWidth && view.cellWidth > 0) {
            return Math.max(1, Math.floor(view.width / view.cellWidth))
        }
        return 1
    }

    function moveOneVerticalStep(view, direction) {
        if (!view || view.currentIndex < 0 || view.count <= 0) {
            return false
        }

        const step = root.verticalStepSize(view)
        const nextIndex = Math.max(0, Math.min(view.count - 1,
                                               view.currentIndex + direction * step))
        if (nextIndex === view.currentIndex) {
            return false
        }
        view.currentIndex = nextIndex
        return true
    }

    function handleViewKeyPressed(view, event) {
        if (!root.panel || !view || !event) {
            return
        }

        if (root.panel.panelKeysBlockedByOverlay()) {
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier)) {
            if (root.directoryModel && view.currentIndex >= 0 && view.currentIndex < view.count) {
                root.directoryModel.toggleSelected(view.currentIndex)
                event.accepted = true
            }
            return
        }

        if (root.isSelectionSuppressingNavigationKey(event.key)
                && root.panel.markKeyboardNavigationActivity) {
            root.panel.markKeyboardNavigationActivity()
        }

        if ((event.modifiers & Qt.ControlModifier)
                && root.isSelectionSuppressingNavigationKey(event.key)) {
            root.panel.disableSelectionOnCurrentIndexChanged = true
            Qt.callLater(function() {
                root.panel.disableSelectionOnCurrentIndexChanged = false
            })
        }

        if (view.currentIndex === -1 && view.count > 0
                && root.isInitialCurrentIndexKey(event.key)) {
            view.currentIndex = (event.key === Qt.Key_Up) ? view.count - 1 : 0
            event.accepted = true
            return
        }

        if (event.modifiers === Qt.NoModifier
                && (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown)) {
            root.moveOneVerticalStep(view, event.key === Qt.Key_PageUp ? -1 : 1)
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            if (view.currentIndex >= 0 && view.currentItem && !view.currentItem.isRenaming) {
                root.panel.openItem(view.currentIndex)
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            root.panel.goUp()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            root.panel.cancelRubberBand(true)
            root.panel.workspaceController.focusActivePanel()
            event.accepted = true
        }
    }
}
