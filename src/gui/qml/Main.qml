import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import WebClip

Window {
    id: window
    width: 680
    height: 780
    minimumWidth: 440
    minimumHeight: 560
    visible: true
    title: "WebClip — Gboard Sync"
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

        // MD3 Top App Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: MD3Theme.surfaceContainer
            Behavior on color { ColorAnimation { duration: 200 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "WebClip"
                    font: MD3Theme.headlineSmall
                    color: MD3Theme.onSurface
                    Layout.fillWidth: true
                }

                // Sleek, minimal status badge (no duplicate disconnect button)
                Rectangle {
                    radius: 14
                    color: controller.connected ? MD3Theme.colorWithAlpha(MD3Theme.primaryContainer, 0.6) : MD3Theme.surfaceContainerHigh
                    implicitHeight: 28
                    implicitWidth: statusRow.implicitWidth + 20

                    Behavior on color { ColorAnimation { duration: 200 } }

                    RowLayout {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 6

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: controller.connected ? "#4CAF50" : (controller.connecting ? "#FF9800" : "#F44336")
                        }

                        Text {
                            text: controller.connected ? "Connected" : (controller.connecting ? "Connecting..." : "Offline")
                            font: MD3Theme.labelSmall
                            color: controller.connected ? MD3Theme.onPrimaryContainer : MD3Theme.onSurfaceVariant
                        }
                    }
                }

                // Theme switch icon button
                MD3IconButton {
                    iconName: MD3Theme.isDark ? "sun" : "moon"
                    iconColor: MD3Theme.onSurfaceVariant
                    onClicked: {
                        var nextMode = MD3Theme.isDark ? 1 : 2
                        controller.themeMode = nextMode
                    }
                }
            }
        }

        // Main Page Content Area with Smooth Transitions
        Item {
            id: contentContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            StackLayout {
                id: stackLayout
                anchors.fill: parent
                currentIndex: navBar.currentIndex

                ClipsPage {
                    controller: controller
                }

                PushPage {
                    controller: controller
                }

                SettingsPage {
                    controller: controller
                }
            }
        }

        // MD3 Navigation Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            color: MD3Theme.surfaceContainer
            Behavior on color { ColorAnimation { duration: 200 } }

            RowLayout {
                id: navBar
                property int currentIndex: 0
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: [
                        { name: "Live Clips", iconName: "clips" },
                        { name: "Push Text", iconName: "send" },
                        { name: "Settings", iconName: "settings" }
                    ]

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: parent.height

                        readonly property bool isSelected: navBar.currentIndex === index

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 4

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 64
                                height: 32
                                radius: 16
                                color: isSelected ? MD3Theme.secondaryContainer : "transparent"
                                Behavior on color { ColorAnimation { duration: 180 } }

                                MD3Icon {
                                    anchors.centerIn: parent
                                    name: modelData.iconName
                                    color: isSelected ? MD3Theme.onSecondaryContainer : MD3Theme.onSurfaceVariant
                                    size: 20
                                }
                            }

                            Text {
                                text: modelData.name
                                font: isSelected ? MD3Theme.labelMedium : MD3Theme.labelSmall
                                color: isSelected ? MD3Theme.onSurface : MD3Theme.onSurfaceVariant
                                Layout.alignment: Qt.AlignHCenter
                                Behavior on color { ColorAnimation { duration: 180 } }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: navBar.currentIndex = index
                        }
                    }
                }
            }
        }
    }

    // Floating MD3 Toast Notification
    Rectangle {
        id: toast
        property bool isError: false
        property alias messageText: toastText.text

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 90
        height: 48
        width: Math.min(window.width - 32, toastText.implicitWidth + 32)
        radius: MD3Theme.cornerXS
        color: isError ? MD3Theme.errorContainer : MD3Theme.surfaceContainerHighest
        opacity: 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        Text {
            id: toastText
            anchors.centerIn: parent
            font: MD3Theme.bodyMedium
            color: toast.isError ? MD3Theme.onErrorContainer : MD3Theme.onSurface
            elide: Text.ElideRight
        }

        Timer {
            id: toastTimer
            interval: 3000
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
