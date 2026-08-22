import "../common"
import "../../style"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: folderDelegate

    required property var sidebar
    required property var workspace
    required property string path
    required property string name
    required property string folderIcon
    required property TreeView treeView
    required property int row
    required property bool isTreeNode
    required property bool expanded
    required property bool hasChildren
    required property bool loading
    required property int depth
    readonly property bool isActive: sidebar.pathsEqual(path, (workspace.activePanel === 0 ? workspace.leftPanel.currentPath : workspace.rightPanel.currentPath))
    readonly property bool isCurrent: {
        if (!treeView.activeFocus)
            return false;

        let model = treeView.selectionModel;
        if (!model)
            return false;

        let cur = model.currentIndex;
        if (cur === undefined || cur === null)
            return false;

        return treeView.rowAtIndex(cur) === row;
    }
    readonly property real baseIndent: 14
    readonly property real indentStep: 20
    readonly property real indicatorSlot: 18
    readonly property real iconSize: 20
    readonly property real contentSpacing: 10
    readonly property real contentRightMargin: 12
    readonly property real minimumNameWidth: 48
    readonly property real maximumVisualIndent: Math.max(0,
        width - baseIndent - indicatorSlot - 8 - iconSize
        - contentSpacing - contentRightMargin - minimumNameWidth)
    readonly property real visualIndent: isTreeNode
        ? Math.min(depth * indentStep, maximumVisualIndent) : 0
    readonly property var panel: workspace.activePanel === 0 ? workspace.leftPanel : workspace.rightPanel

    width: treeView.width
    implicitWidth: treeView.width > 0 ? treeView.width : 1
    implicitHeight: 40
    height: implicitHeight
    padding: 0
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        radius: Theme.radiusMd
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        color: sidebar.sidebarStateFill(folderDelegate.isActive, folderDelegate.isCurrent, rowMouse.containsMouse, rowMouse.down)
        border.color: "transparent"
        border.width: 0

        Behavior on color {
            enabled: !sidebar.effectsReduced

            ColorAnimation {
                duration: Theme.motionFast
            }

        }

    }

    contentItem: Item {
        anchors.fill: parent

        MouseArea {
            id: rowMouse

            anchors.fill: parent
            hoverEnabled: !sidebar.effectsReduced
            cursorShape: Qt.PointingHandCursor
            z: 1
            onPressed: sidebar.prepareNavigation("sidebar-tree-press")
            onClicked: function(mouse) {
                sidebar.openPathInActivePanel(path);
                sidebar.trapTabNavigation = false;
                treeView.forceActiveFocus();
                mouse.accepted = true;
            }
        }

        Rectangle {
            id: depthGuide

            visible: folderDelegate.isTreeNode && folderDelegate.depth > 0 && !sidebar.effectsReduced
            x: folderDelegate.baseIndent + folderDelegate.visualIndent - 8
            y: 4
            width: 1
            height: parent.height - 8
            color: Theme.panelStrokeSubtle
            opacity: folderDelegate.isActive || folderDelegate.isCurrent ? 0.72 : (rowMouse.containsMouse ? 0.58 : 0.42)
        }

        Item {
            id: disclosureArea

            z: 2
            x: folderDelegate.baseIndent + folderDelegate.visualIndent
            y: 0
            width: folderDelegate.indicatorSlot
            height: parent.height
            visible: folderDelegate.isTreeNode && folderDelegate.hasChildren
            opacity: folderDelegate.isActive || folderDelegate.isCurrent ? 1 : (rowMouse.containsMouse ? 0.96 : 0.78)

            RecolorSvgIcon {
                anchors.centerIn: parent
                width: 10
                height: 10
                visible: !sidebar.effectsReduced && !folderDelegate.loading
                rotation: folderDelegate.expanded ? 90 : 0
                opacity: folderDelegate.hasChildren ? 1 : 0.35
                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/chevron-right.svg"
                recolorColor: folderDelegate.isActive || folderDelegate.isCurrent || rowMouse.containsMouse
                              ? Theme.textPrimary
                              : Theme.textSecondary
                sourceSize: Qt.size(10, 10)

                Behavior on rotation {
                    enabled: !sidebar.effectsReduced

                    NumberAnimation {
                        duration: Theme.motionFast
                        easing.type: Easing.OutQuad
                    }

                }

                Behavior on opacity {
                    enabled: !sidebar.effectsReduced

                    NumberAnimation {
                        duration: Theme.motionFast
                    }

                }

            }

            Text {
                anchors.centerIn: parent
                visible: sidebar.effectsReduced && !folderDelegate.loading
                text: folderDelegate.expanded ? ">" : ">"
                rotation: folderDelegate.expanded ? 90 : 0
                color: folderDelegate.isActive || folderDelegate.isCurrent ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                font.bold: true
                opacity: folderDelegate.hasChildren ? 0.85 : 0.35
            }

            BusyIndicator {
                anchors.centerIn: parent
                width: 16
                height: 16
                running: folderDelegate.loading
                visible: folderDelegate.loading
            }

            MouseArea {
                anchors.fill: parent
                onClicked: function(mouse) {
                    folderDelegate.treeView.toggleExpanded(folderDelegate.row);
                    sidebar.trapTabNavigation = false;
                    treeView.forceActiveFocus();
                    mouse.accepted = true;
                }
            }

        }

        Item {
            id: rowArea

            z: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: folderDelegate.baseIndent + folderDelegate.visualIndent + folderDelegate.indicatorSlot + 8
            anchors.rightMargin: folderDelegate.contentRightMargin

            RowLayout {
                anchors.fill: parent
                spacing: folderDelegate.contentSpacing

                RecolorSvgIcon {
                    Layout.preferredWidth: folderDelegate.iconSize
                    Layout.preferredHeight: folderDelegate.iconSize
                    sourcePath: sidebar.resolvedIconSourceFor(folderIcon)
                    recolorColor: sidebar.iconToneFor(folderIcon, folderDelegate.isActive || folderDelegate.isCurrent, rowMouse.containsMouse)
                    cacheKey: "sidebar"
                    sourceSize: Qt.size(folderDelegate.iconSize * 2, folderDelegate.iconSize * 2)
                    asynchronous: true
                    cache: true
                    opacity: folderDelegate.isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? 1 : 0.84
                }

                Label {
                    text: name || ""
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.letterSpacing: 0
                    font.weight: isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? Font.Medium : Font.Normal
                    color: TextColors.sidebarText
                    opacity: isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? 1 : 0.92
                    elide: Text.ElideRight
                }

            }

        }

    }

}
