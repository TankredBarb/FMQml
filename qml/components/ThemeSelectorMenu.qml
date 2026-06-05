import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects
import "../style"
import "common"

Menu {
    id: root

    implicitWidth: 456
    padding: 0
    topPadding: 0
    bottomPadding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: false
    property var customThemes: []
    property var builtInThemes: []

    function openAt(item) {
        reloadBuiltInThemes()
        reloadCustomThemes()
        if (item) {
            popup(item, 0, item.height + 8)
        } else {
            popup()
        }
    }

    function applyScheme(scheme) {
        themeController.scheme = scheme
        root.close()
    }

    function applyCustomTheme(filePath) {
        if (!filePath || filePath.length === 0) {
            return
        }
        if (themeController.loadThemeFromFile(filePath)) {
            reloadCustomThemes()
            root.close()
        }
    }

    function reloadCustomThemes() {
        customThemes = themeController.availableCustomThemes()
    }

    function reloadBuiltInThemes() {
        builtInThemes = themeController.builtInThemeDrafts()
    }

    function themeLibraryFolderUrl() {
        const directory = themeController.customThemeDirectory()
        if (directory.length === 0) {
            return ""
        }

        const normalized = directory.replace(/\\/g, "/")
        if (/^[A-Za-z]:/.test(normalized)) {
            return "file:///" + normalized
        }
        return "file:///" + normalized
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 4
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            radius: Theme.radiusLg
            color: Theme.shadow
            opacity: themeController.isDark ? 0.30 : 0.11
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusLg
            color: Theme.menuSurface
            border.color: Theme.withAlpha(Theme.menuBorder, themeController.isDark ? 0.46 : 0.30)
            border.width: 1
            antialiasing: true
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 1
            height: 1
            radius: 0.5
            color: Theme.withAlpha(Theme.accentText, themeController.isDark ? 0.040 : 0.12)
        }
    }

    contentItem: Item {
        implicitWidth: 456
        implicitHeight: contentColumn.implicitHeight + 24

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                IconTile {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    tileSize: 30
                    iconSize: 16
                    cornerRadius: Theme.radiusMd
                    source: themeController.isDark
                            ? "qrc:/qt/qml/FM/qml/assets/icons/moon.svg"
                            : "qrc:/qt/qml/FM/qml/assets/icons/sun.svg"
                    iconColor: Theme.actionIconColor("theme")
                    tileColor: Theme.withAlpha(Theme.actionIconColor("theme"), themeController.isDark ? 0.18 : 0.11)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        text: "Themes"
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Label {
                        text: themeController.customThemeLoaded
                              ? "Custom theme"
                              : themeController.schemeName
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                visible: themeController.customThemeLoaded
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: Theme.radiusMd
                color: Theme.withAlpha(Theme.categoryUtility, themeController.isDark ? 0.12 : 0.075)
                border.color: Theme.withAlpha(Theme.categoryUtility, themeController.isDark ? 0.34 : 0.24)
                border.width: 1
                antialiasing: true

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: Theme.categoryUtility
                    }

                    Label {
                        Layout.fillWidth: true
                        text: themeController.themeFilePath.length > 0 ? themeController.themeFilePath : "Loaded from file"
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                Repeater {
                    model: root.builtInThemes

                    ThemeSchemeCard {
                        title: modelData.name || ""
                        subtitle: modelData.subtitle || ""
                        bgColor: modelData.colors && modelData.colors.bg ? modelData.colors.bg : Theme.bg
                        surfaceColor: modelData.colors && modelData.colors.surface ? modelData.colors.surface : Theme.surface
                        accentColor: modelData.colors && modelData.colors.accent ? modelData.colors.accent : Theme.accent
                        glowColor: modelData.colors && modelData.colors.activeGlow ? modelData.colors.activeGlow : Theme.activeGlow
                        selected: !themeController.customThemeLoaded && themeController.scheme === index
                        onActivated: root.applyScheme(index)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: customThemesColumn.implicitHeight + 20
                radius: Theme.radiusMd
                color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.52 : 0.74)
                border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.34 : 0.25)
                border.width: 1
                antialiasing: true

                ColumnLayout {
                    id: customThemesColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Label {
                                text: "Saved Themes"
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            Label {
                                text: customThemes.length > 0
                                      ? customThemes.length + (customThemes.length === 1 ? " custom file" : " custom files")
                                      : "No saved theme files"
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        Button {
                            id: loadFileButton
                            flat: true
                            onClicked: {
                                externalThemeDialog.currentFolder = root.themeLibraryFolderUrl()
                                externalThemeDialog.open()
                            }

                            contentItem: RowLayout {
                                spacing: 6

                                RecolorSvgIcon {
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                    sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
                                    recolorColor: Theme.accent
                                    sourceSize: Qt.size(28, 28)
                                }

                                Label {
                                    text: "Open File"
                                    color: Theme.accent
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            background: Rectangle {
                                implicitWidth: 104
                                implicitHeight: 30
                                radius: Theme.radiusMd
                                color: loadFileButton.pressed ? Theme.surfaceActive
                                     : (loadFileButton.hovered
                                        ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.13 : 0.08)
                                        : "transparent")
                                border.color: Theme.withAlpha(Theme.accent, loadFileButton.hovered ? 0.38 : 0.24)
                                border.width: 1
                            }
                        }
                    }

                    Repeater {
                        model: customThemes

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 40
                            radius: Theme.radiusMd
                            color: themeController.customThemeLoaded
                                   && themeController.themeFilePath === modelData.filePath
                                   ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.13 : 0.08)
                                   : (themeMouse.containsMouse
                                      ? Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.86 : 0.95)
                                      : "transparent")
                            border.color: themeController.customThemeLoaded
                                          && themeController.themeFilePath === modelData.filePath
                                          ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.44 : 0.32)
                                          : Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.30 : 0.22)
                            border.width: 1
                            antialiasing: true

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 3
                                radius: Theme.radiusXs
                                color: modelData.colors && modelData.colors.accent ? modelData.colors.accent : Theme.accent
                                opacity: themeController.customThemeLoaded
                                         && themeController.themeFilePath === modelData.filePath ? 0.92 : 0.0
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9
                                anchors.rightMargin: 9
                                spacing: 8

                                Rectangle {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    radius: Theme.radiusSm
                                    color: modelData.colors && modelData.colors.accent ? modelData.colors.accent : Theme.accent
                                    border.color: Theme.withAlpha(Theme.textPrimary, 0.16)
                                    border.width: 1
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Label {
                                        text: modelData.name || modelData.fileName
                                        color: Theme.textPrimary
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: modelData.fileName + " - " + ((modelData.mode || "dark") === "light" ? "Light" : "Dark")
                                        color: Theme.textSecondary
                                        font.pixelSize: 9
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                }

                                Label {
                                    visible: themeController.customThemeLoaded
                                             && themeController.themeFilePath === modelData.filePath
                                    text: "Active"
                                    color: Theme.accent
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }

                            MouseArea {
                                id: themeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.applyCustomTheme(modelData.filePath)
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: externalThemeDialog
        title: "Open Theme File"
        fileMode: FileDialog.OpenFile
        currentFolder: root.themeLibraryFolderUrl()
        nameFilters: ["Theme files (*.json)", "JSON files (*.json)"]
        onAccepted: root.applyCustomTheme(selectedFile.toString())
    }
}
