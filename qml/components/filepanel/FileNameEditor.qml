import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"

Item {
    id: root
    clip: false
    visible: root.active
    enabled: root.active
    z: root.active ? 100 : 0

    property bool active: false
    property string name: ""
    property bool isDirectory: false
    property int index: -1
    property var controller
    property int fontPixelSize: 13
    property int leftMargin: 8
    property int rightMargin: 8
    property int topMargin: 4
    property int bottomMargin: 4
    property int editorHeight: 48
    property int minEditorWidth: 220
    property int maxEditorWidth: 520
    readonly property color renameSelectionColor: Theme.withAlpha(Theme.focusRing, themeController.isDark ? 0.38 : 0.24)
    readonly property color renameSelectedTextColor: Theme.textPrimary
    signal cancelRequested()
    signal commitSucceeded()
    signal commitFailed()
    signal focusLost()

    function trace(stage, detail) {
    }

    function defaultSelectionEnd() {
        const lastDot = root.name.lastIndexOf(".")
        return !root.isDirectory && lastDot > 0 ? lastDot : root.name.length
    }

    function forceEditorFocus(selectText) {
        if (!root.active || !renameLoader.item) {
            root.trace("forceEditorFocus-reject", "select=" + (selectText === true))
            return false
        }

        root.trace("forceEditorFocus-before", "select=" + (selectText === true))
        renameLoader.item.forceActiveFocus()
        if (selectText === true) {
            renameLoader.item.select(0, root.defaultSelectionEnd())
        }
        root.trace("forceEditorFocus-after", "activeFocus=" + renameLoader.item.activeFocus)
        return renameLoader.item.activeFocus
    }

    function editorHasFocus() {
        return Boolean(renameLoader.item && renameLoader.item.activeFocus)
    }

    Loader {
        id: renameLoader
        z: 100
        x: root.leftMargin
        y: Math.round((root.height - height) / 2)
        width: Math.min(Math.max(root.width - root.leftMargin - root.rightMargin, root.minEditorWidth), root.maxEditorWidth)
        height: Math.max(root.editorHeight, root.fontPixelSize + root.topMargin + root.bottomMargin + 18)
        active: root.active
        visible: root.active
        onActiveChanged: root.trace("loader-active-changed", "value=" + active)
        sourceComponent: TextField {
            id: renameInput
            text: root.name
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontPixelSize
            color: Theme.textPrimary
            selectByMouse: true
            leftPadding: 8
            rightPadding: 8
            topPadding: 6
            bottomPadding: 6
            selectionColor: root.renameSelectionColor
            selectedTextColor: root.renameSelectedTextColor
            clip: true
            property bool committing: false
            property bool canceling: false

            opacity: 0
            scale: 0.96
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

            background: Rectangle {
                color: Theme.panelSurfaceStrong
                radius: Theme.radiusSm
                border.color: renameInput.activeFocus ? Theme.focusRing : Theme.panelBorder
                border.width: renameInput.activeFocus ? 1.5 : 1

                Behavior on border.color { ColorAnimation { duration: 120 } }
                Behavior on border.width { NumberAnimation { duration: 120 } }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: renameInput.activeFocus
                        ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.35 : 0.2)
                        : Theme.glassShadow
                    shadowBlur: renameInput.activeFocus ? 12 : 8
                    shadowVerticalOffset: renameInput.activeFocus ? 1 : 2

                    Behavior on shadowColor { ColorAnimation { duration: 120 } }
                    Behavior on shadowBlur { NumberAnimation { duration: 120 } }
                }
            }

            function commitRename() {
                if (root.index >= 0 && root.controller) {
                    const idx = root.index
                    const txt = text
                    const ctrl = root.controller
                    committing = true
                    Qt.callLater(function() {
                        if (ctrl.rename(idx, txt)) {
                            root.commitSucceeded()
                        } else {
                            committing = false
                            if (renameLoader.item) {
                                renameLoader.item.forceActiveFocus()
                                renameLoader.item.selectAll()
                            }
                            root.commitFailed()
                        }
                    })
                }
            }

            onAccepted: commitRename()

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
                    renameInput.selectAll()
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_F2) {
                    if (renameInput.selectionStart === 0 && renameInput.selectionEnd === root.defaultSelectionEnd()) {
                        renameInput.selectAll()
                    } else {
                        renameInput.select(0, root.defaultSelectionEnd())
                    }
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                        || event.key === Qt.Key_Home || event.key === Qt.Key_End
                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                    event.accepted = true
                }
            }

            Keys.onEscapePressed: (event) => {
                canceling = true
                root.trace("escape-cancel")
                root.cancelRequested()
                event.accepted = true
            }

            onActiveFocusChanged: {
                root.trace("textField-activeFocus-changed", "value=" + activeFocus)
                if (!activeFocus && root.active && !committing && !canceling) {
                    root.focusLost()
                }
            }

            Component.onCompleted: {
                root.trace("textField-completed-before-focus")
                opacity = 1.0
                scale = 1.0
                forceActiveFocus()
                select(0, root.defaultSelectionEnd())
                root.trace("textField-completed-after-focus", "activeFocus=" + activeFocus)
            }
        }
    }
}
