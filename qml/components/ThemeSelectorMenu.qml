import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../style"

Menu {
    id: root

    implicitWidth: 420
    padding: 0
    topPadding: 0
    bottomPadding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: false

    function openAt(item) {
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

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 3
            anchors.leftMargin: 2
            anchors.rightMargin: 1
            radius: Theme.radius + 2
            color: "#000000"
            opacity: themeController.isDark ? 0.40 : 0.12
        }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.leftMargin: 1
            radius: Theme.radius + 1
            color: Theme.accent
            opacity: themeController.isDark ? 0.12 : 0.05
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.panelSurfaceStrong
            radius: Theme.radius + 1
            border.color: Theme.panelBorder
            border.width: 1
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            height: 1
            radius: 0.5
            color: themeController.isDark ? "#22ffffff" : "#55ffffff"
            opacity: themeController.isDark ? 0.32 : 0.48
        }
    }

    contentItem: Item {
        implicitWidth: 420
        implicitHeight: contentColumn.implicitHeight + 20

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        text: "Theme Schemes"
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Label {
                        text: "Choose the active color scheme"
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    radius: 11
                    color: Theme.withAlpha(Theme.accent, 0.15)
                    border.color: Theme.withAlpha(Theme.accent, 0.55)
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: "T"
                        color: Theme.accent
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                }
            }

            Rectangle {
                visible: themeController.customThemeLoaded
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 12
                color: Theme.withAlpha(Theme.categoryUtility, themeController.isDark ? 0.14 : 0.10)
                border.color: Theme.withAlpha(Theme.categoryUtility, 0.45)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: Theme.categoryUtility
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            text: "Custom theme loaded"
                            color: Theme.textPrimary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }

                        Label {
                            text: themeController.themeFilePath.length > 0 ? themeController.themeFilePath : "Loaded from file"
                            color: Theme.textSecondary
                            font.pixelSize: 9
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 10
                rowSpacing: 10

                ThemeSchemeCard {
                    title: "Catppuccin Latte"
                    subtitle: "Soft light, blue, mauve"
                    bgColor: "#EFF1F5"
                    surfaceColor: "#E6E9EF"
                    accentColor: "#1E66F5"
                    glowColor: "#8839EF"
                    selected: !themeController.customThemeLoaded && themeController.scheme === 0
                    onActivated: root.applyScheme(0)
                }

                ThemeSchemeCard {
                    title: "Aurora Glass"
                    subtitle: "Teal, orchid, blue"
                    bgColor: "#08111F"
                    surfaceColor: "#102033"
                    accentColor: "#2DD4BF"
                    glowColor: "#C084FC"
                    selected: !themeController.customThemeLoaded && themeController.scheme === 1
                    onActivated: root.applyScheme(1)
                }

                ThemeSchemeCard {
                    title: "Oxide Garden"
                    subtitle: "Paper, rust, patina"
                    bgColor: "#F3EDDF"
                    surfaceColor: "#E8DDC4"
                    accentColor: "#A3442F"
                    glowColor: "#2F6F5E"
                    selected: !themeController.customThemeLoaded && themeController.scheme === 2
                    onActivated: root.applyScheme(2)
                }

                ThemeSchemeCard {
                    title: "Ember Luxe"
                    subtitle: "Amber, ruby, espresso"
                    bgColor: "#100C0A"
                    surfaceColor: "#1B1410"
                    accentColor: "#F59E0B"
                    glowColor: "#F43F5E"
                    selected: !themeController.customThemeLoaded && themeController.scheme === 3
                    onActivated: root.applyScheme(3)
                }
            }
        }
    }
}
