import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

DialogSection {
    id: section

    required property var dialogRoot
    title: "TYPOGRAPHY"
    accentColor: section.dialogRoot.dialogAccent
    fillColor: section.dialogRoot.sectionFill
    borderColor: section.dialogRoot.sectionBorder
    radiusSize: Theme.radiusMd

    SettingsContentBlock {
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Font family"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: "Apply one UI font across the app."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: section.dialogRoot.detailText
                    }
                }

                DialogActionButton {
                    text: "Reset"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    onClicked: section.dialogRoot.resetFontSettings()
                }
            }

            Rectangle {
                id: fontFamilySelectBox
                Layout.fillWidth: true
                implicitHeight: Theme.controlHeight
                radius: Theme.radiusSm
                color: Theme.panelSurfaceSoft
                border.color: fontFamilySelectMouse.containsMouse ? section.dialogRoot.dialogAccent : Theme.panelBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: section.dialogRoot.fontFamilyValue.length > 0 ? section.dialogRoot.fontFamilyValue : "System default"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    RecolorSvgIcon {
                        width: 10
                        height: 10
                        sourcePath: "../../assets/icons/arrow-up.svg"
                        sourceSize: Qt.size(16, 16)
                        recolorEnabled: true
                        recolorColor: Theme.textSecondary
                        rotation: 180
                        opacity: 0.72
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                MouseArea {
                    id: fontFamilySelectMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        section.dialogRoot.openFontSelector()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    text: "Scale"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Label {
                    text: section.dialogRoot.fontScaleValue + "%"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    color: section.dialogRoot.dialogAccent
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Slider {
                id: fontScaleSlider
                Layout.fillWidth: true
                from: 90
                to: 150
                stepSize: 5
                snapMode: Slider.SnapAlways
                value: section.dialogRoot.fontScaleValue
                onMoved: section.dialogRoot.setFontScale(value)

                background: Item {
                    implicitHeight: 20

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 4
                        radius: 2
                        color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.36 : 0.62)
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: fontScaleSlider.visualPosition * parent.width
                        height: 4
                        radius: 2
                        color: section.dialogRoot.dialogAccent
                    }
                }

                handle: Rectangle {
                    x: fontScaleSlider.leftPadding + fontScaleSlider.visualPosition * (fontScaleSlider.availableWidth - width)
                    y: fontScaleSlider.topPadding + fontScaleSlider.availableHeight / 2 - height / 2
                    width: 12
                    height: 12
                    radius: 6
                    color: fontScaleSlider.pressed ? section.dialogRoot.dialogAccent : Theme.panelSurface
                    border.color: section.dialogRoot.dialogAccent
                    border.width: 1
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: previewColumn.implicitHeight + 16
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.44 : 0.62)
                border.color: Theme.withAlpha(section.dialogRoot.dialogAccent, themeController.isDark ? 0.24 : 0.20)
                border.width: 1

                ColumnLayout {
                    id: previewColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    Label {
                        text: "Preview"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: "Folders, files, and dialogs should stay readable at your selected scale."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBodyLarge
                        color: Theme.textPrimary
                    }

                    Label {
                        text: "Caption text uses the same typography system."
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    SettingsContentBlock {
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Custom text colors"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Label {
                        text: "Configure custom colors for UI text elements (e.g. file names, folders, sidebar, status bar)."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: section.dialogRoot.detailText
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                DialogActionButton {
                    text: "Customize"
                    highlighted: false
                    secondaryTextColor: section.dialogRoot.dialogAccent
                    onClicked: section.dialogRoot.openTextColorOverrides()
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }

}
