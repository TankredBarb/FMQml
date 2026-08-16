import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import FM

SettingsContentBlock {
    id: root
    property var dialogRoot: null
    property string colorTarget: ""

    Settings {
        id: codeSettings
        category: "CodeHighlighting"
        property string fontFamily: "DejaVu Sans Mono"
        property string keywordColor: "#7c3aed"
        property string stringColor: "#18794e"
        property string commentColor: "#667085"
        property string literalColor: "#b45309"
        property bool keywordBold: false
        property bool keywordItalic: false
        property bool stringBold: false
        property bool stringItalic: false
        property bool commentBold: false
        property bool commentItalic: false
        property bool literalBold: false
        property bool literalItalic: false
    }

    function resetDefaults() {
        codeSettings.fontFamily = "DejaVu Sans Mono"
        codeSettings.keywordColor = "#7c3aed"
        codeSettings.stringColor = "#18794e"
        codeSettings.commentColor = "#667085"
        codeSettings.literalColor = "#b45309"
        codeSettings.keywordBold = false
        codeSettings.keywordItalic = false
        codeSettings.stringBold = false
        codeSettings.stringItalic = false
        codeSettings.commentBold = false
        codeSettings.commentItalic = false
        codeSettings.literalBold = false
        codeSettings.literalItalic = false
        applyToPreview()
    }

    function applyToPreview() {
        codeSettings.sync()
        if (typeof quickLookController !== "undefined" && quickLookController)
            quickLookController.applyTextDecorationAppearance(
                        codeSettings.fontFamily,
                        codeSettings.keywordColor,
                        codeSettings.stringColor,
                        codeSettings.commentColor,
                        codeSettings.literalColor,
                        [
                            { bold: codeSettings.keywordBold, italic: codeSettings.keywordItalic },
                            { bold: codeSettings.stringBold, italic: codeSettings.stringItalic },
                            { bold: codeSettings.commentBold, italic: codeSettings.commentItalic },
                            { bold: codeSettings.literalBold, italic: codeSettings.literalItalic }
                        ])
    }

    function setStyle(settingName, value) {
        if (settingName === "keywordBold") codeSettings.keywordBold = value
        else if (settingName === "keywordItalic") codeSettings.keywordItalic = value
        else if (settingName === "stringBold") codeSettings.stringBold = value
        else if (settingName === "stringItalic") codeSettings.stringItalic = value
        else if (settingName === "commentBold") codeSettings.commentBold = value
        else if (settingName === "commentItalic") codeSettings.commentItalic = value
        else if (settingName === "literalBold") codeSettings.literalBold = value
        else if (settingName === "literalItalic") codeSettings.literalItalic = value
        applyToPreview()
    }

    function openColor(settingName, value) {
        colorTarget = settingName
        colorDialog.selectedColor = value
        colorDialog.open()
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Code highlighting"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLabel
                font.weight: Font.DemiBold
            }
            FmButton {
                text: "Reset"
                highlighted: false
                onClicked: root.resetDefaults()
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Used only for C/C++, Python, and Bash previews. Ordinary text keeps the application font."
            wrapMode: Text.WordWrap
            color: root.dialogRoot ? root.dialogRoot.detailText : Theme.textSecondary
            font.pixelSize: Theme.fontSizeCaption
        }

        FmButton {
            Layout.fillWidth: true
            highlighted: false
            onClicked: fontSelector.openSelector(
                           codeSettings.fontFamily,
                           typeof appSettings !== "undefined" && appSettings
                           ? appSettings.availableFontFamilies : [])
            contentItem: RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Label {
                    Layout.fillWidth: true
                    text: codeSettings.fontFamily
                    color: Theme.textPrimary
                    font.family: codeSettings.fontFamily
                    elide: Text.ElideRight
                }
                Label { text: "Code font"; color: Theme.textSecondary }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: "Keywords", key: "keywordColor", value: codeSettings.keywordColor,
                      boldKey: "keywordBold", bold: codeSettings.keywordBold,
                      italicKey: "keywordItalic", italic: codeSettings.keywordItalic },
                    { label: "Strings", key: "stringColor", value: codeSettings.stringColor,
                      boldKey: "stringBold", bold: codeSettings.stringBold,
                      italicKey: "stringItalic", italic: codeSettings.stringItalic },
                    { label: "Comments", key: "commentColor", value: codeSettings.commentColor,
                      boldKey: "commentBold", bold: codeSettings.commentBold,
                      italicKey: "commentItalic", italic: codeSettings.commentItalic },
                    { label: "Literals / types", key: "literalColor", value: codeSettings.literalColor,
                      boldKey: "literalBold", bold: codeSettings.literalBold,
                      italicKey: "literalItalic", italic: codeSettings.literalItalic }
                ]
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8

                    FmButton {
                        Layout.fillWidth: true
                        highlighted: false
                        onClicked: root.openColor(modelData.key, modelData.value)
                        contentItem: RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            Rectangle {
                                width: 14
                                height: 14
                                radius: 3
                                color: modelData.value
                                border.color: Theme.panelBorder
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.label
                                color: Theme.textPrimary
                            }
                        }
                    }
                    FmCheckBox {
                        text: "Bold"
                        checked: modelData.bold
                        onClicked: root.setStyle(modelData.boldKey, !modelData.bold)
                    }
                    FmCheckBox {
                        text: "Italic"
                        checked: modelData.italic
                        onClicked: root.setStyle(modelData.italicKey, !modelData.italic)
                    }
                }
            }
        }
    }

    FontSelectorPopup {
        id: fontSelector
        onFontSelected: (family) => {
            codeSettings.fontFamily = family
            root.applyToPreview()
        }
    }

    ColorDialog {
        id: colorDialog
        title: "Select token color"
        onAccepted: {
            const value = selectedColor.toString()
            if (root.colorTarget === "keywordColor") codeSettings.keywordColor = value
            else if (root.colorTarget === "stringColor") codeSettings.stringColor = value
            else if (root.colorTarget === "commentColor") codeSettings.commentColor = value
            else if (root.colorTarget === "literalColor") codeSettings.literalColor = value
            root.applyToPreview()
        }
    }
}
