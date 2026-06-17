import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
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
    property bool wrapText: false
    property bool showLineNumbers: true
    property bool lineHeightFollowsContent: true
    property int fixedLineHeight: 18
    property int lineNumberWidth: 45
    property int textPadding: 24
    property int maximumLineNumbers: 0
    property int maximumUnwrappedTextLength: 262144
    property bool controlsVisible: true
    property string previewKey: ""
    property int fontPixelSize: 13
    property int defaultFontPixelSize: 13
    property bool defaultWrapText: false
    property int minimumFontPixelSize: 9
    property int maximumFontPixelSize: 24
    property string loadingTitle: "Loading preview..."
    property string loadingSubtitle: "Large files are loaded asynchronously."
    property string fontFamily: "Cascadia Code, Consolas, Monospace"
    property bool codeMode: false
    property string languageLabel: ""
    readonly property bool forcedWrapText: root.text.length > root.maximumUnwrappedTextLength
    readonly property bool effectiveWrapText: root.wrapText || root.forcedWrapText
    readonly property int visibleLineNumberCount: root.maximumLineNumbers > 0
                                             ? Math.min(root.lineCount, root.maximumLineNumbers)
                                             : root.lineCount

    signal loadFullTextRequested()
    signal previousTextChunkRequested()
    signal nextTextChunkRequested()

    clip: true

    function adjustFontSize(delta) {
        fontPixelSize = Math.max(minimumFontPixelSize, Math.min(maximumFontPixelSize, fontPixelSize + delta))
    }

    function resetViewPreferences() {
        fontPixelSize = defaultFontPixelSize
        wrapText = defaultWrapText
    }

    Component.onCompleted: resetViewPreferences()
    onPreviewKeyChanged: resetViewPreferences()

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

                IconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons/refresh.svg"
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

                IconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons/arrow-left.svg"
                    iconTone: "view"
                    iconSize: 14
                    implicitWidth: 28
                    implicitHeight: 28
                    visible: root.textChunked
                    enabled: !root.loading && root.textChunkIndex > 0
                    onClicked: root.previousTextChunkRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Previous text chunk"
                }

                Label {
                    text: (root.textChunkIndex + 1) + " / " + root.textChunkCount
                    visible: root.textChunked
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.8
                    Layout.preferredWidth: 54
                    horizontalAlignment: Text.AlignHCenter
                }

                IconButton {
                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons/arrow-right.svg"
                    iconTone: "view"
                    iconSize: 14
                    implicitWidth: 28
                    implicitHeight: 28
                    visible: root.textChunked
                    enabled: !root.loading && root.textChunkIndex + 1 < root.textChunkCount
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

                IconButton {
                    iconSource: root.effectiveWrapText
                                ? "qrc:/qt/qml/FM/qml/assets/icons/text-nowrap.svg"
                                : "qrc:/qt/qml/FM/qml/assets/icons/text-wrap.svg"
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

        RowLayout {
            id: contentLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                id: lineNumbersSidebar
                Layout.fillHeight: true
                Layout.preferredWidth: root.lineNumberWidth
                color: Theme.glassSurfaceSoft
                opacity: root.codeMode ? 0.95 : 1.0
                visible: root.showLineNumbers && root.visibleLineNumberCount > 0
                clip: true

                readonly property real lineSpacing: root.lineCount > 0
                                                    ? (root.lineHeightFollowsContent
                                                       ? Math.max(root.fixedLineHeight, textPreview.contentHeight / root.lineCount)
                                                       : root.fixedLineHeight)
                                                    : root.fixedLineHeight

                Column {
                    id: lineNumbersColumn
                    x: 0
                    y: root.textPadding - (textScrollView.contentItem ? textScrollView.contentItem.contentY : 0)
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: root.visibleLineNumberCount

                        Label {
                            width: parent.width
                            text: index + 1
                            font.family: root.fontFamily
                            font.pixelSize: Math.max(9, root.fontPixelSize - 2)
                            color: Theme.textSecondary
                            opacity: 0.55
                            horizontalAlignment: Text.AlignHCenter
                            height: lineNumbersSidebar.lineSpacing
                        }
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.panelBorder
                    opacity: 0.2
                }
            }

            ScrollView {
                id: textScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                ScrollBar.horizontal.policy: root.effectiveWrapText ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
                background: null
                clip: true

                TextArea {
                    id: textPreview
                    width: root.effectiveWrapText
                           ? Math.max(1, textScrollView.availableWidth)
                           : Math.max(textScrollView.availableWidth, contentWidth + leftPadding + rightPadding)
                    text: root.text
                    readOnly: true
                    color: Theme.textPrimary
                    font.family: root.fontFamily
                    font.pixelSize: root.fontPixelSize
                    wrapMode: root.effectiveWrapText ? Text.Wrap : Text.NoWrap
                    padding: root.textPadding
                    topPadding: root.textPadding
                    bottomPadding: root.textPadding
                    background: null
                    selectByMouse: true
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.accentText
                    opacity: root.loading ? 0.35 : 1.0
                }
            }
        }
    }

    component TextControlButton: Button {
        id: controlButton

        implicitWidth: 30
        implicitHeight: 28
        padding: 0
        hoverEnabled: true

        contentItem: Label {
            text: controlButton.text
            color: controlButton.enabled ? Theme.accent : Theme.textSecondary
            opacity: controlButton.enabled ? 1.0 : 0.45
            font.pixelSize: Theme.fontSizeMicro
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: controlButton.down
                   ? Theme.surfaceActive
                   : (controlButton.hovered ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.20 : 0.14) : "transparent")
            border.color: controlButton.hovered ? Theme.withAlpha(Theme.accent, themeController.isDark ? 0.48 : 0.34) : "transparent"
            border.width: controlButton.hovered ? 1 : 0
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
