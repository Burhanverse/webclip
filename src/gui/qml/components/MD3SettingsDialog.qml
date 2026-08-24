import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import WebClip

QQC.Popup {
    id: root

    required property var controller

    modal: true
    focus: true
    closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside
    anchors.centerIn: QQC.Overlay.overlay
    padding: 0

    width: Math.min(QQC.Overlay.overlay ? QQC.Overlay.overlay.width - 32 : 440, 440)
    height: Math.min(QQC.Overlay.overlay ? QQC.Overlay.overlay.height - 48 : 600, 600)

    background: Rectangle {
        id: bgRect
        radius: 28
        color: MD3Theme.isDark ? "#211D26" : "#FFF7FB"
        border.color: MD3Theme.outlineVariant
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: 29
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.25)
            border.width: 1
            z: -1
        }
    }

    QQC.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Solid Dialog Header (hides scrolling items behind it)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: MD3Theme.isDark ? "#211D26" : "#FFF7FB"
                radius: 28

                // Square out bottom corners of header
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 28
                    color: parent.color
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 12

                    Text {
                        text: "Settings"
                        font: MD3Theme.titleMedium
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    // Circular Close Button
                    Rectangle {
                        width: 32
                        height: 32
                        radius: 16
                        color: closeArea.pressed ? MD3Theme.surfaceContainerHighest : (closeArea.containsMouse ? MD3Theme.surfaceContainerHigh : (MD3Theme.isDark ? "#2E2836" : "#F4E8EE"))

                        MD3Icon {
                            anchors.centerIn: parent
                            name: "close"
                            size: 15
                            color: MD3Theme.onSurface
                        }

                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.close()
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: MD3Theme.outlineVariant
                }
            }

            // Scrollable Settings Content (with bottom padding inside popup)
            Flickable {
                id: flickable
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 12
                contentWidth: width
                contentHeight: settingsCol.implicitHeight + 16
                clip: true
                pixelAligned: true
                flickDeceleration: 3000
                maximumFlickVelocity: 6000
                boundsBehavior: Flickable.DragAndOvershootBounds
                boundsMovement: Flickable.FollowBoundsBehavior

                MD3SmoothScroll {
                    target: flickable
                }

                // Sleek Material 3 Inset Scrollbar
                QQC.ScrollBar.vertical: QQC.ScrollBar {
                    id: vScrollBar
                    policy: QQC.ScrollBar.AsNeeded

                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: 2
                        color: vScrollBar.pressed
                            ? MD3Theme.primary
                            : (vScrollBar.hovered ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(0, 0, 0, 0.25))
                        opacity: vScrollBar.active ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    background: Item {}
                }

                ColumnLayout {
                    id: settingsCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 12
                    anchors.bottomMargin: 16
                    spacing: 16

                    // Section 1: Connection Settings
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Connection"
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: connCol.implicitHeight + 24
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            ColumnLayout {
                                id: connCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 14
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    MD3TextField {
                                        Layout.fillWidth: true
                                        label: "Phone IP / URL"
                                        placeholderText: "10.36.130.44"
                                        text: root.controller.host
                                        onTextChanged: root.controller.host = text
                                    }

                                    MD3TextField {
                                        Layout.preferredWidth: 80
                                        label: "Port"
                                        placeholderText: "8080"
                                        text: root.controller.port.toString()
                                        onTextChanged: {
                                            var p = parseInt(text)
                                            if (!isNaN(p)) root.controller.port = p
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    MD3TextField {
                                        Layout.fillWidth: true
                                        label: "Pairing Code"
                                        placeholderText: "4-digit code"
                                        text: root.controller.code
                                        onTextChanged: root.controller.code = text
                                    }

                                    MD3Button {
                                        Layout.preferredHeight: 44
                                        text: root.controller.connected ? "Disconnect" : (root.controller.connecting ? "Connecting..." : "Connect")
                                        variant: root.controller.connected ? "tonal" : "filled"
                                        iconName: root.controller.connected ? "close" : "sync"
                                        onClicked: root.controller.toggleConnection()
                                    }
                                }
                            }
                        }
                    }

                    // Section 2: Security & Protocols
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Security & Network"
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        // HTTPS Row Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: "Use HTTPS (Port 8081)"; font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: "Encrypt connection between PC and phone"; font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.useHttps
                                    onToggled: root.controller.useHttps = checked
                                }
                            }
                        }

                        // Self-Signed SSL Row Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: "Allow Self-Signed SSL"; font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: "Required for Gboard local certificates"; font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.insecure
                                    onToggled: root.controller.insecure = checked
                                }
                            }
                        }
                    }

                    // Section 3: Sync Behavior
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Sync Behavior"
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        // Auto-sync Row Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: "Automatic Two-Way Sync"; font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: "Instant sync on PC clipboard copy"; font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.autoSync
                                    onToggled: root.controller.autoSync = checked
                                }
                            }
                        }

                        // Material You Pill Slider Row Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 68
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                anchors.topMargin: 10
                                anchors.bottomMargin: 10
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: "Polling Interval: " + root.controller.pollInterval.toFixed(1) + "s"
                                        font: MD3Theme.bodySmall
                                        color: MD3Theme.onSurface
                                        Layout.fillWidth: true
                                    }
                                    MD3Icon {
                                        name: "sync"
                                        size: 14
                                        color: MD3Theme.onSurfaceVariant
                                    }
                                }

                                // Material You Filled Slider Track
                                Item {
                                    id: sliderContainer
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 18

                                    Rectangle {
                                        id: inactiveTrack
                                        anchors.fill: parent
                                        radius: 9
                                        color: MD3Theme.isDark ? "#3C3246" : "#F6D2E0"

                                        Rectangle {
                                            width: 4
                                            height: 4
                                            radius: 2
                                            anchors.right: parent.right
                                            anchors.rightMargin: 6
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: MD3Theme.primary
                                        }
                                    }

                                    Rectangle {
                                        id: activeTrack
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: Math.max(18, Math.min(parent.width, ((root.controller.pollInterval - 0.5) / (5.0 - 0.5)) * parent.width))
                                        radius: 9
                                        color: MD3Theme.primary

                                        Rectangle {
                                            width: 4
                                            height: 4
                                            radius: 2
                                            anchors.left: parent.left
                                            anchors.leftMargin: 6
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: "#FFFFFF"
                                        }
                                    }

                                    Rectangle {
                                        x: Math.max(0, Math.min(sliderContainer.width - width, activeTrack.width - width / 2))
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 4
                                        height: 22
                                        radius: 2
                                        color: MD3Theme.isDark ? "#FFFFFF" : MD3Theme.primary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor

                                        function updateVal(mouse) {
                                            var pct = Math.max(0.0, Math.min(1.0, mouse.x / sliderContainer.width))
                                            var rawVal = 0.5 + pct * (5.0 - 0.5)
                                            var stepped = Math.round(rawVal * 2) / 2
                                            root.controller.pollInterval = Math.max(0.5, Math.min(5.0, stepped))
                                        }

                                        onPressed: (mouse) => updateVal(mouse)
                                        onPositionChanged: (mouse) => {
                                            if (pressed) updateVal(mouse)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Section 4: Appearance & Theming
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Appearance"
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        // Theme Mode Row Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Text {
                                    text: "Theme Mode"
                                    font: MD3Theme.bodySmall
                                    color: MD3Theme.onSurface
                                    Layout.fillWidth: true
                                }

                                Row {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

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
                                            color: root.controller.themeMode === modelData.mode ? MD3Theme.primary : (MD3Theme.isDark ? "#3C3246" : "#F6D2E0")

                                            Text {
                                                id: modeText
                                                anchors.centerIn: parent
                                                text: modelData.label
                                                font: MD3Theme.labelSmall
                                                color: root.controller.themeMode === modelData.mode ? MD3Theme.onPrimary : MD3Theme.onSurface
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.controller.themeMode = modelData.mode
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Accent Color Palette Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: accentCol.implicitHeight + 24
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            ColumnLayout {
                                id: accentCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 14
                                spacing: 10

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
                                            width: pillRow.implicitWidth + 14
                                            height: 26
                                            radius: 13
                                            color: root.controller.accentPreset === modelData.name ? MD3Theme.primary : "transparent"
                                            border.color: root.controller.accentPreset === modelData.name ? "transparent" : (MD3Theme.isDark ? "#483C54" : "#E8CFD9")
                                            border.width: 1

                                            Behavior on color { ColorAnimation { duration: 150 } }

                                            RowLayout {
                                                id: pillRow
                                                anchors.centerIn: parent
                                                spacing: 5

                                                Rectangle {
                                                    width: 9
                                                    height: 9
                                                    radius: 4.5
                                                    color: modelData.color
                                                    border.color: root.controller.accentPreset === modelData.name ? MD3Theme.onPrimary : "transparent"
                                                    border.width: 1
                                                }

                                                Text {
                                                    text: modelData.label
                                                    font: MD3Theme.labelSmall
                                                    color: root.controller.accentPreset === modelData.name ? MD3Theme.onPrimary : MD3Theme.onSurface
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.controller.accentPreset = modelData.name
                                            }
                                        }
                                    }

                                    // Custom Color Pill
                                    Rectangle {
                                        width: customPillRow.implicitWidth + 14
                                        height: 26
                                        radius: 13
                                        color: root.controller.accentPreset === "custom" ? MD3Theme.primary : "transparent"
                                        border.color: root.controller.accentPreset === "custom" ? "transparent" : (MD3Theme.isDark ? "#483C54" : "#E8CFD9")
                                        border.width: 1

                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        RowLayout {
                                            id: customPillRow
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Rectangle {
                                                width: 9
                                                height: 9
                                                radius: 4.5
                                                color: root.controller.customColor
                                                border.color: root.controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.outlineVariant
                                                border.width: 1
                                            }

                                            Text {
                                                text: "Custom"
                                                font: MD3Theme.labelSmall
                                                color: root.controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.onSurface
                                            }

                                            MD3Icon {
                                                name: "palette"
                                                size: 12
                                                color: root.controller.accentPreset === "custom" ? MD3Theme.onPrimary : MD3Theme.onSurfaceVariant
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                root.controller.accentPreset = "custom"
                                                colorPickerDialog.openWithColor(root.controller.customColor)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Engine Badge Row Card
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 52
                            radius: 16
                            color: MD3Theme.isDark ? "#2A2432" : "#FFF0F6"
                            border.color: MD3Theme.isDark ? "#3A3245" : "#F7DFE8"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Text {
                                    text: "Clipboard Engine"
                                    font: MD3Theme.bodySmall
                                    color: MD3Theme.onSurface
                                    Layout.fillWidth: true
                                }
                                MD3Badge {
                                    text: root.controller.clipboardBackend
                                    badgeColor: MD3Theme.surfaceContainerHigh
                                    textColor: MD3Theme.onSurface
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4 }

                        MD3Button {
                            text: "Clear All History"
                            variant: "outlined"
                            iconName: "delete"
                            Layout.fillWidth: true
                            onClicked: root.controller.clipModel.clear()
                        }
                    }
                }
            }
        }
    }

    MD3ColorPickerDialog {
        id: colorPickerDialog
        onAccepted: (col) => {
            root.controller.customColor = col
            root.controller.accentPreset = "custom"
        }
    }
}
