import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../style"
import "../common"

ColumnLayout {
    id: systemSummary

    required property var storageRoot
    spacing: 0
// ── Section Title Header ──────────────────────────────────────────
Item {
    Layout.fillWidth: true
    implicitHeight: 56

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 10

        RecolorSvgIcon {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            sourcePath: "qrc:/qt/qml/FM/qml/assets/icons/computer.svg"
            recolorColor: Theme.actionIconColor("system")
            sourceSize: Qt.size(20, 20)
        }

        Label {
            font.family: Theme.fontFamily
            text: "System Information"
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            color: Theme.textPrimary
        }

        Item { Layout.fillWidth: true }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.panelBorder
        opacity: 0.35
    }
}

// ── Premium Dashboard Card ────────────────────────────────────────
Item {
    id: dashboardCardContainer
    Layout.fillWidth: true
    Layout.leftMargin: 16
    Layout.rightMargin: 16
    Layout.topMargin: 16
    Layout.bottomMargin: 20 + systemSummary.storageRoot.gapAmount
    implicitHeight: 132

    // Shadow underlay (no children)
    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLg
        color: "transparent"
        layer.enabled: !systemSummary.storageRoot.effectsReduced
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, themeController.isDark ? 0.20 : 0.06)
            shadowBlur: 0.8
            shadowVerticalOffset: 3
        }
    }

    SurfaceCard {
        id: dashboardCard
        anchors.fill: parent
        cornerRadius: Theme.radiusLg
        surfaceColor: themeController.isDark
            ? Theme.withAlpha(Theme.panelSurface, 0.78)
            : Theme.withAlpha(Theme.panelSurface, 0.92)
        strokeColor: Theme.panelBorder

        RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 0

        // Left Column (System Info)
        ColumnLayout {
            spacing: 4
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 260

            RowLayout {
                spacing: 8
                Label {
                    font.family: Theme.fontFamily
                    text: systemInfoProvider.computerName
                    font.pixelSize: Theme.scaledSize(15)
                    font.bold: true
                    color: Theme.textPrimary
                }

                InlineBadge {
                    text: systemInfoProvider.osName
                    fillColor: Theme.withAlpha(Theme.accent, 0.14)
                    strokeColor: "transparent"
                    textColor: Theme.accent
                    horizontalPadding: 12
                    badgeHeight: 18
                    fontSize: 9
                    fontWeight: Font.Bold
                }
            }

            // CPU Model Name
            Label {
                font.family: Theme.fontFamily
                text: systemInfoProvider.cpuName || "Detecting CPU..."
                font.pixelSize: Theme.fontSizeCaption
                font.bold: true
                color: Theme.textSecondary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 6
                Label {
                    font.family: Theme.fontFamily
                    text: systemInfoProvider.cpuCores + " Cores (" + systemInfoProvider.cpuArchitecture + ")"
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.75
                }
                Rectangle {
                    width: 3
                    height: 3
                    radius: 1.5
                    color: Theme.textSecondary
                    opacity: 0.5
                }
                Label {
                    font.family: Theme.fontFamily
                    text: "Uptime: " + systemInfoProvider.uptime
                    font.pixelSize: Theme.fontSizeMicro
                    color: Theme.textSecondary
                    opacity: 0.75
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Center Column (Gauges)
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 24

            // RAM Gauge
            ColumnLayout {
                spacing: 4
                Layout.alignment: Qt.AlignHCenter

                Item {
                    width: 56
                    height: 56

                    Canvas {
                        id: ramCanvas
                        anchors.fill: parent
                        property real val: systemInfoProvider.ramUsage
                        onValChanged: {
                            if (!systemSummary.storageRoot.effectsReduced) {
                                requestPaint()
                            }
                        }
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);

                            // Track
                            ctx.beginPath();
                            ctx.arc(width/2, height/2, width/2 - 4, 0, 2*Math.PI);
                            ctx.lineWidth = 4;
                            ctx.strokeStyle = themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05);
                            ctx.stroke();

                            // Active
                            ctx.beginPath();
                            ctx.arc(width/2, height/2, width/2 - 4, -Math.PI/2, -Math.PI/2 + val * 2*Math.PI);
                            ctx.lineWidth = 4;
                            ctx.strokeStyle = Theme.categoryNavigation;
                            ctx.lineCap = "round";
                            ctx.stroke();
                        }
                    }

                    Label {
                        font.family: Theme.fontFamily
                        anchors.centerIn: parent
                        text: Math.round(systemInfoProvider.ramUsage * 100) + "%"
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }

                Label {
                    font.family: Theme.fontFamily
                    text: systemInfoProvider.usedRamGB.toFixed(1) + " / " + systemInfoProvider.totalRamGB.toFixed(0) + " GB"
                    font.pixelSize: Theme.scaledSize(9)
                    font.bold: true
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // CPU Gauge
            ColumnLayout {
                spacing: 4
                Layout.alignment: Qt.AlignHCenter

                Item {
                    width: 56
                    height: 56

                    Canvas {
                        id: cpuCanvas
                        anchors.fill: parent
                        property real val: systemInfoProvider.cpuUsage
                        onValChanged: {
                            if (!systemSummary.storageRoot.effectsReduced) {
                                requestPaint()
                            }
                        }
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);

                            // Track
                            ctx.beginPath();
                            ctx.arc(width/2, height/2, width/2 - 4, 0, 2*Math.PI);
                            ctx.lineWidth = 4;
                            ctx.strokeStyle = themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05);
                            ctx.stroke();

                            // Active
                            ctx.beginPath();
                            ctx.arc(width/2, height/2, width/2 - 4, -Math.PI/2, -Math.PI/2 + val * 2*Math.PI);
                            ctx.lineWidth = 4;
                            ctx.strokeStyle = Theme.categoryInfo;
                            ctx.lineCap = "round";
                            ctx.stroke();
                        }
                    }

                    Label {
                        font.family: Theme.fontFamily
                        anchors.centerIn: parent
                        text: Math.round(systemInfoProvider.cpuUsage * 100) + "%"
                        font.pixelSize: Theme.fontSizeMicro
                        font.bold: true
                        color: Theme.textPrimary
                    }
                }

                Label {
                    font.family: Theme.fontFamily
                    text: "CPU Load"
                    font.pixelSize: Theme.scaledSize(9)
                    font.bold: true
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Right Column (Storage Overview)
        ColumnLayout {
            spacing: 4
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 180

            Label {
                font.family: Theme.fontFamily
                text: "Unified Drive Usage"
                font.pixelSize: Theme.fontSizeCaption
                font.bold: true
                color: Theme.textPrimary
            }

            LinearProgress {
                Layout.fillWidth: true
                readonly property real usage: systemSummary.storageRoot.totalSpaceSum > 0 ? (systemSummary.storageRoot.totalSpaceSum - systemSummary.storageRoot.freeSpaceSum) / systemSummary.storageRoot.totalSpaceSum : 0.0
                value: usage
                trackColor: themeController.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,0,0,0.05)
                fillColor: usage > 0.90 ? Theme.danger : (usage > 0.75 ? Theme.warning : Theme.accent)
            }

            Label {
                font.family: Theme.fontFamily
                text: systemSummary.storageRoot.formatBytes(systemSummary.storageRoot.totalSpaceSum - systemSummary.storageRoot.freeSpaceSum) + " used of " + systemSummary.storageRoot.formatBytes(systemSummary.storageRoot.totalSpaceSum)
                font.pixelSize: Theme.scaledSize(9)
                color: Theme.textSecondary
                opacity: 0.8
            }
        }
    }
}
        } // end dashboardCardContainer

}
