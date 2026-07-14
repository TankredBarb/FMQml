import "../../style"
import "../common"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: placeDelegate

    required property var sidebar
    required property var listView
    required property var workspace
    required property var theme
    required property int index
    required property var modelData
    required property var visualSection
    required property var showSectionHeader
    required property var path
    required property var subtitle
    required property var isDrive
    required property var isReady
    required property var totalSpace
    required property var freeSpace
    required property var fileSystem
    required property var driveType
    required property var name
    required property var usagePercent
    required property var isCritical
    required property var canEject
    required property var canUnmount
    required property var canSafelyRemove
    required property var canMount
    required property var mountId
    required property var actionPending
    readonly property var placeIcon: modelData.icon
    readonly property color sectionTone: sidebar.placeSectionTone(visualSection)
    readonly property string secondaryText: sidebar.placeSecondaryText(visualSection, path, subtitle, isDrive, isReady, totalSpace, freeSpace, fileSystem, driveType)
    readonly property bool hasSecondaryText: secondaryText.length > 0
    readonly property int secondaryLineCount: secondaryText.indexOf("\n") >= 0 ? 2 : 1
    readonly property bool showUsage: isDrive && isReady && Number(totalSpace || 0) > 0
    readonly property int rowHeight: hasSecondaryText || showUsage ? sidebar.placeExpandedRowHeight + (secondaryLineCount > 1 ? sidebar.placeSecondaryFontSize + 2 : 0) : sidebar.placeCompactRowHeight
    readonly property bool isActive: sidebar.selectedPlaceIndex === index
    readonly property bool hasKeyboardCurrent: listView.activeFocus && listView.currentIndex === index

    width: listView.width
    height: (showSectionHeader ? sidebar.placeSectionHeaderHeight : 0) + rowHeight
    padding: 0
    focusPolicy: Qt.NoFocus

    background: Item {
    }

    contentItem: Item {
        anchors.fill: parent

        SidebarPlacesSectionHeader {
            id: placeSectionHeader

            sidebar: placeDelegate.sidebar
            width: parent.width
            height: placeDelegate.showSectionHeader ? sidebar.placeSectionHeaderHeight : 0
            visible: placeDelegate.showSectionHeader
            label: sidebar.placeSectionLabel(placeDelegate.visualSection)
            tone: placeDelegate.sectionTone
        }

        Rectangle {
            id: placeRowBg

            anchors.left: parent.left
            anchors.right: parent.right
            y: placeSectionHeader.height
            height: placeDelegate.rowHeight
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            radius: Theme.radiusMd
            color: sidebar.sidebarStateFill(placeDelegate.isActive, placeDelegate.hasKeyboardCurrent, placeMouse.containsMouse, placeMouse.pressed)
            border.color: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent ? Theme.withAlpha(placeDelegate.sectionTone, theme.isDark ? 0.42 : 0.3) : "transparent"
            border.width: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent ? 1 : 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: sidebar.placeHorizontalPadding
                anchors.rightMargin: sidebar.placeHorizontalPadding
                anchors.topMargin: placeDelegate.hasSecondaryText ? sidebar.placeSecondaryVerticalMargin : 0
                anchors.bottomMargin: placeDelegate.showUsage ? sidebar.placeUsageBottomMargin + sidebar.placeUsageBarHeight : (placeDelegate.hasSecondaryText ? sidebar.placeSecondaryVerticalMargin : 0)
                spacing: sidebar.placeRowSpacing

                RecolorSvgIcon {
                    Layout.preferredWidth: sidebar.placeIconSize
                    Layout.preferredHeight: sidebar.placeIconSize
                    Layout.minimumWidth: sidebar.placeIconSize
                    Layout.minimumHeight: sidebar.placeIconSize
                    Layout.maximumWidth: sidebar.placeIconSize
                    Layout.maximumHeight: sidebar.placeIconSize
                    sourcePath: sidebar.resolvedIconSourceFor(placeIcon)
                    recolorColor: sidebar.iconToneFor(placeIcon, placeDelegate.isActive || placeDelegate.hasKeyboardCurrent, false)
                    recolorEnabled: placeIcon !== "gdrive" && placeIcon !== "mega" && placeIcon !== "telegram"
                    cacheKey: "sidebar"
                    sourceSize: Qt.size(sidebar.placeIconSize * 2, sidebar.placeIconSize * 2)
                    asynchronous: true
                    cache: true
                    opacity: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent || placeMouse.containsMouse ? 1 : 0.88
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 1

                    Label {
                        text: name
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        font.family: Theme.fontFamily
                        font.pixelSize: sidebar.placePrimaryFontSize
                        font.weight: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent || placeMouse.containsMouse ? Font.Medium : Font.Normal
                        color: TextColors.sidebarText
                        opacity: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent || placeMouse.containsMouse ? 1 : 0.94
                        elide: Text.ElideRight
                    }

                    Label {
                        text: placeDelegate.secondaryText
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        visible: placeDelegate.hasSecondaryText
                        font.family: Theme.fontFamily
                        font.pixelSize: sidebar.placeSecondaryFontSize
                        color: Theme.textSecondary
                        opacity: placeDelegate.isActive || placeDelegate.hasKeyboardCurrent || placeMouse.containsMouse ? 0.88 : 0.7
                        maximumLineCount: placeDelegate.secondaryLineCount
                        wrapMode: Text.NoWrap
                        elide: Text.ElideRight
                    }

                }

            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.bottomMargin: sidebar.placeUsageBottomMargin
                height: sidebar.placeUsageBarHeight
                radius: sidebar.placeUsageBarHeight
                visible: placeDelegate.showUsage
                color: Theme.withAlpha(Theme.panelBorder, theme.isDark ? 0.26 : 0.2)

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, Number(usagePercent || 0)))
                    height: parent.height
                    radius: parent.radius
                    color: sidebar.usageColor(usagePercent, isCritical)
                }

            }

            Behavior on color {
                enabled: !sidebar.effectsReduced

                ColorAnimation {
                    duration: Theme.motionFast
                }

            }

        }

        MouseArea {
            id: placeMouse

            x: 0
            y: placeRowBg.y
            width: parent.width
            height: placeRowBg.height
            hoverEnabled: !sidebar.effectsReduced
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            z: 10
            onPressed: {
                sidebar.prepareNavigation("sidebar-place-press");
                sidebar.selectPlace(index);
            }
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton)
                    sidebar.openPlaceDriveMenu(index, sidebar.placePathAt(index), driveType, canEject, canUnmount, canSafelyRemove, canMount, mountId, actionPending, isDrive);

                mouse.accepted = true;
            }
            onDoubleClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    if (canMount)
                        workspace.requestMountVolume(mountId);
                    else
                        sidebar.openPathInActivePanel(sidebar.placePathAt(index));
                }
                mouse.accepted = true;
            }
        }

    }

}
