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

    Label { text: "Typography"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "typography/scale · Active family: " + Theme.fontFamily; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "TYPE SCALE"
        Repeater {
            model: [
                { name: "Title", size: Theme.fontSizeTitle },
                { name: "Subtitle", size: Theme.fontSizeSubtitle },
                { name: "Body large", size: Theme.fontSizeBodyLarge },
                { name: "Label", size: Theme.fontSizeLabel },
                { name: "Caption", size: Theme.fontSizeCaption },
                { name: "Micro", size: Theme.fontSizeMicro }
            ]
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { Layout.preferredWidth: 100; text: modelData.name; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                Label { Layout.fillWidth: true; text: "The quick brown fox jumps over the lazy dog"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: modelData.size; elide: Text.ElideRight }
                Label { text: modelData.size + " px"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro }
            }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "WRAPPING AND EMPHASIS"
        Label { Layout.fillWidth: true; text: "Regular — A deterministic paragraph demonstrating wrapping across every viewport preset without depending on user files or remote content."; color: Theme.textPrimary; wrapMode: Text.WordWrap }
        Label { Layout.fillWidth: true; text: "Medium — Interface labels and important values"; color: Theme.textPrimary; font.weight: Font.Medium; wrapMode: Text.WordWrap }
        Label { Layout.fillWidth: true; text: "DemiBold — Section and navigation emphasis"; color: Theme.textPrimary; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
        Label { Layout.fillWidth: true; text: "Secondary text must remain readable on the selected scene background."; color: Theme.textSecondary; wrapMode: Text.WordWrap }
    }
}
