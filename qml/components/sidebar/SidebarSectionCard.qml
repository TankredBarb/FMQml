import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../common"
import "../framework"
import "../../style"

SurfaceCard {
    id: root

    default property alias contentData: contentHost.data
    property string title: ""
    property string iconSource: ""
    property color iconColor: Theme.actionIconColor("navigation")
    property bool collapsed: false
    property bool compact: false

    signal collapseRequested(bool collapsed)

    readonly property int headerHeight: 37

    clipped: false
    cornerRadius: Theme.radiusLg
    surfaceColor: Theme.opaque(Theme.mixColors(Theme.panelSurface,
                                               Theme.panelSurfaceStrong,
                                               themeController.isDark ? 0.82 : 0.68))
    strokeColor: themeController.isDark
                 ? Theme.withAlpha(Theme.activeAccent, 0.24)
                 : Theme.withAlpha(Theme.panelBorder, 0.42)

    PanelHeaderBackground {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        height: root.collapsed ? root.height - 2 : root.headerHeight - 1
        cornerRadius: Theme.innerRadius(root.cornerRadius, 1)
        roundBottomCorners: root.collapsed
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.compact ? 0 : 14
        anchors.rightMargin: root.compact ? 0 : 14
        height: root.headerHeight - 1
        spacing: 9

        RecolorSvgIcon {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.leftMargin: root.compact ? Math.max(0, (root.width - 16) / 2) : 0
            sourcePath: root.iconSource
            recolorColor: root.iconColor
            sourceSize: Qt.size(32, 32)
            cacheKey: "sidebar-section-header"
            asynchronous: true
            cache: true
        }

        Label {
            Layout.fillWidth: true
            visible: !root.compact
            text: root.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textPrimary
            opacity: 0.82
            elide: Text.ElideRight
        }

        FmIconButton {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            visible: !root.compact
            iconSource: root.collapsed
                        ? "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-down.svg"
                        : "qrc:/qt/qml/FM/qml/assets/icons-classic/arrow-up.svg"
            iconSize: 14
            Accessible.name: root.collapsed
                             ? "Expand " + root.title
                             : "Collapse " + root.title
            ToolTip.text: Accessible.name
            ToolTip.visible: hovered
            onClicked: root.collapseRequested(!root.collapsed)
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.headerHeight
        z: 10
        visible: root.compact
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        Accessible.name: "Expand " + root.title
        ToolTip.visible: containsMouse
        ToolTip.text: root.title
        onClicked: root.collapseRequested(false)
    }

    Item {
        id: contentHost
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.headerHeight
        visible: !root.collapsed
    }
}
