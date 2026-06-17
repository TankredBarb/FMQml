import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

Item {
    id: root

    property var properties: []
    property string title: ""
    property bool alignToBottom: false
    property string verticalPlacement: alignToBottom ? "bottom" : "top"
    property bool placementToggleVisible: false

    signal placementToggleRequested()

    implicitHeight: contentColumn.implicitHeight

    function contentY() {
        if (root.verticalPlacement === "center") {
            return Math.max(0, (root.height - contentColumn.implicitHeight) / 2)
        }
        if (root.verticalPlacement === "bottom") {
            return Math.max(0, root.height - contentColumn.implicitHeight)
        }
        return 0
    }

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function valueFor(label) {
        const items = Array.isArray(root.properties) ? root.properties : []
        for (let i = 0; i < items.length; ++i) {
            if (root.safeText(items[i].label) === label) {
                return root.safeText(items[i].value)
            }
        }
        return ""
    }

    function displayValue(label) {
        const value = root.valueFor(label)
        return value.length > 0 ? value : "-"
    }

    function hasWord(text, words) {
        const value = root.safeText(text).toLowerCase()
        for (let i = 0; i < words.length; ++i) {
            if (value.indexOf(words[i]) >= 0) {
                return true
            }
        }
        return false
    }

    function hasLetter(text, letter) {
        return new RegExp("(^|[^A-Za-z])" + letter + "([^A-Za-z]|$)").test(root.safeText(text))
    }

    function accessChips() {
        const access = root.valueFor("Access")
        const canRead = root.hasWord(access, ["read", "browse"])
        const canWrite = root.hasWord(access, ["modify", "write", "create"])
        const canDelete = root.hasWord(access, ["delete"])
        const canTraverse = root.hasWord(access, ["traverse"])
        const canExecute = root.hasWord(access, ["execute"])
        return [
            { key: "R", tip: canRead ? "Can read / browse" : "Can't read / browse", active: canRead },
            { key: "W", tip: canWrite ? "Can write / create" : "Can't write / create", active: canWrite },
            { key: "D", tip: canDelete ? "Can delete" : "Can't delete", active: canDelete },
            { key: "Tr", tip: canTraverse ? "Can traverse" : "Can't traverse", active: canTraverse },
            { key: "X", tip: canExecute ? "Can execute" : "Can't execute", active: canExecute }
        ]
    }

    function attributeChips() {
        const attrs = root.valueFor("Attributes")
        const hidden = root.hasWord(attrs, ["hidden"]) || root.hasLetter(attrs, "H")
        const readOnly = root.hasWord(attrs, ["read-only", "readonly"]) || root.hasLetter(attrs, "R")
        const system = root.hasWord(attrs, ["system"]) || root.hasLetter(attrs, "S")
        const symlink = root.hasWord(attrs, ["symlink", "link"]) || root.hasLetter(attrs, "L")
        return [
            { key: "H", tip: hidden ? "Hidden" : "Not hidden", active: hidden },
            { key: "R", tip: readOnly ? "Read-only" : "Not read-only", active: readOnly },
            { key: "S", tip: system ? "System" : "Not system", active: system },
            { key: "L", tip: symlink ? "Symlink" : "Not symlink", active: symlink }
        ]
    }

    ColumnLayout {
        id: contentColumn
        width: parent.width
        y: root.contentY()
        spacing: 8

        Behavior on y {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }

        RowLayout {
            visible: root.title.length > 0 || root.placementToggleVisible
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                visible: root.title.length > 0
                text: root.title
                font.bold: true
                font.pixelSize: Theme.fontSizeBody
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Button {
                id: placementButton

                visible: root.placementToggleVisible
                Layout.preferredWidth: 24
                Layout.preferredHeight: 22
                padding: 0
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: root.verticalPlacement === "top" ? "Move details down" : "Move details up"
                ToolTip.delay: 500
                onClicked: root.placementToggleRequested()

                contentItem: RecolorSvgIcon {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    sourcePath: root.verticalPlacement === "top"
                                ? "qrc:/qt/qml/FM/qml/assets/icons/arrow-down.svg"
                                : "qrc:/qt/qml/FM/qml/assets/icons/arrow-up.svg"
                    recolorColor: placementButton.hovered ? Theme.textPrimary : Theme.textSecondary
                }

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: placementButton.down
                           ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.10 : 0.06)
                           : (placementButton.hovered
                              ? Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.065 : 0.040)
                              : "transparent")
                    border.color: placementButton.hovered
                                  ? Theme.withAlpha(Theme.border, themeController.isDark ? 0.50 : 0.36)
                                  : "transparent"
                    border.width: placementButton.hovered ? 1 : 0
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 8

            FactCell {
                Layout.fillWidth: true
                label: "Size"
                value: root.displayValue("Size")
            }

            FactCell {
                Layout.fillWidth: true
                label: "Modified"
                value: root.displayValue("Modified")
            }

            FactCell {
                Layout.fillWidth: true
                label: "Type"
                value: root.displayValue("Type")
            }

            FactCell {
                Layout.fillWidth: true
                label: "Attributes"
                value: root.displayValue("Attributes")
                chips: root.attributeChips()
            }
        }

        FactCell {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            label: "Access"
            value: root.displayValue("Access")
            chips: root.accessChips()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: Theme.radiusMd
            color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
            border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.72 : 0.56)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 8

                Image {
                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                    source: "qrc:/qt/qml/FM/qml/assets/icons/folder.svg"
                    sourceSize: Qt.size(15, 15)
                    opacity: 0.88
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1
                        colorizationColor: Theme.accent
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: "Location"
                        font.pixelSize: Theme.scaledSize(9)
                        font.bold: true
                        color: Theme.textSecondary
                        opacity: 0.82
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("Location")
                        font.pixelSize: Theme.fontSizeCaption
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }

    component FactCell: Rectangle {
        id: cell

        property string label: ""
        property string value: ""
        property var chips: []

        Layout.preferredHeight: 56
        radius: Theme.radiusMd
        color: themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025)
        border.color: Theme.withAlpha(Theme.border, themeController.isDark ? 0.70 : 0.54)
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: cell.label
                font.pixelSize: Theme.scaledSize(9)
                font.bold: true
                color: Theme.textSecondary
                opacity: 0.82
                elide: Text.ElideRight
            }

            RowLayout {
                visible: cell.chips.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 19
                spacing: 3
                clip: true

                Repeater {
                    model: cell.chips

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: modelData.key === "Tr" ? 24 : 20
                        Layout.maximumWidth: modelData.key === "Tr" ? 28 : 24
                        Layout.minimumWidth: modelData.key === "Tr" ? 20 : 17
                        Layout.preferredHeight: 19
                        radius: Theme.radiusSm
                        color: modelData.active
                               ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.17 : 0.11)
                               : (themeController.isDark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.025))
                        border.color: modelData.active
                                      ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.42 : 0.30)
                                      : Theme.withAlpha(Theme.border, themeController.isDark ? 0.58 : 0.44)
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: modelData.key
                            width: parent.width - 4
                            font.pixelSize: Theme.scaledSize(8)
                            font.bold: true
                            color: modelData.active ? Theme.accent : Theme.textSecondary
                            opacity: modelData.active ? 1.0 : 0.55
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }

                        ToolTip.visible: chipMouse.containsMouse
                        ToolTip.text: modelData.tip
                        ToolTip.delay: 500

                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
            }

            Label {
                visible: cell.chips.length === 0
                Layout.fillWidth: true
                text: cell.value.length > 0 ? cell.value : "-"
                font.pixelSize: Theme.fontSizeLabel
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
        }
    }
}
