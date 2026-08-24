import QtQuick
import QtQuick.Layouts
import WebClip

Item {
    id: root

    required property var controller

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Top Connection Status Card
        MD3Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            variant: "filled"
            color: controller.connected ? MD3Theme.colorWithAlpha(MD3Theme.primaryContainer, 0.45) : MD3Theme.surfaceContainerHigh

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: controller.connected ? "#4CAF50" : (controller.connecting ? "#FF9800" : "#F44336")

                    SequentialAnimation on opacity {
                        running: controller.connecting
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.3; duration: 600 }
                        NumberAnimation { from: 0.3; to: 1.0; duration: 600 }
                    }
                }

                Text {
                    text: controller.statusMessage
                    font: MD3Theme.titleSmall
                    color: MD3Theme.onSurface
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                MD3Button {
                    text: controller.connected ? "Disconnect" : (controller.connecting ? "Connecting..." : "Connect")
                    variant: controller.connected ? "outlined" : "filled"
                    iconName: controller.connected ? "close" : "sync"
                    onClicked: controller.toggleConnection()
                }
            }
        }

        // Clip Feed List / Empty State
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty State with smooth fade animation
            ColumnLayout {
                anchors.centerIn: parent
                visible: controller.clipModel.count === 0
                opacity: visible ? 1.0 : 0.0
                spacing: 14

                Behavior on opacity { NumberAnimation { duration: 250 } }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 72
                    height: 72
                    radius: 36
                    color: MD3Theme.primaryContainer

                    MD3Icon {
                        anchors.centerIn: parent
                        name: "clips"
                        color: MD3Theme.onPrimaryContainer
                        size: 32
                    }
                }

                Text {
                    text: "No clips synced yet"
                    font: MD3Theme.titleMedium
                    color: MD3Theme.onSurface
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "Copy text on your phone or PC to automatically sync"
                    font: MD3Theme.bodyMedium
                    color: MD3Theme.onSurfaceVariant
                    Layout.alignment: Qt.AlignHCenter
                }

                Item { Layout.preferredHeight: 6 }

                MD3Button {
                    text: "Push Current Clipboard"
                    variant: "tonal"
                    iconName: "send"
                    Layout.alignment: Qt.AlignHCenter
                    enabled: controller.connected
                    onClicked: controller.pushCurrentClipboard()
                }
            }

            // Smooth Kinetic ListView
            ListView {
                id: listView
                anchors.fill: parent
                visible: controller.clipModel.count > 0
                clip: true
                spacing: 12
                model: controller.clipModel

                flickDeceleration: 1800
                maximumFlickVelocity: 3000
                boundsBehavior: Flickable.DragAndOvershootBounds
                boundsMovement: Flickable.FollowBoundsBehavior

                // Smooth animated transitions for adding and removing clips
                add: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "scale"; from: 0.92; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
                    }
                }

                remove: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; to: 0; duration: 180; easing.type: Easing.InCubic }
                        NumberAnimation { property: "scale"; to: 0.85; duration: 180; easing.type: Easing.InCubic }
                    }
                }

                displaced: Transition {
                    NumberAnimation { property: "y"; duration: 220; easing.type: Easing.OutCubic }
                }

                // Smooth wheel scrolling
                WheelHandler {
                    target: listView
                    onWheel: (event) => {
                        var delta = event.angleDelta.y
                        listView.flick(0, delta * 12)
                    }
                }

                delegate: MD3Card {
                    width: listView.width
                    implicitHeight: cardLayout.implicitHeight + 24
                    variant: "outlined"

                    Behavior on color { ColorAnimation { duration: 150 } }

                    ColumnLayout {
                        id: cardLayout
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 10

                        // Card Header
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            MD3Badge {
                                text: model.source === "phone" ? "Phone" : (model.source === "local" ? "PC" : "Manual")
                                iconName: model.source === "phone" ? "phone" : "laptop"
                                badgeColor: model.source === "phone" ? MD3Theme.primaryContainer : MD3Theme.secondaryContainer
                                textColor: model.source === "phone" ? MD3Theme.onPrimaryContainer : MD3Theme.onSecondaryContainer
                            }

                            Text {
                                text: model.timeFormatted
                                font: MD3Theme.labelSmall
                                color: MD3Theme.onSurfaceVariant
                            }

                            Text {
                                text: "• " + model.charCount + " chars"
                                font: MD3Theme.labelSmall
                                color: MD3Theme.onSurfaceVariant
                                Layout.fillWidth: true
                            }

                            MD3IconButton {
                                iconName: "copy"
                                iconColor: MD3Theme.primary
                                size: 36
                                onClicked: controller.copyToClipboard(model.text)
                            }

                            MD3IconButton {
                                visible: controller.connected && model.source !== "phone"
                                iconName: "send"
                                iconColor: MD3Theme.secondary
                                size: 36
                                onClicked: controller.pushClipboard(model.text)
                            }

                            MD3IconButton {
                                iconName: "delete"
                                iconColor: MD3Theme.outline
                                size: 36
                                onClicked: controller.clipModel.removeClip(index)
                            }
                        }

                        // Clip Content Area
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: clipText.implicitHeight + 16
                            radius: MD3Theme.cornerS
                            color: MD3Theme.surfaceContainerLow

                            TextEdit {
                                id: clipText
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                text: model.text
                                font: MD3Theme.bodyMedium
                                color: MD3Theme.onSurface
                                wrapMode: Text.WrapAnywhere
                                readOnly: true
                                selectByMouse: true
                                selectionColor: MD3Theme.primaryContainer
                                selectedTextColor: MD3Theme.onPrimaryContainer
                            }
                        }
                    }
                }
            }
        }
    }
}
