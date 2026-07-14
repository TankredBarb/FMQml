import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "../style"
import "dialogs"
import "common"

Popup {
    id: root

    property var controller: null
    property string targetPath: ""
    property var targetPaths: []
    property var candidates: []
    property int selectedIndex: -1
    property bool otherSelected: false
    property bool alwaysUseInFm: false

    signal steamProtonRequested(var targetController, string path)

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.69, 420)
    height: Math.min(parent.height * 0.63, 458)
    padding: 15
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(targetController, paths) {
        root.controller = targetController
        root.targetPaths = paths ? Array.from(paths) : []
        root.targetPath = root.targetPaths.length > 0 ? root.targetPaths[0] : ""
        root.candidates = root.controller && root.controller.openWithCandidatesForPaths
                ? root.controller.openWithCandidatesForPaths(root.targetPaths) : []
        root.alwaysUseInFm = false
        root.otherSelected = false
        root.selectedIndex = -1
        for (let index = 0; index < root.candidates.length; ++index) {
            const candidate = root.candidates[index]
            if (candidate && candidate.available === true
                    && (candidate.fmDefault === true || candidate.systemDefault === true)) {
                root.selectedIndex = index
                break
            }
        }
        if (root.selectedIndex < 0) {
            for (let index = 0; index < root.candidates.length; ++index) {
                if (root.candidates[index] && root.candidates[index].available === true) {
                    root.selectedIndex = index
                    break
                }
            }
        }
        root.open()
    }

    function selectedCandidate() {
        return root.selectedIndex >= 0 && root.selectedIndex < root.candidates.length
                ? root.candidates[root.selectedIndex] : null
    }

    function launchSelected() {
        if (!root.controller) return
        if (root.otherSelected) {
            root.controller.openPathWithSystemApplicationChooser(root.targetPath)
            root.close()
            return
        }
        const candidate = root.selectedCandidate()
        if (!candidate || candidate.available !== true
                || (root.targetPaths.length > 1 && candidate.supportsMultipleFiles !== true)) return
        if (candidate.kind === "proton") {
            root.steamProtonRequested(root.controller, root.targetPath)
            root.close()
            return
        }
        if (root.alwaysUseInFm && root.controller.setOpenWithPreferredCandidate) {
            root.controller.setOpenWithPreferredCandidate(root.targetPath, candidate.id)
        }
        root.controller.openPathsWithApplication(root.targetPaths, candidate.id)
        root.close()
    }

    function useSystemDefault() {
        if (!root.controller || !root.controller.clearOpenWithPreferredCandidate) return
        root.controller.clearOpenWithPreferredCandidate(root.targetPath)
        root.candidates = root.controller.openWithCandidatesForPaths(root.targetPaths)
        for (let index = 0; index < root.candidates.length; ++index) {
            const candidate = root.candidates[index]
            if (candidate && candidate.available === true && candidate.systemDefault === true
                    && (root.targetPaths.length <= 1 || candidate.supportsMultipleFiles === true)) {
                root.controller.openPathsWithApplication(root.targetPaths, candidate.id)
                root.close()
                return
            }
        }
    }

    background: DialogShell {
        accentColor: Theme.categoryAction
        shellBorderColor: Theme.withAlpha(Theme.categoryAction, themeController.isDark ? 0.30 : 0.22)
    }

    contentItem: ColumnLayout {
        spacing: 10

        DialogHeader {
            Layout.fillWidth: true
            iconSource: "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
            iconTint: Theme.categoryAction
            accentColor: Theme.categoryAction
            title: "Open With"
            subtitle: root.targetPaths.length > 1 ? root.targetPaths.length + " selected files" : root.targetPath
            showCloseButton: true
            onCloseRequested: root.close()
        }

        Label {
            Layout.fillWidth: true
            visible: root.candidates.length === 0
            text: "No compatible applications were found for this file type."
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }

        ListView {
            id: candidateList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.candidates
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            delegate: Item {
                required property int index
                required property var modelData
                width: candidateList.width
                height: Math.max(44, candidateRow.implicitHeight + 12)

                Rectangle {
                    anchors.fill: parent
                    color: rowMouse.containsMouse
                           ? Theme.withAlpha(Theme.focusRing, themeController.isDark ? 0.34 : 0.22)
                           : "transparent"
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    radius: width / 2
                    visible: root.selectedIndex === index
                    color: Theme.categoryAction
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.withAlpha(Theme.panelBorder, themeController.isDark ? 0.42 : 0.3)
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    enabled: modelData.available === true && (root.targetPaths.length <= 1 || modelData.supportsMultipleFiles === true)
                    hoverEnabled: enabled
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        root.otherSelected = false
                        root.selectedIndex = index
                    }
                }

                RowLayout {
                    id: candidateRow
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6
                    spacing: 8

                    Image {
                        Layout.preferredWidth: 23
                        Layout.preferredHeight: 23
                        Layout.alignment: Qt.AlignTop
                        source: modelData.iconName ? "image://icon/theme/" + encodeURIComponent(modelData.iconName)
                                                   : "qrc:/qt/qml/FM/qml/assets/icons/open.svg"
                        sourceSize: Qt.size(48, 48)
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: modelData.available === true ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Math.round(Theme.fontSizeBody * 0.75)
                            font.weight: root.selectedIndex === index ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.fmDefault === true || modelData.systemDefault === true
                            text: modelData.fmDefault === true ? "FM default" : "System default"
                            color: Theme.categoryAction
                            font.pixelSize: Math.round(Theme.fontSizeCaption * 0.75)
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.available !== true || (root.targetPaths.length > 1 && modelData.supportsMultipleFiles !== true)
                            text: modelData.available !== true ? (modelData.unavailableReason || "Unavailable") : "Does not support multiple files"
                            color: Theme.warning
                            font.pixelSize: Math.round(Theme.fontSizeCaption * 0.75)
                            wrapMode: Text.Wrap
                            Layout.maximumWidth: candidateList.width - 76
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 44
            visible: root.targetPaths.length === 1

            Rectangle {
                anchors.fill: parent
                color: otherMouse.containsMouse || root.otherSelected
                       ? Theme.withAlpha(Theme.focusRing, themeController.isDark ? 0.34 : 0.22)
                       : "transparent"
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                radius: width / 2
                visible: root.otherSelected
                color: Theme.categoryAction
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                spacing: 8

                Image {
                    Layout.preferredWidth: 23
                    Layout.preferredHeight: 23
                    source: "image://icon/theme/system-run"
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    Layout.fillWidth: true
                    text: "Other…"
                    color: Theme.textPrimary
                    font.pixelSize: Math.round(Theme.fontSizeBody * 0.75)
                }
            }

            MouseArea {
                id: otherMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.alwaysUseInFm = false
                    root.selectedIndex = -1
                    root.otherSelected = true
                }
                onDoubleClicked: {
                    root.alwaysUseInFm = false
                    root.selectedIndex = -1
                    root.otherSelected = true
                    root.controller.openPathWithSystemApplicationChooser(root.targetPath)
                    root.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.surface, themeController.isDark ? 0.32 : 0.55)
            border.width: 1
            border.color: Theme.withAlpha(Theme.panelBorder, 0.55)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8

                Item {
                    id: alwaysUseCheck
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    readonly property bool enabledForSelection: root.selectedCandidate() !== null
                                                             && root.selectedCandidate().available === true
                                                             && (root.targetPaths.length <= 1 || root.selectedCandidate().supportsMultipleFiles === true)

                    Rectangle {
                        id: alwaysUseIndicator
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                        height: 14
                        radius: Theme.radiusSm
                        color: root.alwaysUseInFm ? Theme.categoryAction : "transparent"
                        border.color: root.alwaysUseInFm ? Theme.categoryAction : Theme.panelBorder
                        border.width: root.alwaysUseInFm ? 0 : 1

                        Image {
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            source: "qrc:/qt/qml/FM/qml/assets/icons/select-all.svg"
                            visible: root.alwaysUseInFm
                            layer.enabled: true
                            layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "white" }
                        }
                    }

                    Label {
                        anchors.left: alwaysUseIndicator.right
                        anchors.leftMargin: 7
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Always use in FM"
                        color: alwaysUseCheck.enabledForSelection ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Math.round(Theme.fontSizeLabel * 0.75)
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: alwaysUseCheck.enabledForSelection
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: root.alwaysUseInFm = !root.alwaysUseInFm
                    }
                }

                DialogActionButton {
                    text: "System default"
                    highlighted: false
                    secondaryTextColor: Theme.categoryAction
                    enabled: root.candidates.some(candidate => candidate && candidate.available === true && candidate.systemDefault === true)
                    onClicked: root.useSystemDefault()
                }
            }
        }

        DialogFooter {
            Layout.fillWidth: true

            DialogActionButton {
                text: "Cancel"
                Layout.fillWidth: true
                highlighted: false
                onClicked: root.close()
            }
            DialogActionButton {
                text: "Open"
                Layout.fillWidth: true
                highlighted: true
                primaryColor: Theme.categoryAction
                enabled: root.otherSelected
                         || (root.selectedCandidate() !== null && root.selectedCandidate().available === true
                             && (root.targetPaths.length <= 1 || root.selectedCandidate().supportsMultipleFiles === true))
                onClicked: root.launchSelected()
            }
        }
    }
}
