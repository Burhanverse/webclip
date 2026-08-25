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
        color: MD3Theme.surfaceContainer

        Behavior on color { ColorAnimation { duration: 150 } }
    }

    QQC.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: 28
                color: MD3Theme.surfaceContainer

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 28
                    color: MD3Theme.surfaceContainer
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 12

                    Text {
                        text: I18n.tr("settings.title")
                        font: MD3Theme.titleMedium
                        color: MD3Theme.onSurface
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 16
                        color: closeArea.pressed ? MD3Theme.surfaceContainerHighest : (closeArea.containsMouse ? MD3Theme.surfaceContainerHigh : MD3Theme.surfaceContainerLow)

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
            }

            Flickable {
                id: flickable
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 12
                contentWidth: width
                contentHeight: settingsCol.implicitHeight + 16
                clip: true
                flickDeceleration: 1800
                maximumFlickVelocity: 4500
                boundsBehavior: Flickable.DragAndOvershootBounds
                boundsMovement: Flickable.FollowBoundsBehavior

                MD3SmoothScroll {
                    target: flickable
                }

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

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: I18n.tr("settings.connection.section_title")
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: connCol.implicitHeight + 24
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

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
                                        label: I18n.tr("settings.connection.host_label")
                                        placeholderText: "192.168.1.100"
                                        text: root.controller.host
                                        onTextChanged: root.controller.host = text
                                    }

                                    MD3TextField {
                                        Layout.preferredWidth: 80
                                        label: I18n.tr("settings.connection.port_label")
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
                                        label: I18n.tr("settings.connection.code_label")
                                        placeholderText: "4-digit code"
                                        text: root.controller.code
                                        onTextChanged: root.controller.code = text
                                    }

                                    MD3Button {
                                        Layout.preferredHeight: 44
                                        text: root.controller.connected
                                            ? I18n.tr("settings.connection.btn_disconnect")
                                            : (root.controller.connecting ? I18n.tr("settings.connection.btn_connecting") : I18n.tr("settings.connection.btn_connect"))
                                        variant: root.controller.connected ? "tonal" : "filled"
                                        iconName: root.controller.connected ? "close" : "sync"
                                        onClicked: root.controller.toggleConnection()
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: I18n.tr("settings.security.section_title")
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: I18n.tr("settings.security.https_title"); font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: I18n.tr("settings.security.https_subtitle"); font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.useHttps
                                    onToggled: root.controller.useHttps = checked
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: I18n.tr("settings.security.ssl_title"); font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: I18n.tr("settings.security.ssl_subtitle"); font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.insecure
                                    onToggled: root.controller.insecure = checked
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: I18n.tr("settings.sync.section_title")
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: I18n.tr("settings.sync.autosync_title"); font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: I18n.tr("settings.sync.autosync_subtitle"); font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.autoSync
                                    onToggled: root.controller.autoSync = checked
                                }
                            }
                        }

                        Rectangle {
                            id: sliderCard
                            Layout.fillWidth: true
                            implicitHeight: sliderCol.implicitHeight + 24
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            ColumnLayout {
                                id: sliderCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 14
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: I18n.tr("settings.sync.polling_title") + ": " + root.controller.pollInterval.toFixed(1) + "s"
                                        font: MD3Theme.bodySmall
                                        color: MD3Theme.onSurface
                                        Layout.fillWidth: true
                                    }

                                    MD3IconButton {
                                        iconName: "sync"
                                        size: 24
                                        iconColor: root.controller.pollInterval === 1.0 ? MD3Theme.onSurfaceVariant : MD3Theme.primary
                                        opacity: root.controller.pollInterval === 1.0 ? 0.4 : 1.0
                                        onClicked: root.controller.pollInterval = 1.0
                                    }
                                }

                                Item {
                                    id: sliderContainer
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32

                                    readonly property real minVal: 0.5
                                    readonly property real maxVal: 5.0
                                    readonly property real currentVal: root.controller.pollInterval
                                    readonly property real normalizedPos: Math.max(0.0, Math.min(1.0, (currentVal - minVal) / (maxVal - minVal)))

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 16
                                        radius: 8
                                        color: MD3Theme.surfaceContainerHighest
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 16
                                        radius: 8
                                        width: Math.max(16, Math.min(sliderContainer.width, sliderContainer.normalizedPos * sliderContainer.width))
                                        color: MD3Theme.primary

                                        Behavior on width {
                                            enabled: !sliderArea.pressed
                                            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    Rectangle {
                                        x: Math.max(0, Math.min(sliderContainer.width - width, (sliderContainer.normalizedPos * sliderContainer.width) - (width / 2)))
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 6
                                        height: 26
                                        radius: 3
                                        color: sliderArea.pressed ? MD3Theme.onPrimary : (MD3Theme.isDark ? "#FFFFFF" : MD3Theme.onSurface)

                                        Behavior on color { ColorAnimation { duration: 120 } }
                                    }

                                    MouseArea {
                                        id: sliderArea
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor

                                        function updateVal(mouse) {
                                            var pct = Math.max(0.0, Math.min(1.0, mouse.x / sliderContainer.width))
                                            var rawVal = sliderContainer.minVal + pct * (sliderContainer.maxVal - sliderContainer.minVal)
                                            var stepped = Math.round(rawVal * 2) / 2
                                            root.controller.pollInterval = Math.max(sliderContainer.minVal, Math.min(sliderContainer.maxVal, stepped))
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

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: I18n.tr("settings.appearance.section_title")
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Text {
                                    text: I18n.tr("settings.appearance.theme_mode")
                                    font: MD3Theme.bodySmall
                                    color: MD3Theme.onSurface
                                    Layout.fillWidth: true
                                }

                                Row {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                                    Repeater {
                                        model: [
                                            { label: I18n.tr("settings.appearance.mode_system"), mode: 0 },
                                            { label: I18n.tr("settings.appearance.mode_light"), mode: 1 },
                                            { label: I18n.tr("settings.appearance.mode_dark"), mode: 2 },
                                            { label: I18n.tr("settings.appearance.mode_pitch_black"), mode: 3 }
                                        ]

                                        Rectangle {
                                            width: modeText.implicitWidth + 16
                                            height: 26
                                            radius: 13
                                            color: root.controller.themeMode === modelData.mode ? MD3Theme.primary : MD3Theme.surfaceContainerHigh

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

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: accentCol.implicitHeight + 24
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            ColumnLayout {
                                id: accentCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 14
                                spacing: 10

                                Text {
                                    text: I18n.tr("settings.appearance.accent_color")
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
                                            color: root.controller.accentPreset === modelData.name ? MD3Theme.primary : MD3Theme.surfaceContainerHigh

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

                                    Rectangle {
                                        width: customPillRow.implicitWidth + 14
                                        height: 26
                                        radius: 13
                                        color: root.controller.accentPreset === "custom" ? MD3Theme.primary : MD3Theme.surfaceContainerHigh

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
                                            }

                                            Text {
                                                text: I18n.tr("settings.appearance.accent_custom")
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

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: I18n.tr("settings.appearance.thanos_snap_title"); font: MD3Theme.bodySmall; color: MD3Theme.onSurface }
                                    Text { text: I18n.tr("settings.appearance.thanos_snap_subtitle"); font: MD3Theme.labelSmall; color: MD3Theme.onSurfaceVariant }
                                }

                                MD3Switch {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    checked: root.controller.thanosSnapEnabled
                                    onToggled: root.controller.thanosSnapEnabled = checked
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 2 }

                        MD3Button {
                            text: I18n.tr("settings.appearance.btn_clear_history")
                            variant: "outlined"
                            iconName: "delete"
                            Layout.fillWidth: true
                            onClicked: root.controller.clipModel.clear()
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: I18n.tr("settings.about.section_title")
                            font: MD3Theme.labelLarge
                            color: MD3Theme.primary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: aboutCol.implicitHeight + 24
                            radius: 16
                            color: MD3Theme.surfaceContainerLow

                            ColumnLayout {
                                id: aboutCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 14
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Rectangle {
                                        width: 36
                                        height: 36
                                        radius: 18
                                        color: MD3Theme.primaryContainer

                                        MD3Icon {
                                            anchors.centerIn: parent
                                            name: "phone"
                                            size: 18
                                            color: MD3Theme.onPrimaryContainer
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            text: I18n.tr("settings.about.app_name")
                                            font: MD3Theme.titleSmall
                                            color: MD3Theme.onSurface
                                        }

                                        Text {
                                            text: I18n.tr("settings.about.app_subtitle")
                                            font: MD3Theme.labelSmall
                                            color: MD3Theme.onSurfaceVariant
                                        }
                                    }

                                    MD3Badge {
                                        text: "v" + root.controller.appVersion
                                        badgeColor: MD3Theme.primary
                                        textColor: MD3Theme.onPrimary
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: I18n.tr("settings.about.qt_runtime")
                                        font: MD3Theme.bodySmall
                                        color: MD3Theme.onSurface
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: "Qt " + root.controller.qtVersion
                                        font: MD3Theme.labelSmall
                                        color: MD3Theme.onSurfaceVariant
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: I18n.tr("settings.about.engine")
                                        font: MD3Theme.bodySmall
                                        color: MD3Theme.onSurface
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: root.controller.clipboardBackend
                                        font: MD3Theme.labelSmall
                                        color: MD3Theme.primary
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: I18n.tr("settings.about.license")
                                        font: MD3Theme.bodySmall
                                        color: MD3Theme.onSurface
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: I18n.tr("settings.about.license_val")
                                        font: MD3Theme.labelSmall
                                        color: MD3Theme.onSurfaceVariant
                                    }
                                }

                                MD3Button {
                                    text: I18n.tr("settings.about.btn_github")
                                    variant: "tonal"
                                    iconName: "link"
                                    Layout.fillWidth: true
                                    onClicked: root.controller.openUrl("https://github.com/burhanverse/webclip")
                                }
                            }
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
