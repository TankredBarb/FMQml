import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

ToolBar {
    id: root
    
    property bool pathEditing: false
    property string pathEditError: ""
    readonly property bool textEditingActive: pathEditing || searchField.activeFocus
    
    height: 64
    
    background: Rectangle {
        color: Theme.surface
        
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
            opacity: 0.5
        }
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1,1,1, themeController.isDark ? 0.03 : 0.05) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    readonly property var activeController: workspaceController.activePanel === 0
                                            ? workspaceController.leftPanel
                                            : workspaceController.rightPanel

    function focusPath() {
        root.pathEditError = ""
        pathEditor.text = root.activeController.currentPath
        root.pathEditing = true
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function acceptPathEdit() {
        const path = pathEditor.text.trim()
        if (path.length > 0) {
            if (root.activeController.openPath(path)) {
                root.pathEditError = ""
                root.pathEditing = false
                workspaceController.focusActivePanel()
                return
            }
            root.pathEditError = "Path not found"
        } else {
            root.pathEditError = "Enter a valid path"
        }
        pathEditor.forceActiveFocus()
        pathEditor.selectAll()
    }

    function cancelPathEdit() {
        root.pathEditing = false
        root.pathEditError = ""
        workspaceController.focusActivePanel()
    }

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    // Modern Button Component
    component IconButton: ToolButton {
        id: btn
        property string iconSource
        property bool isHighlighted: false
        property int iconSize: 20 // Slightly larger icons
        
        implicitWidth: 40
        implicitHeight: 40
        
        background: Rectangle {
            radius: 10
            color: btn.pressed ? Theme.surfaceActive : (btn.hovered ? Theme.surfaceHover : "transparent")
            border.color: btn.isHighlighted ? Theme.accent : "transparent"
            border.width: 1
            Behavior on color { ColorAnimation { duration: 150 } }
        }
        
        contentItem: Image {
            source: btn.iconSource
            sourceSize: Qt.size(btn.iconSize, btn.iconSize)
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
            // Increased opacity for better visibility in dark theme
            opacity: btn.enabled ? (btn.isHighlighted ? 1.0 : 0.95) : 0.3
            // In dark theme, we might want to ensure icons are bright enough
            layer.enabled: themeController.isDark
            layer.effect: MultiEffect {
                brightness: btn.enabled ? 0.1 : 0.0
                contrast: 0.1
            }
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 4

        // --- LEFT: Navigation & Core ---
        RowLayout {
            spacing: 2
            IconButton {
                iconSource: "../assets/icons/arrow-left.svg"
                enabled: root.activeController.canGoBack
                onClicked: root.activeController.goBack()
                ToolTip.visible: hovered
                ToolTip.text: "Back (Alt+Left)"
            }
            IconButton {
                iconSource: "../assets/icons/arrow-right.svg"
                enabled: root.activeController.canGoForward
                onClicked: root.activeController.goForward()
                ToolTip.visible: hovered
                ToolTip.text: "Forward (Alt+Right)"
            }
            IconButton {
                iconSource: "../assets/icons/arrow-up.svg"
                onClicked: root.activeController.goUp()
                ToolTip.visible: hovered
                ToolTip.text: "Up (Alt+Up)"
            }
            
            Rectangle { width: 1; height: 24; color: Theme.border; opacity: 0.4; Layout.leftMargin: 4; Layout.rightMargin: 4 }
            
            IconButton {
                iconSource: root.activeController.viewMode === 0 ? "../assets/icons/grid.svg" : "../assets/icons/list.svg"
                onClicked: root.activeController.viewMode = (root.activeController.viewMode === 0 ? 1 : 0)
                ToolTip.visible: hovered
                ToolTip.text: root.activeController.viewMode === 0 ? "Switch to Grid" : "Switch to List"
            }
            IconButton {
                iconSource: root.activeController.directoryModel.showHidden ? "../assets/icons/eye-off.svg" : "../assets/icons/eye.svg"
                onClicked: root.activeController.directoryModel.showHidden = !root.activeController.directoryModel.showHidden
                ToolTip.visible: hovered
                ToolTip.text: root.activeController.directoryModel.showHidden ? "Hide Hidden Files" : "Show Hidden Files"
            }
            IconButton {
                iconSource: "../assets/icons/refresh.svg"
                onClicked: root.activeController.refresh()
                ToolTip.visible: hovered
                ToolTip.text: "Refresh (F5)"
            }
        }

        // --- CENTER: Path Bar Island (Expanded) ---
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            
            Rectangle {
                id: pathIsland
                anchors.centerIn: parent
                // Increased width for path bar
                width: Math.min(parent.width - 20, 800)
                height: 40
                radius: 12
                color: themeController.isDark ? Qt.rgba(0,0,0,0.25) : Qt.rgba(0,0,0,0.05)
                border.color: root.pathEditing 
                              ? (root.pathEditError ? Theme.danger : Theme.accent) 
                              : Theme.border
                border.width: root.pathEditing ? 2 : 1
                
                Behavior on border.color { ColorAnimation { duration: 200 } }

                PathBar {
                    anchors.fill: parent
                    anchors.margins: 1
                    controller: root.activeController
                    visible: !root.pathEditing
                }

                TextField {
                    id: pathEditor
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    visible: root.pathEditing
                    text: root.activeController.currentPath
                    placeholderText: "Type path..."
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    verticalAlignment: TextInput.AlignVCenter
                    background: null
                    selectByMouse: true
                    
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.acceptPathEdit()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            root.cancelPathEdit()
                            event.accepted = true
                        }
                    }
                }

                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.pathEditError
                    visible: root.pathEditError.length > 0 && root.pathEditing
                    color: Theme.danger
                    font.pixelSize: 11
                    font.bold: true
                    
                    background: Rectangle {
                        color: Theme.surface
                        radius: 4
                        opacity: 0.9
                    }
                    padding: 2
                    leftPadding: 6
                    rightPadding: 6
                }
            }
        }

        // --- RIGHT: Tools & Selection Actions ---
        RowLayout {
            spacing: 2

            // Selection-specific actions (Copy/Move to opposite)
            IconButton {
                iconSource: "../assets/icons/copy.svg"
                enabled: workspaceController.splitEnabled 
                         && root.activeController.directoryModel.selectedCount > 0
                         && !workspaceController.operationQueue.busy
                onClicked: workspaceController.copyActiveSelectionToOpposite()
                visible: workspaceController.splitEnabled
                isHighlighted: enabled && hovered
                ToolTip.visible: hovered
                ToolTip.text: "Copy to other panel"
            }
            IconButton {
                iconSource: "../assets/icons/move.svg"
                enabled: workspaceController.splitEnabled 
                         && root.activeController.directoryModel.selectedCount > 0
                         && !workspaceController.operationQueue.busy
                onClicked: workspaceController.moveActiveSelectionToOpposite()
                visible: workspaceController.splitEnabled
                isHighlighted: enabled && hovered
                ToolTip.visible: hovered
                ToolTip.text: "Move to other panel"
            }

            Rectangle { 
                width: 1; height: 24; color: Theme.border; opacity: 0.4; 
                Layout.leftMargin: 4; Layout.rightMargin: 4;
                visible: workspaceController.splitEnabled
            }

            IconButton {
                iconSource: "../assets/icons/folder-plus.svg"
                onClicked: root.activeController.createFolder("New Folder")
                ToolTip.visible: hovered
                ToolTip.text: "Create Folder"
            }

            IconButton {
                id: splitBtn
                iconSource: "../assets/icons/columns-2.svg"
                isHighlighted: workspaceController.splitEnabled
                onClicked: workspaceController.toggleSplit()
                ToolTip.visible: hovered
                ToolTip.text: "Toggle Split View (F3)"
            }

            IconButton {
                iconSource: themeController.isDark ? "../assets/icons/sun.svg" : "../assets/icons/moon.svg"
                onClicked: themeController.mode = themeController.isDark ? 0 : 1
                ToolTip.visible: hovered
                ToolTip.text: "Toggle Theme"
            }

            IconButton {
                iconSource: "../assets/icons/info.svg"
                onClicked: helpDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: "Help (F1)"
            }

            // Search Field
            Rectangle {
                Layout.preferredWidth: searchField.activeFocus ? 200 : 140
                Layout.preferredHeight: 36
                radius: 10
                color: themeController.isDark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.05)
                border.color: searchField.activeFocus ? Theme.accent : "transparent"
                border.width: 1
                
                Behavior on Layout.preferredWidth { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    source: "../assets/icons/search.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: 0.6
                }

                TextField {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 34
                    anchors.rightMargin: 8
                    placeholderText: "Search..."
                    text: root.activeController.directoryModel.filterText
                    onTextChanged: root.activeController.directoryModel.filterText = text
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    font.pixelSize: 13
                    background: null
                    verticalAlignment: TextInput.AlignVCenter
                    
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Escape) {
                            text = ""
                            root.activeController.directoryModel.filterText = ""
                            workspaceController.focusActivePanel()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            workspaceController.focusActivePanel()
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
