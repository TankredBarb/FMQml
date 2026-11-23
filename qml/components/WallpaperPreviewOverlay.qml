import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "../style"
import "common"
import "dialogs"
import "framework"

Popup {
    id: root

    property var backdropSource: null
    readonly property color dialogAccent: Theme.accent
    readonly property bool translucentSurface: typeof appSettings !== "undefined" && appSettings
                                               ? appSettings.workspaceDialogsTransparency
                                               : false
    readonly property bool blurSurface: root.translucentSurface
                                        && typeof appSettings !== "undefined" && appSettings
                                        && appSettings.surfaceBlur && root.backdropSource

    x: parent ? Math.max(12, (parent.width - width) / 2) : 0
    y: parent ? Math.max(12, (parent.height - height) / 2) : 0
    width: Math.min(parent ? parent.width - 36 : 980, 980)
    height: Math.min(parent ? parent.height - 36 : 640, 640)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(panelController, path) {
        wallpaperPreviewController.openFor(path, panelController)
        if (!root.opened) {
            root.open()
        }
    }

    onOpened: Qt.callLater(() => contentRoot.forceActiveFocus())

    onClosed: {
        wallpaperPreviewController.close()
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: root.blurSurface ? 1.0 : 0.96; to: 1.0; duration: 150; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0.0; duration: 120; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: root.blurSurface ? 1.0 : 0.97; duration: 120; easing.type: Easing.InCubic }
    }

    Connections {
        target: wallpaperPreviewController
        function onVisibleChanged() {
            if (!wallpaperPreviewController.visible && root.opened) {
                root.close()
            }
        }
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        translucent: root.translucentSurface
        active: root.visible
        backdropSource: root.backdropSource
        backdropTransformItem: root
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 18
        shadowVerticalOffset: 6
    }

    contentItem: Item {
        id: contentRoot
        focus: true

        Keys.onReturnPressed: {
            if (wallpaperPreviewController.canApply) {
                wallpaperPreviewController.apply()
            }
        }
        Keys.onEnterPressed: {
            if (wallpaperPreviewController.canApply) {
                wallpaperPreviewController.apply()
            }
        }

        DialogHeader {
        id: dialogHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/image.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: "Set as Wallpaper"
        subtitle: wallpaperPreviewController.fileName
                  + (wallpaperPreviewController.fileSizeText.length > 0
                     ? " · " + wallpaperPreviewController.fileSizeText
                     : "")
        onCloseRequested: root.close()
    }

    DialogFooter {
        id: dialogFooter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: wallpaperPreviewController.errorText.length > 0
                      ? wallpaperPreviewController.errorText
                      : wallpaperPreviewController.filePath
                color: wallpaperPreviewController.errorText.length > 0
                       ? Theme.danger
                       : Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                elide: Text.ElideMiddle
            }

            FmButton {
                text: "Cancel"
                highlighted: false
                onClicked: root.close()
            }

            FmButton {
                text: "Apply Wallpaper"
                highlighted: true
                primaryColor: root.dialogAccent
                enabled: wallpaperPreviewController.canApply
                onClicked: wallpaperPreviewController.apply()
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

        // Left: Live Desktop Preview
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 380
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.78 : 0.90)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.48)
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Desktop preview"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                    }

                    Item { Layout.fillWidth: true }

                    // Image resolution pill
                    Rectangle {
                        implicitWidth: imgResLabel.implicitWidth + 14
                        implicitHeight: 22
                        radius: 11
                        color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.80 : 0.92)
                        border.color: Theme.withAlpha(Theme.panelBorder, 0.45)

                        Label {
                            id: imgResLabel
                            anchors.centerIn: parent
                            text: "Image " + wallpaperPreviewController.imageResolutionText
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeMicro
                        }
                    }

                    // Screen resolution pill
                    Rectangle {
                        implicitWidth: screenResLabel.implicitWidth + 14
                        implicitHeight: 22
                        radius: 11
                        color: Theme.withAlpha(root.dialogAccent, 0.14)
                        border.color: Theme.withAlpha(root.dialogAccent, 0.36)

                        Label {
                            id: screenResLabel
                            anchors.centerIn: parent
                            text: "Display " + wallpaperPreviewController.targetScreenResolutionText
                            color: root.dialogAccent
                            font.pixelSize: Theme.fontSizeMicro
                            font.weight: Font.DemiBold
                        }
                    }
                }

                // Monitor preview stage
                Item {
                    id: previewStage
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    readonly property var monitorMetrics: wallpaperPreviewController.targetAspectRatio > 0
                                                          ? wallpaperPreviewController.computeMonitorRect(
                                                                Math.max(120, width - 24),
                                                                Math.max(80, height - 28))
                                                          : ({ width: 320, height: 180 })
                    readonly property var imageMetrics: wallpaperPreviewController.targetScreenWidth > 0
                                                        && wallpaperPreviewController.imageWidth >= 0
                                                        ? wallpaperPreviewController.computeImagePreviewMetrics(
                                                              monitorFrame.width,
                                                              monitorFrame.height)
                                                        : ({ scaledWidth: 16, scaledHeight: 16 })

                    // Monitor bezel
                    Rectangle {
                        id: monitorBezel
                        anchors.centerIn: parent
                        width: monitorFrame.width + 12
                        height: monitorFrame.height + 12
                        radius: Theme.radiusMd + 2
                        color: Theme.withAlpha(Theme.bg, themeController.isDark ? 0.92 : 0.88)
                        border.color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.62 : 0.48)
                        border.width: 1

                        // Screen viewport
                        Rectangle {
                            id: monitorFrame
                            anchors.centerIn: parent
                            width: previewStage.monitorMetrics.width
                            height: previewStage.monitorMetrics.height
                            radius: Theme.radiusSm
                            color: wallpaperPreviewController.backgroundColor
                            clip: true

                            // Blurred backdrop source (KDE Blur option for Fit / Center)
                            Image {
                                id: blurBackdropSource
                                anchors.fill: parent
                                source: wallpaperPreviewController.fileUrl
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                visible: false
                            }

                            MultiEffect {
                                anchors.fill: parent
                                source: blurBackdropSource
                                visible: wallpaperPreviewController.backgroundOptionsEnabled
                                         && wallpaperPreviewController.blurBackground
                                         && blurBackdropSource.status === Image.Ready
                                blurEnabled: true
                                blur: 0.85
                                blurMax: 48
                                autoPaddingEnabled: false
                            }

                            // Fill / Fit / Stretch / Tile preview image
                            Image {
                                anchors.fill: parent
                                visible: wallpaperPreviewController.fillMode !== "center"
                                source: wallpaperPreviewController.fileUrl
                                asynchronous: true
                                cache: false
                                smooth: true
                                mipmap: true
                                fillMode: {
                                    switch (wallpaperPreviewController.fillMode) {
                                    case "fit": return Image.PreserveAspectFit
                                    case "stretch": return Image.Stretch
                                    case "tile": return Image.Tile
                                    default: return Image.PreserveAspectCrop
                                    }
                                }
                                sourceSize: wallpaperPreviewController.fillMode === "tile"
                                            ? Qt.size(Math.max(16, previewStage.imageMetrics.scaledWidth),
                                                      Math.max(16, previewStage.imageMetrics.scaledHeight))
                                            : Qt.size(0, 0)
                            }

                            // Center preview image (scaled proportionally to target screen resolution)
                            Image {
                                anchors.centerIn: parent
                                visible: wallpaperPreviewController.fillMode === "center"
                                width: previewStage.imageMetrics.scaledWidth
                                height: previewStage.imageMetrics.scaledHeight
                                source: wallpaperPreviewController.fileUrl
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: false
                                smooth: true
                                mipmap: true
                            }

                            // Mock desktop taskbar / panel
                            Rectangle {
                                visible: wallpaperPreviewController.showDesktopChrome
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: Math.max(16, Math.round(parent.height * 0.075))
                                color: Theme.withAlpha(Theme.bg, 0.72)

                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 1
                                    color: Theme.withAlpha(Theme.panelBorder, 0.35)
                                }

                                Row {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5

                                    Rectangle {
                                        width: 8; height: 8; radius: 4
                                        color: Theme.withAlpha(root.dialogAccent, 0.85)
                                    }
                                    Repeater {
                                        model: 3
                                        Rectangle {
                                            width: 14; height: 6; radius: 3
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: Theme.withAlpha(Theme.textPrimary, 0.32)
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 22; height: 5; radius: 2.5
                                    color: Theme.withAlpha(Theme.textPrimary, 0.42)
                                }
                            }
                        }
                    }

                    // Minimal monitor stand indicator
                    Rectangle {
                        anchors.top: monitorBezel.bottom
                        anchors.horizontalCenter: monitorBezel.horizontalCenter
                        width: Math.max(38, Math.round(monitorBezel.width * 0.14))
                        height: 6
                        color: Theme.withAlpha(Theme.panelBorder, 0.45)
                    }
                    Rectangle {
                        anchors.top: monitorBezel.bottom
                        anchors.topMargin: 6
                        anchors.horizontalCenter: monitorBezel.horizontalCenter
                        width: Math.max(72, Math.round(monitorBezel.width * 0.28))
                        height: 4
                        radius: 2
                        color: Theme.withAlpha(Theme.panelBorder, 0.55)
                    }
                }

                // Bottom preview bar: resolution hint + taskbar toggle
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        visible: wallpaperPreviewController.resolutionHintText.length > 0
                        text: wallpaperPreviewController.resolutionHintText
                        color: wallpaperPreviewController.upscalingWarning
                               ? Theme.warning
                               : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        elide: Text.ElideRight
                    }

                    Label {
                        text: "Desktop bar"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                    }

                    FmSwitch {
                        checked: wallpaperPreviewController.showDesktopChrome
                        onToggled: wallpaperPreviewController.setShowDesktopChrome(checked)
                    }
                }
            }
        }

        // Right: Options Panel
        Flickable {
            Layout.preferredWidth: 312
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: optionsColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FmScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: optionsColumn
                width: parent.width
                spacing: 12

                // Scaling / Fill Mode Section
                DialogSection {
                    title: "SCALING"
                    accentColor: root.dialogAccent

                    Repeater {
                        model: wallpaperPreviewController.fillModes

                        Rectangle {
                            id: modeCard
                            required property var modelData
                            readonly property bool selected: wallpaperPreviewController.fillMode === modelData.id

                            Layout.fillWidth: true
                            implicitHeight: 44
                            radius: Theme.radiusSm
                            color: selected
                                   ? Theme.itemSelectedFill
                                   : (modeMouse.containsMouse
                                      ? Theme.itemHoverFill
                                      : "transparent")
                            border.color: selected
                                          ? Theme.itemSelectedBorder
                                          : Theme.panelStrokeSubtle
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10

                                Rectangle {
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: "transparent"
                                    border.width: 1.5
                                    border.color: modeCard.selected ? Theme.activeAccent : Theme.textSecondary

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 6
                                        height: 6
                                        radius: 3
                                        visible: modeCard.selected
                                        color: Theme.activeAccent
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label {
                                        text: modeCard.modelData.label
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: modeCard.selected ? Font.DemiBold : Font.Normal
                                    }
                                    Label {
                                        text: modeCard.modelData.description
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeMicro
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            MouseArea {
                                id: modeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: wallpaperPreviewController.setFillMode(modeCard.modelData.id)
                            }
                        }
                    }
                }

                // Background Section (Fit / Center)
                DialogSection {
                    title: "BACKGROUND"
                    accentColor: root.dialogAccent
                    opacity: wallpaperPreviewController.backgroundOptionsEnabled ? 1.0 : 0.45
                    enabled: wallpaperPreviewController.backgroundOptionsEnabled

                    RowLayout {
                        Layout.fillWidth: true
                        visible: wallpaperPreviewController.supportsBlur
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: "Blurred image backdrop"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeLabel
                            }
                            Label {
                                text: "Fill empty margins with blurred wallpaper"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeMicro
                            }
                        }

                        FmSwitch {
                            checked: wallpaperPreviewController.blurBackground
                            onToggled: wallpaperPreviewController.setBlurBackground(checked)
                        }
                    }

                    Label {
                        text: "Fill color"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.topMargin: wallpaperPreviewController.supportsBlur ? 4 : 0
                        opacity: wallpaperPreviewController.blurBackground ? 0.45 : 1.0
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        enabled: !wallpaperPreviewController.blurBackground
                        opacity: wallpaperPreviewController.blurBackground ? 0.45 : 1.0

                        Repeater {
                            model: wallpaperPreviewController.backgroundPresets

                            Rectangle {
                                id: swatch
                                required property var modelData
                                readonly property bool selected: Qt.colorEqual(wallpaperPreviewController.backgroundColor, modelData.color)

                                width: 28
                                height: 28
                                radius: Theme.radiusSm
                                color: modelData.color
                                border.width: selected ? 2 : 1
                                border.color: selected
                                              ? root.dialogAccent
                                              : Theme.withAlpha(Theme.panelBorder, 0.55)

                                ToolTip.visible: swatchMouse.containsMouse
                                ToolTip.text: modelData.label

                                MouseArea {
                                    id: swatchMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: wallpaperPreviewController.setBackgroundColor(swatch.modelData.color)
                                }
                            }
                        }
                    }
                }

                // Display Section (Multi-monitor on KDE Plasma)
                DialogSection {
                    visible: wallpaperPreviewController.supportsPerScreen && wallpaperPreviewController.screens.length > 1
                    title: "DISPLAY"
                    accentColor: root.dialogAccent

                    Repeater {
                        model: wallpaperPreviewController.screens

                        Rectangle {
                            id: screenCard
                            required property var modelData
                            readonly property bool selected: wallpaperPreviewController.screenIndex === modelData.index

                            Layout.fillWidth: true
                            implicitHeight: 38
                            radius: Theme.radiusSm
                            color: selected
                                   ? Theme.itemSelectedFill
                                   : (screenMouse.containsMouse
                                      ? Theme.itemHoverFill
                                      : "transparent")
                            border.color: selected
                                          ? Theme.itemSelectedBorder
                                          : Theme.panelStrokeSubtle
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: screenCard.modelData.label
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeLabel
                                    font.weight: screenCard.selected ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: screenCard.modelData.subtitle
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeMicro
                                }
                            }

                            MouseArea {
                                id: screenMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: wallpaperPreviewController.setScreenIndex(screenCard.modelData.index)
                            }
                        }
                    }
                }
            }
        }
    }
    }
}
