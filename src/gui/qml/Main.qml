import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import WebClip

Window {
    id: window
    width: 480
    height: 720
    minimumWidth: 380
    minimumHeight: 520
    visible: true
    title: I18n.tr("app.title")
    color: MD3Theme.surface

    Behavior on color { ColorAnimation { duration: 200 } }

    WebClipController {
        id: controller

        Component.onCompleted: {
            MD3Theme.customColor = controller.customColor
            MD3Theme.themeMode = controller.themeMode
            MD3Theme.accentPreset = controller.accentPreset
        }

        onShowToast: (message, isError) => {
            toast.show(message, isError)
        }

        onThemeModeChanged: {
            MD3Theme.themeMode = controller.themeMode
        }

        onAccentPresetChanged: {
            MD3Theme.accentPreset = controller.accentPreset
        }

        onCustomColorChanged: {
            MD3Theme.customColor = controller.customColor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Modern Phone-style Header App Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: MD3Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 12

                // Avatar / Phone Icon with Online Status Ring
                Item {
                    width: 38
                    height: 38

                    Rectangle {
                        anchors.fill: parent
                        radius: 19
                        color: MD3Theme.primaryContainer

                        MD3Icon {
                            anchors.centerIn: parent
                            name: "phone"
                            color: MD3Theme.onPrimaryContainer
                            size: 20
                        }
                    }

                    // Online green dot / orange / red
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        color: controller.connected ? "#4CAF50" : (controller.connecting ? "#FF9800" : "#F44336")
                        border.color: MD3Theme.surface
                        border.width: 1.5

                        SequentialAnimation on opacity {
                            running: controller.connecting
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.3; duration: 500 }
                            NumberAnimation { from: 0.3; to: 1.0; duration: 500 }
                        }
                    }
                }

                // Title & Subtitle (Chat Contact Header style)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: I18n.tr("app.header_title")
                        font: MD3Theme.titleSmall
                        color: MD3Theme.onSurface
                        elide: Text.ElideRight
                    }

                    Text {
                        text: controller.connected
                            ? I18n.tr("app.status_connected")
                            : (controller.connecting ? I18n.tr("app.status_connecting") : I18n.tr("app.status_offline"))
                        font: MD3Theme.labelSmall
                        color: controller.connected ? "#4CAF50" : MD3Theme.onSurfaceVariant
                        elide: Text.ElideRight

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: controller.toggleConnection()
                        }
                    }
                }

                // Right Action Icons Grouped Neatly
                Row {
                    spacing: 4
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    MD3IconButton {
                        iconName: controller.connected ? "sync" : "link_off"
                        iconColor: controller.connected ? MD3Theme.primary : MD3Theme.onSurfaceVariant
                        size: 34
                        onClicked: controller.toggleConnection()
                    }

                    MD3IconButton {
                        iconName: MD3Theme.isDark ? "sun" : "moon"
                        iconColor: MD3Theme.onSurfaceVariant
                        size: 34
                        onClicked: {
                            var nextMode = MD3Theme.isDark ? 1 : 2
                            controller.themeMode = nextMode
                        }
                    }

                    MD3IconButton {
                        iconName: "settings"
                        iconColor: MD3Theme.onSurfaceVariant
                        size: 34
                        onClicked: settingsDialog.open()
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

        // Main Chat Clips Feed
        ClipsPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: controller
        }
    }

    // Modern Settings Popup Dialog
    MD3SettingsDialog {
        id: settingsDialog
        controller: controller
    }

    // Floating MD3 Toast Notification
    Rectangle {
        id: toast
        property bool isError: false
        property alias messageText: toastText.text

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 76
        height: 38
        width: Math.min(window.width - 32, toastText.implicitWidth + 32)
        radius: 19
        color: isError ? MD3Theme.errorContainer : MD3Theme.surfaceContainerHighest
        opacity: 0
        visible: opacity > 0
        z: 99

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Text {
            id: toastText
            anchors.centerIn: parent
            font: MD3Theme.bodySmall
            color: toast.isError ? MD3Theme.onErrorContainer : MD3Theme.onSurface
            elide: Text.ElideRight
        }

        Timer {
            id: toastTimer
            interval: 2200
            onTriggered: toast.opacity = 0
        }

        function show(msg, err) {
            toast.messageText = msg
            toast.isError = err
            toast.opacity = 1
            toastTimer.restart()
        }
    }
}
