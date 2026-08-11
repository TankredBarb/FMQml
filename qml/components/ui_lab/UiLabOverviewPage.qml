import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../dialogs"
import "../framework"

ColumnLayout {
    id: pageRoot

    property bool stateMatrix: true
    property bool labVisible: false
    property var pageRegistry: []
    signal pageRequested(string pageId)
    width: parent ? parent.width : 700
    spacing: 12

    Label { text: "UI Lab Overview"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label { Layout.fillWidth: true; text: "overview/matrix · A deterministic visual QA surface built exclusively from production FM controls."; color: Theme.textSecondary; wrapMode: Text.WordWrap }

    DialogSection {
        title: "CURRENT ENVIRONMENT"
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 8
            Label { text: "Theme"; color: Theme.textSecondary }
            Label { Layout.fillWidth: true; text: themeController.schemeName + " (" + (themeController.isDark ? "Dark" : "Light") + ")"; color: Theme.textPrimary }
            Label { text: "Font"; color: Theme.textSecondary }
            Label { Layout.fillWidth: true; text: Theme.fontFamily; color: Theme.textPrimary; elide: Text.ElideRight }
            Label { text: "State matrix"; color: Theme.textSecondary }
            Label { Layout.fillWidth: true; text: stateMatrix ? "Expanded states visible" : "Primary states only"; color: Theme.textPrimary }
        }
    }

    DialogSection {
        title: "AVAILABLE COVERAGE"
        GridLayout {
            Layout.fillWidth: true
            columns: width > 620 ? 2 : 1
            rowSpacing: 8
            columnSpacing: 12
            Repeater {
                model: pageRoot.pageRegistry.filter(page => page.id !== "overview")
                delegate: FmButton {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.title
                    onClicked: pageRoot.pageRequested(modelData.id)
                }
            }
        }
    }

    DialogSection {
        visible: stateMatrix
        title: "TESTING NOTES"
        Label { Layout.fillWidth: true; text: "Use Narrow through Full Desktop to inspect responsive geometry. Hover and pressed states remain interactive; deterministic states are shown side by side where the production API supports them."; color: Theme.textSecondary; wrapMode: Text.WordWrap }
    }
}
