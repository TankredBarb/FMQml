import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../framework"
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
                    ScrollBar.vertical: FmScrollBar {
                        id: pageScrollBar
                        parent: page.contentItem
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        policy: ScrollBar.AsNeeded
                    }
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
                        width: pageScrollBar.scrollNeeded
                               ? Math.max(0, pageScrollBar.x - x - 6)
                               : page.availableWidth - 32
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

                                    FmButton {
                                        id: copyAllHashesButton
                                        text: "Copy All"
                                        enabled: page.hasAnyHashResult()
                                        visible: !page.calculator.busy
                                        implicitWidth: 82
                                        implicitHeight: 30

                                        onClicked: page.copyText(page.allHashesText)
                                    }

                                    FmButton {
                                        id: cancelHashesButton
                                        text: "Cancel"
                                        visible: page.calculator.busy
                                        implicitWidth: 74
                                        implicitHeight: 30
                                        secondaryTextColor: Theme.warning
                                        primaryColor: Theme.warning

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

                                        FmTextField {
                                            text: page.calculator.md5
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                        }

                                        FmButton {
                                            id: md5CalculateButton
                                            text: "Calculate"
                                            visible: page.calculator.md5 === ""
                                            enabled: !page.calculator.busy
                                            highlighted: true
                                            implicitWidth: 80
                                            implicitHeight: 32

                                            onClicked: page.calculator.calculate(page.targetPath, "md5")
                                        }

                                        FmIconButton {
                                            id: md5CopyButton
                                            visible: page.calculator.md5 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
                                            iconSize: 14
                                            svgRecolorColor: Theme.textSecondary
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

                                        FmTextField {
                                            text: page.calculator.sha1
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                        }

                                        FmButton {
                                            id: sha1CalculateButton
                                            text: "Calculate"
                                            visible: page.calculator.sha1 === ""
                                            enabled: !page.calculator.busy

                                            highlighted: true
                                            implicitWidth: 80
                                            implicitHeight: 32

                                            onClicked: page.calculator.calculate(page.targetPath, "sha1")
                                        }

                                        FmIconButton {
                                            id: sha1CopyButton
                                            visible: page.calculator.sha1 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
                                            iconSize: 14
                                            svgRecolorColor: Theme.textSecondary
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

                                        FmTextField {
                                            text: page.calculator.sha256
                                            readOnly: true
                                            placeholderText: "Not calculated"
                                            placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                            font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                            Layout.fillWidth: true
                                            color: Theme.textPrimary
                                            selectByMouse: true
                                            leftPadding: 10
                                        }

                                        FmButton {
                                            id: sha256CalculateButton
                                            text: "Calculate"
                                            visible: page.calculator.sha256 === ""
                                            enabled: !page.calculator.busy

                                            highlighted: true
                                            implicitWidth: 80
                                            implicitHeight: 32

                                            onClicked: page.calculator.calculate(page.targetPath, "sha256")
                                        }

                                        FmIconButton {
                                            id: sha256CopyButton
                                            visible: page.calculator.sha256 !== ""
                                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
                                            iconSize: 14
                                            svgRecolorColor: Theme.textSecondary
                                            onClicked: page.copyText(page.calculator.sha256)
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4; Layout.fillWidth: true }
                    }
                }
