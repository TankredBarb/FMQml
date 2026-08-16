import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../common"
import "../framework"
import "../../style"

Item {
    id: root

    property string text: ""
    property int lineCount: 0
    property bool loading: false
    property bool allowLoadFullText: false
    property bool textTruncated: false
    property bool fullTextAvailable: false
    property bool textChunked: false
    property int textChunkIndex: 0
    property int textChunkCount: 0
    property bool hasPreviousPage: textChunkIndex > 0
    property bool hasNextPage: textChunkIndex + 1 < textChunkCount
    property bool showPageControls: true
    property int firstLine: 1
    property bool wrapText: false
    property bool showLineNumbers: true
    property bool lineHeightFollowsContent: true
    property int fixedLineHeight: 18
    property int lineNumberWidth: 45
    property int textPadding: 24
    property int maximumLineNumbers: 0
    property int maximumUnwrappedTextLength: 8192
    property bool controlsVisible: true
    property string previewKey: ""
    property int fontPixelSize: 13
    property int defaultFontPixelSize: 13
    property bool defaultWrapText: false
    property int minimumFontPixelSize: 9
    property int maximumFontPixelSize: 24
    property string loadingTitle: "Loading preview..."
    property string loadingSubtitle: "Large files are loaded asynchronously."
    property string fontFamily: Theme.fontFamily
    property var styleRanges: []
    property color tokenColor1: "#7c3aed"
    property color tokenColor2: "#18794e"
    property color tokenColor3: "#667085"
    property color tokenColor4: "#b45309"
    property bool codeMode: false
    property string languageLabel: ""
    readonly property bool forcedWrapText: false
    readonly property bool effectiveWrapText: root.wrapText
    readonly property int visibleLineNumberCount: root.maximumLineNumbers > 0
                                             ? Math.min(root.lineCount, root.maximumLineNumbers)
                                             : root.lineCount
    property string preferencesKey: ""

    signal loadFullTextRequested()
    signal previousTextChunkRequested()
    signal nextTextChunkRequested()

    clip: true

    function sameColor(first: color, second: color): bool {
        return Math.abs(first.r - second.r) < 0.002
                && Math.abs(first.g - second.g) < 0.002
                && Math.abs(first.b - second.b) < 0.002
    }

    function documentTokenColor(value: color, lightDefault: color, darkDefault: color): color {
        return themeController.isDark && root.sameColor(value, lightDefault) ? darkDefault : value
    }

    function adjustFontSize(delta) {
        fontPixelSize = Math.max(minimumFontPixelSize, Math.min(maximumFontPixelSize, fontPixelSize + delta))
    }

    function resetViewPreferences() {
        fontPixelSize = defaultFontPixelSize
        wrapText = defaultWrapText
    }

    function applyDocumentPreferences() {
        if (root.loading || root.previewKey.length === 0 || root.preferencesKey === root.previewKey) {
            return
        }
        root.resetViewPreferences()
        root.preferencesKey = root.previewKey
    }

    Component.onCompleted: applyDocumentPreferences()
    onPreviewKeyChanged: {
        root.preferencesKey = ""
        root.applyDocumentPreferences()
    }
    onLoadingChanged: root.applyDocumentPreferences()
    onDefaultWrapTextChanged: root.applyDocumentPreferences()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.controlsVisible ? 34 : 0
            color: Theme.glassSurfaceSoft
            visible: root.controlsVisible

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                TextControlButton {
                    text: "A-"
                    enabled: root.fontPixelSize > root.minimumFontPixelSize
                    onClicked: root.adjustFontSize(-1)
                    ToolTip.visible: hovered
                    ToolTip.text: "Decrease text size"
                }

                TextControlButton {
                    text: "A+"
                    enabled: root.fontPixelSize < root.maximumFontPixelSize
                    onClicked: root.adjustFontSize(1)
                    ToolTip.visible: hovered
                    ToolTip.text: "Increase text size"
                }

                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/refresh.svg"
                    iconTone: "refresh"
                    iconSize: 15
                    implicitWidth: 28
                    implicitHeight: 28
                    enabled: root.fontPixelSize !== root.defaultFontPixelSize
                    onClicked: root.fontPixelSize = root.defaultFontPixelSize
                    ToolTip.visible: hovered
                    ToolTip.text: "Reset text size"
                }

                Label {
                    text: root.fontPixelSize + " px"
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.8
                    Layout.preferredWidth: 34
                    horizontalAlignment: Text.AlignHCenter
                }

                Rectangle {
                    Layout.preferredWidth: codeLabel.implicitWidth + 18
                    Layout.preferredHeight: 22
                    radius: Theme.radiusSm
                    visible: root.codeMode && root.languageLabel.length > 0
                    color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.18 : 0.12)
                    border.color: Theme.withAlpha(Theme.accent, themeController.isDark ? 0.42 : 0.30)
                    border.width: 1

                    Label {
                        id: codeLabel
                        anchors.centerIn: parent
                        text: root.languageLabel
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                        color: Theme.accent
                        elide: Text.ElideRight
                    }
                }

                Label {
                    text: root.lineCount > 0 ? root.lineCount + " lines" : ""
                    visible: root.codeMode && root.lineCount > 0
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.75
                    Layout.preferredWidth: Math.max(46, implicitWidth)
                    horizontalAlignment: Text.AlignLeft
                }

                Item {
                    Layout.fillWidth: true
                }

                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-left.svg"
                    iconTone: "view"
                    iconSize: 14
                    implicitWidth: 28
                    implicitHeight: 28
                    visible: root.textChunked && root.showPageControls
                    enabled: !root.loading && root.hasPreviousPage
                    onClicked: root.previousTextChunkRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Previous text chunk"
                }

                Label {
                    text: root.hasNextPage
                          ? "Page " + (root.textChunkIndex + 1)
                          : (root.textChunkIndex > 0 ? "Page " + (root.textChunkIndex + 1) : "")
                    visible: root.textChunked && root.showPageControls
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.8
                    Layout.preferredWidth: 54
                    horizontalAlignment: Text.AlignHCenter
                }

                FmIconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-right.svg"
                    iconTone: "view"
                    iconSize: 14
                    implicitWidth: 28
                    implicitHeight: 28
                    visible: root.textChunked && root.showPageControls
                    enabled: !root.loading && root.hasNextPage
                    onClicked: root.nextTextChunkRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Next text chunk"
                }

                TextControlButton {
                    text: "Load"
                    implicitWidth: 44
                    visible: root.allowLoadFullText && root.textTruncated && root.fullTextAvailable && !root.textChunked
                    enabled: !root.loading
                    onClicked: root.loadFullTextRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Load full text or chunked view"
                }

                FmIconButton {
                    iconSource: root.effectiveWrapText
                                ? "qrc:/qt/qml/FM/qml/assets/icons-classic/text-nowrap.svg"
                                : "qrc:/qt/qml/FM/qml/assets/icons-classic/text-wrap.svg"
                    iconTone: "view"
                    iconSize: 15
                    implicitWidth: 28
                    implicitHeight: 28
                    enabled: !root.forcedWrapText
                    isHighlighted: root.effectiveWrapText
                    onClicked: root.wrapText = !root.wrapText
                    ToolTip.visible: hovered
                    ToolTip.text: root.forcedWrapText ? "Large text is wrapped for stability"
                                                       : (root.wrapText ? "Disable text wrap" : "Enable text wrap")
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.panelBorder
                opacity: 0.18
            }
        }

        FmDocumentView {
            id: documentView
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: root.text
            fontFamily: root.fontFamily
            styleRanges: root.styleRanges
            tokenColor1: root.documentTokenColor(root.tokenColor1, "#7c3aed", "#c678dd")
            tokenColor2: root.documentTokenColor(root.tokenColor2, "#18794e", "#98c379")
            tokenColor3: root.documentTokenColor(root.tokenColor3, "#667085", "#9aa0aa")
            tokenColor4: root.documentTokenColor(root.tokenColor4, "#b45309", "#d19a66")
            fontPixelSize: root.fontPixelSize
            wrap: root.effectiveWrapText
            showLineNumbers: root.showLineNumbers && root.visibleLineNumberCount > 0
            firstLine: root.firstLine
            textPadding: root.textPadding
            lineNumberWidth: root.lineNumberWidth
            opacity: root.loading ? 0.35 : 1.0
        }
    }

    component TextControlButton: FmButton {
        id: controlButton

        implicitWidth: implicitContentWidth + 12
        implicitHeight: 28
        Layout.minimumWidth: implicitWidth
        Layout.preferredWidth: implicitWidth
        Layout.maximumWidth: implicitWidth
        padding: 0
        hoverEnabled: true
        flat: true
        primaryColor: Theme.accent

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? Theme.accent : Theme.textSecondary
            opacity: controlButton.enabled ? 1.0 : 0.45
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

    }

    Rectangle {
        anchors.fill: parent
        z: 1
        visible: root.loading
        color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, themeController.isDark ? 0.72 : 0.78)

        Column {
            anchors.centerIn: parent
            spacing: 10
            width: Math.min(parent.width - 24, 220)

            BusyIndicator {
                running: true
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Label {
                text: root.loadingTitle
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            Label {
                text: root.loadingSubtitle
                color: Theme.textSecondary
                opacity: 0.75
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }
        }
    }
}
