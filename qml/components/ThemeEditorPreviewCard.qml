import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "../style"

Rectangle {
    id: preview

    property var editor
    readonly property string hoveredTokenKey: editor ? editor.hoveredTokenKey : ""
    readonly property bool lightMode: editor && editor.workingState && editor.workingState.mode === "light"

    function previewColor(key, fallback) {
        return editor ? editor.previewColor(key, fallback) : fallback
    }

    function themeName() {
        return editor && editor.themeName().trim().length > 0 ? editor.themeName() : "FM Theme Draft"
    }

    function h(key) {
        return hoveredTokenKey === key && key.length > 0
    }

    function h2(a, b) {
        return h(a) || h(b)
    }

    function h3(a, b, c) {
        return h(a) || h(b) || h(c)
    }

    function h4(a, b, c, d) {
        return h(a) || h(b) || h(c) || h(d)
    }

    function containsToken(keys) {
        if (!keys || hoveredTokenKey.length === 0) {
            return false
        }
        for (let i = 0; i < keys.length; ++i) {
            if (keys[i] === hoveredTokenKey) {
                return true
            }
        }
        return false
    }

    readonly property color pBg: previewColor("bg", Theme.bg)
    readonly property color pSurface: previewColor("surface", Theme.surface)
    readonly property color pSurfHov: previewColor("surfaceHover", Theme.surfaceHover)
    readonly property color pSurfAct: previewColor("surfaceActive", Theme.surfaceActive)
    readonly property color pText: previewColor("textPrimary", Theme.textPrimary)
    readonly property color pTextSec: previewColor("textSecondary", Theme.textSecondary)
    readonly property color pBorder: previewColor("border", Theme.border)
    readonly property color pAccent: previewColor("accent", Theme.accent)
    readonly property color pAccentText: previewColor("accentText", Theme.accentText)
    readonly property color pFocusRing: previewColor("focusRing", Theme.focusRing)
    readonly property color pDanger: previewColor("danger", Theme.danger)
    readonly property color pSuccess: previewColor("success", Theme.success)
    readonly property color pWarning: previewColor("warning", Theme.warning)
    readonly property color pActiveAcc: previewColor("activeAccent", Theme.activeAccent)
    readonly property color pActiveGlow: previewColor("activeGlow", Theme.activeGlow)
    readonly property color pPanelSoft: previewColor("panelSurfaceSoft", Theme.panelSurfaceSoft)
    readonly property color pPanelStrong: previewColor("panelSurfaceStrong", Theme.panelSurfaceStrong)
    readonly property color pPanel: previewColor("panelSurface", Theme.panelSurface)
    readonly property color pPanelBrd: previewColor("panelBorder", Theme.panelBorder)
    readonly property color pCtrl: previewColor("controlSurface", Theme.controlSurface)
    readonly property color pCtrlAct: previewColor("controlSurfaceActive", Theme.controlSurfaceActive)
    readonly property color pCtrlBrd: previewColor("controlBorder", Theme.controlBorder)
    readonly property color pItemHover: previewColor("itemHoverFill", Theme.itemHoverFill)
    readonly property color pItemCur: previewColor("itemCurrentFill", Theme.itemCurrentFill)
    readonly property color pItemCurBrd: previewColor("itemCurrentBorder", Theme.itemCurrentBorder)
    readonly property color pSelFill: previewColor("itemSelectedFill", Theme.itemSelectedFill)
    readonly property color pSelFillInact: previewColor("itemSelectedFillInactive", Theme.itemSelectedFillInactive)
    readonly property color pSelBrd: previewColor("itemSelectedBorder", Theme.itemSelectedBorder)
    readonly property color pSelBrdInact: previewColor("itemSelectedBorderInactive", Theme.itemSelectedBorderInactive)
    readonly property color pStatusRail: previewColor("statusRailFill", Theme.statusRailFill)
    readonly property color pMenuBrd: previewColor("menuBorder", Theme.menuBorder)
    readonly property color pMenuSep: previewColor("menuSeparator", Theme.menuSeparator)
    readonly property color pMenuPress: previewColor("menuItemPressed", Theme.menuItemPressed)
    readonly property color pChromeStart: previewColor("chromeGradientStart", Theme.chromeGradientStart)
    readonly property color pChromeMid: previewColor("chromeGradientMid", Theme.chromeGradientMid)
    readonly property color pChromeEnd: previewColor("chromeGradientEnd", Theme.chromeGradientEnd)
    readonly property color pGlassShadow: previewColor("glassShadow", Theme.glassShadow)
    readonly property color pShadow: previewColor("shadow", Theme.shadow)
    readonly property color pOverlayScrim: previewColor("overlayScrim", Theme.overlayScrim)
    readonly property color pSecondary: previewColor("secondaryAccent", Theme.secondaryAccent)
    readonly property color pWarm: previewColor("warmAccent", Theme.warmAccent)
    readonly property color pCatInfo: previewColor("categoryInfo", Theme.categoryInfo)
    readonly property color pCatNav: previewColor("categoryNavigation", Theme.categoryNavigation)
    readonly property color pCatAction: previewColor("categoryAction", Theme.categoryAction)
    readonly property color pCatUtility: previewColor("categoryUtility", Theme.categoryUtility)
    readonly property color pCatSystem: previewColor("categorySystem", Theme.categorySystem)

    radius: Theme.radiusLg
    clip: true
    color: pBg
    border.color: h2("bg", "border") ? Theme.accent : Theme.withAlpha(pBorder, 0.70)
    border.width: h2("bg", "border") ? 2 : 1

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0.00; color: pBg }
            GradientStop { position: 0.66; color: Theme.withAlpha(pPanelSoft, lightMode ? 0.70 : 0.38) }
            GradientStop { position: 1.00; color: Theme.withAlpha(pAccent, lightMode ? 0.035 : 0.075) }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 68
            radius: 10
            border.color: h4("chromeGradientStart", "chromeGradientMid", "chromeGradientEnd", "panelBorder")
                          ? Theme.accent
                          : Theme.withAlpha(pPanelBrd, 0.78)
            border.width: h4("chromeGradientStart", "chromeGradientMid", "chromeGradientEnd", "panelBorder") ? 2 : 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.00; color: pChromeStart }
                GradientStop { position: 0.52; color: pChromeMid }
                GradientStop { position: 1.00; color: pChromeEnd }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: pAccent
                        border.color: h("accent") ? Theme.accent : "transparent"
                        border.width: h("accent") ? 2 : 0
                    }

                    Label {
                        text: themeName()
                        Layout.preferredWidth: 150
                        color: pText
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        Layout.preferredWidth: 76
                        Layout.preferredHeight: 20
                        radius: 10
                        color: Theme.withAlpha(pWarm, 0.15)
                        border.color: h("warmAccent") ? Theme.accent : Theme.withAlpha(pWarm, 0.42)
                        border.width: h("warmAccent") ? 2 : 1
                        Label {
                            anchors.centerIn: parent
                            text: lightMode ? "Light base" : "Dark base"
                            color: h("warmAccent") ? Theme.accent : pWarm
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                        PreviewTokenHighlight { keys: ["warmAccent"] }
                    }

                    Item { Layout.fillWidth: true }

                    Repeater {
                        model: [
                            { label: "Info", key: "categoryInfo", c: pCatInfo },
                            { label: "Nav", key: "categoryNavigation", c: pCatNav },
                            { label: "Action", key: "categoryAction", c: pCatAction }
                        ]
                        delegate: Rectangle {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 20
                            radius: 10
                            color: Theme.withAlpha(modelData.c, 0.14)
                            border.color: h(modelData.key) ? Theme.accent : Theme.withAlpha(modelData.c, 0.36)
                            border.width: h(modelData.key) ? 2 : 1
                            Label {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: h(modelData.key) ? Theme.accent : modelData.c
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                            PreviewTokenHighlight { keys: [modelData.key] }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Repeater {
                        model: [
                            { label: "Back", active: false },
                            { label: "Up", active: true },
                            { label: "Refresh", active: false }
                        ]
                        delegate: Rectangle {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 26
                            radius: 8
                            color: modelData.active ? pCtrlAct : pCtrl
                            border.color: h3("controlSurface", "controlSurfaceActive", "controlBorder")
                                          ? Theme.accent
                                          : Theme.withAlpha(pCtrlBrd, 0.72)
                            border.width: h3("controlSurface", "controlSurfaceActive", "controlBorder") ? 2 : 1
                            Label {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: pText
                                font.pixelSize: 9
                                font.weight: modelData.active ? Font.DemiBold : Font.Medium
                            }
                            PreviewTokenHighlight {
                                keys: modelData.active
                                      ? ["controlSurfaceActive", "controlBorder", "textPrimary"]
                                      : ["controlSurface", "controlBorder", "textPrimary"]
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        radius: 8
                        color: pCtrl
                        border.color: h3("controlSurface", "controlBorder", "textSecondary")
                                      ? Theme.accent
                                      : Theme.withAlpha(pCtrlBrd, 0.76)
                        border.width: h3("controlSurface", "controlBorder", "textSecondary") ? 2 : 1
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 9
                            anchors.rightMargin: 9
                            spacing: 5
                            Label { text: "C:/"; color: pAccent; font.pixelSize: 9; font.weight: Font.DemiBold }
                            Label {
                                text: "Users / tankr / Documents / FM"
                                Layout.fillWidth: true
                                color: pTextSec
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                        }
                        PreviewTokenHighlight { keys: ["controlSurface", "controlBorder", "textSecondary", "accent"] }
                    }

                    Rectangle {
                        Layout.preferredWidth: 84
                        Layout.preferredHeight: 26
                        radius: 8
                        color: pAccent
                        border.color: h2("accent", "accentText") ? Theme.accent : "transparent"
                        border.width: h2("accent", "accentText") ? 2 : 0
                        Label {
                            anchors.centerIn: parent
                            text: "Apply"
                            color: pAccentText
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                        PreviewTokenHighlight { keys: ["accent", "accentText"] }
                    }
                }
            }
            PreviewTokenHighlight { keys: ["chromeGradientStart", "chromeGradientMid", "chromeGradientEnd", "panelBorder"] }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 138
                Layout.fillHeight: true
                radius: 10
                color: pPanel
                border.color: h2("panelSurface", "panelBorder") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.84)
                border.width: h2("panelSurface", "panelBorder") ? 2 : 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 5

                    Label {
                        text: "Workspace"
                        Layout.fillWidth: true
                        color: pText
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Repeater {
                        model: [
                            { label: "Home", active: true, key: "categoryNavigation", c: pCatNav },
                            { label: "Favorites", active: false, key: "secondaryAccent", c: pSecondary },
                            { label: "System", active: false, key: "categorySystem", c: pCatSystem },
                            { label: "Images", active: false, key: "categoryInfo", c: pCatInfo }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 28
                            radius: 8
                            color: modelData.active ? pSelFillInact : "transparent"
                            border.color: modelData.active ? pSelBrdInact : "transparent"
                            border.width: modelData.active ? 1 : 0
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 7
                                anchors.rightMargin: 6
                                spacing: 7
                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: modelData.c
                                    border.color: h(modelData.key) ? Theme.accent : "transparent"
                                    border.width: h(modelData.key) ? 1 : 0
                                }
                                Label {
                                    text: modelData.label
                                    Layout.fillWidth: true
                                    color: modelData.active ? pText : pTextSec
                                    font.pixelSize: 10
                                    font.weight: modelData.active ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                            }
                            PreviewTokenHighlight {
                                keys: modelData.active
                                      ? ["itemSelectedFillInactive", "itemSelectedBorderInactive", modelData.key, "textPrimary"]
                                      : [modelData.key, "textSecondary"]
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: h("border") ? Theme.accent : Theme.withAlpha(pBorder, 0.44)
                        PreviewTokenHighlight { keys: ["border"] }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 74
                        radius: 9
                        color: pPanelSoft
                        border.color: h2("panelSurfaceSoft", "surfaceHover") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.48)
                        border.width: h2("panelSurfaceSoft", "surfaceHover") ? 2 : 1
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 5
                            Label {
                                text: "Drive C"
                                Layout.fillWidth: true
                                color: pText
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 6
                                radius: 3
                                color: Theme.withAlpha(pAccent, 0.16)
                                Rectangle {
                                    width: parent.width * 0.58
                                    height: parent.height
                                    radius: parent.radius
                                    color: pAccent
                                    border.color: h("accent") ? Theme.accent : "transparent"
                                    border.width: h("accent") ? 1 : 0
                                }
                            }
                            Label {
                                text: "168 GB free"
                                Layout.fillWidth: true
                                color: pTextSec
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                        }
                        PreviewTokenHighlight { keys: ["panelSurfaceSoft", "surfaceHover", "accent", "textPrimary", "textSecondary"] }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 58
                        radius: 9
                        color: pPanelStrong
                        border.color: h("panelSurfaceStrong") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.58)
                        border.width: h("panelSurfaceStrong") ? 2 : 1
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 3
                            Label { text: "Pinned"; color: pTextSec; font.pixelSize: 8 }
                            Label {
                                text: "Theme drafts"
                                Layout.fillWidth: true
                                color: pText
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }
                        PreviewTokenHighlight { keys: ["panelSurfaceStrong", "textPrimary", "textSecondary"] }
                    }

                    Item { Layout.fillHeight: true }
                }
                PreviewTokenHighlight { keys: ["panelSurface", "panelBorder"] }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: activePanel
                    anchors.margins: -6
                    radius: 16
                    color: Theme.withAlpha(pActiveGlow, h2("activeAccent", "activeGlow") ? 0.23 : 0.14)
                    border.color: h2("activeAccent", "activeGlow") ? Theme.accent : pActiveAcc
                    border.width: h2("activeAccent", "activeGlow") ? 2 : 1
                    PreviewTokenHighlight { keys: ["activeAccent", "activeGlow"] }
                }

                Rectangle {
                    id: activePanel
                    anchors.fill: parent
                    radius: 11
                    color: pSurface
                    border.color: h2("surface", "border") ? Theme.accent : Theme.withAlpha(pBorder, 0.66)
                    border.width: h2("surface", "border") ? 2 : 1
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: h("shadow") ? Theme.accent : pShadow
                        shadowBlur: 18
                        shadowVerticalOffset: 1
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 7

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            radius: 8
                            color: pPanelSoft
                            border.color: h("panelSurfaceSoft") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.46)
                            border.width: h("panelSurfaceSoft") ? 2 : 1
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6
                                Label {
                                    text: "Documents"
                                    color: pText
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                                Label { text: "42 items"; color: pTextSec; font.pixelSize: 9 }
                                Item { Layout.fillWidth: true }
                                Repeater {
                                    model: [
                                        { label: "Details", key: "categoryInfo", c: pCatInfo },
                                        { label: "Grid", key: "categoryNavigation", c: pCatNav },
                                        { label: "Brief", key: "categoryUtility", c: pCatUtility }
                                    ]
                                    delegate: Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 18
                                        radius: 9
                                        color: Theme.withAlpha(modelData.c, 0.14)
                                        border.color: h(modelData.key) ? Theme.accent : Theme.withAlpha(modelData.c, 0.34)
                                        border.width: h(modelData.key) ? 2 : 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: h(modelData.key) ? Theme.accent : modelData.c
                                            font.pixelSize: 8
                                            font.weight: Font.DemiBold
                                        }
                                        PreviewTokenHighlight { keys: [modelData.key] }
                                    }
                                }
                            }
                            PreviewTokenHighlight { keys: ["panelSurfaceSoft", "textPrimary", "textSecondary"] }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 9
                                color: Theme.withAlpha(pSurface, lightMode ? 0.72 : 0.42)
                                border.color: h("border") ? Theme.accent : Theme.withAlpha(pBorder, 0.36)
                                border.width: h("border") ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    spacing: 3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        implicitHeight: 20
                                        spacing: 6
                                        Label {
                                            text: "Name"
                                            Layout.fillWidth: true
                                            color: pTextSec
                                            font.pixelSize: 9
                                            font.weight: Font.DemiBold
                                        }
                                        Label {
                                            text: "Size"
                                            Layout.preferredWidth: 44
                                            color: pTextSec
                                            font.pixelSize: 9
                                            horizontalAlignment: Text.AlignRight
                                        }
                                        Label {
                                            text: "Modified"
                                            Layout.preferredWidth: 58
                                            color: pTextSec
                                            font.pixelSize: 9
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 1
                                        color: h("border") ? Theme.accent : Theme.withAlpha(pBorder, 0.42)
                                        PreviewTokenHighlight { keys: ["border"] }
                                    }

                                    Repeater {
                                        model: [
                                            { name: "FMQml", size: "-", date: "Now", state: "current", key: "categoryNavigation", c: pCatNav },
                                            { name: "theme-draft.json", size: "3 KB", date: "Today", state: "selected", key: "categoryAction", c: pCatAction },
                                            { name: "palette-preview.png", size: "1 MB", date: "Today", state: "hover", key: "categoryInfo", c: pCatInfo },
                                            { name: "rename-target.txt", size: "4 KB", date: "1m", state: "focus", key: "warmAccent", c: pWarm },
                                            { name: "logs", size: "-", date: "2d", state: "pressed", key: "categorySystem", c: pCatSystem },
                                            { name: "exports", size: "-", date: "1w", state: "normal", key: "secondaryAccent", c: pSecondary }
                                        ]
                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            implicitHeight: 29
                                            radius: 8

                                            readonly property bool isSelected: modelData.state === "selected"
                                            readonly property bool isHover: modelData.state === "hover"
                                            readonly property bool isCurrent: modelData.state === "current"
                                            readonly property bool isFocus: modelData.state === "focus"
                                            readonly property bool isPressed: modelData.state === "pressed"

                                            color: isSelected ? pSelFill
                                                 : isHover ? pItemHover
                                                 : isCurrent ? pItemCur
                                                 : isPressed ? pSurfAct
                                                 : "transparent"
                                            border.color: isSelected ? pSelBrd
                                                        : (isCurrent || isFocus) ? pItemCurBrd
                                                        : isPressed ? Theme.withAlpha(pBorder, 0.52)
                                                        : "transparent"
                                            border.width: (isSelected || isCurrent || isFocus || isPressed) ? 1 : 0

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 7
                                                anchors.rightMargin: 7
                                                spacing: 7
                                                Rectangle {
                                                    Layout.preferredWidth: 8
                                                    Layout.preferredHeight: 8
                                                    radius: 4
                                                    color: modelData.c
                                                    border.color: h(modelData.key) ? Theme.accent : "transparent"
                                                    border.width: h(modelData.key) ? 1 : 0
                                                }
                                                Rectangle {
                                                    visible: modelData.state === "focus"
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 21
                                                    radius: 6
                                                    color: pCtrl
                                                    border.color: h("focusRing") ? Theme.accent : pFocusRing
                                                    border.width: 2
                                                    Label {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 7
                                                        anchors.rightMargin: 7
                                                        text: modelData.name
                                                        color: pText
                                                        font.pixelSize: 9
                                                        verticalAlignment: Text.AlignVCenter
                                                        elide: Text.ElideRight
                                                    }
                                                    PreviewTokenHighlight { keys: ["focusRing", "controlSurface", "textPrimary"] }
                                                }
                                                Label {
                                                    visible: modelData.state !== "focus"
                                                    text: modelData.name
                                                    Layout.fillWidth: true
                                                    color: pText
                                                    font.pixelSize: 10
                                                    font.weight: isSelected || isCurrent ? Font.DemiBold : Font.Normal
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    text: modelData.size
                                                    Layout.preferredWidth: 44
                                                    color: pTextSec
                                                    font.pixelSize: 9
                                                    horizontalAlignment: Text.AlignRight
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    text: modelData.date
                                                    Layout.preferredWidth: 58
                                                    color: pTextSec
                                                    font.pixelSize: 9
                                                    horizontalAlignment: Text.AlignRight
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            PreviewTokenHighlight {
                                                keys: isSelected ? ["itemSelectedFill", "itemSelectedBorder", modelData.key, "textPrimary", "textSecondary"]
                                                    : isHover ? ["itemHoverFill", modelData.key, "textPrimary", "textSecondary"]
                                                    : isCurrent ? ["itemCurrentFill", "itemCurrentBorder", modelData.key, "textPrimary", "textSecondary"]
                                                    : isFocus ? ["itemCurrentBorder", "focusRing", "controlSurface", "textPrimary", "textSecondary"]
                                                    : isPressed ? ["surfaceActive", "border", modelData.key, "textPrimary", "textSecondary"]
                                                    : [modelData.key, "textPrimary", "textSecondary"]
                                            }
                                        }
                                    }

                                    Item { Layout.fillHeight: true }
                                }
                                PreviewTokenHighlight { keys: ["surface", "border"] }
                            }

                            Rectangle {
                                Layout.preferredWidth: 184
                                Layout.fillHeight: true
                                radius: 9
                                color: pPanelStrong
                                border.color: h("panelSurfaceStrong") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.62)
                                border.width: h("panelSurfaceStrong") ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 7

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 88
                                        radius: 9
                                        color: pPanelSoft
                                        border.color: h("panelSurfaceSoft") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.48)
                                        border.width: h("panelSurfaceSoft") ? 2 : 1
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 7
                                            spacing: 5
                                            Label {
                                                text: "Grid tile"
                                                Layout.fillWidth: true
                                                color: pText
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6
                                                Repeater {
                                                    model: [
                                                        { c: pCatInfo, key: "categoryInfo" },
                                                        { c: pCatNav, key: "categoryNavigation" },
                                                        { c: pCatUtility, key: "categoryUtility" }
                                                    ]
                                                    delegate: Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 38
                                                        radius: 7
                                                        color: Theme.withAlpha(modelData.c, 0.18)
                                                        border.color: h(modelData.key) ? Theme.accent : Theme.withAlpha(modelData.c, 0.34)
                                                        border.width: h(modelData.key) ? 2 : 1
                                                        PreviewTokenHighlight { keys: [modelData.key] }
                                                    }
                                                }
                                            }
                                            Label {
                                                text: "Thumbnails, folders, media"
                                                Layout.fillWidth: true
                                                color: pTextSec
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                        }
                                        PreviewTokenHighlight { keys: ["panelSurfaceSoft", "textPrimary", "textSecondary"] }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 58
                                        radius: 9
                                        color: pCtrl
                                        border.color: h2("controlSurface", "controlBorder") ? Theme.accent : Theme.withAlpha(pCtrlBrd, 0.74)
                                        border.width: h2("controlSurface", "controlBorder") ? 2 : 1
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 7
                                            spacing: 4
                                            Label { text: "Search"; color: pTextSec; font.pixelSize: 8 }
                                            Label {
                                                text: "theme colors"
                                                Layout.fillWidth: true
                                                color: pText
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                        PreviewTokenHighlight { keys: ["controlSurface", "controlBorder", "textPrimary", "textSecondary"] }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        columnSpacing: 6
                                        rowSpacing: 6

                                        Repeater {
                                            model: [
                                                { label: "OK", key: "success", c: pSuccess },
                                                { label: "Warn", key: "warning", c: pWarning },
                                                { label: "Danger", key: "danger", c: pDanger },
                                                { label: "Util", key: "categoryUtility", c: pCatUtility }
                                            ]
                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 26
                                                radius: 8
                                                color: Theme.withAlpha(modelData.c, 0.15)
                                                border.color: h(modelData.key) ? Theme.accent : Theme.withAlpha(modelData.c, 0.40)
                                                border.width: h(modelData.key) ? 2 : 1
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: modelData.label
                                                    color: h(modelData.key) ? Theme.accent : modelData.c
                                                    font.pixelSize: 8
                                                    font.weight: Font.DemiBold
                                                }
                                                PreviewTokenHighlight { keys: [modelData.key] }
                                            }
                                        }
                                    }

                                    Item { Layout.fillHeight: true }
                                }
                                PreviewTokenHighlight { keys: ["panelSurfaceStrong", "panelBorder"] }
                            }
                        }
                    }
                    PreviewTokenHighlight { keys: ["surface", "border", "shadow"] }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            radius: 10
            color: pStatusRail
            border.color: h3("statusRailFill", "panelSurface", "panelBorder") ? Theme.accent : Theme.withAlpha(pPanelBrd, 0.78)
            border.width: h3("statusRailFill", "panelSurface", "panelBorder") ? 2 : 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 9

                ColumnLayout {
                    Layout.preferredWidth: 112
                    spacing: 3
                    Label { text: "Operations"; color: pTextSec; font.pixelSize: 8 }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 5
                        radius: 3
                        color: Theme.withAlpha(pAccent, 0.16)
                        Rectangle {
                            width: parent.width * 0.64
                            height: parent.height
                            radius: parent.radius
                            color: pAccent
                            PreviewTokenHighlight { keys: ["accent"] }
                        }
                    }
                }

                Repeater {
                    model: [
                        { label: "Copied", key: "success", c: pSuccess },
                        { label: "Queued", key: "warning", c: pWarning },
                        { label: "Blocked", key: "danger", c: pDanger }
                    ]
                    delegate: Rectangle {
                        Layout.preferredWidth: 68
                        Layout.preferredHeight: 22
                        radius: 11
                        color: Theme.withAlpha(modelData.c, 0.15)
                        border.color: h(modelData.key) ? Theme.accent : Theme.withAlpha(modelData.c, 0.42)
                        border.width: h(modelData.key) ? 2 : 1
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: h(modelData.key) ? Theme.accent : modelData.c
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                        PreviewTokenHighlight { keys: [modelData.key] }
                    }
                }

                Item { Layout.fillWidth: true }

                Repeater {
                    model: [
                        { label: "Copy", fill: pAccent, borderTone: pAccent, textTone: pAccentText, keys: ["accent", "accentText"] },
                        { label: "Move", fill: pCtrl, borderTone: pCtrlBrd, textTone: pText, keys: ["controlSurface", "controlBorder", "textPrimary"] },
                        { label: "Pressed", fill: pCtrlAct, borderTone: pCtrlBrd, textTone: pText, keys: ["controlSurfaceActive", "controlBorder", "textPrimary"] }
                    ]
                    delegate: Rectangle {
                        Layout.preferredWidth: 70
                        Layout.preferredHeight: 26
                        radius: 8
                        color: modelData.fill
                        border.color: containsToken(modelData.keys) ? Theme.accent : Theme.withAlpha(modelData.borderTone, 0.72)
                        border.width: containsToken(modelData.keys) ? 2 : 1
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: modelData.textTone
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                        PreviewTokenHighlight { keys: modelData.keys }
                    }
                }
            }
            PreviewTokenHighlight { keys: ["statusRailFill", "panelSurface", "panelBorder", "textSecondary"] }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 88
        anchors.rightMargin: 20
        width: 236
        height: 226
        radius: 14
        color: Theme.withAlpha(pOverlayScrim, 0.48)
        border.color: h("overlayScrim") ? Theme.accent : "transparent"
        border.width: h("overlayScrim") ? 2 : 0
        z: 20
        PreviewTokenHighlight { keys: ["overlayScrim"] }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            width: 196
            height: 190
            radius: 11
            color: pPanelStrong
            border.color: h2("menuBorder", "panelSurfaceStrong") ? Theme.accent : pMenuBrd
            border.width: h2("menuBorder", "panelSurfaceStrong") ? 2 : 1
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: h2("glassShadow", "shadow") ? Theme.accent : pGlassShadow
                shadowBlur: 24
                shadowVerticalOffset: 7
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        text: "Context menu"
                        Layout.fillWidth: true
                        color: pText
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 18
                        radius: 9
                        color: Theme.withAlpha(pCatSystem, 0.15)
                        border.color: h("categorySystem") ? Theme.accent : Theme.withAlpha(pCatSystem, 0.40)
                        border.width: h("categorySystem") ? 2 : 1
                        Label {
                            anchors.centerIn: parent
                            text: "sys"
                            color: h("categorySystem") ? Theme.accent : pCatSystem
                            font.pixelSize: 8
                            font.weight: Font.DemiBold
                        }
                        PreviewTokenHighlight { keys: ["categorySystem"] }
                    }
                }

                Repeater {
                    model: [
                        { label: "Open", hint: "Enter", key: "categoryAction", c: pCatAction, pressed: false, danger: false },
                        { label: "Pin to sidebar", hint: "P", key: "warmAccent", c: pWarm, pressed: true, danger: false },
                        { label: "Copy path", hint: "Ctrl+C", key: "secondaryAccent", c: pSecondary, pressed: false, danger: false },
                        { label: "Properties", hint: "Alt+Enter", key: "categoryUtility", c: pCatUtility, pressed: false, danger: false },
                        { label: "Delete", hint: "Del", key: "danger", c: pDanger, pressed: false, danger: true }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 24
                        radius: 8
                        color: modelData.pressed ? pMenuPress : "transparent"
                        border.color: modelData.pressed && h("menuItemPressed") ? Theme.accent : "transparent"
                        border.width: modelData.pressed && h("menuItemPressed") ? 2 : 0
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 7
                            anchors.rightMargin: 7
                            spacing: 7
                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: modelData.danger ? pDanger : modelData.c
                                border.color: h(modelData.key) ? Theme.accent : "transparent"
                                border.width: h(modelData.key) ? 1 : 0
                            }
                            Label {
                                text: modelData.label
                                Layout.fillWidth: true
                                color: modelData.danger ? pDanger : pText
                                font.pixelSize: 9
                                font.weight: modelData.pressed ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.hint
                                color: pTextSec
                                font.pixelSize: 8
                            }
                        }
                        PreviewTokenHighlight {
                            keys: modelData.pressed
                                  ? ["menuItemPressed", modelData.key, "textPrimary", "textSecondary"]
                                  : [modelData.key, "textPrimary", "textSecondary"]
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: h("menuSeparator") ? Theme.accent : pMenuSep
                    PreviewTokenHighlight { keys: ["menuSeparator"] }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [
                            { label: "info", key: "categoryInfo", c: pCatInfo },
                            { label: "nav", key: "categoryNavigation", c: pCatNav },
                            { label: "util", key: "categoryUtility", c: pCatUtility }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 19
                            radius: 9
                            color: Theme.withAlpha(modelData.c, 0.15)
                            border.color: h(modelData.key) ? Theme.accent : Theme.withAlpha(modelData.c, 0.38)
                            border.width: h(modelData.key) ? 2 : 1
                            Label {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: h(modelData.key) ? Theme.accent : modelData.c
                                font.pixelSize: 8
                                font.weight: Font.DemiBold
                            }
                            PreviewTokenHighlight { keys: [modelData.key] }
                        }
                    }
                }
            }
            PreviewTokenHighlight { keys: ["menuBorder", "panelSurfaceStrong", "glassShadow", "shadow"] }
        }
    }

    component PreviewTokenHighlight: Rectangle {
        property var keys: []
        readonly property bool active: preview.containsToken(keys)

        anchors.fill: parent
        radius: parent.radius
        color: active ? Theme.withAlpha(Theme.accent, 0.13) : "transparent"
        border.color: active ? Theme.accent : "transparent"
        border.width: active ? 2 : 0
        z: 10
    }
}
