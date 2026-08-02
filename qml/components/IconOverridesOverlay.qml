import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"
import "settings"
import "framework"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent ? parent.width - 32 : 1080, 1080)
    height: Math.min(parent ? parent.height - 32 : 700, 700)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property var appRoot: null
    property string editingSuffix: ""
    property string pendingRemovalSuffix: ""
    property bool resetAllPending: false
    property string validationMessage: ""
    readonly property bool nativeIconsEnabled: typeof appSettings !== "undefined" && appSettings
                                                ? appSettings.useNativeIcons : true
    readonly property var bundledIcons: fileTypeIconResolver.availableBundledIconNames()

    function previewSource(type, value, available) {
        const sourceValue = value === undefined || value === null ? "" : String(value).trim()
        if (!available || sourceValue.length === 0)
            return "qrc:/qt/qml/FM/qml/assets/filetypes-next/document.svg"
        if (type === "bundled") return "qrc:/qt/qml/FM/qml/assets/filetypes-next/" + sourceValue + ".svg"
        if (type === "theme") return "image://icon/theme/" + encodeURIComponent(sourceValue)
        if (type === "file") return "file://" + encodeURI(sourceValue).replace(/#/g, "%23").replace(/\?/g, "%3F")
        return "qrc:/qt/qml/FM/qml/assets/filetypes-next/document.svg"
    }

    function beginAdd() {
        editingSuffix = ""
        suffixField.text = ""
        sourceType.currentIndex = 0
        bundledPicker.currentIndex = 0
        sourceValue.text = bundledIcons.length > 0 ? bundledIcons[0] : "document"
        validationMessage = ""
        suffixField.forceActiveFocus()
    }

    function beginEdit(rule) {
        editingSuffix = String(rule.suffix || "")
        suffixField.text = editingSuffix
        sourceType.currentIndex = sourceType.indexOfValue(String(rule.sourceType || "bundled"))
        sourceValue.text = String(rule.sourceValue || "")
        bundledPicker.currentIndex = Math.max(0, bundledIcons.indexOf(sourceValue.text))
        validationMessage = ""
        suffixField.forceActiveFocus()
    }

    function saveRule() {
        const suffix = suffixField.text.trim()
        const type = sourceType.currentValue
        const value = type === "bundled" ? bundledPicker.currentText : sourceValue.text.trim()
        if (suffix.length === 0 || value.length === 0) {
            validationMessage = "Enter a suffix and choose an icon."
            return
        }
        if (!fileTypeIconResolver.addOrUpdateIconOverride(suffix, type, value)) {
            validationMessage = "This rule is not valid. Check the suffix and icon source."
            return
        }
        if (editingSuffix.length > 0 && editingSuffix.toLowerCase() !== suffix.replace(/^\.+/, "").toLowerCase()) {
            fileTypeIconResolver.removeIconOverride(editingSuffix)
        }
        beginAdd()
    }

    function requestRemove(suffix) {
        pendingRemovalSuffix = suffix
        resetAllPending = false
        confirmation.open()
    }

    function requestResetAll() {
        pendingRemovalSuffix = ""
        resetAllPending = true
        confirmation.open()
    }

    onOpened: beginAdd()

    background: DialogShell {
        accentColor: Theme.accent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 16
        shadowVerticalOffset: 5
    }

    DialogHeader {
        id: dialogHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        iconSource: "qrc:/qt/qml/FM/qml/assets/filetypes-next/image.svg"
        iconTint: Theme.accent
        accentColor: Theme.accent
        title: "Icon Overrides"
        subtitle: "Choose which icon FM shows for a file suffix when Native icons is enabled."
        closeText: "x"
        onCloseRequested: root.close()
    }

    DialogFooter {
        id: dialogFooter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.nativeIconsEnabled ? "Overrides are active" : "Overrides are saved but currently inactive"
                color: root.nativeIconsEnabled ? Theme.accent : Theme.warning
                font.pixelSize: Theme.fontSizeCaption
            }
            Item { Layout.fillWidth: true }
            DialogActionButton {
                text: "Restore All Defaults"
                highlighted: false
                secondaryTextColor: Theme.danger
                enabled: fileTypeIconResolver.iconOverrides.length > 0
                onClicked: root.requestResetAll()
            }
            DialogActionButton {
                text: "Done"
                highlighted: true
                primaryColor: Theme.accent
                onClicked: root.close()
            }
        }
    }

    RowLayout {
        anchors.top: dialogHeader.bottom
        anchors.bottom: dialogFooter.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 16

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 430
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.36 : 0.64)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.48)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Configured rules"
                        color: Theme.textPrimary
                        font.weight: Font.DemiBold
                        font.pixelSize: Theme.fontSizeLabel
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: fileTypeIconResolver.iconOverrides.length
                        color: Theme.textSecondary
                    }
                }

                ListView {
                    id: rulesList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6
                    clip: true
                    model: fileTypeIconResolver.iconOverrides

                    delegate: Rectangle {
                        required property var modelData
                        width: rulesList.width
                        height: 66
                        radius: Theme.radiusSm
                        color: ruleMouse.containsMouse ? Theme.surfaceHover : Theme.panelSurfaceSoft
                        border.color: root.editingSuffix === modelData.suffix ? Theme.accent : Theme.withAlpha(Theme.panelBorder, 0.42)

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 10
                            Image {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                source: root.previewSource(modelData.sourceType, modelData.sourceValue, modelData.available)
                                fillMode: Image.PreserveAspectFit
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: "." + modelData.suffix; color: Theme.textPrimary; font.weight: Font.DemiBold }
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.sourceType + " · " + modelData.sourceValue
                                          + (modelData.available ? "" : " · unavailable")
                                    color: modelData.available ? Theme.textSecondary : Theme.danger
                                    elide: Text.ElideMiddle
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }
                            FmIconButton {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/rename.svg"
                                iconTone: "accent"
                                iconSize: 16
                                ToolTip.visible: hovered
                                ToolTip.text: "Edit rule"
                                Accessible.name: "Edit rule"
                                onClicked: root.beginEdit(modelData)
                            }
                            FmIconButton {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/delete.svg"
                                iconTone: "danger"
                                iconSize: 16
                                ToolTip.visible: hovered
                                ToolTip.text: "Delete rule"
                                Accessible.name: "Delete rule"
                                onClicked: root.requestRemove(modelData.suffix)
                            }
                        }
                        HoverHandler { id: ruleMouse }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 24, 320)
                        visible: rulesList.count === 0
                        text: "No overrides yet. Add a rule to replace the system icon for a suffix."
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.26 : 0.54)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.42)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    text: root.editingSuffix.length > 0 ? "Edit ." + root.editingSuffix : "Add override"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: "Suffixes are case-insensitive. Compound suffixes such as fb2.zip are supported."
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
                Label { text: "File suffix"; color: Theme.textPrimary; font.weight: Font.Medium }
                FmTextField {
                    id: suffixField
                    Layout.fillWidth: true
                    placeholderText: "epub or fb2.zip"
                    onTextEdited: root.validationMessage = ""
                }
                Label { text: "Icon source"; color: Theme.textPrimary; font.weight: Font.Medium }
                SettingsComboBox {
                    id: sourceType
                    Layout.fillWidth: true
                    model: [{ label: "FM bundled icon", value: "bundled" },
                            { label: "System theme icon", value: "theme" },
                            { label: "Local icon file", value: "file" }]
                    textRole: "label"
                    valueRole: "value"
                    onCurrentValueChanged: root.validationMessage = ""
                }
                SettingsComboBox {
                    id: bundledPicker
                    Layout.fillWidth: true
                    visible: sourceType.currentValue === "bundled"
                    model: root.bundledIcons
                    textRole: ""
                    onCurrentTextChanged: if (visible) sourceValue.text = currentText

                    delegate: ItemDelegate {
                        required property int index
                        required property string modelData
                        width: bundledPicker.width
                        height: Math.max(38, Theme.controlHeight)
                        highlighted: bundledPicker.highlightedIndex === index
                        onClicked: {
                            bundledPicker.currentIndex = index
                            bundledPicker.popup.close()
                        }
                        contentItem: RowLayout {
                            spacing: 8
                            Image {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                source: modelData.length > 0
                                        ? "qrc:/qt/qml/FM/qml/assets/filetypes-next/" + modelData + ".svg"
                                        : ""
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData
                                color: highlighted ? Theme.textPrimary : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLabel
                                font.weight: Font.Normal
                            }
                        }
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: highlighted || hovered ? Theme.menuItemHover : "transparent"
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: sourceType.currentValue !== "bundled"
                    FmTextField {
                        id: sourceValue
                        Layout.fillWidth: true
                        placeholderText: sourceType.currentValue === "theme" ? "Theme icon name, for example application-pdf" : "Absolute path to an icon"
                        onTextEdited: root.validationMessage = ""
                    }
                    DialogActionButton {
                        visible: sourceType.currentValue === "file"
                        text: "Browse"
                        highlighted: false
                        secondaryTextColor: Theme.accent
                        onClicked: iconFileDialog.open()
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    radius: Theme.radiusSm
                    color: Theme.panelSurfaceSoft
                    border.color: Theme.withAlpha(Theme.panelBorder, 0.42)
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        Image {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 58
                            source: root.previewSource(sourceType.currentValue,
                                                       sourceType.currentValue === "bundled" ? bundledPicker.currentText : sourceValue.text,
                                                       true)
                            fillMode: Image.PreserveAspectFit
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "Preview"; color: Theme.textPrimary; font.weight: Font.DemiBold }
                            Label {
                                Layout.fillWidth: true
                                text: suffixField.text.trim().length > 0 ? "Example." + suffixField.text.replace(/^\.+/, "") : "Enter a suffix"
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.validationMessage.length > 0
                    text: root.validationMessage
                    color: Theme.danger
                    wrapMode: Text.WordWrap
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    DialogActionButton {
                        visible: root.editingSuffix.length > 0
                        text: "Cancel Edit"
                        highlighted: false
                        secondaryTextColor: Theme.textSecondary
                        onClicked: root.beginAdd()
                    }
                    DialogActionButton {
                        text: root.editingSuffix.length > 0 ? "Save Changes" : "Add Override"
                        highlighted: true
                        primaryColor: Theme.accent
                        onClicked: root.saveRule()
                    }
                }
            }
        }
    }

    FileDialog {
        id: iconFileDialog
        title: "Choose Icon File"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Icon files (*.svg *.png *.ico *.jpg *.jpeg *.webp)", "All files (*)"]
        onAccepted: {
            sourceValue.text = selectedFile.toLocalFile()
            root.validationMessage = ""
        }
    }

    Popup {
        id: confirmation
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 80, 430)
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: DialogShell { accentColor: Theme.danger; shellColor: Theme.panelSurface }
        contentItem: ColumnLayout {
            spacing: 14
            Label {
                Layout.fillWidth: true
                text: root.resetAllPending ? "Restore all default icons?" : "Restore default for ." + root.pendingRemovalSuffix + "?"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: root.resetAllPending ? "Every saved icon override will be removed." : "This suffix will use its system icon again."
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                DialogActionButton { text: "Cancel"; highlighted: false; onClicked: confirmation.close() }
                DialogActionButton {
                    text: "Restore"
                    highlighted: true
                    primaryColor: Theme.danger
                    onClicked: {
                        if (root.resetAllPending) fileTypeIconResolver.clearIconOverrides()
                        else fileTypeIconResolver.removeIconOverride(root.pendingRemovalSuffix)
                        if (root.resetAllPending || root.editingSuffix === root.pendingRemovalSuffix) root.beginAdd()
                        confirmation.close()
                    }
                }
            }
        }
    }
}
