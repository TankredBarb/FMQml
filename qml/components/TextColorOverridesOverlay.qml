import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"
import "framework"

Popup {
    id: root

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent ? parent.width - 32 : 1040, 1040)
    height: Math.min(parent ? parent.height - 32 : 760, 760)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property var appRoot: null
    property var stagedOverrides: ({})
    property string activeEditRoleId: ""
    property color dialogAccent: Theme.accent
    property int currentGroupIndex: 0
    readonly property var roleGroups: ["File panels", "Navigation", "App chrome and workflows"]
    readonly property string activeGroup: roleGroups[currentGroupIndex]

    function rolesMetadata() {
        return typeof appSettings !== "undefined" && appSettings ? appSettings.rolesMetadata() : []
    }

    function roleFallback(role) {
        return role.fallbackToken === "textPrimary" ? Theme.textPrimary : Theme.textSecondary
    }

    function roleEnabled(roleId) {
        return !!(stagedOverrides && stagedOverrides[roleId] && stagedOverrides[roleId].enabled)
    }

    function resetStagedMap() {
        var rawMap = typeof appSettings !== "undefined" && appSettings ? appSettings.textColorOverrides : ({})
        var clone = {}
        var roles = rolesMetadata()
        for (var i = 0; i < roles.length; ++i) {
            var roleId = roles[i].id
            var source = rawMap && rawMap[roleId] ? rawMap[roleId] : ({})
            clone[roleId] = { enabled: !!source.enabled, color: source.color || "" }
        }
        if (rawMap) {
            for (var key in rawMap) {
                if (!clone[key])
                    clone[key] = rawMap[key]
            }
        }
        stagedOverrides = clone
    }

    function saveStagedMap() {
        if (typeof appSettings !== "undefined" && appSettings)
            appSettings.saveTextColorOverrides(stagedOverrides)
    }

    function resolvePreviewColor(roleId, fallbackColor) {
        var entry = stagedOverrides ? stagedOverrides[roleId] : null
        return entry && entry.enabled && entry.color ? entry.color : fallbackColor
    }

    function updateRoleEnabled(role, enabled) {
        var entry = stagedOverrides[role.id] || ({ enabled: false, color: "" })
        entry.enabled = enabled
        if (enabled && !entry.color)
            entry.color = roleFallback(role).toString()
        stagedOverrides[role.id] = entry
        stagedOverrides = Object.assign({}, stagedOverrides)
    }

    function updateRoleColor(roleId, colorHex) {
        var entry = stagedOverrides[roleId] || ({ enabled: false, color: "" })
        entry.color = colorHex
        entry.enabled = true
        stagedOverrides[roleId] = entry
        stagedOverrides = Object.assign({}, stagedOverrides)
    }

    function resetRoleToDefault(roleId) {
        var entry = stagedOverrides[roleId] || ({ enabled: false, color: "" })
        entry.enabled = false
        stagedOverrides[roleId] = entry
        stagedOverrides = Object.assign({}, stagedOverrides)
    }

    function resetAllRoles() {
        var roles = rolesMetadata()
        for (var i = 0; i < roles.length; ++i)
            resetRoleToDefault(roles[i].id)
    }

    function openPicker(role) {
        activeEditRoleId = role.id
        colorPicker.selectedColor = resolvePreviewColor(role.id, roleFallback(role))
        colorPicker.open()
    }

    function contrastWarning(role) {
        var textColor = Qt.color(resolvePreviewColor(role.id, roleFallback(role)))
        var normalBackground = role.id === "sidebarText" ? Theme.surface : Theme.panelSurface
        if (role.id === "thisPcText")
            normalBackground = Theme.panelSurfaceSoft
        var normalRatio = Theme.contrastRatio(normalBackground, textColor)
        var selectedRatio = Theme.contrastRatio(Theme.itemSelectedFill, textColor)
        if (normalRatio < 4.5 && selectedRatio < 4.5)
            return "Low contrast on normal and selected backgrounds"
        if (normalRatio < 4.5)
            return "Low contrast on the normal background"
        if (selectedRatio < 4.5)
            return "Low contrast on selected rows"
        return ""
    }

    onOpened: resetStagedMap()

    background: DialogShell {
        accentColor: root.dialogAccent
        shellColor: Theme.panelSurface
        shellBorderColor: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.30)
        shadowBlur: 16
        shadowVerticalOffset: 5
    }

    ColorDialog {
        id: colorPicker
        title: "Choose text color"
        onAccepted: {
            if (root.activeEditRoleId.length > 0)
                root.updateRoleColor(root.activeEditRoleId, selectedColor.toString())
        }
    }

    DialogHeader {
        id: dialogHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/theme.svg"
        iconTint: root.dialogAccent
        accentColor: root.dialogAccent
        title: "Text Colors"
        subtitle: "Override only the text roles you want to change. Everything else follows the theme."
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
            spacing: 10

            DialogActionButton {
                text: "Reset All"
                highlighted: false
                secondaryTextColor: Theme.danger
                onClicked: root.resetAllRoles()
            }
            Item { Layout.fillWidth: true }
            DialogActionButton {
                text: "Cancel"
                highlighted: false
                secondaryTextColor: Theme.textSecondary
                onClicked: root.close()
            }
            DialogActionButton {
                text: "Apply"
                highlighted: false
                secondaryTextColor: root.dialogAccent
                onClicked: root.saveStagedMap()
            }
            DialogActionButton {
                text: "Done"
                highlighted: true
                primaryColor: root.dialogAccent
                onClicked: {
                    root.saveStagedMap()
                    root.close()
                }
            }
        }
    }

    FmTabBar {
        id: tabContainer
        anchors.top: dialogHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 14
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 40
        accentColor: root.dialogAccent
        currentIndex: root.currentGroupIndex
        model: [{ text: "File Panels", value: 0 },
                { text: "Navigation", value: 1 },
                { text: "App Chrome & Workflows", value: 2 }]
        onActivated: (index, value) => root.currentGroupIndex = value
    }

    RowLayout {
        id: editorLayout
        anchors.top: tabContainer.bottom
        anchors.bottom: dialogFooter.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 12
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 470
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
                        implicitWidth: previewBadge.implicitWidth + 16
                        implicitHeight: 24
                        radius: 12
                        color: Theme.withAlpha(root.dialogAccent, 0.16)
                        border.color: Theme.withAlpha(root.dialogAccent, 0.42)
                        Label {
                            id: previewBadge
                            anchors.centerIn: parent
                            text: root.activeGroup
                            color: root.dialogAccent
                            font.pixelSize: Theme.fontSizeCaption
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Every enabled override is shown together in a simplified application scene."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    id: previewScene
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 430
                    radius: Theme.radiusMd
                    color: themeController.isDark ? "#18212c" : "#e8edf3"
                    border.color: Theme.withAlpha(Theme.panelBorder, 0.48)
                    clip: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 94
                        color: themeController.isDark ? "#111922" : "#d9e1ea"
                    }

                    Column {
                        x: 14
                        y: 22
                        width: 68
                        spacing: 15
                        Repeater {
                            model: ["Home", "Files", "Cloud", "Recent"]
                            delegate: Row {
                                required property int index
                                required property string modelData
                                spacing: 6
                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 3
                                    color: index === root.currentGroupIndex
                                           ? root.dialogAccent : Theme.withAlpha(Theme.textSecondary, 0.38)
                                }
                                Label {
                                    text: modelData
                                    color: index === root.currentGroupIndex ? Theme.textPrimary : Theme.textSecondary
                                    font.pixelSize: 9
                                }
                            }
                        }
                    }

                    Rectangle {
                        x: 112
                        y: 22
                        width: parent.width - 134
                        height: 38
                        radius: 9
                        color: Theme.withAlpha(Theme.panelSurfaceStrong, 0.78)
                        border.color: Theme.withAlpha(root.dialogAccent, 0.32)
                        Label {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            text: root.activeGroup
                            color: Theme.textPrimary
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }

                    ColumnLayout {
                        x: 112
                        y: 76
                        width: parent.width - 134
                        height: parent.height - 98
                        spacing: 9

                        Repeater {
                            model: root.rolesMetadata()
                            delegate: Rectangle {
                                id: previewRole
                                required property var modelData
                                readonly property bool belongsHere: modelData.group === root.activeGroup
                                readonly property color effectiveColor: root.resolvePreviewColor(modelData.id, root.roleFallback(modelData))
                                Layout.fillWidth: true
                                Layout.fillHeight: belongsHere
                                visible: belongsHere
                                implicitHeight: belongsHere ? 58 : 0
                                height: visible ? implicitHeight : 0
                                radius: 9
                                color: Theme.withAlpha(Theme.panelSurface, 0.78)
                                border.color: root.roleEnabled(modelData.id)
                                              ? Theme.withAlpha(root.dialogAccent, 0.58)
                                              : Theme.withAlpha(Theme.panelBorder, 0.48)

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 11
                                    spacing: 10
                                    Rectangle {
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 28
                                        radius: 7
                                        color: Theme.withAlpha(root.dialogAccent, 0.22)
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 12
                                            height: 10
                                            radius: 3
                                            color: Theme.withAlpha(root.dialogAccent, 0.68)
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Label {
                                            Layout.fillWidth: true
                                            text: previewRole.modelData.sample
                                            color: previewRole.effectiveColor
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeBody
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: previewRole.modelData.name
                                            color: Theme.withAlpha(previewRole.effectiveColor, 0.72)
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeMicro
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Label {
                                        text: root.roleEnabled(previewRole.modelData.id) ? "CUSTOM" : "THEME"
                                        color: root.roleEnabled(previewRole.modelData.id)
                                               ? root.dialogAccent : Theme.textSecondary
                                        font.pixelSize: 8
                                        font.weight: Font.DemiBold
                                    }
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
            Layout.minimumWidth: 350
            radius: Theme.radiusMd
            color: Theme.withAlpha(Theme.panelSurfaceStrong, themeController.isDark ? 0.36 : 0.64)
            border.color: Theme.withAlpha(Theme.panelBorder, 0.48)

            ScrollView {
        id: rolesView
                anchors.fill: parent
                anchors.margins: 10
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: FmScrollBar {
            id: rolesScrollBar
            parent: rolesView.contentItem
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            width: rolesScrollBar.scrollNeeded ? Math.max(0, rolesScrollBar.x - 8) : rolesView.availableWidth
            spacing: 8

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: 4
                        text: "Text roles"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLabel
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        Layout.bottomMargin: 4
                        text: "Enable only the roles that should stop following the theme."
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                    }

            Repeater {
                model: root.rolesMetadata()
                delegate: Rectangle {
                            id: roleRow
                            required property var modelData
                            readonly property bool belongsHere: modelData.group === root.activeGroup
                            readonly property color effectiveColor: root.resolvePreviewColor(modelData.id, root.roleFallback(modelData))
                            readonly property string warningText: root.roleEnabled(modelData.id) ? root.contrastWarning(modelData) : ""

                            Layout.fillWidth: true
                            visible: belongsHere
                            implicitHeight: belongsHere ? controlColumn.implicitHeight + 22 : 0
                            height: visible ? implicitHeight : 0
                            radius: Theme.radiusSm
                            color: Theme.withAlpha(Theme.panelSurfaceSoft,
                                                   root.roleEnabled(modelData.id) ? 0.82 : 0.58)
                            border.color: root.roleEnabled(modelData.id)
                                          ? Theme.withAlpha(root.dialogAccent, 0.46)
                                          : Theme.withAlpha(Theme.panelBorder, 0.34)

                            ColumnLayout {
                                id: controlColumn
                                anchors.fill: parent
                                anchors.margins: 11
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: roleRow.modelData.name
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLabel
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: roleRow.modelData.description
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                    }
                                    FmSwitch {
                                        text: ""
                                        checked: root.roleEnabled(roleRow.modelData.id)
                                        onToggled: root.updateRoleEnabled(roleRow.modelData, checked)
                                    }
                                }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        FmButton {
                                            id: colorButton
                                            Layout.fillWidth: true
                                            highlighted: false
                                            text: root.roleEnabled(roleRow.modelData.id)
                                                  ? roleRow.effectiveColor.toString().toUpperCase()
                                                  : "Choose custom color"
                                            secondaryTextColor: Theme.textPrimary
                                            contentItem: RowLayout {
                                                spacing: 7
                                                Rectangle {
                                                    Layout.preferredWidth: 22
                                                    Layout.preferredHeight: 16
                                                    radius: Theme.radiusXs
                                                    color: roleRow.effectiveColor
                                                    border.color: Theme.withAlpha(Theme.readableOn(color, Theme.textPrimary), 0.62)
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: colorButton.text
                                                    color: Theme.textPrimary
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: Theme.fontSizeMicro
                                                    horizontalAlignment: Text.AlignHCenter
                                                    elide: Text.ElideRight
                                                }
                                            }
                                            onClicked: root.openPicker(roleRow.modelData)
                                        }
                                        FmIconButton {
                                            visible: root.roleEnabled(roleRow.modelData.id)
                                            Layout.preferredWidth: 32
                                            Layout.preferredHeight: 32
                                            iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/refresh.svg"
                                            iconSize: 15
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Use theme default"
                                            onClicked: root.resetRoleToDefault(roleRow.modelData.id)
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: roleRow.warningText.length > 0
                                        text: roleRow.warningText
                                        color: Theme.warning
                                        font.pixelSize: Theme.fontSizeMicro
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
            }
        }
            }
        }
    }
}
