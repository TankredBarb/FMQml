import QtQuick
import QtQuick.Window
import "style"

Window {
    id: root

    width: 860
    height: 520
    visible: false
    flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: Theme.bg
    title: "FM"

    property int statusIndex: 0
    property bool secondaryInstance: false

    readonly property var statuses: [
        "Loading workspace",
        "Applying theme",
        "Preparing panels",
        "Restoring session",
        "Building file model"
    ]

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.bg }
            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.05 : 0.03) }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 3
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.categoryInfo }
            GradientStop { position: 0.48; color: Theme.accent }
            GradientStop { position: 1.0; color: Theme.warmAccent }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 720
        height: 376
        radius: 24
        color: Theme.panelSurfaceStrong
        border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.92 : 0.82)
        border.width: 1
    }

    Rectangle {
        anchors.centerIn: parent
        width: 720
        height: 376
        radius: 24
        color: "transparent"
        border.color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.025)
        border.width: 1
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: (parent.width - 720) / 2
        anchors.topMargin: (parent.height - 376) / 2 + 24
        anchors.bottomMargin: (parent.height - 376) / 2 + 24
        width: 5
        radius: 2.5
        color: Theme.categoryAction
    }

    Rectangle {
        anchors.centerIn: parent
        width: 720
        height: 376
        radius: 24
        color: "transparent"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.08 : 0.05) }
            GradientStop { position: 0.52; color: Qt.rgba(0, 0, 0, 0) }
            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.05 : 0.03) }
        }
    }

    Item {
        anchors.centerIn: parent
        width: 720
        height: 376

        Rectangle {
            anchors.fill: parent
            radius: 24
            color: "transparent"
            border.color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.025)
            border.width: 1
        }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 38
            anchors.rightMargin: 38
            anchors.topMargin: 30
            anchors.bottomMargin: 28
            spacing: 18

            Row {
                width: parent.width
                spacing: 18

                Rectangle {
                    width: 84
                    height: 84
                    radius: 22
                    color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.16 : 0.10)
                    border.color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.34 : 0.22)
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/FM/qml/assets/icons/app_icon.png"
                        width: 50
                        height: 50
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "FM"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 42
                        font.weight: Font.Bold
                        font.letterSpacing: 1.0
                    }

                    Text {
                        text: "File manager"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                    }
                }

                Item { width: 1; height: 1 }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 206
                    height: 58
                    radius: 18
                    color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.13 : 0.08)
                    border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.55 : 0.38)
                    border.width: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        Text {
                            text: themeController.customThemeLoaded ? "CUSTOM THEME" : "CURRENT THEME"
                            color: Theme.withAlpha(Theme.textSecondary, 0.92)
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                        }

                        Text {
                            text: themeController.schemeName
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.62 : 0.44)
            }

            Column {
                width: parent.width
                spacing: 10

                Text {
                    width: parent.width
                    text: root.secondaryInstance ? "FM is already running" : root.statuses[root.statusIndex]
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Text {
                    width: parent.width
                    text: root.secondaryInstance
                          ? "Only one instance is allowed. This window will close shortly."
                          : "Initializing application shell and restoring workspace state"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            Row {
                width: parent.width
                spacing: 12

                Rectangle {
                    width: 152
                    height: 40
                    radius: 18
                    color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.12 : 0.08)
                    border.color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.28 : 0.18)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.secondaryInstance ? "Instance active" : "Workspace readying"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                Rectangle {
                    width: 134
                    height: 40
                    radius: 18
                    color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.12 : 0.08)
                    border.color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.28 : 0.18)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.secondaryInstance ? "Launch blocked" : "Theme syncing"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                Rectangle {
                    width: 122
                    height: 40
                    radius: 18
                    color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.12 : 0.08)
                    border.color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.28 : 0.18)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.secondaryInstance ? "Closing" : "Panels online"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 8

                Row {
                    width: parent.width
                    spacing: 12

                    Rectangle {
                        width: 94
                        height: 52
                        radius: 16
                        color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.14 : 0.09)
                        border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.34 : 0.22)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 6
                                radius: 3
                                color: Theme.accent
                            }

                            Text {
                                text: "Accent"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Rectangle {
                        width: 94
                        height: 52
                        radius: 16
                        color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.09)
                        border.color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.34 : 0.22)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 6
                                radius: 3
                                color: Theme.categoryInfo
                            }

                            Text {
                                text: "Info"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Rectangle {
                        width: 94
                        height: 52
                        radius: 16
                        color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.14 : 0.09)
                        border.color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.34 : 0.22)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 6
                                radius: 3
                                color: Theme.warmAccent
                            }

                            Text {
                                text: "Warm"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Rectangle {
                        width: 94
                        height: 52
                        radius: 16
                        color: Theme.withAlpha(Theme.activeGlow, themeController.isDark ? 0.14 : 0.09)
                        border.color: Theme.withAlpha(Theme.activeGlow, themeController.isDark ? 0.34 : 0.22)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 6
                                radius: 3
                                color: Theme.activeGlow
                            }

                            Text {
                                text: "Glow"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Rectangle {
                        width: 94
                        height: 52
                        radius: 16
                        color: Theme.withAlpha(Theme.success, themeController.isDark ? 0.14 : 0.09)
                        border.color: Theme.withAlpha(Theme.success, themeController.isDark ? 0.34 : 0.22)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 6
                                radius: 3
                                color: Theme.success
                            }

                            Text {
                                text: "Success"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Rectangle {
                        width: 94
                        height: 52
                        radius: 16
                        color: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.14 : 0.09)
                        border.color: Theme.withAlpha(Theme.warning, themeController.isDark ? 0.34 : 0.22)
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 6
                                radius: 3
                                color: Theme.warning
                            }

                            Text {
                                text: "Warning"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }
                    }
                }
            }

        }
    }

    Timer {
        interval: 520
        repeat: true
        running: !root.secondaryInstance
        onTriggered: root.statusIndex = (root.statusIndex + 1) % root.statuses.length
    }
}
