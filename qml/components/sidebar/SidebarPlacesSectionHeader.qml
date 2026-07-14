import "../../style"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: sectionRoot

    required property var sidebar
    property string label: ""
    property color tone: Theme.accent

    width: parent ? parent.width : 0
    height: sidebar.placeSectionHeaderHeight

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        spacing: 8

        Label {
            text: sectionRoot.label
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            font.letterSpacing: 0
            color: Theme.textSecondary
            opacity: 0.78
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.panelStrokeSubtle
        }

    }

}
