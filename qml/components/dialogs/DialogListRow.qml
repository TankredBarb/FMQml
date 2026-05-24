import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"

Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property string iconSource: ""
    property bool isLink: false
    property bool emphasizeValue: false
    property bool showValue: value.length > 0
    property color valueColor: Theme.textPrimary
    property color accentColor: Theme.accent

    Layout.fillWidth: true
    radius: Theme.radiusSm
    color: Theme.panelSurface
    border.color: Theme.panelBorder
    border.width: 1
    implicitHeight: rowLayout.implicitHeight + 12

    RowLayout {
        id: rowLayout
        anchors.fill: parent
        anchors.margins: 6
        spacing: 8

        Image {
            visible: root.iconSource.length > 0
            source: root.iconSource
            sourceSize: Qt.size(14, 14)
            smooth: true
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: root.accentColor
            }
        }

        Label {
            visible: root.label.length > 0
            text: root.label
            Layout.preferredWidth: 100
            Layout.alignment: Qt.AlignTop
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Label {
            visible: root.showValue
            text: root.value.length > 0 ? root.value : "-"
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            color: root.isLink ? Theme.accent : root.valueColor
            font.pixelSize: 12
            font.weight: root.emphasizeValue ? Font.DemiBold : Font.Normal
            elide: Text.ElideMiddle
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
        }
    }
}
