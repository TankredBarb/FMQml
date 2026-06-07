import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../dialogs"
import "../preview"
import "../../style"

Dialog {
    id: root

    property string messageText: ""
    property string subtitleText: "Plugin action"
    property var propertiesModel: []
    readonly property bool hasProperties: root.propertiesModel && root.propertiesModel.length > 0

    function showResult(result) {
        root.title = String(result.title || "Plugin Action")
        root.messageText = String(result.message || "Action completed.")
        root.subtitleText = String(result.subtitle || "Plugin action")
        root.propertiesModel = result.properties || []
        root.open()
    }

    modal: true
    focus: true
    parent: Overlay.overlay
    padding: 0
    width: Math.min(520, Math.max(320, (parent ? parent.width : 520) - 64))
    height: root.hasProperties
            ? Math.min(560, Math.max(360, (parent ? parent.height : 560) - 80))
            : Math.min(260, Math.max(210, (parent ? parent.height : 260) - 80))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: DialogShell {
        accentColor: Theme.categoryInfo
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/info.svg"
        iconTint: Theme.categoryInfo
        accentColor: Theme.categoryInfo
        title: root.title
        subtitle: root.subtitleText
        closeText: "x"
        onCloseRequested: root.close()
    }

    footer: DialogFooter {
        Item { Layout.fillWidth: true }

        DialogActionButton {
            text: "OK"
            highlighted: true
            primaryColor: Theme.categoryInfo
            primaryHoverColor: Theme.categoryInfo
            primaryPressedColor: Theme.categoryInfo
            onClicked: root.close()
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 10
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.close()
                event.accepted = true
            }
        }

        Label {
            visible: root.messageText.length > 0
            Layout.fillWidth: true
            Layout.topMargin: root.hasProperties ? 0 : 14
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            text: root.messageText
            color: root.hasProperties ? Theme.textSecondary : Theme.textPrimary
            font.pixelSize: root.hasProperties ? 12 : 13
            wrapMode: Text.WrapAnywhere
        }

        PreviewPropertiesList {
            visible: root.hasProperties
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 4
            properties: root.propertiesModel
            rowRadius: Theme.radiusMd
            rowPadding: 10
            labelPixelSize: 9
            valuePixelSize: 12
            rowSpacing: 6
        }

        Item {
            visible: !root.hasProperties
            Layout.fillHeight: true
        }
    }
}
