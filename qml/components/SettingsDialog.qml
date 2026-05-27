import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "dialogs"

Dialog {
    id: root

    title: "Settings"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 48 : 680, 680)
    height: Math.min(parent ? parent.height - 48 : 560, 560)
    padding: 0

    property var appRoot: null
    property bool workspaceResetPending: false
    property bool splitViewEnabled: false
    property bool previewPaneEnabled: false
    property bool hiddenFilesEnabled: false
    property bool nativeIconsEnabled: true
    property bool highQualitySystemIconsEnabled: true
    property bool thumbnailsEnabled: true
    property bool simplifyVisualsForPerformanceEnabled: true
    readonly property color dialogAccent: Theme.accent
    readonly property color sectionFill: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.72 : 0.88)
    readonly property color sectionBorder: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.92 : 0.78)
    readonly property color rowFill: Theme.withAlpha(Theme.panelSurface, themeController.isDark ? 0.78 : 0.92)
    readonly property color rowFillHover: Theme.withAlpha(Theme.surfaceHover, themeController.isDark ? 0.70 : 0.82)

    onOpened: {
        workspaceResetPending = false
        refreshState()
        Qt.callLater(() => contentItem.forceActiveFocus())
    }

    function workspace() {
        return typeof workspaceController !== "undefined" ? workspaceController : null
    }

    function refreshState() {
        const workspaceCtrl = workspace()
        splitViewEnabled = workspaceCtrl ? workspaceCtrl.splitEnabled : false
        previewPaneEnabled = root.appRoot ? root.appRoot.previewPaneVisible : false
        hiddenFilesEnabled = workspaceCtrl && workspaceCtrl.leftPanel
                             ? workspaceCtrl.leftPanel.directoryModel.showHidden
                             : false
        nativeIconsEnabled = typeof appSettings !== "undefined" && appSettings
                             ? appSettings.useNativeIcons
                             : true
        highQualitySystemIconsEnabled = typeof appSettings !== "undefined" && appSettings
                                        ? appSettings.useHighQualitySystemIcons
                                        : true
        thumbnailsEnabled = typeof appSettings !== "undefined" && appSettings
                            ? appSettings.showThumbnails
                            : true
        simplifyVisualsForPerformanceEnabled = typeof appSettings !== "undefined" && appSettings
                                              ? appSettings.simplifyVisualsForPerformance
                                              : true
    }

    function setSplitViewEnabled(enabled) {
        const workspaceCtrl = workspace()
        splitViewEnabled = enabled
        if (workspaceCtrl && workspaceCtrl.splitEnabled !== enabled) {
            workspaceCtrl.splitEnabled = enabled
        }
    }

    function setPreviewPaneEnabled(enabled) {
        previewPaneEnabled = enabled
        if (root.appRoot && root.appRoot.previewPaneVisible !== enabled) {
            root.appRoot.setPreviewPaneVisible(enabled)
        }
    }

    function setHiddenFilesEnabled(enabled) {
        const workspaceCtrl = workspace()
        hiddenFilesEnabled = enabled
        if (!workspaceCtrl) {
            return
        }
        workspaceCtrl.leftPanel.directoryModel.showHidden = enabled
        workspaceCtrl.rightPanel.directoryModel.showHidden = enabled
        workspaceCtrl.treeModel.showHidden = enabled
    }

    function setNativeIconsEnabled(enabled) {
        nativeIconsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings && appSettings.useNativeIcons !== enabled) {
            appSettings.useNativeIcons = enabled
        }
    }

    function setThumbnailsEnabled(enabled) {
        thumbnailsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings && appSettings.showThumbnails !== enabled) {
            appSettings.showThumbnails = enabled
        }
    }

    function setHighQualitySystemIconsEnabled(enabled) {
        highQualitySystemIconsEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.useHighQualitySystemIcons !== enabled) {
            appSettings.useHighQualitySystemIcons = enabled
        }
    }

    function setSimplifyVisualsForPerformanceEnabled(enabled) {
        simplifyVisualsForPerformanceEnabled = enabled
        if (typeof appSettings !== "undefined" && appSettings
                && appSettings.simplifyVisualsForPerformance !== enabled) {
            appSettings.simplifyVisualsForPerformance = enabled
        }
    }

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.panelBorder
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/settings.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: root.title
        subtitle: "Workspace, preview, and persistence"
        closeText: "x"
        onCloseRequested: root.accept()
    }

    footer: DialogFooter {
        Item {
            Layout.fillWidth: true
        }

        DialogActionButton {
            text: "Close"
            highlighted: true
            primaryColor: root.dialogAccent
            primaryHoverColor: Qt.lighter(root.dialogAccent, 1.1)
            primaryPressedColor: Qt.darker(root.dialogAccent, 1.1)
            onClicked: root.accept()
        }
    }

    contentItem: ColumnLayout {
        implicitWidth: root.width
        implicitHeight: root.height - (root.header ? root.header.height : 0) - (root.footer ? root.footer.height : 0)
        spacing: 0
        clip: true
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.accept()
                event.accepted = true
            }
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Pane {
                width: scrollView.availableWidth
                padding: 20
                background: null

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    DialogSection {
                        title: "WORKSPACE"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder

                        SettingsToggleRow {
                            title: "Split view"
                            subtitle: "Show the second file panel"
                            checked: root.splitViewEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setSplitViewEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Preview pane"
                            subtitle: "Keep the right preview pane visible"
                            checked: root.previewPaneEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setPreviewPaneEnabled(checked)
                        }

                    }

                    DialogSection {
                        title: "FILES"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder

                        SettingsToggleRow {
                            title: "Hidden files"
                            subtitle: "Show hidden entries in panels and folder tree"
                            checked: root.hiddenFilesEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setHiddenFilesEnabled(checked)
                        }
                    }

                    DialogSection {
                        title: "PERFORMANCE"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder

                        SettingsToggleRow {
                            title: "Native icons"
                            subtitle: "Use Windows Shell icons instead of bundled file type icons"
                            checked: root.nativeIconsEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setNativeIconsEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Use high quality system icons"
                            subtitle: "Request larger Windows Shell icons for big icon views to avoid scaling artifacts"
                            checked: root.highQualitySystemIconsEnabled
                            toggleEnabled: root.nativeIconsEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setHighQualitySystemIconsEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Thumbnails"
                            subtitle: "Show generated previews in Grid and Brief views"
                            checked: root.thumbnailsEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setThumbnailsEnabled(checked)
                        }

                        SettingsToggleRow {
                            title: "Simplify visuals for performance"
                            subtitle: "Use lighter preview and reduced visual effects during live resize"
                            checked: root.simplifyVisualsForPerformanceEnabled
                            accentColor: root.dialogAccent
                            onToggled: (checked) => root.setSimplifyVisualsForPerformanceEnabled(checked)
                        }
                    }

                    DialogSection {
                        title: "SESSION"
                        accentColor: root.dialogAccent
                        fillColor: root.sectionFill
                        borderColor: root.sectionBorder

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: "FM saves window geometry, current folders, split layout, view modes, sorting, hidden files, and preview state."
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: 12
                                color: Theme.textSecondary
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: "Reset saved workspace"
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    Label {
                                        text: root.workspaceResetPending
                                              ? "Workspace state has been reset. Current runtime state will continue until the app is restarted."
                                              : "Clear saved layout and folder state for the next launch."
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: 11
                                        color: root.workspaceResetPending ? Theme.success : Theme.textSecondary
                                    }
                                }

                                DialogActionButton {
                                    text: "Reset"
                                    highlighted: false
                                    enabled: !root.workspaceResetPending
                                    secondaryTextColor: root.workspaceResetPending ? Theme.textSecondary : root.dialogAccent
                                    onClicked: {
                                        if (root.appRoot) {
                                            root.appRoot.resetSavedWorkspaceState()
                                            root.workspaceResetPending = true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: root.workspace()
        function onSplitEnabledChanged() {
            root.splitViewEnabled = root.workspace() ? root.workspace().splitEnabled : false
        }
    }

    Connections {
        target: root.appRoot
        function onPreviewPaneVisibleChanged() {
            root.previewPaneEnabled = root.appRoot ? root.appRoot.previewPaneVisible : false
        }
    }

    Connections {
        target: root.workspace() && root.workspace().leftPanel
                ? root.workspace().leftPanel.directoryModel
                : null
        function onShowHiddenChanged() {
            root.hiddenFilesEnabled = root.workspace() && root.workspace().leftPanel
                                      ? root.workspace().leftPanel.directoryModel.showHidden
                                      : false
        }
    }

    Connections {
        target: typeof appSettings !== "undefined" ? appSettings : null
        function onUseNativeIconsChanged() {
            root.nativeIconsEnabled = appSettings ? appSettings.useNativeIcons : true
        }
        function onUseHighQualitySystemIconsChanged() {
            root.highQualitySystemIconsEnabled = appSettings ? appSettings.useHighQualitySystemIcons : true
        }
        function onShowThumbnailsChanged() {
            root.thumbnailsEnabled = appSettings ? appSettings.showThumbnails : true
        }
        function onSimplifyVisualsForPerformanceChanged() {
            root.simplifyVisualsForPerformanceEnabled = appSettings ? appSettings.simplifyVisualsForPerformance : true
        }
    }

    component SettingsToggleRow: Rectangle {
        id: row

        property string title: ""
        property string subtitle: ""
        property bool checked: false
        property bool toggleEnabled: true
        property color accentColor: Theme.accent
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: Math.max(52, rowLayout.implicitHeight + 12)
        radius: Theme.radiusSm
        color: rowMouse.containsMouse ? root.rowFillHover : root.rowFill
        border.color: row.checked
                      ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.42 : 0.34)
                      : Theme.panelBorder
        border.width: 1
        opacity: row.toggleEnabled ? 1.0 : 0.55

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: row.title
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    text: row.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
            }

            Switch {
                id: switchControl
                checked: row.checked
                enabled: row.toggleEnabled
                Layout.preferredWidth: 46
                Layout.preferredHeight: 26

                indicator: Rectangle {
                    implicitWidth: 42
                    implicitHeight: 22
                    x: switchControl.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: height / 2
                    color: switchControl.checked
                           ? Theme.withAlpha(row.accentColor, themeController.isDark ? 0.50 : 0.36)
                           : Theme.panelSurfaceSoft
                    border.color: switchControl.checked ? row.accentColor : Theme.panelBorder
                    border.width: 1

                    Rectangle {
                        x: switchControl.checked ? parent.width - width - 3 : 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        radius: 8
                        color: switchControl.checked ? row.accentColor : Theme.textSecondary

                        Behavior on x {
                            NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
                        }
                    }
                }

                contentItem: Item {}
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            enabled: row.toggleEnabled
            cursorShape: row.toggleEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: row.toggled(!row.checked)
        }
    }
}
