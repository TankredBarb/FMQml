import "../style"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common"
import "framework"

Rectangle {
    id: tokenRow

    required property var editor
    required property var modelData
    readonly property var token: modelData
    readonly property bool isChanged: editor.tokenChanged(token.key)
    readonly property color rowAccent: editor.tokenAreaAccent(token.key)
    readonly property string areaTitle: editor.tokenAreaTitle(token.key)

    Layout.fillWidth: true
    implicitHeight: Math.max(64, tokenLayout.implicitHeight + 14)
    radius: Theme.radiusSm
    color: rowHover.hovered ? editor.tokenRowFillHover : editor.tokenRowFill
    border.color: tokenRow.isChanged ? Theme.withAlpha(tokenRow.rowAccent, themeController.isDark ? 0.62 : 0.44) : editor.tokenRowBorder
    border.width: 1

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 7
        width: 2
        radius: 1
        color: Theme.withAlpha(tokenRow.rowAccent, tokenRow.isChanged || rowHover.hovered ? 0.9 : 0.56)
    }

    HoverHandler {
        id: rowHover

        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onHoveredChanged: {
            if (hovered)
                editor.hoveredTokenKey = token.key;
            else if (editor.hoveredTokenKey === token.key)
                editor.hoveredTokenKey = "";
        }
    }

    ToolTip {
        visible: rowHover.hovered
        delay: 600

        contentItem: Label {
            text: "<b>" + token.title + "</b> (" + token.key + ")<br/>" + (token.hint ? token.hint + "<br/>" : "") + "<i>Affects: " + tokenRow.areaTitle + "</i>" + (tokenRow.isChanged ? "<br/><font color='" + tokenRow.rowAccent + "'>Changed (Initial: " + editor.previewColorFromState(editor.initialState, token.key) + ")</font>" : "")
            textFormat: Text.RichText
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        background: Rectangle {
            color: Theme.panelSurface
            border.color: Theme.panelBorder
            border.width: 1
            radius: Theme.radiusSm
        }

    }

    RowLayout {
        id: tokenLayout

        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        anchors.topMargin: 7
        anchors.bottomMargin: 7
        spacing: 10

        Rectangle {
            id: colorSwatch

            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            radius: 7
            color: editor.previewColor(token.key, Theme.accent)
            border.color: Theme.withAlpha(Theme.textPrimary, 0.18)
            border.width: 1

            TapHandler {
                onTapped: editor.openPickerForToken(token.key, token.title)
            }

            HoverHandler {
                id: tokenSwatchHover

                cursorShape: Qt.PointingHandCursor
            }

            ToolTip {
                visible: tokenSwatchHover.hovered
                delay: 400
                text: "Click to pick color for " + token.title
            }

        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: token.title
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: tokenRow.isChanged
                    Layout.preferredWidth: changedLabel.implicitWidth + 10
                    Layout.preferredHeight: 18
                    radius: 9
                    color: Theme.withAlpha(tokenRow.rowAccent, themeController.isDark ? 0.18 : 0.12)
                    border.color: Theme.withAlpha(tokenRow.rowAccent, themeController.isDark ? 0.48 : 0.34)
                    border.width: 1

                    Label {
                        id: changedLabel

                        anchors.centerIn: parent
                        text: "Changed"
                        color: tokenRow.rowAccent
                        font.pixelSize: Theme.scaledSize(9)
                        font.weight: Font.DemiBold
                    }

                }

            }

            Label {
                text: token.hint ? token.hint : tokenRow.areaTitle
                Layout.fillWidth: true
                font.pixelSize: Theme.fontSizeMicro
                color: editor.tokenHintText
                elide: Text.ElideRight
            }

            Label {
                text: token.key + " / " + tokenRow.areaTitle
                Layout.fillWidth: true
                font.pixelSize: Theme.scaledSize(9)
                font.family: "monospace"
                color: Theme.textSecondary
                elide: Text.ElideRight
            }

        }

        ColumnLayout {
            Layout.preferredWidth: 104
            spacing: 4

            PremiumTextField {
                Layout.fillWidth: true
                implicitHeight: 26
                leftPadding: 7
                rightPadding: 7
                font.pixelSize: Theme.fontSizeMicro
                font.family: "monospace"
                premiumRadius: 5
                text: editor.colorValue(token.key)
                placeholderText: "#FFFFFFFF"
                onTextEdited: editor.setColorValue(token.key, text)
            }

            FmButton {
                id: resetTokenButton

                visible: tokenRow.isChanged
                text: "Reset"
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: 54
                Layout.preferredHeight: 22
                secondaryTextColor: Theme.textSecondary
                primaryColor: tokenRow.rowAccent
                onClicked: editor.resetTokenToDefault(token.key)
                ToolTip.visible: hovered
                ToolTip.text: "Reset to default"

            }

        }

    }

}
