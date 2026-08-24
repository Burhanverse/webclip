import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import WebClip

Item {
    id: root

    required property var controller

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Chat Feed Stream
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty State
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

            // Chat Feed ListView
            ListView {
                id: listView
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 12
                anchors.bottomMargin: 12
                visible: controller.clipModel.count > 0
                clip: true
                spacing: 10
                model: controller.clipModel

                pixelAligned: true
                flickDeceleration: 3000
                maximumFlickVelocity: 6000
                boundsBehavior: Flickable.DragAndOvershootBounds
                boundsMovement: Flickable.FollowBoundsBehavior

                add: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
                    }
                }

                remove: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; to: 0; duration: 160; easing.type: Easing.InCubic }
                        NumberAnimation { property: "scale"; to: 0.9; duration: 160; easing.type: Easing.InCubic }
                    }
                }

                displaced: Transition {
                    NumberAnimation { property: "y"; duration: 200; easing.type: Easing.OutCubic }
                }

                MD3SmoothScroll {
                    target: listView
                }

                QQC.ScrollBar.vertical: QQC.ScrollBar {
                    policy: QQC.ScrollBar.AsNeeded
                }

                onCountChanged: Qt.callLater(listView.positionViewAtEnd)
                Component.onCompleted: Qt.callLater(listView.positionViewAtEnd)

                delegate: Item {
                    id: delegateItem
                    width: listView.width
                    implicitHeight: bubbleCol.implicitHeight + 6

                    readonly property bool isFromPhone: model.source === "phone"
                    property bool expanded: false
                    readonly property bool isLong: (model.charCount > 350 || (model.text && model.text.split('\n').length > 5))

                    HoverHandler {
                        id: hoverHandler
                    }

                    ColumnLayout {
                        id: bubbleCol
                        width: Math.min(listView.width * 0.82, Math.max(190, bubbleContent.implicitWidth + 28))
                        anchors.left: isFromPhone ? parent.left : undefined
                        anchors.right: !isFromPhone ? parent.right : undefined
                        spacing: 3

                        // Rounded Chat Bubble
                        Rectangle {
                            id: bubbleCard
                            Layout.fillWidth: true
                            implicitHeight: isLong && !delegateItem.expanded
                                ? Math.min(120, bubbleContent.implicitHeight + 20)
                                : bubbleContent.implicitHeight + 20
                            radius: 18
                            color: isFromPhone
                                ? (MD3Theme.isDark ? "#2C2834" : "#EAE6ED")
                                : (MD3Theme.isDark ? "#483857" : "#EADDFF")
                            clip: true

                            Behavior on implicitHeight { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            // Phone chat bubble tail (square corner on anchor side)
                            Rectangle {
                                width: 12
                                height: 12
                                anchors.bottom: parent.bottom
                                anchors.left: isFromPhone ? parent.left : undefined
                                anchors.right: !isFromPhone ? parent.right : undefined
                                color: parent.color
                            }

                            TextEdit {
                                id: bubbleContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 10
                                text: model.text
                                font: MD3Theme.bodyMedium
                                color: isFromPhone
                                    ? (MD3Theme.isDark ? "#E6E1E5" : "#1C1B1F")
                                    : (MD3Theme.isDark ? "#F5EEFA" : "#21005D")
                                wrapMode: Text.WrapAnywhere
                                readOnly: true
                                selectByMouse: true
                                selectionColor: MD3Theme.primary
                                selectedTextColor: MD3Theme.onPrimary
                            }

                            // Solid bottom backdrop & expand button for long clips
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 30
                                visible: delegateItem.isLong && !delegateItem.expanded
                                color: isFromPhone
                                    ? (MD3Theme.isDark ? "#2C2834" : "#EAE6ED")
                                    : (MD3Theme.isDark ? "#483857" : "#EADDFF")

                                // Top seamless gradient
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.top
                                    height: 18
                                    gradient: Gradient {
                                        orientation: Gradient.Vertical
                                        GradientStop { position: 0.0; color: "transparent" }
                                        GradientStop {
                                            position: 1.0
                                            color: isFromPhone
                                                ? (MD3Theme.isDark ? "#2C2834" : "#EAE6ED")
                                                : (MD3Theme.isDark ? "#483857" : "#EADDFF")
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: showText.implicitWidth + 20
                                    height: 22
                                    radius: 11
                                    color: isFromPhone
                                        ? (MD3Theme.isDark ? "#3D3748" : "#DDD7E2")
                                        : (MD3Theme.isDark ? "#5D4970" : "#D8C4F4")

                                    Text {
                                        id: showText
                                        anchors.centerIn: parent
                                        text: I18n.tr("chat.show_full_clip")
                                        font: MD3Theme.labelSmall
                                        color: MD3Theme.primary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: delegateItem.expanded = true
                                    }
                                }
                            }
                        }

                        // Bottom Metadata (Timestamp, Source & Quick Hover Actions)
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4
                            spacing: 8

                            Text {
                                text: (isFromPhone ? I18n.tr("chat.source_phone") + " • " : I18n.tr("chat.source_pc") + " • ") + model.timeFormatted
                                font: MD3Theme.labelSmall
                                color: MD3Theme.onSurfaceVariant
                                Layout.fillWidth: true
                                horizontalAlignment: isFromPhone ? Text.AlignLeft : Text.AlignRight
                            }

                            Row {
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                opacity: hoverHandler.hovered ? 1.0 : 0.0
                                visible: opacity > 0
                                spacing: 4

                                Behavior on opacity { NumberAnimation { duration: 150 } }

                                MD3IconButton {
                                    iconName: "copy"
                                    iconColor: MD3Theme.primary
                                    size: 22
                                    onClicked: controller.copyToClipboard(model.text)
                                }

                                MD3IconButton {
                                    visible: controller.connected && !isFromPhone
                                    iconName: "send"
                                    iconColor: MD3Theme.secondary
                                    size: 22
                                    onClicked: controller.pushClipboard(model.text)
                                }

                                MD3IconButton {
                                    iconName: "delete"
                                    iconColor: MD3Theme.outline
                                    size: 22
                                    onClicked: controller.clipModel.removeClip(index)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Google Messages Style Bottom Input Dock
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

                // Rounded Input Pill Container
                Rectangle {
                    id: inputPill
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 24
                    color: MD3Theme.isDark ? "#2A2533" : "#F2ECF4"
                    border.color: msgInput.activeFocus ? MD3Theme.primary : "transparent"
                    border.width: 1.5

                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    // Inside-Pill Paste Icon Button (anchored to right)
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
                            onClicked: msgInput.paste()
                        }
                    }

                    // Placeholder Text (Vertically centered)
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

                    // Single-line text input (Vertically centered)
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

                // Circular Send FAB
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

    function sendClip() {
        var txt = msgInput.text.trim()
        if (txt.length > 0 && controller.connected) {
            if (controller.pushClipboard(txt)) {
                msgInput.text = ""
            }
        }
    }
}
