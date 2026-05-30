import QtQuick

Item {
    id: root

    required property var controller
    required property var panel

    required property int index
    required property string name
    required property string path
    required property bool isDirectory
    required property bool isSelected
    required property bool isHidden
    required property bool isArchiveFile
    required property bool isIsoImageFile
    required property string sizeText
    required property string modifiedText
    required property string createdText
    required property string attributesText
    required property string suffix

    property bool currentItem: false
    property bool panelActive: true
    property bool scrolling: false
    property bool pendingRename: false
    property real visualOffsetX: 0
    readonly property bool lightweightRequested: root.panel && root.panel.lightweightDelegates
    readonly property bool resizeOptimized: root.lightweightRequested && !root.pendingRename
    readonly property bool isRenaming: fullLoader.item ? fullLoader.item.isRenaming : false

    signal clicked(var mouse)
    signal doubleClicked()
    signal rightClicked()
    signal emptySpaceRightClicked()

    implicitHeight: fullLoader.item ? fullLoader.item.implicitHeight : resizeSurface.implicitHeight

    function startRename() {
        if (fullLoader.item) {
            fullLoader.item.startRename()
        } else {
            root.pendingRename = true
        }
    }

    onResizeOptimizedChanged: {
        if (!root.resizeOptimized && root.pendingRename) {
            Qt.callLater(() => {
                if (fullLoader.item) {
                    root.pendingRename = false
                    fullLoader.item.startRename()
                }
            })
        }
    }

    Loader {
        id: fullLoader
        anchors.fill: parent
        active: !root.resizeOptimized
        visible: active
        sourceComponent: fullDelegateComponent
    }

    FileTableResizeDelegate {
        id: resizeSurface
        anchors.fill: parent
        visible: root.resizeOptimized
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
        sizeText: root.sizeText
        modifiedText: root.modifiedText
        suffix: root.suffix
        currentItem: root.currentItem
        panelActive: root.panelActive
        scrolling: true
        onClicked: (mouse) => root.clicked(mouse)
        onRightClicked: root.rightClicked()
        onEmptySpaceRightClicked: root.emptySpaceRightClicked()
        onDoubleClicked: root.doubleClicked()
    }

    Component {
        id: fullDelegateComponent

        FileTableDelegate {
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
            sizeText: root.sizeText
            modifiedText: root.modifiedText
            createdText: root.createdText
            attributesText: root.attributesText
            suffix: root.suffix
            currentItem: root.currentItem
            panelActive: root.panelActive
            scrolling: root.scrolling
            visualOffsetX: root.visualOffsetX
            onClicked: (mouse) => root.clicked(mouse)
            onRightClicked: root.rightClicked()
            onEmptySpaceRightClicked: root.emptySpaceRightClicked()
            onDoubleClicked: root.doubleClicked()
        }
    }
}
