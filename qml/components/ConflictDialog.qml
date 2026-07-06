import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.9, 520)
    padding: 20

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    property string sourcePath: ""
    property string destinationPath: ""
    property real sourceSize: 0
    property var sourceModified: new Date()
    property real destSize: 0
    property var destModified: new Date()
    property bool applyToAll: false
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true

    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }

    function fileNameFor(path) {
        if (!path) return ""
        const parts = String(path).split(/[/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : path
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: DialogShell {
        accentColor: Theme.warning
        shellBorderColor: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.34 : 0.24)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                workspaceController.operationQueue.resolveConflict(OperationQueue.Cancel, false)
                root.close()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, root.applyToAll)
                root.close()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
            iconTint: Theme.warning
            accentColor: Theme.warning
            title: "File Conflict"
            subtitle: "A file with this name already exists. How do you want to proceed?"
            showCloseButton: false
        }

        // CARDS CONTAINER
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            FileConflictCard {
                title: "Existing File"
                path: root.destinationPath
                size: root.destSize
                modified: root.destModified
                isDest: true
                Layout.fillWidth: true
            }

            FileConflictCard {
                title: "New File"
                path: root.sourcePath
                size: root.sourceSize
                modified: root.sourceModified
                isDest: false
                Layout.fillWidth: true
            }
        }

        // APPLY TO ALL
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            
            CheckBox {
                id: applyAllCheck
                text: "Apply to all remaining conflicts"
                checked: root.applyToAll
                onCheckedChanged: root.applyToAll = checked
                
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    radius: Theme.radiusSm
                    border.color: applyAllCheck.checked ? Theme.warning : Theme.panelBorder
                    border.width: applyAllCheck.checked ? 0 : 1
                    color: applyAllCheck.checked ? Theme.warning : "transparent"
                    
                    Image {
                        anchors.centerIn: parent
                        source: "../assets/icons/select-all.svg"
                        sourceSize: Qt.size(10, 10)
                        visible: applyAllCheck.checked
                        layer.enabled: true
                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" }
                    }
                }

                contentItem: Label {
                    text: applyAllCheck.text
                    font.pixelSize: Theme.fontSizeLabel
                    color: Theme.textPrimary
                    leftPadding: 26
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Replace"
                Layout.fillWidth: true
                highlighted: true
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Replace, root.applyToAll)
                    root.close()
                }
            }

            DialogActionButton {
                text: "Keep Both"
                Layout.fillWidth: true
                highlighted: false
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.KeepBoth, root.applyToAll)
                    root.close()
                }
            }

            DialogActionButton {
                text: "Cancel"
                Layout.preferredWidth: 100
                highlighted: false
                secondaryTextColor: Theme.danger
                onClicked: {
                    workspaceController.operationQueue.resolveConflict(OperationQueue.Cancel, false)
                    root.close()
                }
            }
        }
    }

    // INTERNAL COMPONENTS
    component FileConflictCard : SurfaceCard {
        property string title: ""
        property string path: ""
        property real size: 0
        property var modified
        property bool isDest: false

        implicitHeight: Math.max(72, cardLayout.implicitHeight + 20)
        surfaceColor: Theme.withAlpha(isDest ? Theme.danger : Theme.warning, themeController.isDark ? 0.07 : 0.04)
        strokeColor: Theme.withAlpha(isDest ? Theme.danger : Theme.warning, themeController.isDark ? 0.24 : 0.18)

        RowLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignTop
                radius: Theme.radiusSm
                color: Theme.withAlpha(isDest ? Theme.danger : Theme.warning, themeController.isDark ? 0.10 : 0.065)
                border.color: Theme.withAlpha(isDest ? Theme.danger : Theme.warning, themeController.isDark ? 0.26 : 0.18)
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    source: path !== ""
                            ? (root.useNativeIcons
                               ? "image://icon/" + encodeURIComponent(path)
                               : fileTypeIconResolver.iconForPath(path))
                            : ""
                    sourceSize: Qt.size(24, 24)
                    smooth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                RowLayout {
                    spacing: 8
                    Label {
                        text: title
                        font.pixelSize: Theme.fontSizeMicro
                        font.weight: Font.DemiBold
                        color: isDest ? Theme.danger : Theme.warning
                        elide: Text.ElideRight
                    }
                    Label {
                        text: root.formatSize(size)
                        font.pixelSize: Theme.fontSizeMicro
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                Label {
                    text: root.fileNameFor(path)
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label {
                    text: "Modified: " + Qt.formatDateTime(modified, "dd MMM yyyy, hh:mm")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}
