import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    property var paths: []
    property var itemDetails: []
    property string panelLabel: ""
    property var deleteDetails: ({})
    property var appRoot: null
    property bool confirmPending: false
    property bool asAdministrator: false

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    
    width: Math.min(parent.width * 0.9, 400)
    padding: 20

    modal: true
    focus: true
    closePolicy: root.confirmPending ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    onOpened: Qt.callLater(() => contentItem.forceActiveFocus())

    readonly property int itemCount: Array.isArray(paths) ? paths.length : 0
    readonly property int maxVisibleItems: 5
    readonly property bool hasMore: itemCount > maxVisibleItems
    readonly property bool blocked: !!deleteDetails.blocked
    readonly property bool warning: !!deleteDetails.warning
    readonly property bool requiresExplicitConfirmation: !!deleteDetails.requiresExplicitConfirmation
    readonly property string dialogTitle: deleteDetails.title || (root.itemCount === 1 ? "Delete item?" : "Delete " + root.itemCount + " items?")
    readonly property string dialogSubtitle: deleteDetails.subtitle || "This action cannot be undone."
    readonly property string detailText: deleteDetails.details || ""
    readonly property string confirmPhrase: deleteDetails.confirmPhrase || ""
    readonly property string destructiveButtonText: root.asAdministrator ? "Delete as\nAdministrator" : (deleteDetails.buttonText || "Delete Forever")
    readonly property bool useNativeIcons: typeof appSettings !== "undefined" && appSettings
                                           ? appSettings.useNativeIcons
                                           : true

    function canConfirmDelete() {
        return !root.blocked
            && (!root.requiresExplicitConfirmation
                || confirmationField.text.trim().toUpperCase() === root.confirmPhrase.toUpperCase())
    }

    function openFor(targetPaths, label, details, items, administrator) {
        root.paths = targetPaths || []
        root.itemDetails = items ? Array.from(items) : []
        root.panelLabel = label || ""
        root.deleteDetails = details || ({})
        root.asAdministrator = administrator === true
        root.confirmPending = false
        confirmDeleteTimer.stop()
        confirmationField.text = ""
        if (root.itemCount > 0) {
            root.open()
        }
    }

    function confirmDeleteAfterPreviewRelease() {
        if (root.confirmPending || !root.canConfirmDelete()) {
            return
        }
        root.confirmPending = true
        if (root.appRoot && root.appRoot.releasePreviewForPaths) {
            root.appRoot.releasePreviewForPaths(root.paths, true)
        }
        confirmDeleteTimer.restart()
    }

    Timer {
        id: confirmDeleteTimer
        interval: 60
        repeat: false
        onTriggered: {
            const confirmed = root.asAdministrator
                    ? workspaceController.confirmDeleteAsAdministrator(root.paths)
                    : workspaceController.confirmDelete(root.paths)
            root.confirmPending = false
            if (confirmed) {
                root.close()
            } else if (root.appRoot && root.appRoot.finishOperationPreviewSuppression) {
                root.appRoot.finishOperationPreviewSuppression()
            }
        }
    }

    function fileNameFor(path) {
        if (!path) return ""
        const parts = String(path).split(/[/\\]/).filter(p => p.length > 0)
        return parts.length > 0 ? parts[parts.length - 1] : path
    }

    function itemNameAt(index) {
        const path = root.paths[index]
        if (Array.isArray(root.itemDetails)) {
            const direct = root.itemDetails[index]
            if (direct && direct.path === path && direct.name) {
                return direct.name
            }
            for (let i = 0; i < root.itemDetails.length; ++i) {
                const item = root.itemDetails[i]
                if (item && item.path === path && item.name) {
                    return item.name
                }
            }
        }
        return root.fileNameFor(path)
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    background: DialogShell {
        accentColor: Theme.danger
        shellBorderColor: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.34 : 0.24)
    }

    contentItem: ColumnLayout {
        spacing: 16
        focus: true

        Keys.onPressed: (event) => {
            if (root.confirmPending) {
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if ((event.key === Qt.Key_Enter || event.key === Qt.Key_Return) && root.canConfirmDelete()) {
                root.confirmDeleteAfterPreviewRelease()
                event.accepted = true
            }
        }

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/delete.svg"
            iconTint: Theme.danger
            accentColor: Theme.danger
            title: root.dialogTitle
            subtitle: root.dialogSubtitle
            showCloseButton: false
        }

        Rectangle {
            visible: root.warning || root.blocked || root.detailText.length > 0
            Layout.fillWidth: true
            implicitHeight: warningLayout.implicitHeight + 18
            radius: Theme.radiusSm
            color: Theme.withAlpha(root.blocked ? Theme.warning : Theme.danger,
                                   themeController.isDark ? 0.10 : 0.06)
            border.width: 1
            border.color: Theme.withAlpha(root.blocked ? Theme.warning : Theme.danger, 0.30)

            ColumnLayout {
                id: warningLayout
                anchors.fill: parent
                anchors.margins: 9
                spacing: 4

                Label {
                    visible: root.blocked
                    text: "This target is protected and cannot be deleted from here."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.warning
                }

                Label {
                    visible: !root.blocked && root.warning
                    text: root.requiresExplicitConfirmation
                          ? "This is a higher-risk permanent delete. Type the confirmation phrase to continue."
                          : "Review this permanent delete carefully before continuing."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    color: Theme.danger
                }

                Label {
                    visible: root.detailText.length > 0
                    text: root.detailText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                }
            }
        }

        SurfaceCard {
            Layout.fillWidth: true
            implicitHeight: listLayout.implicitHeight + 16
            surfaceColor: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.07 : 0.04)
            strokeColor: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.24 : 0.18)

            ColumnLayout {
                id: listLayout
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Repeater {
                    model: Math.min(root.itemCount, root.maxVisibleItems)
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 28
                        radius: Theme.radiusSm
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 8

                            Image {
                                source: root.useNativeIcons
                                        ? "image://icon/" + encodeURIComponent(root.paths[index])
                                        : fileTypeIconResolver.iconForPath(root.paths[index])
                                sourceSize: Qt.size(16, 16)
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }

                            Label {
                                text: root.itemNameAt(index)
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeLabel
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }

                // "And more" indicator
                Rectangle {
                    visible: root.hasMore
                    Layout.fillWidth: true
                    height: 28
                    color: "transparent"

                    Label {
                        anchors.centerIn: parent
                        text: "... and " + (root.itemCount - root.maxVisibleItems) + " more items"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        font.italic: true
                    }
                }
            }
        }

        ColumnLayout {
            visible: root.requiresExplicitConfirmation && !root.blocked
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Type " + root.confirmPhrase + " to confirm permanent deletion."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textSecondary
            }

            PremiumTextField {
                id: confirmationField
                Layout.fillWidth: true
                placeholderText: root.confirmPhrase
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: root.blocked ? "Close" : "Cancel"
                Layout.fillWidth: true
                highlighted: false
                enabled: !root.confirmPending
                onClicked: root.close()
            }

            DialogActionButton {
                visible: !root.blocked
                text: root.destructiveButtonText
                Layout.fillWidth: true
                highlighted: true
                enabled: root.canConfirmDelete() && !root.confirmPending
                primaryColor: Theme.danger
                onClicked: root.confirmDeleteAfterPreviewRelease()
            }
        }
    }
}
