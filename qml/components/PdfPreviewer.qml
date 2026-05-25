import QtQuick
import QtQuick.Pdf
import QtQuick.Layouts
import QtQuick.Controls
import "../style"

Item {
    id: root
    property string sourcePath: ""

    clip: true

    property string pdfSourceUrl: root.sourcePath.length > 0
                                  ? ("file:///" + root.sourcePath.replace(/\\/g, "/"))
                                  : ""
    property int currentPage: 0

    PdfDocument {
        id: pdfDoc
        source: root.pdfSourceUrl
    }

    function clampPage(page) {
        if (pdfDoc.pageCount <= 0) {
            return 0
        }
        return Math.max(0, Math.min(pdfDoc.pageCount - 1, page))
    }

    function fitCurrentPage() {
        currentPage = clampPage(currentPage)
    }

    onSourcePathChanged: {
        currentPage = 0
    }

    onWidthChanged: {
        // Keep the visible page stable; the image itself adapts to the viewport.
    }

    onHeightChanged: {
        // Keep the visible page stable; the image itself adapts to the viewport.
    }

    Image {
        id: pdfImage
        anchors.fill: parent
        visible: pdfDoc.status === 2
        source: pdfDoc.status === 2 ? root.pdfSourceUrl : ""
        currentFrame: root.currentPage
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        sourceSize: Qt.size(Math.max(width, 1), Math.max(height, 1))
        smooth: true
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: pdfDoc.status === 1
        visible: running
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: pdfDoc.status === 4
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 96
            height: 96
            radius: 18
            color: Qt.rgba(219/255, 68/255, 55/255, 0.15)

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

    Rectangle {
        id: navBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 16
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

            Button {
                id: prevBtn
                flat: true
                enabled: root.currentPage > 0
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.currentPage = root.clampPage(root.currentPage - 1)

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

            TextField {
                id: pageInput
                text: (root.currentPage + 1).toString()
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
                    top: Math.max(1, pdfDoc.pageCount)
                }

                onAccepted: {
                    const targetPage = parseInt(text) - 1
                    if (targetPage >= 0 && targetPage < pdfDoc.pageCount) {
                        root.currentPage = targetPage
                    }
                    pageInput.focus = false
                }

                Connections {
                    target: root
                    function onCurrentPageChanged() {
                        if (!pageInput.activeFocus) {
                            pageInput.text = (root.currentPage + 1).toString()
                        }
                    }
                }
            }

            Label {
                text: "/ " + pdfDoc.pageCount
                font.pixelSize: 12
                color: Theme.textSecondary
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
            }

            Button {
                id: nextBtn
                flat: true
                enabled: root.currentPage < pdfDoc.pageCount - 1
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.currentPage = root.clampPage(root.currentPage + 1)

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
