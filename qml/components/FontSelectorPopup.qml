import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"

Popup {
    id: popup

    property var allFontFamilies: []
    property string currentFontFamily: ""
    signal fontSelected(string family)

    width: 360
    height: 480
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: DialogShell {
        accentColor: Theme.accent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 24
        shadowVerticalOffset: 8
        accentVisible: true
    }

    // internal states
    property string searchQuery: ""
    property var filteredFonts: []

    function openSelector(currentValue, availableList) {
        currentFontFamily = currentValue || ""
        allFontFamilies = availableList || []
        searchField.text = ""
        searchQuery = ""
        updateFilteredFonts()
        open()
        searchField.forceActiveFocus()
        // Scroll to selected font
        Qt.callLater(scrollToCurrent)
    }

    function scrollToCurrent() {
        if (currentFontFamily === "") {
            listView.positionViewAtBeginning()
            listView.currentIndex = 0
            return
        }
        for (let i = 0; i < filteredFonts.length; ++i) {
            if (filteredFonts[i].value === currentFontFamily) {
                listView.currentIndex = i
                listView.positionViewAtIndex(i, ListView.Center)
                return
            }
        }
        listView.currentIndex = -1
    }

    function updateFilteredFonts() {
        let list = []
        let query = searchQuery.trim().toLowerCase()

        // 1. Add "System Default" (if query is empty or matches default)
        if (query === "" || "system default".indexOf(query) !== -1 || "default".indexOf(query) !== -1) {
            list.push({ label: "System Default", value: "", isDefault: true })
        }

        // We want to filter allFontFamilies
        for (let i = 0; i < allFontFamilies.length; ++i) {
            let fontName = allFontFamilies[i]
            if (fontName.length === 0) continue

            // Check match
            if (query !== "" && fontName.toLowerCase().indexOf(query) === -1) {
                continue
            }

            list.push({ label: fontName, value: fontName, isDefault: false })
        }

        filteredFonts = list
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: "Select Font"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSubtitle
                font.weight: Font.DemiBold
            }

            // Close button
            Button {
                id: closeBtn
                flat: true
                implicitWidth: 24
                implicitHeight: 24
                padding: 0
                background: null

                contentItem: Text {
                    text: "×"
                    color: closeBtn.hovered ? Theme.accent : Theme.textSecondary
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: popup.close()
            }
        }

        // Search Input
        PremiumTextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Search fonts..."
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel

            onTextChanged: {
                popup.searchQuery = text
                popup.updateFilteredFonts()
                if (popup.filteredFonts.length > 0) {
                    listView.currentIndex = 0
                } else {
                    listView.currentIndex = -1
                }
            }

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Down) {
                    listView.forceActiveFocus()
                    if (listView.currentIndex < listView.count - 1) {
                        listView.currentIndex++
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                    if (listView.currentIndex >= 0 && listView.currentIndex < popup.filteredFonts.length) {
                        let selectedItem = popup.filteredFonts[listView.currentIndex]
                        popup.fontSelected(selectedItem.value)
                        popup.close()
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    popup.close()
                    event.accepted = true
                }
            }
        }

        // Font List
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: popup.filteredFonts
            currentIndex: -1
            spacing: 2

            ScrollBar.vertical: ScrollBar {
                id: verticalScrollBar
                policy: ScrollBar.AsNeeded
                interactive: true
                width: 8

                background: Item {
                    implicitWidth: 8
                }

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: Theme.withAlpha(Theme.textSecondary,
                                           verticalScrollBar.pressed ? 0.46
                                                                     : (verticalScrollBar.active ? 0.30 : 0.18))
                }
            }

            delegate: ItemDelegate {
                width: listView.width - (verticalScrollBar.visible ? 10 : 0)
                height: 48
                highlighted: listView.currentIndex === index
                hoverEnabled: true

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        // Font Name rendered in its own typeface!
                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: highlighted ? Theme.textPrimary : Theme.textPrimary
                            font.family: modelData.isDefault ? Theme.defaultFontFamily : modelData.value
                            font.pixelSize: Theme.fontSizeLabel
                            font.weight: highlighted ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }

                        // Sample text
                        Label {
                            Layout.fillWidth: true
                            text: modelData.isDefault ? "System default typeface" : "The quick brown fox jumps over the lazy dog"
                            color: highlighted ? Theme.withAlpha(Theme.textPrimary, 0.7) : Theme.textSecondary
                            font.family: modelData.isDefault ? Theme.defaultFontFamily : modelData.value
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                            opacity: 0.8
                        }
                    }

                    // Checkmark indicator for currently active font in settings
                    Label {
                        text: "✓"
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeSubtitle
                        font.weight: Font.Bold
                        visible: popup.currentFontFamily === modelData.value
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: highlighted ? Theme.menuItemHover : (hovered ? Theme.withAlpha(Theme.menuItemHover, 0.5) : "transparent")
                }

                onClicked: {
                    popup.fontSelected(modelData.value)
                    popup.close()
                }
            }

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Up) {
                    if (listView.currentIndex > 0) {
                        listView.currentIndex--
                    } else {
                        searchField.forceActiveFocus()
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Down) {
                    if (listView.currentIndex < listView.count - 1) {
                        listView.currentIndex++
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                    if (listView.currentIndex >= 0 && listView.currentIndex < popup.filteredFonts.length) {
                        let selectedItem = popup.filteredFonts[listView.currentIndex]
                        popup.fontSelected(selectedItem.value)
                        popup.close()
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    popup.close()
                    event.accepted = true
                }
            }
        }
    }
}
