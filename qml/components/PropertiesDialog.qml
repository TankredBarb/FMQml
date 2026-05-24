import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "dialogs"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: 420
    padding: 0
    height: Math.min(mainLayout.implicitHeight, parent ? parent.height * 0.95 : 640)
    visible: propertiesController.visible
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: DialogShell {}

    component PropertyRow : DialogListRow {}

    component SectionCard : DialogSection {}

    readonly property bool multiMode: propertiesController.selectedCount > 1

    contentItem: ColumnLayout {
        id: mainLayout
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                root.close()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: root.multiMode
                ? "qrc:/qt/qml/FM/qml/assets/icons/select-all.svg"
                : (propertiesController.path !== "" ? "image://icon/" + encodeURIComponent(propertiesController.path) : "qrc:/qt/qml/FM/qml/assets/icons/document.svg")
            title: propertiesController.name
            subtitle: propertiesController.typeText
            closeText: "x"
            onCloseRequested: root.close()
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.panelBorder
            opacity: 0.4
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentColumn.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true

            ColumnLayout {
                id: contentColumn
                x: 16
                width: scrollView.availableWidth - 32
                spacing: 12
                
                Item { height: 4; Layout.fillWidth: true } // Top padding spacer

                // Overview Card
                SectionCard {
                    title: "OVERVIEW"
                    
                    PropertyRow {
                        label: root.multiMode ? "Parent" : "Location"
                        value: propertiesController.path
                        isLink: !root.multiMode
                    }

                    PropertyRow {
                        visible: root.multiMode
                        label: "Selection"
                        value: propertiesController.selectedCount + " items"
                    }

                    PropertyRow {
                        visible: root.multiMode && propertiesController.typeText.length > 0
                        label: "Type"
                        value: propertiesController.typeText
                    }

                    PropertyRow {
                        label: "Total Size"
                        value: propertiesController.sizeText + (propertiesController.isCalculating ? " (calculating)" : "")
                        emphasizeValue: true
                    }

                    PropertyRow {
                        visible: !root.multiMode && !!propertiesController.isDirectory
                        label: "Contents"
                        value: propertiesController.fileCount + " files, " + propertiesController.folderCount + " folders"
                    }
                }

                // File Details Card
                SectionCard {
                    title: "FILE DETAILS"
                    visible: !root.multiMode && propertiesController.extraProperties.length > 0

                    Repeater {
                        model: propertiesController.extraProperties
                        PropertyRow {
                            label: modelData.label
                            value: modelData.value
                            emphasizeValue: true
                        }
                    }
                }

                // Timestamps Card
                SectionCard {
                    title: "TIMESTAMPS"

                    PropertyRow {
                        label: root.multiMode ? "Oldest created" : "Created"
                        value: propertiesController.created
                    }

                    PropertyRow {
                        label: root.multiMode ? "Latest modified" : "Modified"
                        value: propertiesController.modified
                    }

                    PropertyRow {
                        label: root.multiMode ? "Latest accessed" : "Accessed"
                        value: propertiesController.accessed
                    }
                }

                // Permissions Card
                SectionCard {
                    title: "PERMISSIONS"
                    visible: !root.multiMode

                    Repeater {
                        model: [
                            { name: "Read", icon: "eye" },
                            { name: "Write", icon: "move" },
                            { name: "Execute", icon: "terminal" }
                        ]

                        PropertyRow {
                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/" + modelData.icon + ".svg"
                            label: modelData.name
                            value: ""
                        }
                    }
                }

                // Selected Items Card
                SectionCard {
                    title: "SELECTED ITEMS"
                    visible: root.multiMode

                    Repeater {
                        model: propertiesController.selectedPaths

                        PropertyRow {
                            iconSource: "image://icon/" + encodeURIComponent(modelData)
                            label: modelData.split(/[/\\]/).pop()
                            value: ""
                        }
                    }
                }

                Item { height: 4; Layout.fillWidth: true } // Bottom padding spacer
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Done"
                highlighted: true
                onClicked: root.close()
            }
        }
    }

    onClosed: propertiesController.visible = false
    onVisibleChanged: {
        if (!visible && propertiesController.visible) {
            propertiesController.visible = false
        }
    }
}
