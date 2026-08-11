import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../framework"
import "../dialogs"

ColumnLayout {
    property bool stateMatrix: true
    property bool labVisible: false
    width: parent ? parent.width : 700
    spacing: 12

    Label { text: "Tabs and Navigation"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "tabs/matrix · Change tabs with pointer or keyboard focus and inspect highlight movement."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "TAB COUNTS"
        FmTabBar { Layout.fillWidth: true; model: ["General", "Details"] }
        FmTabBar { Layout.fillWidth: true; model: ["One", "Two", "Three", "Four"]; currentIndex: 1 }
        FmTabBar { visible: stateMatrix; Layout.fillWidth: true; model: ["Overview", "A deliberately long navigation label", "Activity"]; currentIndex: 2 }
    }

    DialogSection {
        title: "OBJECT MODEL"
        FmTabBar {
            id: objectTabs
            Layout.fillWidth: true
            model: [{ text: "Files", value: "files" }, { text: "Preview", value: "preview" }, { text: "Activity", value: "activity" }]
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 72
            radius: Theme.radiusSm
            color: Theme.panelSurfaceSoft
            border.color: Theme.panelBorder
            Label { anchors.centerIn: parent; text: "Selected route: " + objectTabs.model[objectTabs.currentIndex].value; color: Theme.textPrimary }
        }
    }
}
