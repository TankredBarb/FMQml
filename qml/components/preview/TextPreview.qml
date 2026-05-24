import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"

Item {
    id: root

    property string text: ""
    property int lineCount: 0
    property bool loading: false
    property bool wrapText: false
    property bool showLineNumbers: true
    property bool lineHeightFollowsContent: true
    property int fixedLineHeight: 18
    property int lineNumberWidth: 45
    property int textPadding: 24
    property int maximumLineNumbers: 0
    property string loadingTitle: "Loading preview..."
    property string loadingSubtitle: "Large files are loaded asynchronously."
    property string fontFamily: "Cascadia Code, Consolas, Monospace"

    clip: true

    RowLayout {
        id: contentLayout
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: lineNumbersSidebar
            Layout.fillHeight: true
            Layout.preferredWidth: root.lineNumberWidth
            color: Theme.glassSurfaceSoft
            visible: root.showLineNumbers && root.lineCount > 0
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
                    model: root.maximumLineNumbers > 0
                           ? Math.min(root.lineCount, root.maximumLineNumbers)
                           : root.lineCount

                    Label {
                        width: parent.width
                        text: index + 1
                        font.family: root.fontFamily
                        font.pixelSize: 11
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
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            background: null
            clip: true

            TextArea {
                id: textPreview
                text: root.text
                readOnly: true
                color: Theme.textPrimary
                font.family: root.fontFamily
                font.pixelSize: 13
                wrapMode: root.wrapText ? Text.Wrap : Text.NoWrap
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
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            Label {
                text: root.loadingSubtitle
                color: Theme.textSecondary
                opacity: 0.75
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }
        }
    }
}
