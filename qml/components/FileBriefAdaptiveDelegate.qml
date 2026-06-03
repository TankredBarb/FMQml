import QtQuick

Item {
    id: root

    required property var controller
    property var panel
    required property int index
    required property string name
    required property string path
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property bool isImage
    required property bool hasThumbnail
    required property string sizeText
    required property string suffix

    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false
    property bool resizeOptimized: false
    property bool pendingRename: false
    property string pendingRenamePath: ""
    property real visualOffsetX: 0
    readonly property bool lightweightActive: root.resizeOptimized && !root.pendingRename
    readonly property bool isRenaming: fullLoader.item ? fullLoader.item.isRenaming : false

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: fullLoader.item ? fullLoader.item.implicitHeight : resizeSurface.implicitHeight

    function resetTransientState() {
        opacity = 1.0
        visualOffsetX = 0
        pendingRename = false
        pendingRenamePath = ""
    }

    function startRename() {
        if (fullLoader.item) {
            fullLoader.item.startRename()
        } else {
            root.pendingRename = true
            root.pendingRenamePath = root.path
        }
    }

    function cancelRename() {
        root.pendingRename = false
        root.pendingRenamePath = ""
        if (fullLoader.item && fullLoader.item.cancelRename) {
            fullLoader.item.cancelRename()
        }
    }

    function pendingRenameStillValid(expectedPath) {
        return root.pendingRename
                && root.pendingRenamePath.length > 0
                && root.pendingRenamePath === expectedPath
                && root.path === expectedPath
                && root.panel
                && root.panel.isRenaming
                && root.panel.pendingInlineRenamePath.length > 0
                && root.panel.samePanelPath(root.panel.pendingInlineRenamePath, expectedPath)
    }

    function focusRenameEditor(selectText) {
        if (fullLoader.item && fullLoader.item.focusRenameEditor) {
            return fullLoader.item.focusRenameEditor(selectText)
        }
        return false
    }

    function renameEditorHasFocus() {
        return Boolean(fullLoader.item && fullLoader.item.renameEditorHasFocus && fullLoader.item.renameEditorHasFocus())
    }

    onPathChanged: resetTransientState()

    GridView.onPooled: resetTransientState()
    GridView.onReused: resetTransientState()

    onLightweightActiveChanged: {
        if (!root.lightweightActive && root.pendingRename) {
            const expectedPath = root.pendingRenamePath
            Qt.callLater(() => {
                if (!root.pendingRenameStillValid(expectedPath)) {
                    root.pendingRename = false
                    root.pendingRenamePath = ""
                    return
                }
                if (fullLoader.item) {
                    root.pendingRename = false
                    root.pendingRenamePath = ""
                    fullLoader.item.startRename()
                }
            })
        }
    }

    Loader {
        id: fullLoader
        anchors.fill: parent
        active: !root.lightweightActive
        visible: active
        sourceComponent: fullDelegateComponent
    }

    FileBriefResizeDelegate {
        id: resizeSurface
        anchors.fill: parent
        visible: root.lightweightActive
        controller: root.controller
        index: root.index
        name: root.name
        path: root.path
        isDirectory: root.isDirectory
        isSelected: root.isSelected
        isHidden: root.isHidden
        isArchiveFile: root.isArchiveFile
        isIsoImageFile: root.isIsoImageFile
        suffix: root.suffix
        currentItem: root.currentItem
        panelActive: root.panelActive
        onClicked: (mouse) => root.clicked(mouse)
        onRightClicked: root.rightClicked()
        onDoubleClicked: root.doubleClicked()
    }

    Component {
        id: fullDelegateComponent

        FileBriefDelegate {
            anchors.fill: parent
            controller: root.controller
            panel: root.panel
            index: root.index
            name: root.name
            path: root.path
            isDirectory: root.isDirectory
            isSelected: root.isSelected
            isHidden: root.isHidden
            isArchiveFile: root.isArchiveFile
            isIsoImageFile: root.isIsoImageFile
            isImage: root.isImage
            hasThumbnail: root.hasThumbnail
            sizeText: root.sizeText
            suffix: root.suffix
            currentItem: root.currentItem
            panelActive: root.panelActive
            scrolling: root.scrolling
            resizeOptimized: root.lightweightActive
            visualOffsetX: root.visualOffsetX
            onClicked: (mouse) => root.clicked(mouse)
            onRightClicked: root.rightClicked()
            onDoubleClicked: root.doubleClicked()
        }
    }
}
