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

    property real sweep: 0.0
    property real pulse: 0.0
    property real pulse2: 0.0
    property int statusIndex: 0

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
            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.06 : 0.04) }
        }
    }

    Rectangle {
        x: -120
        y: -100
        width: 420
        height: 420
        radius: 210
        color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.14 : 0.08)
        opacity: 0.9
        scale: 1.0 + Math.sin(root.pulse) * 0.02
    }

    Rectangle {
        x: parent.width - 300
        y: 40
        width: 360
        height: 360
        radius: 180
        color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.12 : 0.07)
        opacity: 0.85
        scale: 1.0 + Math.sin(root.pulse2) * 0.018
    }

    Rectangle {
        x: parent.width * 0.62
        y: parent.height - 180
        width: 260
        height: 260
        radius: 130
        color: Theme.withAlpha(Theme.activeGlow, themeController.isDark ? 0.08 : 0.05)
        opacity: 0.7
    }

    Rectangle {
        anchors.centerIn: parent
        width: 640
        height: 340
        radius: 28
        color: Theme.panelSurfaceStrong
        border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.95 : 0.85)
        border.width: 1
    }

    Rectangle {
        anchors.centerIn: parent
        width: 640
        height: 340
        radius: 28
        color: "transparent"
        border.color: Theme.withAlpha(Theme.textPrimary, themeController.isDark ? 0.05 : 0.03)
        border.width: 1
    }

    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: (parent.width - 640) / 2
        width: 5
        height: 250
        radius: 2.5
        color: Theme.categoryAction
    }

    Column {
        anchors.fill: parent
        anchors.leftMargin: (parent.width - 640) / 2 + 42
        anchors.rightMargin: (parent.width - 640) / 2 + 42
        anchors.topMargin: (parent.height - 340) / 2 + 34
        anchors.bottomMargin: (parent.height - 340) / 2 + 28
        spacing: 22

        Row {
            spacing: 18

            Rectangle {
                width: 72
                height: 72
                radius: 20
                color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.18 : 0.10)
                border.color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.36 : 0.24)
                border.width: 1

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/FM/qml/assets/icons/app_icon.png"
                    width: 44
                    height: 44
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text {
                    text: "FM"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 38
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
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.85 : 0.60)
        }

        Column {
            width: parent.width
            spacing: 12

            Row {
                width: parent.width

                Text {
                    text: root.statuses[root.statusIndex]
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }

                Item { width: 1; height: 1 }

                Text {
                    text: themeController.customThemeLoaded ? "Custom theme" : themeController.schemeName
                    color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.86 : 0.72)
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }
            }

            Text {
                width: parent.width
                text: "Initializing application shell and restoring workspace state"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Item {
                width: parent.width
                height: 30

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 4
                    radius: 2
                    color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.85 : 0.55)
                }

                Rectangle {
                    width: 120
                    height: 4
                    radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.categoryAction
                    x: -120 + ((parent.width + 120) * root.sweep)
                }
            }

            Row {
                spacing: 14

                Rectangle {
                    width: 156
                    height: 34
                    radius: 10
                    color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.12 : 0.08)
                    border.color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.28 : 0.18)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Workspace readying"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                Rectangle {
                    width: 128
                    height: 34
                    radius: 10
                    color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.12 : 0.08)
                    border.color: Theme.withAlpha(Theme.warmAccent, themeController.isDark ? 0.28 : 0.18)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Theme syncing"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                Rectangle {
                    width: 118
                    height: 34
                    radius: 10
                    color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.12 : 0.08)
                    border.color: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.28 : 0.18)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Panels online"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.bottomMargin: 22
        text: "FM"
        color: Theme.withAlpha(Theme.textSecondary, themeController.isDark ? 0.55 : 0.45)
        font.family: Theme.fontFamily
        font.pixelSize: 10
        font.weight: Font.Medium
        font.letterSpacing: 2
    }

    NumberAnimation on sweep {
        from: 0.0
        to: 1.0
        duration: 1100
        loops: Animation.Infinite
        running: true
    }

    NumberAnimation on pulse {
        from: 0.0
        to: 6.28318
        duration: 7200
        loops: Animation.Infinite
        running: true
    }

    NumberAnimation on pulse2 {
        from: 0.0
        to: 6.28318
        duration: 9400
        loops: Animation.Infinite
        running: true
    }

    Timer {
        interval: 520
        repeat: true
        running: true
        onTriggered: root.statusIndex = (root.statusIndex + 1) % root.statuses.length
    }
}
