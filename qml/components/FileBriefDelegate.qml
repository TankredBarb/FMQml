import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Item {
    id: root

    required property var    controller
    required property int    index
    required property string name
    required property string path
    required property bool   isDirectory
    required property bool   isSelected
    required property bool   isHidden
    required property bool   isImage
    required property bool   hasThumbnail
    required property string sizeText
    required property string suffix

    property bool currentItem:    false
    property bool panelActive:    true
    property bool scrolling:      false
    property bool isRenaming:     false
    property real visualOffsetX:  0

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: 28

    // ── Dynamic Scaling ────────────────────────────────────────────────────────
    readonly property int   baseHeight: 28
    readonly property real  scaleFactor: height / baseHeight
    readonly property int   iconSize: Math.max(16, Math.min(48, Math.round(16 * scaleFactor)))
    readonly property int   fontSize: Math.max(11, Math.min(16, Math.round(12 * (1.0 + (scaleFactor - 1.0) * 0.5))))

    // ── Opacity for hidden files ───────────────────────────────────────────────
    opacity: isHidden ? 0.55 : 1.0

    // ── Type colour for the dot indicator ─────────────────────────────────────
    readonly property color dotColor: {
        if (isDirectory) return "#3b82f6"
        const s = suffix.toLowerCase()
        if (["jpg","jpeg","png","gif","bmp","webp","avif","heic","tiff","svg"].indexOf(s) >= 0) return "#10b981"
        if (["mp4","mov","avi","mkv","webm","wmv","flv","m4v"].indexOf(s) >= 0) return "#8b5cf6"
        if (["mp3","flac","wav","aac","ogg","m4a","opus"].indexOf(s) >= 0) return "#f59e0b"
        if (["zip","rar","7z","tar","gz","bz2","xz","cab"].indexOf(s) >= 0) return "#f97316"
        if (s === "pdf") return "#ef4444"
        if (["md","txt","doc","docx","rtf","odt"].indexOf(s) >= 0) return "#64748b"
        return Theme.border
    }

    // ── Reset on reuse ─────────────────────────────────────────────────────────
    onPathChanged: {
        isRenaming = false
        visualOffsetX = 0
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    Component.onCompleted: {
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    GridView.onPooled: {
        isRenaming = false
        visualOffsetX = 0
        if (root.controller.hoveredPath === root.path) {
            root.controller.hoveredPath = ""
        }
    }

    GridView.onReused: {
        isRenaming = false
        visualOffsetX = 0
        opacity = Qt.binding(() => isHidden ? 0.55 : 1.0)
        Qt.callLater(() => {
            if (hover) {
                hover.enabled = false
                hover.enabled = true
            }
        })
    }

    function startRename() {
        root.isRenaming = true
    }

    // ── Background ─────────────────────────────────────────────────────────────
    Rectangle {
        id: bgRect
        anchors.fill: parent
        anchors.leftMargin:   3
        anchors.rightMargin:  3
        anchors.topMargin:    1
        anchors.bottomMargin: 0
        radius: 4

        color: isSelected
               ? (root.panelActive ? Theme.itemSelectedFill : Theme.itemSelectedFillInactive)
               : (root.currentItem
                  ? Theme.itemCurrentFill
                  : ((hover.hovered && !root.scrolling)
                     ? Qt.rgba(Theme.itemHoverFill.r, Theme.itemHoverFill.g, Theme.itemHoverFill.b,
                               Theme.itemHoverFill.a + (themeController.isDark ? 0.02 : 0.015))
                     : "transparent"))

        border.color: isSelected
                      ? (root.panelActive ? Theme.itemSelectedBorder : Theme.itemSelectedBorderInactive)
                      : (root.currentItem ? Theme.itemCurrentBorder : "transparent")
        border.width: isSelected || root.currentItem ? 1 : 0

        // Left accent bar
        Rectangle {
            anchors.left:   parent.left
            anchors.top:    parent.top
            anchors.bottom: parent.bottom
            width: (root.currentItem || isSelected) ? 3 : 0
            radius: 1.5
            color: isSelected ? Theme.accent : Theme.itemCurrentBorder
            visible: width > 0
        }

        Behavior on color { ColorAnimation { duration: 80 } }
    }

    // ── Hover / mouse ──────────────────────────────────────────────────────────
    HoverHandler {
        id: hover
        enabled: true
        onHoveredChanged: {
            if (root.scrolling) return
            if (hovered) {
                root.controller.hoveredPath = root.path
            } else if (root.controller.hoveredPath === root.path) {
                root.controller.hoveredPath = ""
            }
        }
    }

    onScrollingChanged: {
        if (!scrolling) {
            Qt.callLater(() => {
                if (hover) {
                    hover.enabled = false
                    hover.enabled = true
                    if (hover.hovered) {
                        root.controller.hoveredPath = root.path
                    }
                }
            })
        }
    }

    Connections {
        target: root.controller ? root.controller.directoryModel : null
        ignoreUnknownSignals: true
        function onLoadingChanged() {
            if (root.controller && root.controller.directoryModel && !root.controller.directoryModel.loading) {
                Qt.callLater(() => {
                    if (hover) {
                        hover.enabled = false
                        hover.enabled = true
                        if (hover.hovered) {
                            root.controller.hoveredPath = root.path
                        }
                    }
                })
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) root.rightClicked()
            else root.clicked(mouse)
        }
        onDoubleClicked: root.doubleClicked()
    }

    // ── Rename overlay ─────────────────────────────────────────────────────────
    Loader {
        id: renameLoader
        anchors.fill: parent
        anchors.leftMargin:  34
        anchors.rightMargin: 6
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        active:  root.isRenaming
        visible: root.isRenaming
        sourceComponent: TextField {
            id: renameInput
            text: root.name
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 12
            color: Theme.textPrimary
            selectByMouse: true
            leftPadding: 6
            rightPadding: 6
            
            opacity: 0
            scale: 0.96
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

            background: Rectangle {
                id: bgRect
                color: themeController.isDark ? Qt.rgba(18/255, 18/255, 24/255, 0.92) : Qt.rgba(255/255, 255/255, 255/255, 0.96)
                radius: 4
                border.color: renameInput.activeFocus ? Theme.accent : Theme.border
                border.width: renameInput.activeFocus ? 1.5 : 1
                
                Behavior on border.color { ColorAnimation { duration: 120 } }
                Behavior on border.width { NumberAnimation { duration: 120 } }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: renameInput.activeFocus 
                        ? (themeController.isDark ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)) 
                        : Theme.glassShadow
                    shadowBlur: renameInput.activeFocus ? 10 : 6
                    shadowVerticalOffset: renameInput.activeFocus ? 1 : 2
                    
                    Behavior on shadowColor { ColorAnimation { duration: 120 } }
                    Behavior on shadowBlur { NumberAnimation { duration: 120 } }
                }
            }
            onAccepted: {
                if (root.index >= 0) {
                    const idx  = root.index
                    const txt  = text
                    const ctrl = root.controller
                    Qt.callLater(function() {
                        if (ctrl.rename(idx, txt)) {
                            root.isRenaming = false
                        } else {
                            if (renameLoader.item) {
                                renameLoader.item.forceActiveFocus()
                                renameLoader.item.selectAll()
                            }
                        }
                    })
                }
            }
            
            Keys.onEscapePressed: (event) => {
                root.isRenaming = false
                event.accepted = true
            }

            onActiveFocusChanged: if (!activeFocus) root.isRenaming = false
            Component.onCompleted: {
                opacity = 1.0
                scale = 1.0
                forceActiveFocus()
                const lastDot = root.name.lastIndexOf(".")
                if (!root.isDirectory && lastDot > 0) select(0, lastDot)
                else selectAll()
            }
        }
    }

    // ── Content row ────────────────────────────────────────────────────────────
    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin:  8
        anchors.rightMargin: 6
        spacing: 5
        visible: !root.isRenaming
        transform: Translate { x: root.visualOffsetX }

        // Type dot
        Rectangle {
            width:  Math.max(4, Math.round(5 * (1.0 + (scaleFactor - 1.0) * 0.3)))
            height: width
            radius: width / 2
            color: root.dotColor
            Layout.alignment: Qt.AlignVCenter
            opacity: 0.85
        }

        // Icon or thumbnail
        Item {
            Layout.preferredWidth:  root.iconSize
            Layout.preferredHeight: root.iconSize
            Layout.alignment: Qt.AlignVCenter

            // System icon
            Image {
                id: iconImg
                anchors.fill: parent
                source: "image://icon/" + encodeURIComponent(root.path + (root.isDirectory ? "?directory=true" : ""))
                sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
                asynchronous: true
                cache: true
                smooth: true
                mipmap: false
                visible: !root.hasThumbnail || thumbImg.status !== Image.Ready
            }

            // Thumbnail
            Image {
                id: thumbImg
                anchors.fill: parent
                source: root.hasThumbnail ? ("image://thumbnail/" + encodeURIComponent(root.path)) : ""
                sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                visible: root.hasThumbnail && status === Image.Ready

                layer.enabled: visible
                layer.effect: null
            }

            // clip thumbnail corners
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                radius: Math.max(2, root.iconSize / 8)
                clip: true
                visible: thumbImg.visible
            }
        }

        // File name
        Label {
            Layout.fillWidth: true
            text: root.name
            color: Theme.textPrimary
            font.pixelSize: root.fontSize
            font.weight: (isSelected || root.currentItem) ? Font.Medium : Font.Normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        // Size badge (files only)
        Label {
            text: root.sizeText
            color: Theme.textSecondary
            font.pixelSize: Math.max(9, root.fontSize - 2)
            opacity: 0.65
            Layout.preferredWidth: Math.max(52, 52 * scaleFactor * 0.7)
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            visible: !root.isDirectory && root.sizeText !== ""
            elide: Text.ElideRight
        }
    }
}
