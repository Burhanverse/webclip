import QtQuick
import QtQuick.Layouts
import WebClip

Item {
    id: root

    required property var controller

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            spacing: 4
            Text {
                text: "Push to Phone"
                font: MD3Theme.headlineSmall
                color: MD3Theme.onSurface
            }

            Text {
                text: "Send text directly to your phone's Gboard clipboard."
                font: MD3Theme.bodyMedium
                color: MD3Theme.onSurfaceVariant
            }
        }

        // Textarea Card
        MD3Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            variant: "outlined"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Flickable {
                    id: flickable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: pushInput.implicitHeight
                    clip: true

                    TextEdit {
                        id: pushInput
                        width: flickable.width
                        font: MD3Theme.bodyLarge
                        color: MD3Theme.onSurface
                        wrapMode: Text.WrapAnywhere
                        selectByMouse: true
                        selectionColor: MD3Theme.primaryContainer
                        selectedTextColor: MD3Theme.onPrimaryContainer

                        Text {
                            anchors.fill: parent
                            visible: !pushInput.text && !pushInput.activeFocus
                            text: "Type or paste text here to send..."
                            font: pushInput.font
                            color: MD3Theme.onSurfaceVariant
                        }
                    }
                }

                // Stats & Clear Row
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: pushInput.text.length + " characters • " + (pushInput.text.trim() === "" ? 0 : pushInput.text.trim().split(/\s+/).length) + " words"
                        font: MD3Theme.bodySmall
                        color: MD3Theme.onSurfaceVariant
                        Layout.fillWidth: true
                    }

                    MD3Button {
                        text: "Clear"
                        variant: "text"
                        enabled: pushInput.text.length > 0
                        onClicked: pushInput.text = ""
                    }
                }
            }
        }

        // Bottom Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            MD3Button {
                text: "Paste Clipboard"
                variant: "outlined"
                iconName: "paste"
                onClicked: pushInput.paste()
            }

            Item { Layout.fillWidth: true }

            MD3Button {
                text: "Send to Phone"
                variant: "filled"
                iconName: "send"
                enabled: pushInput.text.trim().length > 0 && controller.connected
                onClicked: {
                    if (controller.pushClipboard(pushInput.text)) {
                        pushInput.text = ""
                    }
                }
            }
        }
    }
}
