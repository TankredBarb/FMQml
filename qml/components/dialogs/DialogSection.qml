import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: root

    property string title: ""
    property color accentColor: Theme.accent
    property color fillColor: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.055 : 0.035)
    property color borderColor: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.22 : 0.16)
    property int radiusSize: Theme.radiusMd
    // Если true — секция растягивается по высоте (Layout.fillHeight снаружи),
    // контент тоже получает fillHeight. Используется для PreviewSection.
    property bool expandContent: false

    default property alias content: sectionContent.data

    Layout.fillWidth: true
    color: root.fillColor
    border.color: root.borderColor
    border.width: 1
    radius: root.radiusSize
    implicitHeight: expandContent ? 0 : (sectionLayout.implicitHeight + 24)

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 3
        radius: 1.5
        visible: root.title.length > 0
        color: Theme.withAlpha(root.accentColor, themeController.isDark ? 0.74 : 0.60)
    }

    ColumnLayout {
        id: sectionLayout
        anchors.fill: parent
        anchors.margins: 12
        anchors.leftMargin: root.title.length > 0 ? 18 : 12
        spacing: 6
        // При expandContent sectionLayout заполняет весь parent через anchors.fill,
        // поэтому fillHeight нужен только для sectionContent внутри него.

        Label {
            visible: root.title.length > 0
            text: root.title
            font.pixelSize: Theme.scaledSize(9)
            font.weight: Font.DemiBold
            font.letterSpacing: 1.0
            color: root.accentColor
            Layout.bottomMargin: 2
        }

        ColumnLayout {
            id: sectionContent
            Layout.fillWidth: true
            Layout.fillHeight: root.expandContent
            spacing: 4
        }
    }
}
