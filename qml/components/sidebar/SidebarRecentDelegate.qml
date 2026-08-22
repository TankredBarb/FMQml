import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../filepanel"
import "../../style"

ItemDelegate {
    id: row

    required property var sidebar
    required property var listView
    required property var modelData
    required property int index
    readonly property string itemName: modelData.name || ""
    readonly property string itemTargetPath: modelData.targetPath || ""
    readonly property string itemDisplayPath: modelData.displayPath || ""
    readonly property string itemSuffix: modelData.suffix || ""
    readonly property bool itemExists: modelData.exists === true
    readonly property bool itemIsDirectory: modelData.isDirectory === true
    readonly property int itemVisitCount: modelData.visitCount || 0
    readonly property bool isCurrent: listView.activeFocus && listView.currentIndex === index

    width: listView.width
    height: Math.max(44, Theme.fontSizeBody + Theme.fontSizeCaption + 18)
    padding: 0
    focusPolicy: Qt.NoFocus

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 9
        anchors.rightMargin: 9
        spacing: 9

        FileIconCell {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            path: row.itemTargetPath
            isDirectory: row.itemIsDirectory
            suffix: row.itemSuffix
            useNativeIcons: typeof appSettings !== "undefined" && appSettings ? appSettings.useNativeIcons : true
            showThumbnail: false
            iconSize: 20
            opacity: row.itemExists ? 1.0 : 0.42
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: row.itemName
                color: row.itemExists ? TextColors.sidebarText : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: row.isCurrent ? Font.Medium : Font.Normal
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: row.itemExists
                      ? row.itemVisitCount + (row.itemVisitCount === 1 ? " visit - " : " visits - ") + row.itemDisplayPath
                      : "Unavailable - " + row.itemDisplayPath
                color: row.itemExists ? Theme.textSecondary : Theme.warning
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        radius: Theme.radiusMd
        color: "transparent"
        gradient: Gradient {
            GradientStop {
                position: 0
                color: row.sidebar.sidebarStateFillTop(false, row.isCurrent, row.hovered, row.pressed)
            }
            GradientStop {
                position: 1
                color: row.sidebar.sidebarStateFillBottom(false, row.isCurrent, row.hovered, row.pressed)
            }
        }
        border.color: "transparent"
    }

    onPressed: {
        sidebar.prepareNavigation("sidebar-recent-press")
        sidebar.trapTabNavigation = false
        listView.currentIndex = index
        listView.forceActiveFocus()
    }
    onDoubleClicked: {
        if (row.itemExists) row.sidebar.openPathInActivePanel(row.itemTargetPath)
    }
}
