import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"

ColumnLayout {
    property bool stateMatrix: true
    property bool labVisible: false
    width: parent ? parent.width : 700
    spacing: 12

    Label { text: "Colors and Surfaces"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "colors/core · Live semantic tokens from " + themeController.schemeName + "."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "CORE TOKENS"
        GridLayout {
            Layout.fillWidth: true
            columns: width > 620 ? 4 : 2
            rowSpacing: 8
            columnSpacing: 8
            Repeater {
                model: [
                    { name: "Background", value: Theme.bg }, { name: "Surface", value: Theme.surface },
                    { name: "Panel", value: Theme.panelSurface }, { name: "Panel strong", value: Theme.panelSurfaceStrong },
                    { name: "Accent", value: Theme.accent }, { name: "Active", value: Theme.activeAccent },
                    { name: "Success", value: Theme.success }, { name: "Warning", value: Theme.warning },
                    { name: "Danger", value: Theme.danger }, { name: "Focus", value: Theme.focusRing },
                    { name: "Text", value: Theme.textPrimary }, { name: "Secondary", value: Theme.textSecondary }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 66
                    radius: Theme.radiusSm
                    color: modelData.value
                    border.color: Theme.panelBorder
                    Label { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7; text: modelData.name; color: Theme.readableOn(modelData.value, Theme.textPrimary); font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideRight }
                }
            }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "SURFACE STACK"
        Rectangle { Layout.fillWidth: true; implicitHeight: 64; radius: Theme.radiusMd; color: Theme.panelSurfaceSoft; border.color: Theme.panelBorder; Label { anchors.centerIn: parent; text: "Soft panel surface"; color: Theme.textPrimary } }
        Rectangle { Layout.fillWidth: true; implicitHeight: 64; radius: Theme.radiusMd; color: Theme.panelSurface; border.color: Theme.panelBorder; Label { anchors.centerIn: parent; text: "Panel surface"; color: Theme.textPrimary } }
        Rectangle { Layout.fillWidth: true; implicitHeight: 64; radius: Theme.radiusMd; color: Theme.panelSurfaceStrong; border.color: Theme.panelBorder; Label { anchors.centerIn: parent; text: "Strong panel surface"; color: Theme.textPrimary } }
    }
}
