import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"

Item {
    id: root
    clip: false

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
    signal cancelRequested()
    signal commitSucceeded()
    signal commitFailed()

    Loader {
        id: renameLoader
        z: 100
        x: root.leftMargin
        y: Math.round((root.height - height) / 2)
        width: Math.min(Math.max(root.width - root.leftMargin - root.rightMargin, root.minEditorWidth), root.maxEditorWidth)
        height: Math.max(root.editorHeight, root.fontPixelSize + root.topMargin + root.bottomMargin + 18)
        active: root.active
        visible: root.active
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
            selectionColor: Theme.accent
            selectedTextColor: "white"

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

            onAccepted: {
                if (root.index >= 0 && root.controller) {
                    const idx = root.index
                    const txt = text
                    const ctrl = root.controller
                    Qt.callLater(function() {
                        if (ctrl.rename(idx, txt)) {
                            root.commitSucceeded()
                        } else {
                            if (renameLoader.item) {
                                renameLoader.item.forceActiveFocus()
                                renameLoader.item.selectAll()
                            }
                            root.commitFailed()
                        }
                    })
                }
            }

            Keys.onEscapePressed: (event) => {
                root.cancelRequested()
                event.accepted = true
            }

            onActiveFocusChanged: if (!activeFocus) root.cancelRequested()

            Component.onCompleted: {
                opacity = 1.0
                scale = 1.0
                forceActiveFocus()
                let lastDot = root.name.lastIndexOf(".")
                if (!root.isDirectory && lastDot > 0) {
                    select(0, lastDot)
                } else {
                    selectAll()
                }
            }
        }
    }
}
