import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import WebClip

Flickable {
    id: root

    required property var controller

    contentWidth: width
    contentHeight: contentCol.implicitHeight + 40
    clip: true
    flickDeceleration: 1800
    maximumFlickVelocity: 3000
    boundsBehavior: Flickable.DragAndOvershootBounds
    boundsMovement: Flickable.FollowBoundsBehavior

    WheelHandler {
        target: root
        onWheel: (event) => {
            var delta = event.angleDelta.y
            root.flick(0, delta * 12)
        }
    }

    ColumnLayout {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 16

        // Connection Card
        MD3Card {
            Layout.fillWidth: true
            implicitHeight: connLayout.implicitHeight + 32
            variant: "filled"

            ColumnLayout {
                id: connLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Connection Settings"
                        font: MD3Theme.titleMedium
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: controller.connected ? "#4CAF50" : (controller.connecting ? "#FF9800" : "#F44336")
                    }

                    Text {
                        text: controller.connected ? "Connected" : (controller.connecting ? "Connecting..." : "Offline")
                        font: MD3Theme.labelMedium
                        color: controller.connected ? "#4CAF50" : MD3Theme.onSurfaceVariant
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    MD3TextField {
                        Layout.fillWidth: true
                        label: "Phone IP / URL"
                        placeholderText: "e.g. 10.36.130.44"
                        text: controller.host
                        onTextChanged: controller.host = text
                    }

                    MD3TextField {
                        Layout.preferredWidth: 100
                        label: "Port"
                        placeholderText: "8080"
                        text: controller.port.toString()
                        onTextChanged: {
                            var p = parseInt(text)
                            if (!isNaN(p)) controller.port = p
                        }
                    }
                }

                MD3TextField {
                    Layout.fillWidth: true
                    label: "Pairing Code"
                    placeholderText: "4-digit code shown in Gboard"
                    text: controller.code
                    onTextChanged: controller.code = text
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: "Use HTTPS (Port 8081)"
                            font: MD3Theme.bodyLarge
                            color: MD3Theme.onSurface
                        }
                        Text {
                            text: "Encrypt connection between PC and phone"
                            font: MD3Theme.bodySmall
                            color: MD3Theme.onSurfaceVariant
                        }
                    }

                    MD3Switch {
                        checked: controller.useHttps
                        onToggled: controller.useHttps = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: "Allow Self-Signed SSL"
                            font: MD3Theme.bodyLarge
                            color: MD3Theme.onSurface
                        }
                        Text {
                            text: "Required for Gboard's local HTTPS certificate"
                            font: MD3Theme.bodySmall
                            color: MD3Theme.onSurfaceVariant
                        }
                    }

                    MD3Switch {
                        checked: controller.insecure
                        onToggled: controller.insecure = checked
                    }
                }

                // Dedicated Connect / Disconnect Action Button
                MD3Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: controller.connected ? "Disconnect from Phone" : (controller.connecting ? "Connecting..." : "Connect to Phone")
                    variant: controller.connected ? "tonal" : "filled"
                    iconName: controller.connected ? "close" : "sync"
                    onClicked: controller.toggleConnection()
                }
            }
        }

        // Sync Behavior Card
        MD3Card {
            Layout.fillWidth: true
            implicitHeight: syncLayout.implicitHeight + 32
            variant: "filled"

            ColumnLayout {
                id: syncLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "Sync Behavior"
                    font: MD3Theme.titleMedium
                    color: MD3Theme.onSurface
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: "Automatic Two-Way Sync"
                            font: MD3Theme.bodyLarge
                            color: MD3Theme.onSurface
                        }
                        Text {
                            text: "Instant sync on local clipboard copy (PC -> Phone)"
                            font: MD3Theme.bodySmall
                            color: MD3Theme.onSurfaceVariant
                        }
                    }

                    MD3Switch {
                        checked: controller.autoSync
                        onToggled: controller.autoSync = checked
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Local Polling Fallback Interval"
                            font: MD3Theme.bodyLarge
                            color: MD3Theme.onSurface
                            Layout.fillWidth: true
                        }
                        Text {
                            text: controller.pollInterval.toFixed(1) + " s"
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }
                    }

                    QQC.Slider {
                        Layout.fillWidth: true
                        from: 0.5
                        to: 5.0
                        stepSize: 0.5
                        value: controller.pollInterval
                        onMoved: controller.pollInterval = value
                    }
                }
            }
        }

        // Appearance & Accent Card
        MD3Card {
            Layout.fillWidth: true
            implicitHeight: appLayout.implicitHeight + 32
            variant: "filled"

            ColumnLayout {
                id: appLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "Appearance & Theming"
                    font: MD3Theme.titleMedium
                    color: MD3Theme.onSurface
                }

                // Theme Mode Row
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Theme Mode"
                        font: MD3Theme.bodyLarge
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    Row {
                        spacing: 6

                        MD3Button {
                            text: "System"
                            variant: controller.themeMode === 0 ? "filled" : "outlined"
                            onClicked: controller.themeMode = 0
                        }

                        MD3Button {
                            text: "Light"
                            variant: controller.themeMode === 1 ? "filled" : "outlined"
                            onClicked: controller.themeMode = 1
                        }

                        MD3Button {
                            text: "Dark"
                            variant: controller.themeMode === 2 ? "filled" : "outlined"
                            onClicked: controller.themeMode = 2
                        }
                    }
                }

                // Accent Presets
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Accent Color"
                        font: MD3Theme.bodyLarge
                        color: MD3Theme.onSurface
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8

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
                                width: pillRow.implicitWidth + 24
                                height: 36
                                radius: 18
                                color: controller.accentPreset === modelData.name ? MD3Theme.primary : "transparent"
                                border.color: controller.accentPreset === modelData.name ? "transparent" : MD3Theme.outlineVariant
                                border.width: 1

                                Behavior on color { ColorAnimation { duration: 180 } }

                                RowLayout {
                                    id: pillRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: modelData.color
                                        border.color: controller.accentPreset === modelData.name ? MD3Theme.onPrimary : "transparent"
                                        border.width: 1
                                    }

                                    Text {
                                        text: modelData.label
                                        font: MD3Theme.labelMedium
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
                            width: customPillRow.implicitWidth + 24
                            height: 36
                            radius: 18
                            color: controller.accentPreset === "custom" ? MD3Theme.primary : "transparent"
                            border.color: controller.accentPreset === "custom" ? "transparent" : MD3Theme.outlineVariant
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 180 } }

                            RowLayout {
                                id: customPillRow
                                anchors.centerIn: parent
                                spacing: 8

                                Rectangle {
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: controller.customColor
                                    border.color: controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.outlineVariant
                                    border.width: 1.5
                                }

                                Text {
                                    text: "Custom"
                                    font: MD3Theme.labelMedium
                                    color: controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.onSurface
                                }

                                MD3Icon {
                                    name: "palette"
                                    size: 14
                                    color: controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.onSurfaceVariant
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: controller.accentPreset = "custom"
                            }
                        }
                    }

                    // Interactive Custom Color Picker Panel
                    Rectangle {
                        Layout.fillWidth: true
                        radius: MD3Theme.cornerM
                        color: MD3Theme.surfaceContainer
                        border.color: MD3Theme.outlineVariant
                        border.width: 1
                        clip: true
                        visible: controller.accentPreset === "custom"

                        implicitHeight: pickerCol.implicitHeight + 32

                        ColumnLayout {
                            id: pickerCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 16

                            RowLayout {
                                Layout.fillWidth: true
                                MD3Icon {
                                    name: "palette"
                                    size: 20
                                    color: MD3Theme.primary
                                }
                                Text {
                                    text: "Custom Accent Tuner"
                                    font: MD3Theme.titleSmall
                                    color: MD3Theme.onSurface
                                    Layout.fillWidth: true
                                }
                            }

                            MD3ColorPicker {
                                Layout.fillWidth: true
                                selectedColor: controller.customColor
                                onColorSelected: (col) => {
                                    controller.setCustomColor(col)
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Clipboard Engine"
                        font: MD3Theme.bodyLarge
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }
                    MD3Badge {
                        text: controller.clipboardBackend
                        badgeColor: MD3Theme.surfaceContainerHigh
                        textColor: MD3Theme.onSurface
                    }
                }

                Item { Layout.preferredHeight: 4 }

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
}
