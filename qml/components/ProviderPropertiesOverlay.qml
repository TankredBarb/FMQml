import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "dialogs"

Popup {
    id: root

    x: parent ? Math.max(12, (parent.width - width) / 2) : 0
    y: parent ? Math.max(12, (parent.height - height) / 2) : 0
    width: parent ? Math.min(620, parent.width - 24) : 620
    height: parent ? Math.min(mainLayout.implicitHeight, parent.height * 0.92) : mainLayout.implicitHeight
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    visible: providerPropertiesController.visible

    readonly property color accentColor: Theme.categoryInfo

    function copySummary() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(providerPropertiesController.exportableText())
            copiedTooltip.show("Summary copied")
        }
    }

    function copyPath() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(providerPropertiesController.path)
            copiedTooltip.show("Path copied")
        }
    }

    function copyJson() {
        if (typeof workspaceController !== "undefined" && workspaceController) {
            workspaceController.copyTextToClipboard(providerPropertiesController.exportableJson())
            copiedTooltip.show("JSON copied")
        }
    }

    onClosed: providerPropertiesController.visible = false
    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    background: DialogShell {
        accentColor: root.accentColor
    }

    component PropertyRow : Rectangle {
        id: row

        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        implicitWidth: 0
        implicitHeight: Math.max(38, rowLayout.implicitHeight + 12)
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? Theme.panelSurfaceSoft : Theme.panelSurface
        border.color: Theme.panelBorder
        border.width: 1
        clip: true

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10

            Label {
                text: row.label
                Layout.preferredWidth: 128
                Layout.maximumWidth: 150
                Layout.minimumWidth: 92
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                text: row.value
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                color: Theme.textPrimary
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 4
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onClicked: {
                if (row.value.length > 0 && typeof workspaceController !== "undefined" && workspaceController) {
                    workspaceController.copyTextToClipboard(row.value)
                    copiedTooltip.show("Value copied")
                }
            }
        }
    }

    component SectionCard : DialogSection {
        accentColor: root.accentColor
        fillColor: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.055 : 0.035)
        borderColor: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.22 : 0.16)
    }

    contentItem: FocusScope {
        implicitWidth: mainLayout.implicitWidth
        implicitHeight: mainLayout.implicitHeight

        ColumnLayout {
            id: mainLayout
            anchors.fill: parent
            spacing: 0

            DialogHeader {
                Layout.fillWidth: true
                iconSource: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
                iconTint: root.accentColor
                title: providerPropertiesController.name.length > 0 ? providerPropertiesController.name : "Provider Properties"
                subtitle: providerPropertiesController.providerName
                closeText: "x"
                onCloseRequested: providerPropertiesController.visible = false
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                implicitHeight: statusColumn.implicitHeight + 14
                radius: Theme.radiusSm
                color: Theme.withAlpha(providerPropertiesController.errorText.length > 0 ? Theme.danger : root.accentColor,
                                       themeController.isDark ? 0.10 : 0.07)
                border.color: Theme.withAlpha(providerPropertiesController.errorText.length > 0 ? Theme.danger : root.accentColor,
                                             themeController.isDark ? 0.32 : 0.22)
                border.width: 1
                visible: statusLabel.text.length > 0

                ColumnLayout {
                    id: statusColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Label {
                        id: statusLabel
                        Layout.fillWidth: true
                        text: providerPropertiesController.errorText.length > 0
                              ? providerPropertiesController.errorText
                              : providerPropertiesController.statusText
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: providerPropertiesController.errorText.length > 0 ? Theme.danger : Theme.textSecondary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 5
                        radius: height / 2
                        color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.62 : 0.48)
                        visible: providerPropertiesController.calculatingSize
                        clip: true

                        Rectangle {
                            id: progressThumb

                            width: parent.width * 0.34
                            height: parent.height
                            radius: height / 2
                            color: root.accentColor
                            opacity: 0.86
                            x: -width

                            SequentialAnimation {
                                loops: Animation.Infinite
                                running: providerPropertiesController.calculatingSize
                                NumberAnimation {
                                    target: progressThumb
                                    property: "x"
                                    from: -progressThumb.width
                                    to: progressThumb.parent.width
                                    duration: 1050
                                    easing.type: Easing.InOutCubic
                                }
                            }
                        }
                    }
                }
            }

            ScrollView {
                id: detailsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: detailsScroll.availableWidth
                    spacing: 10

                    Repeater {
                        model: providerPropertiesController.propertyGroups

                        SectionCard {
                            Layout.fillWidth: true
                            title: modelData.title || ""

                            Repeater {
                                model: modelData.rows || []

                                PropertyRow {
                                    label: modelData.label || ""
                                    value: modelData.value || ""
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: footerFlow.implicitHeight + 20
                color: "transparent"

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.panelBorder
                    opacity: 0.4
                }

                Flow {
                    id: footerFlow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 8

                    DialogActionButton {
                        text: "Copy Summary"
                        highlighted: true
                        primaryColor: root.accentColor
                        primaryHoverColor: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.90 : 0.84)
                        primaryPressedColor: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.78 : 0.72)
                        onClicked: root.copySummary()
                    }

                    DialogActionButton {
                        text: "Copy Path"
                        enabled: providerPropertiesController.path.length > 0
                        onClicked: root.copyPath()
                    }

                    DialogActionButton {
                        text: "Copy JSON"
                        onClicked: root.copyJson()
                    }

                    DialogActionButton {
                        text: "Cancel"
                        visible: providerPropertiesController.calculatingSize
                        secondaryTextColor: Theme.warning
                        onClicked: providerPropertiesController.cancel()
                    }

                    DialogActionButton {
                        text: "Close"
                        onClicked: providerPropertiesController.visible = false
                    }
                }
            }
        }

        ToolTip {
            id: copiedTooltip
            timeout: 1400

            function show(message) {
                text = message
                open()
            }
        }
    }
}
