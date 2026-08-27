import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../common"
import "../dialogs"
import "../framework"

ColumnLayout {
    id: pageRoot

    property bool stateMatrix: true
    property bool labVisible: false
    property int viewMode: 0
    property int previewState: 0
    property bool translucent: true
    property bool blurEnabled: true
    property int selectedEntry: 2
    width: parent ? parent.width : 700
    spacing: 12

    readonly property var entries: [
        { name: "Applications", folder: true, icon: "folder.svg", meta: "12 items" },
        { name: "Design references", folder: true, icon: "folder.svg", meta: "34 items" },
        { name: "cover-concept.png", folder: false, icon: "image.svg", meta: "1.8 MiB" },
        { name: "Documents", folder: true, icon: "folder.svg", meta: "8 items" },
        { name: "release-notes.md", folder: false, icon: "document.svg", meta: "14 KiB" },
        { name: "Downloads", folder: true, icon: "folder.svg", meta: "9+ items" },
        { name: "A deliberately long folder name for constrained geometry", folder: true, icon: "folder.svg", meta: "6 items" },
        { name: "palette.svg", folder: false, icon: "image.svg", meta: "42 KiB" },
        { name: "Archive", folder: true, icon: "folder.svg", meta: "3 items" }
    ]

    function stateLabel() {
        return ["Ready", "Loading", "Empty", "Unavailable"][previewState]
    }

    Label { text: "Hover Previews and Folder Peek"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTitle; font.weight: Font.DemiBold }
    Label {
        Layout.fillWidth: true
        text: "hover-preview/interactive · Deterministic media and folder hover cards plus a pinned folder navigation surface. No filesystem access is performed."
        color: Theme.textSecondary
        wrapMode: Text.WordWrap
    }

    DialogSection {
        title: "SCENE CONTROLS"
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "View"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
            FmComboBox {
                Layout.preferredWidth: 130
                model: ["Grid", "List"]
                currentIndex: pageRoot.viewMode
                onActivated: index => pageRoot.viewMode = index
            }
            Label { text: "State"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
            FmComboBox {
                Layout.preferredWidth: 150
                model: ["Ready", "Loading", "Empty", "Unavailable"]
                currentIndex: pageRoot.previewState
                onActivated: index => pageRoot.previewState = index
            }
            FmSwitch { text: "Translucent"; checked: pageRoot.translucent; onToggled: pageRoot.translucent = checked }
            FmSwitch { text: "Blur"; checked: pageRoot.blurEnabled; enabled: pageRoot.translucent; onToggled: pageRoot.blurEnabled = checked }
            Item { Layout.fillWidth: true }
            InlineBadge { text: pageRoot.stateLabel(); textColor: pageRoot.previewState === 3 ? Theme.warning : Theme.categoryInfo }
        }
    }

    DialogSection {
        title: "MEDIA HOVER PREVIEW"

        Item {
            id: mediaScene
            Layout.fillWidth: true
            Layout.preferredHeight: 382
            clip: true

            Rectangle {
                id: mediaBackdrop
                anchors.fill: parent
                radius: Theme.radiusMd
                color: Theme.panelSurfaceStrong
                border.color: Theme.panelBorder

                Repeater {
                    model: 20
                    Rectangle {
                        required property int index
                        width: 94
                        height: 68
                        radius: 7
                        x: 18 + (index % 5) * Math.max(104, (mediaBackdrop.width - 48) / 5)
                        y: 18 + Math.floor(index / 5) * 84
                        color: Theme.withAlpha(index % 2 === 0 ? Theme.warmAccent : Theme.categoryInfo,
                                               themeController.isDark ? 0.14 : 0.10)
                        border.color: Theme.withAlpha(Theme.panelBorder, 0.55)
                    }
                }
            }

            TranslucentSurface {
                width: Math.min(304, parent.width - 28)
                height: 330
                x: Math.max(14, parent.width - width - 18)
                y: 26
                translucent: pageRoot.translucent
                active: true
                backgroundBlurEnabled: pageRoot.blurEnabled
                blurStrength: 72
                backdropSource: mediaBackdrop
                backdropTransformItem: mediaScene
                cornerRadius: 8
                baseColor: pageRoot.translucent
                           ? Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.48 : 0.62)
                           : Theme.panelSurface
                gradientStrength: 0.52
                borderColor: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.56 : 0.42)
                shadowEnabled: false

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 7

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 148
                        radius: 6
                        clip: true
                        color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.07 : 0.04)

                        Rectangle {
                            anchors.fill: parent
                            visible: pageRoot.previewState === 0
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0; color: Theme.withAlpha(Theme.warmAccent, 0.86) }
                                GradientStop { position: 0.48; color: Theme.withAlpha(Theme.categoryNavigation, 0.72) }
                                GradientStop { position: 1; color: Theme.withAlpha(Theme.categoryInfo, 0.82) }
                            }
                            Rectangle { x: 24; y: 22; width: 92; height: 92; radius: 46; color: Theme.withAlpha("white", 0.24) }
                            Rectangle { x: parent.width - 116; y: 34; width: 82; height: 82; rotation: 18; radius: 16; color: Theme.withAlpha(Theme.bg, 0.34) }
                        }

                        FmProgressRing { anchors.centerIn: parent; visible: pageRoot.previewState === 1; running: visible }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 24, 210)
                            spacing: 6
                            visible: pageRoot.previewState > 1
                            RecolorSvgIcon {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                sourcePath: pageRoot.previewState === 2
                                            ? "qrc:/qt/qml/FM/qml/assets/icons-classic/image.svg"
                                            : "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
                                sourceSize: Qt.size(42, 42)
                                recolorColor: pageRoot.previewState === 3 ? Theme.warning : Theme.categoryInfo
                            }
                            Label {
                                Layout.fillWidth: true
                                text: pageRoot.previewState === 2 ? "Thumbnail unavailable" : "Provider preview unavailable"
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 8
                            width: dimensionsLabel.implicitWidth + 14
                            height: 20
                            radius: 4
                            visible: pageRoot.previewState === 0
                            color: Theme.withAlpha(Theme.panelSurface, 0.82)
                            Label { id: dimensionsLabel; anchors.centerIn: parent; text: "2560x1440"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMicro }
                        }
                    }

                    Label { Layout.fillWidth: true; text: "Image"; color: Theme.textPrimary; font.weight: Font.DemiBold; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 3
                        Label { text: "Name"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro }
                        Label { Layout.fillWidth: true; text: "cover-concept.png"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMicro; horizontalAlignment: Text.AlignRight; elide: Text.ElideMiddle }
                        Label { text: "Size"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro }
                        Label { Layout.fillWidth: true; text: "1.8 MiB"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMicro; horizontalAlignment: Text.AlignRight }
                        Label { text: "Modified"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro }
                        Label { Layout.fillWidth: true; text: "Today, 11:48"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMicro; horizontalAlignment: Text.AlignRight }
                    }

                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true
                        FmButton { Layout.fillWidth: true; text: "Open"; enabled: pageRoot.previewState !== 1 }
                        FmButton { Layout.fillWidth: true; text: "Quick Look"; highlighted: true; enabled: pageRoot.previewState === 0 }
                    }
                }
            }
        }
    }

    DialogSection {
        title: "FOLDER HOVER PREVIEW"

        Item {
            id: hoverScene
            Layout.fillWidth: true
            Layout.preferredHeight: 380
            clip: true

            Rectangle {
                id: hoverBackdrop
                anchors.fill: parent
                radius: Theme.radiusMd
                color: Theme.panelSurfaceStrong
                border.color: Theme.panelBorder

                Repeater {
                    model: 30
                    Rectangle {
                        required property int index
                        width: 76
                        height: 58
                        radius: 7
                        x: 18 + (index % 6) * Math.max(86, (hoverBackdrop.width - 54) / 6)
                        y: 18 + Math.floor(index / 6) * 76
                        color: Theme.withAlpha(index % 3 === 0 ? Theme.categoryNavigation : Theme.categoryInfo,
                                               themeController.isDark ? 0.13 : 0.09)
                        border.color: Theme.withAlpha(Theme.panelBorder, 0.55)
                    }
                }
            }

            TranslucentSurface {
                id: hoverSurface
                width: Math.min(304, parent.width - 28)
                height: 316
                x: Math.max(14, parent.width - width - 18)
                y: 34
                translucent: pageRoot.translucent
                active: true
                backgroundBlurEnabled: pageRoot.blurEnabled
                blurStrength: 72
                backdropSource: hoverBackdrop
                backdropTransformItem: hoverScene
                cornerRadius: 8
                baseColor: pageRoot.translucent
                           ? Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.48 : 0.62)
                           : Theme.panelSurface
                gradientStrength: 0.52
                borderColor: Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.56 : 0.42)
                shadowEnabled: false

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        RecolorSvgIcon {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            sourcePath: "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"
                            sourceSize: Qt.size(20, 20)
                            recolorColor: Theme.categoryNavigation
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label { Layout.fillWidth: true; text: "Design references"; color: Theme.textPrimary; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; text: pageRoot.previewState === 0 ? "9+ items · bounded preview" : pageRoot.stateLabel(); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideRight }
                        }
                        FmIconButton {
                            iconSource: pageRoot.viewMode === 0
                                        ? "qrc:/qt/qml/FM/qml/assets/icons-classic/list.svg"
                                        : "qrc:/qt/qml/FM/qml/assets/icons-classic/layout-grid.svg"
                            Accessible.name: pageRoot.viewMode === 0 ? "Use list view" : "Use grid view"
                            ToolTip.visible: hovered
                            ToolTip.text: pageRoot.viewMode === 0 ? "Use list view" : "Use grid view"
                            onClicked: pageRoot.viewMode = pageRoot.viewMode === 0 ? 1 : 0
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 3
                            columnSpacing: 6
                            rowSpacing: 6
                            visible: pageRoot.previewState === 0 && pageRoot.viewMode === 0
                            Repeater {
                                model: pageRoot.entries.slice(0, 6)
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 6
                                    color: Theme.withAlpha(Theme.controlSurface, 0.78)
                                    border.color: Theme.withAlpha(Theme.controlBorder, 0.72)
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 2
                                        Image { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 32; Layout.preferredHeight: 32; source: "qrc:/qt/qml/FM/qml/assets/icons-classic/" + modelData.icon; sourceSize: Qt.size(32, 32) }
                                        Label { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeMicro; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            visible: pageRoot.previewState === 0 && pageRoot.viewMode === 1
                            Repeater {
                                model: pageRoot.entries.slice(0, 6)
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 5
                                    color: Theme.withAlpha(Theme.controlSurface, 0.72)
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 7
                                        anchors.rightMargin: 7
                                        spacing: 7
                                        Image { Layout.preferredWidth: 24; Layout.preferredHeight: 24; source: "qrc:/qt/qml/FM/qml/assets/icons-classic/" + modelData.icon; sourceSize: Qt.size(24, 24) }
                                        Label { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight }
                                        Label { text: modelData.meta; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 24, 210)
                            spacing: 8
                            visible: pageRoot.previewState !== 0
                            FmProgressRing { Layout.alignment: Qt.AlignHCenter; visible: pageRoot.previewState === 1; running: visible }
                            RecolorSvgIcon {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 38
                                Layout.preferredHeight: 38
                                visible: pageRoot.previewState > 1
                                sourcePath: pageRoot.previewState === 2
                                            ? "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"
                                            : "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
                                sourceSize: Qt.size(38, 38)
                                recolorColor: pageRoot.previewState === 3 ? Theme.warning : Theme.categoryNavigation
                            }
                            Label {
                                Layout.fillWidth: true
                                text: pageRoot.previewState === 1 ? "Loading folder…"
                                      : (pageRoot.previewState === 2 ? "Empty folder" : "Preview unavailable for this provider")
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Label { Layout.fillWidth: true; text: "/home/camilo/Design references"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideMiddle }
                    RowLayout {
                        Layout.fillWidth: true
                        FmButton { Layout.fillWidth: true; text: "Open"; enabled: pageRoot.previewState !== 1 }
                        FmButton { Layout.fillWidth: true; text: "Peek"; highlighted: true; enabled: pageRoot.previewState !== 1 }
                    }
                }
            }
        }
    }

    DialogSection {
        title: "FOLDER PEEK"

        Item {
            id: peekScene
            Layout.fillWidth: true
            Layout.preferredHeight: 570
            clip: true

            Rectangle {
                id: peekBackdrop
                anchors.fill: parent
                radius: Theme.radiusMd
                color: Theme.panelSurfaceStrong
                border.color: Theme.panelBorder
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.13 : 0.08) }
                    GradientStop { position: 1; color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.08 : 0.05) }
                }

                Repeater {
                    model: 24
                    Rectangle {
                        required property int index
                        width: 112
                        height: 54
                        radius: 7
                        x: 16 + (index % 4) * Math.max(122, (peekBackdrop.width - 44) / 4)
                        y: 16 + Math.floor(index / 4) * 86
                        color: Theme.withAlpha(index % 2 === 0 ? Theme.categoryNavigation : Theme.warmAccent,
                                               themeController.isDark ? 0.16 : 0.11)
                        border.color: Theme.withAlpha(Theme.panelBorder, 0.62)
                    }
                }
            }

            TranslucentSurface {
                width: Math.min(548, parent.width - 28)
                height: 526
                anchors.centerIn: parent
                translucent: pageRoot.translucent
                active: true
                backgroundBlurEnabled: pageRoot.blurEnabled
                blurStrength: 72
                backdropSource: peekBackdrop
                backdropTransformItem: peekScene
                cornerRadius: 10
                baseColor: pageRoot.translucent
                           ? Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.46 : 0.60)
                           : Theme.panelSurface
                gradientStrength: 0.55
                borderColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.54 : 0.40)
                shadowEnabled: false

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-left.svg"; Accessible.name: "Back" }
                        FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-up.svg"; Accessible.name: "Up" }
                        Label { Layout.fillWidth: true; text: "Design references"; color: Theme.textPrimary; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        FmIconButton {
                            iconSource: pageRoot.viewMode === 0
                                        ? "qrc:/qt/qml/FM/qml/assets/icons-classic/list.svg"
                                        : "qrc:/qt/qml/FM/qml/assets/icons-classic/layout-grid.svg"
                            Accessible.name: pageRoot.viewMode === 0 ? "Use list view" : "Use grid view"
                            onClicked: pageRoot.viewMode = pageRoot.viewMode === 0 ? 1 : 0
                        }
                        FmIconButton {
                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/panel-open.svg"
                            isHighlighted: true
                            Accessible.name: "Open in panel"
                            ToolTip.visible: hovered
                            ToolTip.text: "Open in panel"
                        }
                        FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/close.svg"; Accessible.name: "Close Folder Peek" }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: Theme.withAlpha(Theme.controlSurface, pageRoot.translucent ? 0.64 : 0.96)
                        border.color: Theme.withAlpha(Theme.controlBorder, 0.72)
                        clip: true

                        GridView {
                            anchors.fill: parent
                            anchors.margins: 8
                            visible: pageRoot.previewState === 0 && pageRoot.viewMode === 0
                            cellWidth: Math.max(104, Math.floor(width / Math.max(2, Math.floor(width / 112))))
                            cellHeight: 116
                            model: pageRoot.entries
                            clip: true
                            delegate: Rectangle {
                                required property int index
                                required property var modelData
                                width: GridView.view.cellWidth - 7
                                height: GridView.view.cellHeight - 7
                                radius: 7
                                color: index === pageRoot.selectedEntry
                                       ? Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.22 : 0.15)
                                       : Theme.withAlpha(Theme.panelSurface, 0.58)
                                border.color: index === pageRoot.selectedEntry ? Theme.categoryNavigation : Theme.withAlpha(Theme.panelBorder, 0.65)
                                TapHandler { onTapped: pageRoot.selectedEntry = index }
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    spacing: 3
                                    Image { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 54; Layout.preferredHeight: 54; source: "qrc:/qt/qml/FM/qml/assets/icons-classic/" + modelData.icon; sourceSize: Qt.size(54, 54) }
                                    Label { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight }
                                    Label { Layout.fillWidth: true; text: modelData.meta; color: Theme.textSecondary; horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.fontSizeMicro; elide: Text.ElideRight }
                                }
                            }
                        }

                        ListView {
                            anchors.fill: parent
                            anchors.margins: 8
                            visible: pageRoot.previewState === 0 && pageRoot.viewMode === 1
                            spacing: 5
                            model: pageRoot.entries
                            clip: true
                            delegate: Rectangle {
                                required property int index
                                required property var modelData
                                width: ListView.view.width
                                height: 44
                                radius: 6
                                color: index === pageRoot.selectedEntry
                                       ? Theme.withAlpha(Theme.categoryNavigation, themeController.isDark ? 0.22 : 0.15)
                                       : Theme.withAlpha(Theme.panelSurface, 0.58)
                                border.color: index === pageRoot.selectedEntry ? Theme.categoryNavigation : Theme.withAlpha(Theme.panelBorder, 0.65)
                                TapHandler { onTapped: pageRoot.selectedEntry = index }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    Image { Layout.preferredWidth: 30; Layout.preferredHeight: 30; source: "qrc:/qt/qml/FM/qml/assets/icons-classic/" + modelData.icon; sourceSize: Qt.size(30, 30) }
                                    Label { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; elide: Text.ElideRight }
                                    Label { text: modelData.meta; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 32, 280)
                            spacing: 9
                            visible: pageRoot.previewState !== 0
                            FmProgressRing { Layout.alignment: Qt.AlignHCenter; visible: pageRoot.previewState === 1; running: visible }
                            RecolorSvgIcon {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 54
                                Layout.preferredHeight: 54
                                visible: pageRoot.previewState > 1
                                sourcePath: pageRoot.previewState === 2
                                            ? "qrc:/qt/qml/FM/qml/assets/icons-classic/folder.svg"
                                            : "qrc:/qt/qml/FM/qml/assets/icons-classic/info.svg"
                                sourceSize: Qt.size(54, 54)
                                recolorColor: pageRoot.previewState === 3 ? Theme.warning : Theme.categoryNavigation
                            }
                            Label {
                                Layout.fillWidth: true
                                text: pageRoot.previewState === 1 ? "Loading folder…"
                                      : (pageRoot.previewState === 2 ? "This folder is empty" : "This provider does not support Folder Peek yet")
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            FmButton { Layout.alignment: Qt.AlignHCenter; visible: pageRoot.previewState === 3; text: "Open in panel" }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/home.svg"; Accessible.name: "Home" }
                        FmButton { text: "/"; flat: true }
                        FmButton { text: "home"; flat: true }
                        FmButton { text: "camilo"; flat: true }
                        FmButton { Layout.fillWidth: true; text: "Design references"; highlighted: true }
                        FmIconButton { iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg"; Accessible.name: "Folder Peek menu" }
                    }
                }
            }
        }
    }

    DialogSection {
        visible: pageRoot.stateMatrix
        title: "STATE MATRIX"
        GridLayout {
            Layout.fillWidth: true
            columns: width > 680 ? 4 : 2
            columnSpacing: 8
            rowSpacing: 8
            Repeater {
                model: [
                    { title: "Bounded", subtitle: "9+ items", color: Theme.categoryNavigation },
                    { title: "Loading", subtitle: "Cancellable", color: Theme.categoryInfo },
                    { title: "Empty", subtitle: "No children", color: Theme.textSecondary },
                    { title: "Unavailable", subtitle: "Provider fallback", color: Theme.warning }
                ]
                delegate: SurfaceCard {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 86
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Label { Layout.fillWidth: true; text: modelData.title; color: modelData.color; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        Label { Layout.fillWidth: true; text: modelData.subtitle; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
    }
}
