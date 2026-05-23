import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import FM
import "../style"

Dialog {
    id: root

    title: isComparison ? "Compare File Checksums" : "File Checksums"
    modal: true
    focus: true
    anchors.centerIn: parent
    width: 620
    height: 540
    padding: 0

    background: Rectangle {
        color: Theme.surface
        radius: 12
        border.color: Theme.border
        border.width: 1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glassShadow
            shadowBlur: 20
            shadowVerticalOffset: 8
        }
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
                font.pixelSize: 12; verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: highlighted ? Theme.itemHoverFill : "transparent"
                radius: 6
            }
            highlighted: combo.highlightedIndex === index
        }

        indicator: Image {
            x: combo.width - width - 10
            y: (combo.height - height) / 2
            width: 10; height: 10; source: "../assets/icons/arrow-up.svg"
            rotation: combo.opened ? 0 : 180; opacity: 0.5
            layer.enabled: true; layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textPrimary }
        }

        contentItem: Label {
            leftPadding: 10; text: combo.displayText; font.pixelSize: 12
            color: Theme.textPrimary; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }

        background: Rectangle {
            implicitHeight: 36; radius: 6; color: Theme.surfaceHover
            border.color: combo.opened ? Theme.accent : Theme.border
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
                color: Theme.menuSurface; radius: 6; border.color: Theme.menuBorder
                layer.enabled: true; layer.effect: MultiEffect { shadowEnabled: true; shadowColor: Theme.glassShadow; shadowBlur: 15 }
            }
        }
    }

    onOpened: {
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

    header: Item {
        height: 60
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 12
            
            Image {
                source: "../assets/icons/settings.svg" // fallback to settings or info icon
                Layout.preferredWidth: 24; Layout.preferredHeight: 24
                layer.enabled: true
                layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.accent }
            }
            
            ColumnLayout {
                spacing: 2
                Label {
                    text: root.title
                    font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.textPrimary
                }
                Label {
                    text: "Computes MD5, SHA-1, and SHA-256 digests"
                    font.pixelSize: 11; color: Theme.textSecondary
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border; opacity: 0.4 }
    }

    footer: Rectangle {
        height: 64
        color: "transparent"
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border; opacity: 0.4 }
        
        RowLayout {
            anchors.fill: parent
            anchors.rightMargin: 20
            spacing: 12
            Item { Layout.fillWidth: true }
            
            Button {
                text: "Cancel"
                visible: root.controller && root.controller.checksumCalculator && root.controller.checksumCalculator.busy
                onClicked: {
                    if (root.controller && root.controller.checksumCalculator) {
                        root.controller.checksumCalculator.abort()
                    }
                    root.reject()
                }
                flat: true
                font.pixelSize: 12
                
                contentItem: Label {
                    text: parent.text
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
            
            Button {
                text: "Close"
                highlighted: true
                onClicked: root.accept()
                
                contentItem: Label {
                    text: parent.text
                    font.pixelSize: 12; font.weight: Font.Medium
                    color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 100; implicitHeight: 36
                    radius: 6
                    color: parent.enabled ? (parent.pressed ? Qt.darker(Theme.accent, 1.1) : Theme.accent) : Theme.border
                }
            }
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
            height: root.isComparison ? 76 : 48
            color: Theme.surfaceHover
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6
                
                RowLayout {
                    spacing: 8
                    Image {
                        source: "../assets/icons/document.svg"
                        Layout.preferredWidth: 16; Layout.preferredHeight: 16
                        layer.enabled: true
                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.accent }
                    }
                    Label {
                        text: root.path1.split(/[/\\]/).pop()
                        font.pixelSize: 12; font.weight: Font.Medium; color: Theme.textPrimary
                        elide: Text.ElideMiddle; Layout.fillWidth: true
                    }
                    Label {
                        visible: root.isComparison
                        text: "[File 1]"
                        font.pixelSize: 10; font.bold: true; color: Theme.accent
                    }
                }
                
                RowLayout {
                    visible: root.isComparison
                    spacing: 8
                    Image {
                        source: "../assets/icons/document.svg"
                        Layout.preferredWidth: 16; Layout.preferredHeight: 16
                        layer.enabled: true
                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "#3b82f6" }
                    }
                    Label {
                        text: root.path2.split(/[/\\]/).pop()
                        font.pixelSize: 12; font.weight: Font.Medium; color: Theme.textPrimary
                        elide: Text.ElideMiddle; Layout.fillWidth: true
                    }
                    Label {
                        text: "[File 2]"
                        font.pixelSize: 10; font.bold: true; color: "#3b82f6"
                    }
                }
            }
            
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border; opacity: 0.3 }
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
                        font.pixelSize: 12; font.weight: Font.Medium; color: Theme.textSecondary
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
                        
                        background: Rectangle { implicitHeight: 6; color: Theme.surfaceHover; radius: 3 }
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
                        font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter; color: Theme.textSecondary
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
                                font.pixelSize: 10; font.bold: true; color: Theme.textSecondary; leftPadding: 2
                            }
                            
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                
                                TextField {
                                    text: modelData.value; readOnly: true
                                    placeholderText: "Not calculated"
                                    placeholderTextColor: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.4)
                                    font.family: "Consolas"; font.pixelSize: 11
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 10
                                    background: Rectangle {
                                        color: Theme.surfaceHover
                                        radius: 6
                                        border.color: Theme.border; border.width: 1
                                    }
                                }
                                
                                Button {
                                    text: "Calculate"
                                    visible: modelData.value === ""
                                    enabled: !(root.controller && root.controller.checksumCalculator && root.controller.checksumCalculator.busy)
                                    
                                    contentItem: Label {
                                        text: parent.text
                                        font.pixelSize: 11; font.weight: Font.Medium
                                        color: parent.enabled ? "white" : Theme.textSecondary
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        implicitWidth: 80; implicitHeight: 32
                                        radius: 6
                                        color: parent.enabled ? (parent.pressed ? Qt.darker(Theme.accent, 1.1) : Theme.accent) : Theme.border
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
                                        radius: 6
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.itemHoverFill : "transparent")
                                    }
                                    contentItem: Image {
                                        source: "../assets/icons/copy.svg"
                                        anchors.centerIn: parent
                                        width: 14; height: 14
                                        layer.enabled: true
                                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textSecondary }
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
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        radius: 8
                        color: root.isMatch
                            ? Qt.rgba(0.14, 0.78, 0.44, 0.08)
                            : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.08)
                        border.color: root.isMatch
                            ? Qt.rgba(0.14, 0.78, 0.44, 0.2)
                            : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.2)
                        border.width: 1
                        
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
                                    colorizationColor: root.isMatch ? "#22c55e" : Theme.danger
                                }
                            }
                            
                            ColumnLayout {
                                spacing: 1
                                Label {
                                    text: root.isMatch ? "Checksums Match" : "Checksums Do Not Match"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: root.isMatch ? "#22c55e" : Theme.danger
                                }
                                Label {
                                    text: root.isMatch
                                        ? "The file contents are verified to be identical."
                                        : "The file contents are different."
                                    font.pixelSize: 11; color: Theme.textSecondary
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
                                    font.pixelSize: 11; font.weight: Font.Bold
                                    color: Theme.textSecondary
                                }
                                
                                Rectangle {
                                    implicitWidth: 50; implicitHeight: 16; radius: 3
                                    color: modelData.val1 === modelData.val2 ? Qt.rgba(0.14, 0.78, 0.44, 0.1) : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.1)
                                    border.color: modelData.val1 === modelData.val2 ? Qt.rgba(0.14, 0.78, 0.44, 0.2) : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.2)
                                    border.width: 1
                                    
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.val1 === modelData.val2 ? "MATCH" : "MISMATCH"
                                        font.pixelSize: 8; font.bold: true
                                        color: modelData.val1 === modelData.val2 ? "#22c55e" : Theme.danger
                                    }
                                }
                            }
                            
                            // File 1 Hash row
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                Label {
                                    text: "File 1:"
                                    font.pixelSize: 10; color: Theme.textSecondary
                                    Layout.preferredWidth: 36
                                }
                                TextField {
                                    text: modelData.val1; readOnly: true
                                    font.family: "Consolas"; font.pixelSize: 10
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 8
                                    background: Rectangle {
                                        color: Theme.surfaceHover
                                        radius: 4
                                        border.color: Theme.border; border.width: 1
                                    }
                                }
                                Button {
                                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                    flat: true
                                    background: Rectangle {
                                        radius: 4
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.itemHoverFill : "transparent")
                                    }
                                    contentItem: Image {
                                        source: "../assets/icons/copy.svg"
                                        anchors.centerIn: parent
                                        width: 12; height: 12
                                        layer.enabled: true
                                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textSecondary }
                                    }
                                    onClicked: workspaceController.copyTextToClipboard(modelData.val1)
                                }
                            }
                            
                            // File 2 Hash row
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                Label {
                                    text: "File 2:"
                                    font.pixelSize: 10; color: Theme.textSecondary
                                    Layout.preferredWidth: 36
                                }
                                TextField {
                                    text: modelData.val2; readOnly: true
                                    font.family: "Consolas"; font.pixelSize: 10
                                    Layout.fillWidth: true; color: Theme.textPrimary
                                    selectByMouse: true; leftPadding: 8
                                    background: Rectangle {
                                        color: Theme.surfaceHover
                                        radius: 4
                                        border.color: Theme.border; border.width: 1
                                    }
                                }
                                Button {
                                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                    flat: true
                                    background: Rectangle {
                                        radius: 4
                                        color: parent.pressed ? Theme.surfaceActive : (parent.hovered ? Theme.itemHoverFill : "transparent")
                                    }
                                    contentItem: Image {
                                        source: "../assets/icons/copy.svg"
                                        anchors.centerIn: parent
                                        width: 12; height: 12
                                        layer.enabled: true
                                        layer.effect: MultiEffect { colorization: 1.0; colorizationColor: Theme.textSecondary }
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
                    font.pixelSize: 12; font.italic: true
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
