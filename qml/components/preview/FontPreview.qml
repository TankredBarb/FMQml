import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string path: ""
    property string name: ""
    property string sizeText: ""
    property string modifiedText: ""
    property string extension: ""
    property var extraProperties: []
    property bool compact: false
    property bool showDetails: false
    property int samplePixelSize: compact ? 48 : 78
    readonly property int defaultSamplePixelSize: compact ? 48 : 78
    readonly property string loadedFamily: fontLoader.status === FontLoader.Ready ? fontLoader.name : ""
    readonly property string displayFamily: familyText.length > 0 ? familyText : (name.length > 0 ? name : "Font")
    readonly property string familyText: extraValue("Family")
    readonly property string styleText: extraValue("Style")
    readonly property string weightText: extraValue("Weight")
    readonly property string ascentText: extraValue("Ascent").length > 0 ? "Ascent " + extraValue("Ascent") : ""
    readonly property string descentText: extraValue("Descent").length > 0 ? "Descent " + extraValue("Descent") : ""
    readonly property color paperColor: themeController.isDark ? Qt.rgba(0.96, 0.97, 0.94, 1.0)
                                                              : Qt.rgba(1.0, 1.0, 1.0, 1.0)
    readonly property color inkColor: Qt.rgba(0.08, 0.09, 0.10, 1.0)

    clip: true

    function safeText(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function extraValue(label) {
        const extras = Array.isArray(root.extraProperties) ? root.extraProperties : []
        for (let i = 0; i < extras.length; i++) {
            if (safeText(extras[i].label) === label) {
                return safeText(extras[i].value)
            }
        }
        return ""
    }

    function adjustSample(delta) {
        samplePixelSize = Math.max(28, Math.min(120, samplePixelSize + delta))
    }

    FontLoader {
        id: fontLoader
        source: root.path.length > 0 ? "file:///" + root.path.replace(/\\/g, "/") : ""
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 10 : 18
        spacing: root.compact ? 10 : 14

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.compact ? 128 : 220
            radius: Theme.panelRadius
            color: root.paperColor
            border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.44 : 0.34)
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.compact ? 14 : 22
                spacing: root.compact ? 8 : 12

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.showDetails
                    spacing: 6

                    TextControlButton {
                        text: "A-"
                        enabled: root.samplePixelSize > 28
                        onClicked: root.adjustSample(-4)
                    }

                    TextControlButton {
                        text: "A+"
                        enabled: root.samplePixelSize < 120
                        onClicked: root.adjustSample(4)
                    }

                    TextControlButton {
                        text: "Reset"
                        implicitWidth: 46
                        enabled: root.samplePixelSize !== root.defaultSamplePixelSize
                        onClicked: root.samplePixelSize = root.defaultSamplePixelSize
                    }

                    Item { Layout.fillWidth: true }
                }

                PreviewMetaStrip {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(parent.width, root.compact ? 250 : 420)
                    compact: root.compact
                    accentColor: Theme.categoryInfo
                    items: [root.familyText, root.weightText, root.ascentText, root.descentText]
                }

                Label {
                    Layout.fillWidth: true
                    text: "Aa"
                    font.family: root.loadedFamily.length > 0 ? root.loadedFamily : font.family
                    font.pixelSize: root.samplePixelSize
                    font.weight: Font.Normal
                    color: root.inkColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    Layout.fillWidth: true
                    text: "The quick brown fox jumps over the lazy dog"
                    font.family: root.loadedFamily.length > 0 ? root.loadedFamily : font.family
                    font.pixelSize: root.compact ? Theme.scaledSize(15) : Theme.scaledSize(22)
                    color: root.inkColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: !root.compact
                    text: "ABCDEFGHIJKLMNOPQRSTUVWXYZ  0123456789"
                    font.family: root.loadedFamily.length > 0 ? root.loadedFamily : font.family
                    font.pixelSize: Theme.scaledSize(15)
                    color: Theme.withAlpha(root.inkColor, 0.72)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    text: root.displayFamily
                    font.pixelSize: root.compact ? Theme.fontSizeLabel : Theme.scaledSize(15)
                    font.bold: true
                    color: root.inkColor
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: root.styleText.length > 0 ? root.styleText : (root.weightText.length > 0 ? root.weightText : root.sizeText)
                    font.pixelSize: root.compact ? Theme.fontSizeMicro : Theme.fontSizeLabel
                    color: Theme.withAlpha(root.inkColor, 0.68)
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        PreviewPropertiesList {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            visible: root.showDetails
            title: "Font"
            properties: root.extraProperties
            rowRadius: Theme.radiusMd
            rowPadding: 10
            labelPixelSize: 10
            valuePixelSize: 12
            rowSpacing: 7
        }
    }

    component TextControlButton: Button {
        id: controlButton

        implicitWidth: 30
        implicitHeight: 28
        padding: 0
        hoverEnabled: true

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? root.inkColor : Theme.withAlpha(root.inkColor, 0.42)
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: controlButton.down
                   ? Theme.withAlpha(root.inkColor, 0.16)
                   : (controlButton.hovered ? Theme.withAlpha(root.inkColor, 0.08) : "transparent")
            border.color: controlButton.hovered ? Theme.withAlpha(root.inkColor, 0.22) : "transparent"
            border.width: controlButton.hovered ? 1 : 0
        }
    }
}
