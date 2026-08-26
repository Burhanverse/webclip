import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import WebClip

Item {
    id: root

    required property var controller

    property string fullPreviewUrl: ""
    property bool fullPreviewVisible: false

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

    FileDialog {
        id: saveImageDialog
        title: "Save Image"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG Image (*.png)", "JPEG Image (*.jpg)", "All files (*)"]
        defaultSuffix: "png"
        property int targetClipIndex: -1
        onAccepted: {
            if (selectedFile && targetClipIndex >= 0) {
                controller.saveImage(targetClipIndex, selectedFile.toString())
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0) {
                for (var i = 0; i < drop.urls.length; ++i) {
                    var u = drop.urls[i].toString()
                    controller.pushImage(u)
                }
            } else if (drop.hasText && drop.text.length > 0) {
                controller.pushClipboard(drop.text)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                visible: controller.clipModel.count === 0
                opacity: visible ? 1.0 : 0.0
                spacing: 12

                Behavior on opacity { NumberAnimation { duration: 200 } }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 56
                    height: 56
                    radius: 28
                    color: MD3Theme.primaryContainer

                    MD3Icon {
                        anchors.centerIn: parent
                        name: "phone"
                        color: MD3Theme.onPrimaryContainer
                        size: 26
                    }
                }

                Text {
                    text: I18n.tr("chat.empty_title")
                    font: MD3Theme.titleSmall
                    color: MD3Theme.onSurface
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: I18n.tr("chat.empty_subtitle")
                    font: MD3Theme.bodySmall
                    color: MD3Theme.onSurfaceVariant
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            ClipListItem {
                id: clipsList
                anchors.fill: parent
                visible: controller.clipModel.count > 0
                controller: root.controller

                onSaveImageRequested: (index) => {
                    saveImageDialog.targetClipIndex = index
                    saveImageDialog.open()
                }

                onFullPreviewRequested: (imageUrl) => {
                    root.fullPreviewUrl = imageUrl
                    root.fullPreviewVisible = true
                }
            }
        }

        Rectangle {
            id: chatInputDock
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: MD3Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: 10

                Rectangle {
                    id: attachBtn
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 22
                    color: attachArea.pressed ? MD3Theme.surfaceContainerHighest : (attachArea.containsMouse ? MD3Theme.surfaceContainerHigh : "transparent")

                    MD3Icon {
                        anchors.centerIn: parent
                        name: "image"
                        size: 22
                        color: MD3Theme.primary
                    }

                    MouseArea {
                        id: attachArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: openImageDialog.open()
                    }
                }

                Rectangle {
                    id: inputPill
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 24
                    color: MD3Theme.surfaceContainerHigh
                    border.color: msgInput.activeFocus ? MD3Theme.primary : "transparent"
                    border.width: 1.5

                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    Rectangle {
                        id: pasteBtn
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36
                        height: 36
                        radius: 18
                        color: pasteArea.pressed ? MD3Theme.surfaceContainerHighest : (pasteArea.containsMouse ? MD3Theme.surfaceContainerHigh : "transparent")

                        MD3Icon {
                            anchors.centerIn: parent
                            name: "paste"
                            size: 18
                            color: MD3Theme.onSurfaceVariant
                        }

                        MouseArea {
                            id: pasteArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (controller.pushCurrentClipboard()) {
                                    msgInput.text = ""
                                } else {
                                    msgInput.paste()
                                }
                            }
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: pasteBtn.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !msgInput.text && !msgInput.activeFocus
                        text: I18n.tr("chat.message_placeholder")
                        font: MD3Theme.bodyLarge
                        color: MD3Theme.onSurfaceVariant
                        elide: Text.ElideRight
                    }

                    TextInput {
                        id: msgInput
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: pasteBtn.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        font: MD3Theme.bodyLarge
                        color: MD3Theme.onSurface
                        selectByMouse: true
                        selectionColor: MD3Theme.primaryContainer
                        selectedTextColor: MD3Theme.onPrimaryContainer
                        clip: true

                        Keys.onReturnPressed: (event) => {
                            sendClip()
                            event.accepted = true
                        }

                        Keys.onEnterPressed: (event) => {
                            sendClip()
                            event.accepted = true
                        }
                    }
                }

                Rectangle {
                    id: sendFab
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    color: msgInput.text.trim().length > 0 && controller.connected
                        ? MD3Theme.primary
                        : (MD3Theme.isDark ? "#2A2533" : "#F2ECF4")

                    Behavior on color { ColorAnimation { duration: 150 } }

                    MD3Icon {
                        anchors.centerIn: parent
                        name: "send"
                        color: msgInput.text.trim().length > 0 && controller.connected
                            ? MD3Theme.onPrimary
                            : MD3Theme.onSurfaceVariant
                        size: 20
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: msgInput.text.trim().length > 0 && controller.connected
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: sendClip()
                    }
                }
            }
        }
    }

    Rectangle {
        id: imageModalOverlay
        anchors.fill: parent
        visible: root.fullPreviewVisible
        color: Qt.rgba(0, 0, 0, 0.85)
        z: 999

        opacity: visible ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 180 } }

        MouseArea {
            anchors.fill: parent
            onClicked: root.fullPreviewVisible = false
        }

        Item {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 800)
            height: Math.min(parent.height - 80, 600)

            Image {
                anchors.fill: parent
                source: root.fullPreviewUrl
                fillMode: Image.PreserveAspectFit
                mipmap: true
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 16
            width: 40
            height: 40
            radius: 20
            color: closeMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.15)

            MD3Icon {
                anchors.centerIn: parent
                name: "close"
                size: 22
                color: "#FFFFFF"
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.fullPreviewVisible = false
            }
        }
    }

    function sendClip() {
        var txt = msgInput.text.trim()
        if (txt.length > 0 && controller.connected) {
            if (controller.pushClipboard(txt)) {
                msgInput.text = ""
            }
        }
    }
}
