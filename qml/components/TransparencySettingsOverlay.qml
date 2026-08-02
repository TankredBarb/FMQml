import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "common"
import "framework"
import "dialogs"
import "settings"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent ? parent.width - 32 : 1080, 1080)
    height: Math.min(parent ? parent.height - 32 : 720, 720)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property var appRoot: null
    readonly property color dialogAccent: Theme.accent
    readonly property int strength: typeof appSettings !== "undefined" && appSettings
                                    ? appSettings.commandPaletteTransparencyStrength
                                    : 60
    readonly property real strengthRatio: root.strength / 100.0
    readonly property real paletteAlpha: themeController.isDark
                                         ? 1.0 - root.strengthRatio * 0.48
                                         : 1.0 - root.strengthRatio * 0.40
    readonly property real dialogAlpha: themeController.isDark
                                        ? 1.0 - root.strengthRatio * 0.32
                                        : 1.0 - root.strengthRatio * 0.26
    readonly property real hoverAlpha: themeController.isDark
                                       ? 1.0 - root.strengthRatio * 0.32
                                       : 1.0 - root.strengthRatio * 0.26
    readonly property bool palettePreviewEnabled: typeof appSettings !== "undefined" && appSettings
                                                   ? appSettings.commandPaletteTransparency
                                                   : true
    readonly property bool workspacePreviewEnabled: typeof appSettings !== "undefined" && appSettings
                                                     ? appSettings.workspaceDialogsTransparency
                                                     : false
    readonly property bool hoverPreviewEnabled: typeof appSettings !== "undefined" && appSettings
                                                 ? appSettings.hoverPreviewTransparency
                                                 : false
    readonly property bool propertiesPreviewEnabled: typeof appSettings !== "undefined" && appSettings
                                                      ? appSettings.propertiesDialogTransparency
                                                      : false
    readonly property bool surfaceBlurEnabled: typeof appSettings !== "undefined" && appSettings
                                               ? appSettings.surfaceBlur
                                               : false
    readonly property int surfaceBlurStrength: typeof appSettings !== "undefined" && appSettings
                                               ? appSettings.surfaceBlurStrength
                                               : 72

    function setSetting(name, value) {
        if (typeof appSettings !== "undefined" && appSettings && appSettings[name] !== value) {
            appSettings[name] = value
        }
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 16
        shadowVerticalOffset: 5
    }

    DialogHeader {
        id: dialogHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/sun.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: "Surface Effects"
        subtitle: "Tune gradients, transparency, and blur across supported surfaces."
        closeText: "x"
        onCloseRequested: root.close()
    }

    DialogFooter {
        id: dialogFooter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "Changes are applied immediately"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
            }
            Item { Layout.fillWidth: true }
            DialogActionButton {
                text: "Done"
                highlighted: true
                primaryColor: root.dialogAccent
                onClicked: root.close()
            }
        }
    }

    RowLayout {
        anchors.top: dialogHeader.bottom
        anchors.bottom: dialogFooter.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 480
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.82 : 0.92)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.48)
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Live preview"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: previewPercent.implicitWidth + 16
                        implicitHeight: 24
                        radius: 12
                        color: Theme.withAlpha(root.dialogAccent, 0.16)
                        border.color: Theme.withAlpha(root.dialogAccent, 0.42)
                        Label {
                            id: previewPercent
                            anchors.centerIn: parent
                            text: root.strength + "%"
                            color: root.dialogAccent
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Each example follows its matching switch. Turn it off to compare with the opaque surface."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }

                Item {
                    id: previewScene
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 430
                    readonly property real lowerCardWidth: (width - 154) / 3

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusMd
                        color: themeController.isDark ? "#18212c" : "#e8edf3"

                        Item {
                            id: previewBackdrop
                            anchors.fill: parent

                            Rectangle {
                                anchors.fill: parent
                                color: themeController.isDark ? "#18212c" : "#e8edf3"
                            }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 112
                            color: themeController.isDark ? "#111922" : "#d9e1ea"
                        }

                        Column {
                            x: 16
                            y: 22
                            width: 80
                            spacing: 14
                            Repeater {
                                model: ["Home", "Images", "Projects", "Archive", "Cloud"]
                                delegate: Row {
                                    required property int index
                                    required property string modelData
                                    spacing: 7
                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 3
                                        color: index === 2 ? root.dialogAccent : Theme.withAlpha(Theme.textSecondary, 0.42)
                                    }
                                    Label {
                                        text: modelData
                                        color: index === 2 ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        Grid {
                            x: 132
                            y: 32
                            columns: 3
                            spacing: 16
                            Repeater {
                                model: 12
                                delegate: Column {
                                    required property int index
                                    spacing: 5
                                    Rectangle {
                                        width: 72
                                        height: 54
                                        radius: 7
                                        color: index % 3 === 0
                                               ? Theme.withAlpha(root.dialogAccent, 0.32)
                                               : Theme.withAlpha(Theme.textSecondary, 0.18)
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 18
                                            radius: 4
                                            color: Theme.withAlpha(Theme.textPrimary, 0.20)
                                        }
                                    }
                                    Rectangle {
                                        width: 52 + (index % 3) * 8
                                        height: 5
                                        radius: 2.5
                                        color: Theme.withAlpha(Theme.textPrimary, 0.28)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            x: 123
                            y: parent.height - height - 23
                            width: previewScene.lowerCardWidth - 6
                            height: 120
                            radius: 9
                            color: themeController.isDark ? "#253c52" : "#b8d2e8"

                            Row {
                                anchors.centerIn: parent
                                spacing: 7
                                Repeater {
                                    model: ["#e08a5b", "#5ba7e0"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: Math.max(38, (previewScene.lowerCardWidth - 28) / 2)
                                        height: 82
                                        radius: 8
                                        color: modelData
                                        opacity: 0.82
                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            anchors.margins: 8
                                            height: 7
                                            radius: 3.5
                                            color: Theme.withAlpha("white", 0.62)
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            x: 131 + previewScene.lowerCardWidth
                            y: parent.height - height - 23
                            width: previewScene.lowerCardWidth - 6
                            height: 120
                            radius: 9
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#4f88c6" }
                                GradientStop { position: 1.0; color: "#7c5bc2" }
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: Math.min(72, parent.width - 30)
                                height: 72
                                radius: 12
                                color: Theme.withAlpha("white", 0.26)
                            }
                        }

                        Rectangle {
                            x: 139 + previewScene.lowerCardWidth * 2
                            y: parent.height - height - 23
                            width: previewScene.lowerCardWidth - 6
                            height: 120
                            radius: 9
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#dd6b7b" }
                                GradientStop { position: 0.48; color: "#735fd6" }
                                GradientStop { position: 1.0; color: "#3b9bc2" }
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: Math.min(72, parent.width - 30)
                                height: 72
                                radius: 36
                                color: Theme.withAlpha("white", 0.26)
                            }
                        }

                        }

                        TranslucentSurface {
                            x: Math.round((parent.width - width) / 2) + 34
                            y: 48
                            width: Math.min(parent.width - 160, 330)
                            height: 150
                            translucent: root.palettePreviewEnabled
                            active: true
                            backgroundBlurEnabled: root.surfaceBlurEnabled && root.palettePreviewEnabled
                            blurStrength: blurStrengthSlider.value
                            backdropSource: previewBackdrop
                            backdropTransformItem: previewScene
                            cornerRadius: 12
                            baseColor: root.palettePreviewEnabled
                                       ? Theme.withAlpha(Theme.panelSurfaceStrong, root.paletteAlpha)
                                       : Theme.panelSurface
                            gradientStrength: 0.62
                            borderColor: Theme.withAlpha(root.dialogAccent, 0.46)
                            shadowBlur: 18
                            shadowVerticalOffset: 6

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 9
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Command palette / Settings"; color: Theme.textPrimary; font.weight: Font.DemiBold }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: root.palettePreviewEnabled ? "ON" : "OFF"
                                        color: root.palettePreviewEnabled ? root.dialogAccent : Theme.textSecondary
                                        font.pixelSize: 9
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 30
                                    radius: 7
                                    color: Theme.withAlpha(Theme.surface, 0.46)
                                    border.color: Theme.withAlpha(Theme.panelBorder, 0.52)
                                    Label { anchors.centerIn: parent; text: "Search files and actions"; color: Theme.textSecondary; font.pixelSize: 11 }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Open Quick Look"; color: Theme.textPrimary; font.pixelSize: 11 }
                                    Item { Layout.fillWidth: true }
                                    Label { text: "Space"; color: root.dialogAccent; font.pixelSize: 10 }
                                }
                            }
                        }

                        TranslucentSurface {
                            x: 120
                            y: parent.height - height - 20
                            width: previewScene.lowerCardWidth
                            height: 126
                            translucent: root.workspacePreviewEnabled
                            active: true
                            backgroundBlurEnabled: root.surfaceBlurEnabled && root.workspacePreviewEnabled
                            blurStrength: blurStrengthSlider.value
                            backdropSource: previewBackdrop
                            backdropTransformItem: previewScene
                            cornerRadius: 12
                            baseColor: root.workspacePreviewEnabled
                                       ? Theme.withAlpha(Theme.panelSurfaceStrong, root.dialogAlpha)
                                       : Theme.panelSurface
                            gradientStrength: 0.62
                            borderColor: Theme.withAlpha(root.dialogAccent, 0.46)
                            shadowBlur: 18
                            shadowVerticalOffset: 6

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 5
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Workspace"; color: Theme.textPrimary; font.weight: Font.DemiBold; font.pixelSize: 10 }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: root.workspacePreviewEnabled ? "ON" : "OFF"
                                        color: root.workspacePreviewEnabled ? root.dialogAccent : Theme.textSecondary
                                        font.pixelSize: 9
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Label { text: "Search and analysis"; color: Theme.textSecondary; Layout.fillWidth: true; font.pixelSize: 9 }
                                Item { Layout.fillHeight: true }
                                Rectangle { Layout.fillWidth: true; height: 5; radius: 2.5; color: Theme.withAlpha(root.dialogAccent, 0.42) }
                            }
                        }

                        TranslucentSurface {
                            x: 128 + previewScene.lowerCardWidth
                            y: parent.height - height - 20
                            width: previewScene.lowerCardWidth
                            height: 126
                            translucent: root.propertiesPreviewEnabled
                            active: true
                            backgroundBlurEnabled: root.surfaceBlurEnabled && root.propertiesPreviewEnabled
                            blurStrength: blurStrengthSlider.value
                            backdropSource: previewBackdrop
                            backdropTransformItem: previewScene
                            cornerRadius: 12
                            baseColor: root.propertiesPreviewEnabled
                                       ? Theme.withAlpha(Theme.panelSurfaceStrong, root.dialogAlpha)
                                       : Theme.panelSurface
                            gradientStrength: 0.62
                            borderColor: Theme.withAlpha(root.dialogAccent, 0.46)
                            shadowBlur: 18
                            shadowVerticalOffset: 6

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 5
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Properties"; color: Theme.textPrimary; font.weight: Font.DemiBold; font.pixelSize: 10 }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: root.propertiesPreviewEnabled ? "ON" : "OFF"
                                        color: root.propertiesPreviewEnabled ? root.dialogAccent : Theme.textSecondary
                                        font.pixelSize: 9
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Label { text: "image.png"; color: Theme.textSecondary; font.pixelSize: 9 }
                                Rectangle { Layout.fillWidth: true; height: 5; radius: 2.5; color: Theme.withAlpha(Theme.textPrimary, 0.28) }
                                Rectangle { Layout.fillWidth: true; height: 5; radius: 2.5; color: Theme.withAlpha(Theme.textPrimary, 0.18) }
                                Item { Layout.fillHeight: true }
                            }
                        }

                        TranslucentSurface {
                            x: 136 + previewScene.lowerCardWidth * 2
                            y: parent.height - height - 20
                            width: previewScene.lowerCardWidth
                            height: 126
                            translucent: root.hoverPreviewEnabled
                            active: true
                            backgroundBlurEnabled: root.surfaceBlurEnabled && root.hoverPreviewEnabled
                            blurStrength: blurStrengthSlider.value
                            backdropSource: previewBackdrop
                            backdropTransformItem: previewScene
                            cornerRadius: 12
                            baseColor: root.hoverPreviewEnabled
                                       ? Theme.withAlpha(Theme.panelSurfaceStrong, root.hoverAlpha)
                                       : Theme.panelSurface
                            startColor: Theme.chromeGradientStart
                            midColor: Theme.chromeGradientMid
                            endColor: Theme.panelSurface
                            gradientStrength: 0.5
                            borderColor: Theme.withAlpha(root.dialogAccent, 0.46)
                            shadowBlur: 18
                            shadowVerticalOffset: 6

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 7
                                Rectangle { width: 34; height: 48; radius: 6; color: Theme.withAlpha(root.dialogAccent, 0.30) }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: "Hover"; color: Theme.textPrimary; font.weight: Font.DemiBold; font.pixelSize: 10 }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: root.hoverPreviewEnabled ? "ON" : "OFF"
                                            color: root.hoverPreviewEnabled ? root.dialogAccent : Theme.textSecondary
                                            font.pixelSize: 9
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    Label { text: "image.png"; color: Theme.textSecondary; font.pixelSize: 9 }
                                    Item { Layout.fillHeight: true }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 390
            Layout.minimumWidth: 340
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.36 : 0.64)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.48)

            ScrollView {
                id: effectsScrollView
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                contentWidth: availableWidth

                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: FmScrollBar {
                    id: effectsScrollBar
                    parent: effectsScrollView.contentItem
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    policy: ScrollBar.AsNeeded
                }

                ColumnLayout {
                    id: effectsColumn
                    width: effectsScrollBar.scrollNeeded
                           ? Math.max(0, effectsScrollBar.x - 6)
                           : effectsScrollView.availableWidth
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: 4
                        text: "Appearance"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                    }

                    SettingsToggleRow {
                        title: "Gradient colors"
                        subtitle: "Use subtle gradient surfaces in app chrome"
                        checked: appSettings ? appSettings.useGradientColors : true
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("useGradientColors", checked)
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: strengthColumn.implicitHeight + 22
                        radius: Theme.radiusSm
                        color: Theme.withAlpha(Theme.panelSurfaceSoft, 0.72)
                        border.color: Theme.withAlpha(Theme.panelBorder, 0.38)

                        ColumnLayout {
                            id: strengthColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 11
                            spacing: 7

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Transparency strength"; color: Theme.textPrimary; font.weight: Font.DemiBold; font.pixelSize: Theme.fontSizeLabel }
                                Item { Layout.fillWidth: true }
                                Label { text: root.strength + "%"; color: root.dialogAccent; font.weight: Font.DemiBold }
                            }

                            Slider {
                                id: transparencySlider
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                stepSize: 5
                                snapMode: Slider.SnapAlways
                                value: root.strength
                                onMoved: root.setSetting("commandPaletteTransparencyStrength", Math.round(value))

                                background: Item {
                                    implicitHeight: 20

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 4
                                        radius: 2
                                        color: Theme.withAlpha(Theme.panelBorder,
                                                               themeController.isDark ? 0.36 : 0.62)
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: transparencySlider.visualPosition * parent.width
                                        height: 4
                                        radius: 2
                                        color: root.dialogAccent
                                    }
                                }

                                handle: Rectangle {
                                    x: transparencySlider.leftPadding
                                       + transparencySlider.visualPosition
                                       * (transparencySlider.availableWidth - width)
                                    y: transparencySlider.topPadding
                                       + transparencySlider.availableHeight / 2 - height / 2
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: transparencySlider.pressed
                                           ? root.dialogAccent
                                           : Theme.panelSurface
                                    border.color: root.dialogAccent
                                    border.width: 2
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Shared by every translucent surface"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeCaption
                            }
                        }
                    }

                    SettingsToggleRow {
                        title: "Surface blur"
                        subtitle: "Soften the workspace visible through every enabled translucent surface"
                        checked: root.surfaceBlurEnabled
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("surfaceBlur", checked)
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: blurStrengthColumn.implicitHeight + 22
                        radius: Theme.radiusSm
                        color: Theme.withAlpha(Theme.panelSurfaceSoft, root.surfaceBlurEnabled ? 0.72 : 0.42)
                        border.color: Theme.withAlpha(Theme.panelBorder, root.surfaceBlurEnabled ? 0.38 : 0.24)
                        opacity: root.surfaceBlurEnabled ? 1.0 : 0.58

                        ColumnLayout {
                            id: blurStrengthColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 11
                            spacing: 7

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Blur strength"; color: Theme.textPrimary; font.weight: Font.DemiBold; font.pixelSize: Theme.fontSizeLabel }
                                Item { Layout.fillWidth: true }
                                Label { text: root.surfaceBlurStrength + "%"; color: root.dialogAccent; font.weight: Font.DemiBold }
                            }

                            Slider {
                                id: blurStrengthSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                stepSize: 5
                                snapMode: Slider.SnapAlways
                                enabled: root.surfaceBlurEnabled
                                value: root.surfaceBlurStrength
                                onValueChanged: root.setSetting("surfaceBlurStrength", Math.round(value))

                                background: Item {
                                    implicitHeight: 20
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 4
                                        radius: 2
                                        color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.36 : 0.62)
                                    }
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: blurStrengthSlider.visualPosition * parent.width
                                        height: 4
                                        radius: 2
                                        color: root.dialogAccent
                                    }
                                }

                                handle: Rectangle {
                                    x: blurStrengthSlider.leftPadding
                                       + blurStrengthSlider.visualPosition
                                       * (blurStrengthSlider.availableWidth - width)
                                    y: blurStrengthSlider.topPadding
                                       + blurStrengthSlider.availableHeight / 2 - height / 2
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: blurStrengthSlider.pressed ? root.dialogAccent : Theme.panelSurface
                                    border.color: root.dialogAccent
                                    border.width: 2
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Low keeps the glass clear; high creates a dense frosted surface"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeCaption
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: 4
                        Layout.topMargin: 8
                        text: "Apply to"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                    }

                    SettingsToggleRow {
                        title: "Command palette & Settings"
                        subtitle: "Translucent command palette and application settings window"
                        checked: appSettings ? appSettings.commandPaletteTransparency : true
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("commandPaletteTransparency", checked)
                    }

                    SettingsToggleRow {
                        title: "Hover preview"
                        subtitle: "Floating file preview cards"
                        checked: appSettings ? appSettings.hoverPreviewTransparency : false
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("hoverPreviewTransparency", checked)
                    }

                    SettingsToggleRow {
                        title: "Quick Look"
                        subtitle: "Quick Look outer shell"
                        checked: appSettings ? appSettings.quickLookTransparency : false
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("quickLookTransparency", checked)
                    }

                    SettingsToggleRow {
                        title: "Properties"
                        subtitle: "File properties outer shell"
                        checked: appSettings ? appSettings.propertiesDialogTransparency : false
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("propertiesDialogTransparency", checked)
                    }

                    SettingsToggleRow {
                        title: "Workspace dialogs"
                        subtitle: "Usage, search, compare, rename, and checksum"
                        checked: appSettings ? appSettings.workspaceDialogsTransparency : false
                        accentColor: root.dialogAccent
                        onToggled: checked => root.setSetting("workspaceDialogsTransparency", checked)
                    }
                }
            }
        }
    }
}
