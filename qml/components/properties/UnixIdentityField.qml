import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"

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

    PremiumTextField {
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

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.panelSurface
            border.width: 1
            border.color: Theme.panelBorder
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

            delegate: Rectangle {
                required property var modelData
                width: suggestionList.width
                height: 32
                radius: Theme.radiusXs
                color: suggestionMouse.containsMouse ? Theme.panelSurfaceSoft : "transparent"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    text: modelData.label || ""
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textPrimary
                }

                MouseArea {
                    id: suggestionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: suggestionList.selectChoice(modelData.name || "")
                }
            }
        }
    }
}
