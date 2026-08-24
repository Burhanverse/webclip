import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import WebClip

Item {
    id: root

    required property var controller

    FileDialog {
        id: openImageDialog
        title: "Select Image to Send"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.webp *.bmp *.gif)", "All files (*)"]
        onAccepted: {
            if (selectedFile) {
                controller.pushImage(selectedFile.toString())
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0) {
                for (var i = 0; i < drop.urls.length; ++i) {
                    controller.pushImage(drop.urls[i].toString())
                }
            } else if (drop.hasText && drop.text.length > 0) {
                pushInput.text = drop.text
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Header Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Push to Phone"
                font: MD3Theme.titleSmall
                font.weight: Font.SemiBold
                color: MD3Theme.onSurface
            }

            Text {
                text: "• Send text or image to your phone's Gboard clipboard"
                font: MD3Theme.bodySmall
                color: MD3Theme.onSurfaceVariant
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // Textarea Card
        MD3Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            variant: "outlined"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Flickable {
                    id: flickable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: pushInput.implicitHeight
                    clip: true

                    MD3SmoothScroll {
                        target: flickable
                    }

                    QQC.ScrollBar.vertical: QQC.ScrollBar {
                        policy: QQC.ScrollBar.AsNeeded
                    }

                    TextEdit {
                        id: pushInput
                        width: flickable.width
                        font: MD3Theme.bodyMedium
                        color: MD3Theme.onSurface
                        wrapMode: Text.WrapAnywhere
                        selectByMouse: true
                        selectionColor: MD3Theme.primaryContainer
                        selectedTextColor: MD3Theme.onPrimaryContainer

                        Text {
                            anchors.fill: parent
                            visible: !pushInput.text && !pushInput.activeFocus
                            text: "Type or paste text here to send, or attach an image below..."
                            font: pushInput.font
                            color: MD3Theme.onSurfaceVariant
                        }
                    }
                }

                // Stats & Clear Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: pushInput.text.length + " chars • " + (pushInput.text.trim() === "" ? 0 : pushInput.text.trim().split(/\s+/).length) + " words"
                        font: MD3Theme.labelSmall
                        color: MD3Theme.onSurfaceVariant
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "Clear"
                        font: MD3Theme.labelSmall
                        font.weight: Font.Medium
                        color: pushInput.text.length > 0 ? MD3Theme.primary : MD3Theme.outline
                        visible: pushInput.text.length > 0

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pushInput.text = ""
                        }
                    }
                }
            }
        }

        // Bottom Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MD3Button {
                text: "Paste Clipboard"
                variant: "outlined"
                iconName: "paste"
                onClicked: {
                    if (!controller.pushCurrentClipboard()) {
                        pushInput.paste()
                    }
                }
            }

            MD3Button {
                text: "Attach Image"
                variant: "outlined"
                iconName: "image"
                onClicked: openImageDialog.open()
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
