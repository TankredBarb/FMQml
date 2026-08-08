import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FM
import "../../style"
import "../common"
import "../framework"

ColumnLayout {
    id: identityField

    required property var choices
    property string currentValue: ""
    property string placeholder: ""
    signal edited(string value)

    function openSuggestions() {
        const position = input.mapToItem(null, 0, input.height + 4)
        suggestionsPopup.x = position.x
        suggestionsPopup.y = position.y
        suggestionsPopup.open()
    }

    function closeSuggestions() {
        suggestionsPopup.close()
    }

    readonly property var filteredChoices: {
        const query = String(input.text || "").trim().toLowerCase()
        const values = Array.from(choices || [])
        return values.filter(choice => {
            const name = String(choice.name || "").toLowerCase()
            const id = String(choice.id || "")
            return query.length === 0 || name.indexOf(query) >= 0 || id.indexOf(query) >= 0
        }).slice(0, 8)
    }

    FmTextField {
        id: input
        Layout.fillWidth: true
        text: identityField.currentValue
        placeholderText: identityField.placeholder
        onTextEdited: {
            identityField.edited(text)
            if (identityField.filteredChoices.length > 0) {
                identityField.openSuggestions()
            }
        }
        onActiveFocusChanged: {
            if (activeFocus && identityField.filteredChoices.length > 0) {
                identityField.openSuggestions()
            }
        }
    }

    Popup {
        id: suggestionsPopup
        parent: Overlay.overlay
        width: input.width
        height: Math.min(224, suggestionList.contentHeight + 8)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: FmMenuVisual {
            textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
            surfaceColor: Theme.menuSurface
            borderColor: Theme.menuBorder
            accentColor: Theme.accent
            shadowColor: Theme.shadow
            dark: themeController.isDark
        }

        ListView {
            id: suggestionList
            function selectChoice(value) {
                identityField.edited(value)
                identityField.closeSuggestions()
            }
            anchors.fill: parent
            clip: true
            model: identityField.filteredChoices
            spacing: 2

            delegate: FmMenuItem {
                required property var modelData
                width: suggestionList.width
                height: 32
                text: modelData.label || ""
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                onClicked: suggestionList.selectChoice(modelData.name || "")
            }
        }
    }
}
