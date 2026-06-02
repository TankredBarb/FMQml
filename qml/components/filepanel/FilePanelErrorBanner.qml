import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window
import "../../style"

Rectangle {
    id: root

    property var errorInfo: ({})
    readonly property string errorCode: errorInfo && errorInfo.code ? String(errorInfo.code) : ""
    readonly property string errorTitle: errorInfo && errorInfo.title ? String(errorInfo.title) : "Operation failed"
    readonly property string errorMessage: errorInfo && errorInfo.message ? String(errorInfo.message) : ""
    readonly property string errorPath: errorInfo && errorInfo.path ? String(errorInfo.path) : ""
    property string displayErrorPath: errorPath
    readonly property var errorActions: errorInfo && errorInfo.actions ? errorInfo.actions : []
    readonly property bool canRetry: root.errorActions.indexOf("retry") >= 0
    readonly property bool canRefresh: root.errorActions.indexOf("refresh") >= 0
    readonly property bool canCopyPath: root.errorActions.indexOf("copyPath") >= 0 && root.errorPath.length > 0
    readonly property bool canRestartAsAdmin: root.errorCode === "accessDenied"
                                              && root.errorActions.indexOf("restartAsAdmin") >= 0
                                              && typeof adminController !== "undefined"
                                              && adminController
                                              && !adminController.isElevated
    readonly property bool hasError: errorCode.length > 0 && errorCode !== "none" && errorMessage.length > 0
    readonly property color solidPanelSurface: root.opaque(Theme.panelSurface)
    readonly property color solidPanelSurfaceStrong: root.opaque(Theme.panelSurfaceStrong)
    readonly property color solidSurfaceActive: root.opaque(Theme.surfaceActive)
    property int autoDismissMs: 5000

    signal retryRequested()
    signal refreshRequested()
    signal copyPathRequested()
    signal adminRequested()
    signal dismissRequested()

    visible: hasError
    implicitHeight: hasError ? Math.max(76, bannerLayout.implicitHeight + 20) : 0
    radius: Theme.radiusSm
    color: root.solidPanelSurfaceStrong
    border.color: Theme.withAlpha(Theme.danger, themeController.isDark ? 0.72 : 0.46)
    border.width: 1

    layer.enabled: visible
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowBlur: 0.34
        shadowVerticalOffset: 6
        shadowOpacity: 0.16
        shadowColor: Theme.shadow
    }

    HoverHandler {
        id: bannerHover
    }

    Timer {
        id: autoDismissTimer
        interval: root.autoDismissMs
        repeat: false
        onTriggered: root.dismissRequested()
    }

    function updateAutoDismissTimer() {
        if (!root.hasError) {
            autoDismissTimer.stop()
            return
        }

        if (bannerHover.hovered) {
            autoDismissTimer.stop()
            return
        }

        autoDismissTimer.restart()
    }

    function opaque(c) {
        return Qt.rgba(c.r, c.g, c.b, 1)
    }

    onHasErrorChanged: updateAutoDismissTimer()
    onErrorCodeChanged: updateAutoDismissTimer()
    onErrorMessageChanged: updateAutoDismissTimer()
    onVisibleChanged: updateAutoDismissTimer()

    RowLayout {
        id: bannerLayout
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 8
            Layout.fillHeight: true
            radius: 4
            color: Theme.danger
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: root.errorTitle
                font.pixelSize: 12
                font.bold: true
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: root.errorMessage
                font.pixelSize: 11
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: root.errorPath.length > 0
                text: root.displayErrorPath
                font.pixelSize: 10
                color: Theme.textSecondary
                opacity: 0.74
                elide: Text.ElideMiddle
            }
        }

        RowLayout {
            spacing: 8

            BannerButton {
                visible: root.canRetry
                text: "Retry"
                onClicked: root.retryRequested()
            }

            BannerButton {
                visible: root.canRefresh
                text: "Refresh"
                onClicked: root.refreshRequested()
            }

            BannerButton {
                visible: root.canRestartAsAdmin
                text: "Run as admin"
                onClicked: root.adminRequested()
            }

            BannerButton {
                visible: root.canCopyPath
                text: "Copy path"
                onClicked: root.copyPathRequested()
            }
        }
    }

    Connections {
        target: bannerHover
        function onHoveredChanged() { root.updateAutoDismissTimer() }
    }

    component BannerButton: Rectangle {
        id: button

        property string text: ""
        signal clicked()

        Layout.preferredWidth: Math.max(74, label.implicitWidth + 20)
        Layout.preferredHeight: 28
        radius: Theme.radiusSm
        color: buttonMouse.containsMouse && button.enabled
               ? root.solidSurfaceActive
               : root.solidPanelSurface
        border.color: button.enabled ? Theme.withAlpha(Theme.danger, 0.34) : Theme.panelBorder
        border.width: 1
        opacity: button.enabled ? 1.0 : 0.45

        Label {
            id: label
            anchors.centerIn: parent
            text: button.text
            font.pixelSize: 11
            font.bold: true
            color: button.enabled ? Theme.textPrimary : Theme.textSecondary
        }

        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            enabled: button.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }
}
