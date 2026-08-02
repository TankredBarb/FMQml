import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"
import "common"
import "framework"
import "dialogs"

Dialog {
    id: root

    property var backdropSource: null

    title: isComparison ? "Compare File Checksums" : "File Checksums"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: 620
    height: 540
    padding: 0

    background: DialogShell {
        translucent: typeof appSettings !== "undefined" && appSettings
                     ? appSettings.workspaceDialogsTransparency
                     : false
        backdropSource: root.backdropSource
        backdropTransformItem: root
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

    component ThemedComboBox : FmComboBox {
        font.pixelSize: Theme.fontSizeLabel
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
                sourcePath: "../assets/icons-classic/document.svg"
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
        iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/settings.svg"
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
            ScrollBar.vertical: FmScrollBar {
                id: checksumScrollBar
                parent: mainScroll.contentItem
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                policy: ScrollBar.AsNeeded
            }
            
            ColumnLayout {
                width: checksumScrollBar.scrollNeeded
                       ? Math.max(0, checksumScrollBar.x - 6)
                       : mainScroll.availableWidth
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
                    
                    FmProgressBar {
                        id: prog
                        Layout.fillWidth: true
                        value: (root.controller && root.controller.checksumCalculator) ? root.controller.checksumCalculator.progress : 0
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
                                
                                FmTextField {
                                    text: modelData.value; readOnly: true
                                    placeholderText: "Not calculated"
                                    placeholderTextColor: Theme.withAlpha(Theme.textSecondary, 0.4)
                                    font.family: "Consolas"; font.pixelSize: Theme.fontSizeCaption
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 10
                                }
                                
                                FmButton {
                                    text: "Calculate"
                                    visible: modelData.value === ""
                                    enabled: !(root.controller && root.controller.checksumCalculator && root.controller.checksumCalculator.busy)
                                    highlighted: true
                                    implicitWidth: 80
                                    implicitHeight: 32
                                    
                                    onClicked: {
                                        root.activeAlgorithm = modelData.algoKey
                                        root.calculationStep = 0
                                        root.controller.checksumCalculator.calculate(root.path1, modelData.algoKey)
                                    }
                                }

                                FmIconButton {
                                    visible: modelData.value !== ""
                                    Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
                                    iconSize: 14
                                    svgRecolorColor: Theme.textSecondary
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
                                    ? "../assets/icons-classic/select-all.svg"
                                    : "../assets/icons-classic/info.svg"
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
                                FmTextField {
                                    text: modelData.val1; readOnly: true
                                    font.family: "Consolas"; font.pixelSize: Theme.fontSizeMicro
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 8
                                }
                                FmIconButton {
                                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
                                    iconSize: 12
                                    svgRecolorColor: Theme.textSecondary
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
                                FmTextField {
                                    text: modelData.val2; readOnly: true
                                    font.family: "Consolas"; font.pixelSize: Theme.fontSizeMicro
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 8
                                }
                                FmIconButton {
                                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                    iconSource: "qrc:/qt/qml/FM/qml/assets/icons-classic/copy.svg"
                                    iconSize: 12
                                    svgRecolorColor: Theme.textSecondary
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
