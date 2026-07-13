import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

ScrollView {
                    id: page
                    visible: page.pageVisible
                    required property int currentIndex
    required property bool pageVisible
    required property var controller
    required property var rows
    required property string sectionTitle
    required property bool adminEditMode
    required property int pendingMode
    required property bool modeDirty
    required property bool recursively
    required property string pendingOwner
    required property string pendingGroup
    required property bool ownershipDirty
    required property var modeEnabled
    required property var setModeBit
    required property var modeOctal
    required property var resetMode
    required property var applyMode
    required property var setRecursive
    required property var ownerEdited
    required property var groupEdited
    required property var resetOwnership
    required property var applyOwnership
    required property var tabContentY
    readonly property real contentImplicitHeight: contentLayout.implicitHeight

    anchors.fill: parent
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true
                    enabled: page.currentIndex === 3

                    opacity: page.currentIndex === 3 ? 1.0 : 0.0
                    z: page.currentIndex === 3 ? 1 : 0
                    transform: Translate {
                        x: page.currentIndex === 3 ? 0 : (3 < page.currentIndex ? -400 : 400)
                        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutQuad } }

                    ColumnLayout {
                        id: contentLayout
                        x: 16
                        y: page.tabContentY(page, contentLayout)
                        width: page.availableWidth - 32
                        spacing: 12

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }

                        DialogSection {
                            title: page.sectionTitle
                            visible: page.rows.length > 0

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                InlineBadge {
                                    visible: page.adminEditMode
                                    text: "ADMINISTRATOR MODE"
                                    textColor: Theme.warning
                                    fillColor: Theme.withAlpha(Theme.warning, 0.12)
                                    strokeColor: Theme.withAlpha(Theme.warning, 0.28)
                                    fontSize: 9
                                    fontWeight: Font.Bold
                                }

                                Repeater {
                                    model: page.rows

                                    DialogListRow {
                                        required property var modelData
                                        label: modelData && modelData.label ? modelData.label : ""
                                        value: modelData && modelData.value ? modelData.value : ""
                                        valueMaximumLineCount: 2
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: !page.adminEditMode
                                             && page.controller.unixEditNotice.length > 0
                                    text: page.controller.unixEditNotice
                                    wrapMode: Text.WordWrap
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.warning
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: page.controller.canEditUnixMode || page.adminEditMode

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 4
                                        height: 1
                                        color: Theme.panelBorder
                                    }

                                    Label {
                                        text: "Permissions"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    PermissionModeRow { title: "Owner"; readBit: 256; writeBit: 128; executeBit: 64; modeEnabled: page.modeEnabled; setModeBit: page.setModeBit }
                                    PermissionModeRow { title: "Group"; readBit: 32; writeBit: 16; executeBit: 8; modeEnabled: page.modeEnabled; setModeBit: page.setModeBit }
                                    PermissionModeRow { title: "Others"; readBit: 4; writeBit: 2; executeBit: 1; modeEnabled: page.modeEnabled; setModeBit: page.setModeBit }

                                    SpecialModeToggle {
                                        visible: page.controller.isDirectory
                                        title: "Apply recursively"
                                        subtitle: "Apply changed permissions to this item and all contents."
                                        checked: page.recursively
                                        onToggled: function(checked) { page.setRecursive(checked) }
                                    }

                                    Label {
                                        text: "Advanced permissions"
                                        Layout.topMargin: 4
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: Font.DemiBold
                                        color: Theme.warning
                                    }

                                    SpecialModeToggle {
                                        title: "Set user ID"
                                        subtitle: "Runs an executable with the file owner's identity."
                                        checked: page.modeEnabled(2048)
                                        onToggled: function(checked) { page.setModeBit(2048, checked) }
                                    }

                                    SpecialModeToggle {
                                        title: "Set group ID"
                                        subtitle: page.controller.isDirectory
                                                  ? "New items inherit this directory's group."
                                                  : "Runs an executable with the file group's identity."
                                        checked: page.modeEnabled(1024)
                                        onToggled: function(checked) { page.setModeBit(1024, checked) }
                                    }

                                    SpecialModeToggle {
                                        title: "Sticky"
                                        subtitle: "Restricts deletion and rename inside a directory."
                                        checked: page.modeEnabled(512)
                                        onToggled: function(checked) { page.setModeBit(512, checked) }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 2
                                        spacing: 8

                                        Label {
                                            text: "Pending mode: " + page.modeOctal(page.pendingMode)
                                            Layout.preferredWidth: 112
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: Theme.textSecondary
                                        }

                                        Item { Layout.fillWidth: true }

                                        DialogActionButton {
                                            text: "Reset"
                                            enabled: page.modeDirty
                                            onClicked: page.resetMode()
                                        }

                                        DialogActionButton {
                                            text: "Apply"
                                            highlighted: true
                                            enabled: page.modeDirty
                                            onClicked: page.applyMode()
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: page.controller.unixModeError.length > 0
                                        text: page.controller.unixModeError
                                        wrapMode: Text.WordWrap
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.danger
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: page.controller.canEditUnixMode || page.adminEditMode

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 4
                                        height: 1
                                        color: Theme.panelBorder
                                    }

                                    Label {
                                        text: "Ownership"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: page.adminEditMode
                                        spacing: 10

                                        Label {
                                            text: "Owner"
                                            Layout.preferredWidth: 72
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.weight: Font.DemiBold
                                            color: Theme.textSecondary
                                        }

                                        UnixIdentityField {
                                            Layout.fillWidth: true
                                            currentValue: page.pendingOwner
                                            placeholder: "User name or UID"
                                            choices: page.controller.unixUsers
                                            onEdited: value => page.ownerEdited(value)
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Label {
                                            text: "Group"
                                            Layout.preferredWidth: 72
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.weight: Font.DemiBold
                                            color: Theme.textSecondary
                                        }

                                        UnixIdentityField {
                                            Layout.fillWidth: true
                                            currentValue: page.pendingGroup
                                            placeholder: "Group name or GID"
                                            choices: page.adminEditMode
                                                     ? page.controller.unixGroups
                                                     : page.controller.editableUnixGroups
                                            onEdited: value => page.groupEdited(value)
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Label {
                                            text: page.adminEditMode
                                                  ? "Names or numeric IDs are accepted."
                                                  : "You may change the group for an item you own."
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: Theme.textSecondary
                                        }

                                        DialogActionButton {
                                            text: "Reset"
                                            enabled: page.ownershipDirty
                                            onClicked: page.resetOwnership()
                                        }

                                        DialogActionButton {
                                            text: "Apply"
                                            highlighted: true
                                            enabled: page.ownershipDirty
                                            onClicked: page.applyOwnership()
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }
