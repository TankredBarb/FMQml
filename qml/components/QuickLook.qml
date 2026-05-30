import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"
import "preview"

Popup {
    id: root

    property string previewPath: ""
    readonly property bool useHighQualitySystemIcons: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.useHighQualitySystemIcons
                                                      : true
    readonly property string displayPath: root.previewPath.length > 0 ? root.previewPath : quickLookController.path

    function displayTitle() {
        if (quickLookController.name.length > 0) {
            return quickLookController.name
        }
        if (root.displayPath.length === 0) {
            return "Preview"
        }
        if (root.displayPath === "devices://") {
            return "Devices and Drives"
        }
        if (root.displayPath === "favorites://") {
            return "Favorites"
        }

        const parts = root.displayPath.split(/[/\\]/)
        const tail = parts.length > 0 ? parts[parts.length - 1] : root.displayPath
        return tail.length > 0 ? tail : root.displayPath
    }

    function displayIconSource() {
        if (root.displayPath.length === 0 || root.displayPath === "devices://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
        }
        if (root.displayPath === "favorites://") {
            return "qrc:/qt/qml/FM/qml/assets/icons/star.svg"
        }
        const query = quickLookController.directory
            ? ("?directory=true&hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
            : ("?hq=" + (root.useHighQualitySystemIcons ? "1" : "0"))
        return "image://icon/" + encodeURIComponent(root.displayPath + query)
    }

    function displaySubtitle() {
        if (quickLookController.mimeName === "drive") {
            return quickLookController.extension.length > 0 ? quickLookController.extension.toUpperCase() : "Drive Preview"
        }
        if (quickLookController.type.length === 0) {
            return "Preview"
        }
        return quickLookController.type.toUpperCase() + " Preview"
    }

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.84, 960)
    height: Math.min(parent.height * 0.84, 720)
    
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 250; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.95; duration: 150; easing.type: Easing.InCubic }
    }

    background: DialogShell {}

    contentItem: ColumnLayout {
        spacing: 0
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.close()
                event.accepted = true
            }
        }

        PreviewHeader {
            Layout.fillWidth: true
            iconSource: root.displayIconSource()
            title: root.displayTitle()
            subtitle: root.displaySubtitle()
            closeIconSource: "qrc:/qt/qml/FM/qml/assets/lucide-toolbar/eye-off.svg"
            closeIconTint: Theme.textSecondary
            closeIconTintHover: Theme.textPrimary
            onCloseRequested: root.close()
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.panelBorder
            opacity: themeController.isDark ? 0.34 : 0.26
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            PreviewRenderer {
                anchors.fill: parent
                mode: "quicklook"
                path: root.displayPath
                type: quickLookController.type
                name: quickLookController.name
                mimeName: quickLookController.mimeName
                extension: quickLookController.extension
                directory: quickLookController.directory
                sizeText: quickLookController.sizeText
                modifiedText: quickLookController.modifiedText
                absolutePath: quickLookController.absolutePath
                hidden: quickLookController.hidden
                symlink: quickLookController.symlink
                permissionsText: quickLookController.permissionsText
                attributesText: quickLookController.attributesText
                content: quickLookController.content
                lineCount: quickLookController.lines
                loading: quickLookController.loading
                extraProperties: quickLookController.extraProperties
                hasPdfSupport: quickLookController.hasPdfSupport
                sourceSizeWidth: 2048
                sourceSizeHeight: 2048
            }
        }
    }

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())
}
