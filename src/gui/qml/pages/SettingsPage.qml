import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import WebClip

Flickable {
    id: root

    required property var controller

    contentWidth: width
    contentHeight: contentCol.implicitHeight + 20
    clip: true
    flickDeceleration: 1800
    maximumFlickVelocity: 3000
    boundsBehavior: Flickable.DragAndOvershootBounds
    boundsMovement: Flickable.FollowBoundsBehavior

    MD3SmoothScroll {
        target: root
    }

    QQC.ScrollBar.vertical: QQC.ScrollBar {
        policy: QQC.ScrollBar.AsNeeded
    }

    ColumnLayout {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 8

        // Connection Card
        MD3Card {
            Layout.fillWidth: true
            implicitHeight: connLayout.implicitHeight + 20
            variant: "filled"

            ColumnLayout {
                id: connLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Connection Settings"
                        font: MD3Theme.titleSmall
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        width: 7
                        height: 7
                        radius: 3.5
                        color: controller.connected ? "#4CAF50" : (controller.connecting ? "#FF9800" : "#F44336")
                    }

                    Text {
                        text: controller.connected ? "Connected" : (controller.connecting ? "Connecting..." : "Offline")
                        font: MD3Theme.labelSmall
                        color: controller.connected ? "#4CAF50" : MD3Theme.onSurfaceVariant
                    }
                }

                // IP and Port in one row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    MD3TextField {
                        Layout.fillWidth: true
                        label: "Phone IP / Host"
                        placeholderText: "10.36.130.44"
                        text: controller.host
                        onTextChanged: controller.host = text
                    }

                    MD3TextField {
                        Layout.preferredWidth: 80
                        label: "Port"
                        placeholderText: "8080"
                        text: controller.port.toString()
                        onTextChanged: {
                            var p = parseInt(text)
                            if (!isNaN(p)) controller.port = p
                        }
                    }
                }

                // Pairing Code and Connect Action in one row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    MD3TextField {
                        Layout.fillWidth: true
                        label: "Pairing Code"
                        placeholderText: "4-digit code in Gboard"
                        text: controller.code
                        onTextChanged: controller.code = text
                    }

                    MD3Button {
                        Layout.preferredHeight: 44
                        text: controller.connected ? "Disconnect" : (controller.connecting ? "Connecting..." : "Connect")
                        variant: controller.connected ? "tonal" : "filled"
                        iconName: controller.connected ? "close" : "sync"
                        onClicked: controller.toggleConnection()
                    }
                }

                // Options Rows
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Use HTTPS (Port 8081)"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    MD3Switch {
                        checked: controller.useHttps
                        onToggled: controller.useHttps = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Allow Self-Signed SSL"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    MD3Switch {
                        checked: controller.insecure
                        onToggled: controller.insecure = checked
                    }
                }
            }
        }

        // Sync Behavior Card
        MD3Card {
            Layout.fillWidth: true
            implicitHeight: syncLayout.implicitHeight + 20
            variant: "filled"

            ColumnLayout {
                id: syncLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Sync Behavior"
                    font: MD3Theme.titleSmall
                    color: MD3Theme.onSurface
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            text: "Automatic Two-Way Sync"
                            font: MD3Theme.bodySmall
                            color: MD3Theme.onSurface
                        }
                        Text {
                            text: "Instant sync on local PC copy"
                            font: MD3Theme.labelSmall
                            color: MD3Theme.onSurfaceVariant
                        }
                    }

                    MD3Switch {
                        checked: controller.autoSync
                        onToggled: controller.autoSync = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "Fallback Polling"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurface
                    }

                    QQC.Slider {
                        Layout.fillWidth: true
                        from: 0.5
                        to: 5.0
                        stepSize: 0.5
                        value: controller.pollInterval
                        onMoved: controller.pollInterval = value
                    }

                    Text {
                        text: controller.pollInterval.toFixed(1) + " s"
                        font: MD3Theme.labelSmall
                        color: MD3Theme.primary
                    }
                }
            }
        }

        // Appearance & Accent Card
        MD3Card {
            Layout.fillWidth: true
            implicitHeight: appLayout.implicitHeight + 20
            variant: "filled"

            ColumnLayout {
                id: appLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Appearance & Theming"
                    font: MD3Theme.titleSmall
                    color: MD3Theme.onSurface
                }

                // Theme Mode Row
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Theme Mode"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    Row {
                        spacing: 4

                        Repeater {
                            model: [
                                { label: "System", mode: 0 },
                                { label: "Light", mode: 1 },
                                { label: "Dark", mode: 2 }
                            ]

                            Rectangle {
                                width: modeText.implicitWidth + 16
                                height: 26
                                radius: 13
                                color: controller.themeMode === modelData.mode ? MD3Theme.primary : MD3Theme.surfaceContainerHigh
                                border.color: controller.themeMode === modelData.mode ? "transparent" : MD3Theme.outlineVariant
                                border.width: 1

                                Text {
                                    id: modeText
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font: MD3Theme.labelSmall
                                    color: controller.themeMode === modelData.mode ? MD3Theme.onPrimary : MD3Theme.onSurface
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: controller.themeMode = modelData.mode
                                }
                            }
                        }
                    }
                }

                // Accent Presets
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Accent Color"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurface
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: [
                                { name: "purple", label: "Purple", color: "#6750A4" },
                                { name: "blue", label: "Blue", color: "#2196F3" },
                                { name: "teal", label: "Teal", color: "#009688" },
                                { name: "green", label: "Green", color: "#4CAF50" },
                                { name: "orange", label: "Orange", color: "#FF9800" },
                                { name: "red", label: "Red", color: "#F44336" },
                                { name: "pink", label: "Pink", color: "#E91E63" }
                            ]

                            Rectangle {
                                width: pillRow.implicitWidth + 16
                                height: 28
                                radius: 14
                                color: controller.accentPreset === modelData.name ? MD3Theme.primary : "transparent"
                                border.color: controller.accentPreset === modelData.name ? "transparent" : MD3Theme.outlineVariant
                                border.width: 1

                                Behavior on color { ColorAnimation { duration: 150 } }

                                RowLayout {
                                    id: pillRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Rectangle {
                                        width: 10
                                        height: 10
                                        radius: 5
                                        color: modelData.color
                                        border.color: controller.accentPreset === modelData.name ? MD3Theme.onPrimary : "transparent"
                                        border.width: 1
                                    }

                                    Text {
                                        text: modelData.label
                                        font: MD3Theme.labelSmall
                                        color: controller.accentPreset === modelData.name ? MD3Theme.onPrimary : MD3Theme.onSurface
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: controller.accentPreset = modelData.name
                                }
                            }
                        }

                        // Custom Color Pill
                        Rectangle {
                            width: customPillRow.implicitWidth + 16
                            height: 28
                            radius: 14
                            color: controller.accentPreset === "custom" ? MD3Theme.primary : "transparent"
                            border.color: controller.accentPreset === "custom" ? "transparent" : MD3Theme.outlineVariant
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                id: customPillRow
                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: controller.customColor
                                    border.color: controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.outlineVariant
                                    border.width: 1
                                }

                                Text {
                                    text: "Custom"
                                    font: MD3Theme.labelSmall
                                    color: controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.onSurface
                                }

                                MD3Icon {
                                    name: "palette"
                                    size: 12
                                    color: controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.onSurfaceVariant
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    controller.accentPreset = "custom"
                                    colorPickerDialog.openWithColor(controller.customColor)
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Clipboard Engine"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }
                    MD3Badge {
                        text: controller.clipboardBackend
                        badgeColor: MD3Theme.surfaceContainerHigh
                        textColor: MD3Theme.onSurface
                    }
                }

                MD3Button {
                    text: "Clear All History"
                    variant: "outlined"
                    iconName: "delete"
                    Layout.fillWidth: true
                    onClicked: controller.clipModel.clear()
                }
            }
        }
    }

    MD3ColorPickerDialog {
        id: colorPickerDialog
        onAccepted: (col) => {
            controller.customColor = col
            controller.accentPreset = "custom"
        }
    }
}
