import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../framework"
import "../dialogs"

ScrollView {
                    id: page
                    required property int currentIndex
    required property bool multiMode
    required property var controller
    required property var capabilityRows
    required property var attributeRows
    required property string capabilitiesTitle
    required property string attributesTitle
    required property var fileNameForPath
    required property var parentPathForPath
    required property var displayPath
    required property var capabilityDescription
    required property var tabContentY
    property bool useNativeIcons: true
    readonly property real contentImplicitHeight: contentLayout.implicitHeight

    anchors.fill: parent
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical: FmScrollBar {
                        id: pageScrollBar
                        parent: page.contentItem
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        policy: ScrollBar.AsNeeded
                    }
                    clip: true
                    enabled: page.currentIndex === 2

                    opacity: page.currentIndex === 2 ? 1.0 : 0.0
                    z: page.currentIndex === 2 ? 1 : 0
                    transform: Translate {
                        x: page.currentIndex === 2 ? 0 : (2 < page.currentIndex ? -400 : 400)
                        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }

                    ColumnLayout {
                        id: contentLayout
                        x: 16
                        y: page.tabContentY(page, contentLayout)
                        width: pageScrollBar.scrollNeeded
                               ? Math.max(0, pageScrollBar.x - x - 6)
                               : page.availableWidth - 32
                        spacing: 12

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                        DialogSection {
                            title: "SELECTED ITEMS"
                            visible: page.multiMode

                            ListView {
                                id: selectedPathsList
                                visible: page.multiMode
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(320, Math.max(112, page.controller.selectedCount * 44))
                                clip: true
                                model: page.controller.selectedPaths
                                spacing: 4
                                boundsBehavior: Flickable.StopAtBounds
                                cacheBuffer: Math.max(0, height * 2)
                                reuseItems: true
                                delegate: SelectedPathRow {
                                    required property string modelData
                                    readonly property string pathValue: modelData
                                    width: selectedPathsScrollBar.scrollNeeded
                                           ? Math.max(0, selectedPathsScrollBar.x - 6)
                                           : selectedPathsList.width
                                    filePath: modelData
                                    fileName: page.fileNameForPath(pathValue)
                                    parentPath: page.displayPath(page.parentPathForPath(pathValue))
                                    isDirectory: page.controller.isPathDir(pathValue)
                                    suffix: page.controller.getPathSuffix(pathValue).toLowerCase()
                                    useNativeIcons: page.useNativeIcons
                                }

                                ScrollBar.vertical: FmScrollBar {
                                    id: selectedPathsScrollBar
                                    policy: selectedPathsList.contentHeight > selectedPathsList.height
                                            ? ScrollBar.AlwaysOn
                                            : ScrollBar.AsNeeded
                                }
                            }
                        }

                        DialogSection {
                            title: page.capabilitiesTitle
                            visible: !page.multiMode && page.capabilityRows.length > 0

                            Repeater {
                                model: page.capabilityRows

                                AccessCapabilityRow {
                                    required property var modelData
                                    label: (modelData && modelData.label) ? modelData.label : ""
                                    value: (modelData && modelData.value ? modelData.value : "")
                                    allowed: modelData && modelData.allowed ? true : false
                                    accessState: (modelData && modelData.state) ? modelData.state : (allowed ? "allowed" : "denied")
                                    description: page.capabilityDescription((modelData && modelData.label) ? modelData.label : "",
                                                                            modelData && modelData.allowed ? true : false,
                                                                            (modelData && modelData.state) ? modelData.state : "")
                                }
                            }
                        }

                        DialogSection {
                            title: page.attributesTitle
                            visible: !page.multiMode && page.attributeRows.length > 0

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                AttributeToggleRow {
                                    visible: page.controller.canEditAttributes
                                    title: "Hidden"
                                    subtitle: "Hide this item from normal file listings."
                                    checked: page.controller.hiddenAttribute
                                    accentColor: Theme.warning
                                    onToggled: (checked) => page.controller.setHiddenAttribute(checked)
                                }

                                AttributeToggleRow {
                                    visible: page.controller.canEditAttributes
                                    title: "Read-only"
                                    subtitle: "Mark this item as read-only at the filesystem attribute level."
                                    checked: page.controller.readOnlyAttribute
                                    accentColor: Theme.accent
                                    onToggled: (checked) => page.controller.setReadOnlyAttribute(checked)
                                }

                                Repeater {
                                    model: page.attributeRows

                                    DialogListRow {
                                        required property var modelData
                                        visible: !(modelData && modelData.editable)
                                        label: modelData && modelData.label ? modelData.label : ""
                                        value: modelData && modelData.value ? modelData.value : ""
                                        valueColor: modelData && modelData.enabled ? Theme.warning : Theme.textSecondary
                                    }
                                }

                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }
