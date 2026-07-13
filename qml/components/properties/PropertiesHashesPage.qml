import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"

ScrollView {
                    id: page
                    required property int currentIndex
    required property bool pageVisible
    required property var calculator
    required property string targetPath
    required property string allHashesText
    required property var copyText
    required property var tabContentY
    readonly property real contentImplicitHeight: contentLayout.implicitHeight

    function hasAnyHashResult() {
        return calculator.md5 !== "" || calculator.sha1 !== "" || calculator.sha256 !== ""
    }

    anchors.fill: parent
                    visible: page.pageVisible
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true
                    enabled: page.currentIndex === 4

                    opacity: page.currentIndex === 4 ? 1.0 : 0.0
                    z: page.currentIndex === 4 ? 1 : 0
                    transform: Translate {
                        x: page.currentIndex === 4 ? 0 : (4 < page.currentIndex ? -400 : 400)
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
                            title: "FILE CHECKSUMS"

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    ProgressRing {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        visible: page.calculator.busy
                                        running: page.calculator.busy
                                        value: page.calculator.progress
                                        accentColor: Theme.accent
                                    }

                                    Label {
                                        text: page.calculator.busy
                                              ? "Calculating " + Math.floor(page.calculator.progress * 100) + "%"
                                              : "Calculate hashes for this file and copy deterministic output with file context."
                                        Layout.fillWidth: true
                                        color: page.calculator.busy ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.weight: page.calculator.busy ? Font.Medium : Font.Normal
                                        elide: Text.ElideRight
                                    }

                                    Button {
                                        id: copyAllHashesButton
                                        text: "Copy All"
                                        enabled: page.hasAnyHashResult()
                                        visible: !page.calculator.busy

                                        contentItem: Label {
                                            text: copyAllHashesButton.text
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.weight: Font.Medium
                                            color: copyAllHashesButton.enabled ? Theme.textPrimary : Theme.textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        background: Rectangle {
                                            implicitWidth: 82
                                            implicitHeight: 30
                                            radius: Theme.radiusSm
                                            color: copyAllHashesButton.enabled
                                                   ? (copyAllHashesButton.hovered ? Theme.panelSurfaceSoft : Theme.panelSurface)
                                                   : Theme.panelBorder
                                            border.color: Theme.panelBorder
                                            border.width: 1
                                        }

                                        onClicked: page.copyText(page.allHashesText)
                                    }

                                    Button {
                                        id: cancelHashesButton
                                        text: "Cancel"
                                        visible: page.calculator.busy

                                        contentItem: Label {
                                            text: cancelHashesButton.text
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.weight: Font.Medium
                                            color: Theme.warning
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        background: Rectangle {
                                            implicitWidth: 74
                                            implicitHeight: 30
                                            radius: Theme.radiusSm
                                            color: cancelHashesButton.hovered ? Theme.panelSurfaceSoft : Theme.panelSurface
                                            border.color: Theme.withAlpha(Theme.warning, 0.45)
                                            border.width: 1
                                        }

                                        onClicked: page.calculator.abort()
                                    }
                                }

                                // MD5 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "MD5"
                                        font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: page.calculator.md5
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                            background: Rectangle {
                                                color: Theme.panelSurfaceSoft
                                                radius: Theme.radiusSm
                                                border.color: Theme.panelBorder; border.width: 1
                                            }
                                        }

                                        Button {
                                            id: md5CalculateButton
                                            text: "Calculate"
                                            visible: page.calculator.md5 === ""
                                            enabled: !page.calculator.busy

                                            contentItem: Label {
                                                text: md5CalculateButton.text
                                                font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                                color: md5CalculateButton.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: md5CalculateButton.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: page.calculator.calculate(page.targetPath, "md5")
                                        }

                                        Button {
                                            id: md5CopyButton
                                            visible: page.calculator.md5 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: md5CopyButton.pressed ? Theme.surfaceActive : (md5CopyButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: RecolorSvgIcon {
                                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                                                recolorColor: Theme.textSecondary
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                            }
                                            onClicked: page.copyText(page.calculator.md5)
                                        }
                                    }
                                }

                                // SHA-1 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "SHA-1"
                                        font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: page.calculator.sha1
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                            background: Rectangle {
                                                color: Theme.panelSurfaceSoft
                                                radius: Theme.radiusSm
                                                border.color: Theme.panelBorder; border.width: 1
                                            }
                                        }

                                        Button {
                                            id: sha1CalculateButton
                                            text: "Calculate"
                                            visible: page.calculator.sha1 === ""
                                            enabled: !page.calculator.busy

                                            contentItem: Label {
                                                text: sha1CalculateButton.text
                                                font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                                color: sha1CalculateButton.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: sha1CalculateButton.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: page.calculator.calculate(page.targetPath, "sha1")
                                        }

                                        Button {
                                            id: sha1CopyButton
                                            visible: page.calculator.sha1 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: sha1CopyButton.pressed ? Theme.surfaceActive : (sha1CopyButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: RecolorSvgIcon {
                                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                                                recolorColor: Theme.textSecondary
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                            }
                                            onClicked: page.copyText(page.calculator.sha1)
                                        }
                                    }
                                }

                                // SHA-256 Row
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Label {
                                        text: "SHA-256"
                                        font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        TextField {
                                            text: page.calculator.sha256
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                            background: Rectangle {
                                                color: Theme.panelSurfaceSoft
                                                radius: Theme.radiusSm
                                                border.color: Theme.panelBorder; border.width: 1
                                            }
                                        }

                                        Button {
                                            id: sha256CalculateButton
                                            text: "Calculate"
                                            visible: page.calculator.sha256 === ""
                                            enabled: !page.calculator.busy

                                            contentItem: Label {
                                                text: sha256CalculateButton.text
                                                font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                                color: sha256CalculateButton.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                implicitWidth: 80; implicitHeight: 32
                                                radius: Theme.radiusSm
                                                color: sha256CalculateButton.enabled ? Theme.accent : Theme.panelBorder
                                            }

                                            onClicked: page.calculator.calculate(page.targetPath, "sha256")
                                        }

                                        Button {
                                            id: sha256CopyButton
                                            visible: page.calculator.sha256 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            flat: true
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: sha256CopyButton.pressed ? Theme.surfaceActive : (sha256CopyButton.hovered ? Theme.panelSurfaceSoft : "transparent")
                                            }
                                            contentItem: RecolorSvgIcon {
                                                sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/clipboard-copy.svg"
                                                recolorColor: Theme.textSecondary
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                            }
                                            onClicked: page.copyText(page.calculator.sha256)
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }
