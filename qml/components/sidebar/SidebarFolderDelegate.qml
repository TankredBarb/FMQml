import "../common"
import "../../style"
import QtQml.Models
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: folderDelegate

    required property var sidebar
    required property var workspace
    required property var theme
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
            x: folderDelegate.baseIndent + (folderDelegate.depth * folderDelegate.indentStep) - 8
            y: 4
            width: 1
            height: parent.height - 8
            color: Theme.panelStrokeSubtle
            opacity: folderDelegate.isActive || folderDelegate.isCurrent ? 0.72 : (rowMouse.containsMouse ? 0.58 : 0.42)
        }

        Item {
            id: disclosureArea

            z: 2
            x: folderDelegate.baseIndent + (folderDelegate.isTreeNode ? folderDelegate.depth * folderDelegate.indentStep : 0)
            y: 0
            width: folderDelegate.indicatorSlot
            height: parent.height
            visible: folderDelegate.isTreeNode && folderDelegate.hasChildren
            opacity: folderDelegate.isActive || folderDelegate.isCurrent ? 1 : (rowMouse.containsMouse ? 0.96 : 0.78)

            Canvas {
                id: chevronCanvas

                anchors.centerIn: parent
                width: 12
                height: 12
                visible: !sidebar.effectsReduced && !folderDelegate.loading
                rotation: folderDelegate.expanded ? 90 : 0
                opacity: folderDelegate.hasChildren ? 1 : 0.35
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = folderDelegate.isActive || folderDelegate.isCurrent || rowMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary;
                    ctx.lineWidth = 1.25;
                    ctx.lineCap = "round";
                    ctx.lineJoin = "round";
                    ctx.beginPath();
                    ctx.moveTo(4, 2.5);
                    ctx.lineTo(7.5, 6);
                    ctx.lineTo(4, 9.5);
                    ctx.stroke();
                }

                Connections {
                    function onIsActiveChanged() {
                        if (!sidebar.effectsReduced)
                            chevronCanvas.requestPaint();

                    }

                    target: folderDelegate
                }

                Connections {
                    function onContainsMouseChanged() {
                        if (!sidebar.effectsReduced)
                            chevronCanvas.requestPaint();

                    }

                    target: rowMouse
                }

                Connections {
                    function onThemeChanged() {
                        if (!sidebar.effectsReduced)
                            chevronCanvas.requestPaint();

                    }

                    target: theme
                }

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
            anchors.leftMargin: folderDelegate.baseIndent + (folderDelegate.isTreeNode ? folderDelegate.depth * folderDelegate.indentStep : 0) + folderDelegate.indicatorSlot + 8
            anchors.rightMargin: 12

            RowLayout {
                anchors.fill: parent
                spacing: 10

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
