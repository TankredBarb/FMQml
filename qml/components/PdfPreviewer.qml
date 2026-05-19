import QtQuick
import QtQuick.Pdf
import QtQuick.Layouts
import QtQuick.Controls
import "../style"

Item {
    id: root
    property string sourcePath: ""

    clip: true

    PdfDocument {
        id: pdfDoc
        source: root.sourcePath.length > 0 ? ("file:///" + root.sourcePath.replace(/\\/g, "/")) : ""
    }

    PdfPageView {
        id: pdfView
        anchors.fill: parent
        document: pdfDoc
        visible: pdfDoc.status === 2 // Ready
        
        onWidthChanged: fitTimer.restart()
        onHeightChanged: fitTimer.restart()
        onCurrentPageChanged: fitTimer.restart()
        
        onStatusChanged: {
            if (status === 2) {
                fitTimer.restart()
            }
        }
        
        // Debounce timer for scaling to avoid layout loops or jittering
        Timer {
            id: fitTimer
            interval: 50
            repeat: false
            onTriggered: pdfView.fitPage()
        }
        
        function fitPage() {
            if (pdfView.width > 0 && pdfView.height > 0 && pdfDoc.status === 2) {
                pdfView.scaleToPage(pdfView.width, pdfView.height)
            }
        }

        // Clicking the viewport unfocuses the page input
        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onPressed: (mouse) => {
                pageInput.focus = false
                mouse.accepted = false // let selection/interaction pass through
            }
        }
    }

    // Busy Indicator during load
    BusyIndicator {
        anchors.centerIn: parent
        running: pdfDoc.status === 1 // Loading
    }

    // Error Fallback
    ColumnLayout {
        anchors.centerIn: parent
        visible: pdfDoc.status === 4 // Error
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 96
            height: 96
            radius: 18
            color: Qt.rgba(219/255, 68/255, 55/255, 0.15) // PDF red tint

            Image {
                anchors.centerIn: parent
                source: "../assets/icons/document.svg"
                sourceSize: Qt.size(44, 44)
                opacity: 0.8
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            Label {
                text: "Failed to load PDF"
                font.bold: true
                font.pixelSize: 15
                color: Theme.textPrimary
                Layout.alignment: Qt.AlignHCenter
            }
            Label {
                text: "The document may be corrupted"
                font.pixelSize: 11
                color: Theme.textSecondary
                Layout.alignment: Qt.AlignHCenter
                opacity: 0.7
            }
        }
    }


    // Floating Navigation Bar
    Rectangle {
        id: navBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 16
        
        // Hide if the document has 1 or fewer pages or failed to load
        visible: pdfDoc.status === 2 && pdfDoc.pageCount > 1
        
        height: 40
        implicitWidth: navRow.width + 24
        radius: 20
        color: Theme.glassSurfaceStrong
        border.color: Theme.glassBorder
        border.width: 1
        
        Row {
            id: navRow
            anchors.centerIn: parent
            spacing: 8
            
            // Previous button
            Button {
                id: prevBtn
                flat: true
                enabled: pdfView.currentPage > 0
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    pdfView.goToPage(pdfView.currentPage - 1)
                }
                
                background: Rectangle {
                    implicitWidth: 28
                    implicitHeight: 28
                    radius: 14
                    color: prevBtn.hovered ? (themeController.isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.05)) : "transparent"
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
                
                contentItem: Image {
                    source: "../assets/icons/arrow-left.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: prevBtn.enabled ? (prevBtn.hovered ? 1.0 : 0.7) : 0.25
                    anchors.centerIn: parent
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                }
            }
            
            // Interactive Page Input
            TextField {
                id: pageInput
                text: (pdfView.currentPage + 1).toString()
                font.pixelSize: 12
                font.bold: true
                color: Theme.textPrimary
                width: 36
                height: 26
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                selectByMouse: true
                
                background: Rectangle {
                    color: pageInput.activeFocus 
                        ? (themeController.isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.05)) 
                        : "transparent"
                    border.color: pageInput.activeFocus ? Theme.accent : "transparent"
                    border.width: 1
                    radius: 6
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
                
                validator: IntValidator {
                    bottom: 1
                    top: pdfDoc.pageCount
                }
                
                onAccepted: {
                    var targetPage = parseInt(text) - 1;
                    if (targetPage >= 0 && targetPage < pdfDoc.pageCount) {
                        pdfView.goToPage(targetPage);
                    }
                    pageInput.focus = false
                }
                
                Connections {
                    target: pdfView
                    function onCurrentPageChanged() {
                        if (!pageInput.activeFocus) {
                            pageInput.text = (pdfView.currentPage + 1).toString();
                        }
                    }
                }
            }
            
            // Page Count display
            Label {
                text: "/ " + pdfDoc.pageCount
                font.pixelSize: 12
                color: Theme.textSecondary
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
            }
            
            // Next button
            Button {
                id: nextBtn
                flat: true
                enabled: pdfView.currentPage < pdfDoc.pageCount - 1
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    pdfView.goToPage(pdfView.currentPage + 1)
                }
                
                background: Rectangle {
                    implicitWidth: 28
                    implicitHeight: 28
                    radius: 14
                    color: nextBtn.hovered ? (themeController.isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.05)) : "transparent"
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
                
                contentItem: Image {
                    source: "../assets/icons/arrow-right.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: nextBtn.enabled ? (nextBtn.hovered ? 1.0 : 0.7) : 0.25
                    anchors.centerIn: parent
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                }
            }
        }
    }
}
