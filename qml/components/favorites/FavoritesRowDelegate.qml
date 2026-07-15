import "../../style"
import "../common"
import "../filepanel"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: row

    required property var favoritesView
    required property var theme
    required property var modelData
    required property int index
    property var listView
    property bool rowPinned: true
    readonly property bool isCurrent: listView && listView.activeFocus && listView.currentIndex === index
    readonly property int actionHitMargin: rowPinned ? 160 : 68
    readonly property string favoriteId: modelData.id || ""
    readonly property string itemName: modelData.name || ""
    readonly property string itemTargetPath: modelData.targetPath || ""
    readonly property string itemDisplayPath: modelData.displayPath || ""
    readonly property string itemSuffix: modelData.suffix || ""
    readonly property var itemTags: modelData.tags || []
    readonly property string itemTagsText: favoritesView.tagsLabel(itemTags)
    readonly property bool itemExists: modelData.exists === true
    readonly property bool itemIsDirectory: modelData.isDirectory === true
    readonly property bool itemHasCustomLabel: modelData.hasCustomLabel === true
    readonly property int itemVisitCount: modelData.visitCount || 0
    readonly property real itemUsageProgress: modelData.usageProgress || 0

    width: listView ? listView.width : 1
    height: rowPinned ? 54 : 58
    padding: 0

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: row.actionHitMargin
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 2
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                const p = mapToItem(favoritesView, mouse.x, mouse.y);
                favoritesView.popupContextMenu(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath, row.itemExists, row.itemIsDirectory, row.rowPinned, p.x, p.y);
            } else {
                favoritesView.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath, row.itemExists, row.itemIsDirectory, row.rowPinned);
            }
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton && row.itemExists)
                favoritesView.openFavoriteTarget(row.itemTargetPath, row.itemIsDirectory);

        }
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 10

        FileIconCell {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            path: row.itemTargetPath
            isDirectory: row.itemIsDirectory
            suffix: row.itemSuffix
            useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
            showThumbnail: false
            iconSize: 22
            opacity: row.itemExists ? 1 : 0.45
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: row.itemName
                color: !row.itemExists ? Theme.textSecondary : row.itemHasCustomLabel ? Theme.categoryInfo : Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.maximumWidth: Math.max(80, row.width * 0.38)
                    visible: row.itemExists && row.rowPinned && row.itemTagsText.length > 0
                    text: row.itemTagsText
                    color: favoritesView.tagAccent
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        if (!row.itemExists)
                            return "Missing target - " + row.itemDisplayPath;

                        if (!row.rowPinned)
                            return favoritesView.visitLabel(row.itemVisitCount) + " - " + row.itemDisplayPath;

                        if (row.itemTagsText.length > 0)
                            return "- " + row.itemDisplayPath;

                        return row.itemDisplayPath;
                    }
                    color: row.itemExists ? Theme.textSecondary : Theme.warning
                    font.pixelSize: Theme.fontSizeCaption
                    elide: Text.ElideRight
                }

            }

            LinearProgress {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                visible: !row.rowPinned
                value: row.itemUsageProgress
                trackColor: Theme.withAlpha(Theme.panelBorder, theme.isDark ? 0.42 : 0.55)
                fillColor: Theme.categoryUtility
                preserveMinimumFill: true
            }

        }

        IconButton {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            visible: row.itemExists
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/folder-open.svg"
            iconTone: "open"
            iconSize: 15
            onClicked: favoritesView.openFavoriteTarget(row.itemTargetPath, row.itemIsDirectory)
            ToolTip.visible: hovered
            ToolTip.text: "Open in panel"
        }

        IconButton {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            visible: row.rowPinned
            enabled: row.itemTargetPath.length > 0
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/rename.svg"
            iconTone: "rename"
            iconSize: 15
            onClicked: {
                favoritesView.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath, row.itemExists, row.itemIsDirectory, row.rowPinned);
                favoritesView.editSelectedPinnedLabel();
            }
            ToolTip.visible: hovered
            ToolTip.text: "Edit Label"
        }

        IconButton {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            visible: row.rowPinned
            enabled: row.itemTargetPath.length > 0
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/tag.svg"
            iconTone: "action"
            svgRecolorColor: favoritesView.tagAccent
            iconSize: 15
            onClicked: {
                favoritesView.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath, row.itemExists, row.itemIsDirectory, row.rowPinned);
                favoritesView.editSelectedPinnedTags();
            }
            ToolTip.visible: hovered
            ToolTip.text: "Edit Tags"
        }

        IconButton {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            visible: row.rowPinned
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/star-off.svg"
            iconTone: "favorite"
            iconSize: 15
            onClicked: {
                favoritesView.selectRow(row.listView, index, row.favoriteId, row.itemName, row.itemTargetPath, row.itemExists, row.itemIsDirectory, row.rowPinned);
                favoritesView.removeFavorite(row.itemTargetPath);
            }
            ToolTip.visible: hovered
            ToolTip.text: "Unpin from Favorites"
        }

    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: {
            if (row.pressed)
                return Theme.surfaceActive;

            if (row.isCurrent)
                return Theme.itemCurrentFill;

            if (row.hovered)
                return Theme.itemHoverFill;

            return Theme.panelSurfaceSoft;
        }
        border.color: {
            if (row.isCurrent)
                return Theme.itemCurrentBorder;

            return row.itemExists ? Theme.panelBorder : Theme.withAlpha(Theme.warning, 0.36);
        }
        border.width: row.isCurrent ? 2 : 1
    }

}
