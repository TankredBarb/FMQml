import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"
import "common"
import "dialogs"

Dialog {
    id: root

    title: isComparison ? "Compare File Checksums" : "File Checksums"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: 620
    height: 540
    padding: 0

    background: DialogShell {
        accentColor: Theme.categoryInfo
        shellBorderColor: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.28 : 0.20)
    }

    property string path1: ""
    property string path2: ""
    property bool isComparison: path2.length > 0
    property var controller: null

    // Calculated hashes stored in local state
    property string hash1_md5: ""
    property string hash1_sha1: ""
    property string hash1_sha256: ""
    
    property string hash2_md5: ""
    property string hash2_sha1: ""
    property string hash2_sha256: ""
    
    // 0: computing file 1, 1: computing file 2, 2: completed/idle
    property int calculationStep: 2
    property string activeAlgorithm: "sha256"

    readonly property bool isMatch: {
        if (activeAlgorithm === "sha256") return hash1_sha256 !== "" && hash1_sha256 === hash2_sha256
        if (activeAlgorithm === "sha1") return hash1_sha1 !== "" && hash1_sha1 === hash2_sha1
        if (activeAlgorithm === "md5") return hash1_md5 !== "" && hash1_md5 === hash2_md5
        return false
    }

    // Custom ComboBox
    component ThemedComboBox : ComboBox {
        id: combo
        
        delegate: ItemDelegate {
            width: combo.width; height: 36
            contentItem: Label {
                text: modelData
                color: highlighted ? Theme.accent : Theme.textPrimary
                font.pixelSize: Theme.fontSizeLabel; verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: highlighted ? Theme.itemHoverFill : "transparent"
                radius: Theme.radiusSm
            }
            highlighted: combo.highlightedIndex === index
        }

        indicator: RecolorSvgIcon {
            x: combo.width - width - 10
            y: (combo.height - height) / 2
            width: 10; height: 10; sourcePath: "../assets/icons/arrow-up.svg"
            recolorColor: Theme.textPrimary
            rotation: combo.opened ? 0 : 180; opacity: 0.5
        }

        contentItem: Label {
            leftPadding: 10; text: combo.displayText; font.pixelSize: Theme.fontSizeLabel
            color: Theme.textPrimary; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }

        background: Rectangle {
            implicitHeight: 36; radius: Theme.radiusSm; color: Theme.panelSurfaceSoft
            border.color: combo.opened ? Theme.accent : Theme.panelBorder
            border.width: combo.opened ? 2 : 1
        }

        popup: Popup {
            y: combo.height + 4; width: combo.width
            implicitHeight: combo.model && combo.model.length ? (combo.model.length * 36 + 8) : 100; padding: 4
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            background: Rectangle {
                color: Theme.menuSurface; radius: Theme.radiusSm; border.color: Theme.menuBorder
                layer.enabled: true; layer.effect: MultiEffect { shadowEnabled: true; shadowColor: Theme.glassShadow; shadowBlur: 15 }
            }
        }
    }

    component FileHeaderRow : Rectangle {
        id: fileHeaderRow

        required property string filePath
        required property string tagText

        Layout.fillWidth: true
        Layout.preferredHeight: 24
        radius: Theme.radiusSm
        color: Theme.withAlpha(Theme.panelSurfaceSoft, themeController.isDark ? 0.44 : 0.62)
        border.color: Theme.withAlpha(Theme.panelBorder, 0.72)
        border.width: 1
        clip: true

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            RecolorSvgIcon {
                sourcePath: "../assets/icons/document.svg"
                recolorColor: Theme.categoryInfo
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }

            Label {
                text: fileHeaderRow.filePath.split(/[/\\]/).pop()
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLabel
                font.weight: Font.Medium
                elide: Text.ElideMiddle
                verticalAlignment: Text.AlignVCenter
            }

            Label {
                text: fileHeaderRow.tagText
                color: Theme.categoryInfo
                font.pixelSize: Theme.fontSizeMicro
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    onOpened: {
        Qt.callLater(() => contentItem.forceActiveFocus())
        hash1_md5 = ""
        hash1_sha1 = ""
        hash1_sha256 = ""
        hash2_md5 = ""
        hash2_sha1 = ""
        hash2_sha256 = ""
        
        if (root.isComparison) {
            root.activeAlgorithm = "sha256"
            root.startComparisonCalculations()
        } else {
            root.activeAlgorithm = ""
            root.calculationStep = 2
            if (root.controller && root.controller.checksumCalculator) {
                root.controller.checksumCalculator.clear()
            }
        }
    }

    onClosed: {
        if (root.controller && root.controller.checksumCalculator) {
            root.controller.checksumCalculator.abort()
        }
        calculationStep = 2
    }

    function startComparisonCalculations() {
        if (!root.controller || !root.controller.checksumCalculator) return
        
        root.controller.checksumCalculator.abort()
        
        hash1_md5 = ""
        hash1_sha1 = ""
        hash1_sha256 = ""
        hash2_md5 = ""
        hash2_sha1 = ""
        hash2_sha256 = ""
        
        calculationStep = 0
        root.controller.checksumCalculator.calculate(root.path1, root.activeAlgorithm)
    }

    Connections {
        target: (root.controller && root.controller.checksumCalculator) ? root.controller.checksumCalculator : null
        
        function onFinished() {
            let calc = root.controller.checksumCalculator
            if (root.calculationStep === 0) {
                if (calc.md5 !== "") root.hash1_md5 = calc.md5
                if (calc.sha1 !== "") root.hash1_sha1 = calc.sha1
                if (calc.sha256 !== "") root.hash1_sha256 = calc.sha256
                
                if (root.isComparison && root.path2.length > 0) {
                    root.calculationStep = 1
                    calc.calculate(root.path2, root.activeAlgorithm)
                } else {
                    root.calculationStep = 2
                }
            } else if (root.calculationStep === 1) {
                if (calc.md5 !== "") root.hash2_md5 = calc.md5
                if (calc.sha1 !== "") root.hash2_sha1 = calc.sha1
                if (calc.sha256 !== "") root.hash2_sha256 = calc.sha256
                root.calculationStep = 2
            }
        }
        
        function onErrorOccurred(errorMsg) {
            console.log("[Checksum] Error calculating hash:", errorMsg)
            root.calculationStep = 2
        }
    }

    header: DialogHeader {
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons/settings.svg"
        iconTint: Theme.categoryInfo
        accentColor: Theme.categoryInfo
        title: root.title
        subtitle: "Computes MD5, SHA-1, and SHA-256 digests"
        closeText: "x"
        onCloseRequested: root.accept()
    }

    footer: DialogFooter {
        Item {
            Layout.fillWidth: true
        }

        DialogActionButton {
            visible: root.controller && root.controller.checksumCalculator && root.controller.checksumCalculator.busy
            text: "Cancel"
            highlighted: false
            onClicked: {
                if (root.controller && root.controller.checksumCalculator) {
                    root.controller.checksumCalculator.abort()
                }
                root.reject()
            }
        }

        DialogActionButton {
            text: "Close"
            highlighted: true
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
            if (event.key === Qt.Key_Escape) {
                if (root.controller && root.controller.checksumCalculator) {
                    root.controller.checksumCalculator.abort()
                }
                root.reject()
                event.accepted = true
            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.accept()
                event.accepted = true
            }
        }
        
        // --- File Info Bar ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.isComparison ? 76 : 48
            color: Theme.withAlpha(Theme.categoryInfo, themeController.isDark ? 0.08 : 0.045)
            clip: true
            
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 6
                
                FileHeaderRow {
                    filePath: root.path1
                    tagText: root.isComparison ? "FILE 1" : "FILE"
                }
                
                FileHeaderRow {
                    visible: root.isComparison
                    filePath: root.path2
                    tagText: "FILE 2"
                }
            }
            
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.panelBorder; opacity: 0.3 }
        }

        // --- Scrollable Details ---
        ScrollView {
            id: mainScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            leftPadding: 20
            rightPadding: 20
            topPadding: 16
            bottomPadding: 16
            
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            
            ColumnLayout {
                width: mainScroll.width - 40
                spacing: 16
                
                // --- Algorithm Selector (Comparison Mode) ---
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.isComparison
                    Layout.bottomMargin: 4

                    Label {
                        text: "Hash Algorithm:"
                        font.pixelSize: Theme.fontSizeLabel; font.weight: Font.Medium; color: Theme.textSecondary
                    }

                    ThemedComboBox {
                        id: algoSelector
                        Layout.preferredWidth: 120
                        model: ["SHA-256", "SHA-1", "MD5"]
                        
                        currentIndex: {
                            if (root.activeAlgorithm === "sha256") return 0
                            if (root.activeAlgorithm === "sha1") return 1
                            if (root.activeAlgorithm === "md5") return 2
                            return 0
                        }
                        
                        onActivated: (index) => {
                            let algoMap = ["sha256", "sha1", "md5"]
                            let newAlgo = algoMap[index]
                            if (root.activeAlgorithm !== newAlgo) {
                                root.activeAlgorithm = newAlgo
                                root.startComparisonCalculations()
                            }
                        }
                    }
                    
                    Item { Layout.fillWidth: true }
                }

                // --- Progress Indicator ---
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.controller && root.controller.checksumCalculator && root.controller.checksumCalculator.busy
                    spacing: 10
                    
                    ProgressBar {
                        id: prog
                        Layout.fillWidth: true
                        value: (root.controller && root.controller.checksumCalculator) ? root.controller.checksumCalculator.progress : 0
                        
                        background: Rectangle { implicitHeight: 6; color: Theme.panelSurfaceSoft; radius: Theme.radiusSm }
                        contentItem: Item {
                            Rectangle {
                                width: prog.visualPosition * parent.width
                                height: parent.height
                                radius: 3
                                color: Theme.accent
                            }
                        }
                    }
                    
                    Label {
                        text: {
                            if (!root.controller || !root.controller.checksumCalculator) return ""
                            let filename = root.calculationStep === 0 
                                ? root.path1.split(/[/\\]/).pop() 
                                : root.path2.split(/[/\\]/).pop()
                            let stepText = root.isComparison 
                                ? "file " + (root.calculationStep + 1) + " of 2: " 
                                : ""
                            let algoText = root.activeAlgorithm ? root.activeAlgorithm.toUpperCase() + " " : ""
                            return "Calculating " + algoText + "for " + stepText + filename + "... " + Math.floor(prog.value * 100) + "%"
                        }
                        font.pixelSize: Theme.fontSizeLabel; Layout.alignment: Qt.AlignHCenter; color: Theme.textSecondary
                    }
                }
                
                // --- Single File Hash Results ---
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !root.isComparison
                    spacing: 16
                    
                    Repeater {
                        model: [
                            { label: "MD5", value: root.hash1_md5, algoKey: "md5" },
                            { label: "SHA-1", value: root.hash1_sha1, algoKey: "sha1" },
                            { label: "SHA-256", value: root.hash1_sha256, algoKey: "sha256" }
                        ]
                        
                        delegate: ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            
                            Label {
                                text: modelData.label
                                font.pixelSize: Theme.fontSizeMicro; font.bold: true; color: Theme.textSecondary; leftPadding: 2
                            }
                            
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                
                                TextField {
                                    text: modelData.value; readOnly: true
                                    placeholderText: "Not calculated"
                                    placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                    font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 10
                                    background: Rectangle {
                                        color: Theme.panelSurfaceSoft
                                        radius: Theme.radiusSm
                                        border.color: Theme.panelBorder; border.width: 1
                                    }
                                }
                                
                                Button {
                                    text: "Calculate"
                                    visible: modelData.value === ""
                                    enabled: !(root.controller && root.controller.checksumCalculator && root.controller.checksumCalculator.busy)
                                    
                                    contentItem: Label {
                                        text: parent.text
                                        font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Medium
                                        color: parent.enabled ? Theme.readableOn(Theme.accent, Theme.accentText) : Theme.textSecondary
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        implicitWidth: 80; implicitHeight: 32
                                        radius: Theme.radiusSm
                                        color: parent.enabled ? Theme.accent : Theme.panelBorder
                                    }
                                    
                                    onClicked: {
                                        root.activeAlgorithm = modelData.algoKey
                                        root.calculationStep = 0
                                        root.controller.checksumCalculator.calculate(root.path1, modelData.algoKey)
                                    }
                                }

                                Button {
                                    visible: modelData.value !== ""
                                    Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                    flat: true
                                    background: Rectangle {
                                        radius: Theme.radiusSm
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
                                    }
                                    contentItem: RecolorSvgIcon {
                                        sourcePath: "../assets/icons/clipboard-copy.svg"
                                        recolorColor: Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 14; height: 14
                                    }
                                    onClicked: workspaceController.copyTextToClipboard(modelData.value)
                                }
                            }
                        }
                    }
                }
                
                // --- Two Files Comparison Results ---
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.isComparison && root.calculationStep === 2
                    spacing: 16
                    
                    // Matching status card
                    SurfaceCard {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        cornerRadius: Theme.radiusMd
                        surfaceColor: root.isMatch
                            ? Theme.withAlpha(Theme.success, 0.08)
                            : Theme.withAlpha(Theme.danger, 0.08)
                        strokeColor: root.isMatch
                            ? Theme.withAlpha(Theme.success, 0.2)
                            : Theme.withAlpha(Theme.danger, 0.2)
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10
                            
                            Image {
                                source: root.isMatch
                                    ? "../assets/icons/select-all.svg"
                                    : "../assets/icons/info.svg"
                                Layout.preferredWidth: 20; Layout.preferredHeight: 20
                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    colorization: 1.0;
                                    colorizationColor: root.isMatch ? Theme.success : Theme.danger
                                }
                            }
                            
                            ColumnLayout {
                                spacing: 1
                                Label {
                                    text: root.isMatch ? "Checksums Match" : "Checksums Do Not Match"
                                    font.pixelSize: Theme.fontSizeBody; font.weight: Font.DemiBold
                                    color: root.isMatch ? Theme.success : Theme.danger
                                }
                                Label {
                                    text: root.isMatch
                                        ? "The file contents are verified to be identical."
                                        : "The file contents are different."
                                    font.pixelSize: Theme.fontSizeCaption; color: Theme.textSecondary
                                }
                            }
                        }
                    }
                    
                    // Checksum comparisons details list
                    Repeater {
                        model: {
                            if (root.activeAlgorithm === "sha256") {
                                return [{ label: "SHA-256", val1: root.hash1_sha256, val2: root.hash2_sha256 }]
                            } else if (root.activeAlgorithm === "sha1") {
                                return [{ label: "SHA-1", val1: root.hash1_sha1, val2: root.hash2_sha1 }]
                            } else {
                                return [{ label: "MD5", val1: root.hash1_md5, val2: root.hash2_md5 }]
                            }
                        }
                        
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            
                            // Hash type header with match badge
                            RowLayout {
                                spacing: 8
                                Label {
                                    text: modelData.label
                                    font.pixelSize: Theme.fontSizeCaption; font.weight: Font.Bold
                                    color: Theme.textSecondary
                                }
                                
                                InlineBadge {
                                    text: modelData.val1 === modelData.val2 ? "MATCH" : "MISMATCH"
                                    fillColor: modelData.val1 === modelData.val2 ? Theme.withAlpha(Theme.success, 0.10) : Theme.withAlpha(Theme.danger, 0.10)
                                    strokeColor: modelData.val1 === modelData.val2 ? Theme.withAlpha(Theme.success, 0.20) : Theme.withAlpha(Theme.danger, 0.20)
                                    textColor: modelData.val1 === modelData.val2 ? Theme.success : Theme.danger
                                    horizontalPadding: 10
                                    badgeHeight: 16
                                    fontSize: 8
                                    fontWeight: Font.Bold
                                }
                            }
                            
                            // File 1 Hash row
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                Label {
                                    text: "File 1:"
                                    font.pixelSize: Theme.fontSizeMicro; color: Theme.textSecondary
                                    Layout.preferredWidth: 36
                                }
                                TextField {
                                    text: modelData.val1; readOnly: true
                                    font.family: "Consolas"; font.pixelSize: Theme.fontSizeMicro
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 8
                                    background: Rectangle {
                                        color: Theme.panelSurfaceSoft
                                        radius: Theme.radiusSm
                                        border.color: Theme.panelBorder; border.width: 1
                                    }
                                }
                                Button {
                                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                    flat: true
                                    background: Rectangle {
                                        radius: Theme.radiusSm
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
                                    }
                                    contentItem: RecolorSvgIcon {
                                        sourcePath: "../assets/icons/clipboard-copy.svg"
                                        recolorColor: Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 12; height: 12
                                    }
                                    onClicked: workspaceController.copyTextToClipboard(modelData.val1)
                                }
                            }
                            
                            // File 2 Hash row
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                Label {
                                    text: "File 2:"
                                    font.pixelSize: Theme.fontSizeMicro; color: Theme.textSecondary
                                    Layout.preferredWidth: 36
                                }
                                TextField {
                                    text: modelData.val2; readOnly: true
                                    font.family: "Consolas"; font.pixelSize: Theme.fontSizeMicro
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 8
                                    background: Rectangle {
                                        color: Theme.panelSurfaceSoft
                                        radius: Theme.radiusSm
                                        border.color: Theme.panelBorder; border.width: 1
                                    }
                                }
                                Button {
                                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                    flat: true
                                    background: Rectangle {
                                        radius: Theme.radiusSm
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.panelSurfaceSoft : "transparent")
                                    }
                                    contentItem: RecolorSvgIcon {
                                        sourcePath: "../assets/icons/clipboard-copy.svg"
                                        recolorColor: Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 12; height: 12
                                    }
                                    onClicked: workspaceController.copyTextToClipboard(modelData.val2)
                                }
                            }
                            
                            Item { Layout.preferredHeight: 4 } // Spacer
                        }
                    }
                }
                
                // --- System Error Message ---
                Label {
                    id: errLabel
                    text: (root.controller && root.controller.checksumCalculator) ? root.controller.checksumCalculator.error : ""
                    visible: text.length > 0
                    color: Theme.danger
                    font.pixelSize: Theme.fontSizeLabel; font.italic: true
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
