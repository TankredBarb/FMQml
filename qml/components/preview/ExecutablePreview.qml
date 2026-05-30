import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string type: "executable"
    property string path: ""
    property string name: ""
    property string sizeText: ""
    property var extraProperties: []

    clip: true

    readonly property string titleText: type === "shortcut" ? "Shortcut" : "Application"
    readonly property string headerTitle: type === "shortcut" ? "Shortcut" : "Version Information"
    readonly property int iconSize: type === "shortcut" ? 128 : 96
    readonly property int cardSize: type === "shortcut" ? 0 : 140
    readonly property bool compactLayout: width < 560
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true

    function shortcutTarget() {
        return root.path.length > 0 ? root.displayPath(root.path) : "-"
    }

    function displayPath(path) {
        if (!path || String(path).length === 0) {
            return ""
        }
        const value = String(path)
        if (value.indexOf("archive://") === 0 || value.indexOf("devices://") === 0) {
            return value
        }
        return Qt.platform.os === "windows" ? value.replace(/\//g, "\\") : value
    }

    function nonEmptyString(value, fallback) {
        return value && String(value).length > 0 ? String(value) : fallback
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compactLayout ? 20 : 40
        spacing: root.compactLayout ? 18 : 40

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: !root.compactLayout
            visible: !root.compactLayout
            spacing: 40

            ColumnLayout {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                spacing: 16
                Layout.alignment: Qt.AlignVCenter

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: root.cardSize
                    height: root.cardSize
                    visible: root.type !== "shortcut"
                    radius: 28
                    color: themeController.isDark ? Theme.withAlpha(Theme.textPrimary, 0.05)
                                                  : Theme.withAlpha(Theme.textPrimary, 0.03)
                    border.color: Theme.panelBorder
                    border.width: 1

                    ImagePreview {
                        anchors.centerIn: parent
                        explicitSource: root.path.length > 0
                                        ? "image://icon/" + encodeURIComponent(root.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                                        : ""
                        sourceSizeWidth: root.iconSize
                        sourceSizeHeight: root.iconSize
                        showBusyIndicator: false
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.type === "shortcut"
                        source: root.path.length > 0
                                ? "image://icon/" + encodeURIComponent(root.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                                : ""
                        sourceSize: Qt.size(root.iconSize, root.iconSize)
                        smooth: true
                    }

                    Label {
                        text: root.nonEmptyString(root.name, root.type === "shortcut" ? "Shortcut" : "Application")
                        font.bold: true
                        font.pixelSize: 18
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                    }

                    Label {
                        visible: root.type !== "shortcut"
                        text: root.sizeText
                        font.pixelSize: 13
                        color: Theme.textSecondary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.8
                    }

                    Label {
                        text: root.titleText
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        font.weight: Font.Bold
                        color: Theme.accent
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.9
                    }
                }

                Item { Layout.fillHeight: true }
            }

            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: Theme.panelBorder
                opacity: 0.2
                visible: root.type !== "shortcut"
            }

            PreviewPropertiesList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: root.headerTitle
                properties: root.type === "shortcut"
                            ? [{ label: "Target", value: root.shortcutTarget() }]
                            : root.extraProperties
                rowRadius: 10
                rowPadding: 12
                labelPixelSize: 11
                valuePixelSize: 13
                rowSpacing: 10
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.compactLayout
            spacing: 14

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: root.type === "shortcut" ? 0 : 120
                height: root.type === "shortcut" ? 0 : 120
                visible: root.type !== "shortcut"
                radius: 24
                color: themeController.isDark ? Theme.withAlpha(Theme.textPrimary, 0.05)
                                              : Theme.withAlpha(Theme.textPrimary, 0.03)
                border.color: Theme.panelBorder
                border.width: 1

                ImagePreview {
                    anchors.centerIn: parent
                    explicitSource: root.path.length > 0
                                    ? "image://icon/" + encodeURIComponent(root.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                                    : ""
                    sourceSizeWidth: root.iconSize
                    sourceSizeHeight: root.iconSize
                    showBusyIndicator: false
                }
            }

            Image {
                Layout.alignment: Qt.AlignHCenter
                visible: root.type === "shortcut"
                source: root.path.length > 0
                        ? "image://icon/" + encodeURIComponent(root.path + "?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
                        : ""
                sourceSize: Qt.size(root.iconSize, root.iconSize)
                smooth: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: root.nonEmptyString(root.name, root.type === "shortcut" ? "Shortcut" : "Application")
                    font.bold: true
                    font.pixelSize: 18
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }

                Label {
                    visible: root.type !== "shortcut"
                    text: root.sizeText
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.8
                }

                Label {
                    text: root.titleText
                    font.pixelSize: 11
                    font.capitalization: Font.AllUppercase
                    font.weight: Font.Bold
                    color: Theme.accent
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.9
                }
            }

            PreviewPropertiesList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: root.headerTitle
                properties: root.type === "shortcut"
                            ? [{ label: "Target", value: root.shortcutTarget() }]
                            : root.extraProperties
                rowRadius: 10
                rowPadding: 12
                labelPixelSize: 11
                valuePixelSize: 13
                rowSpacing: 10
            }
        }
    }
}
