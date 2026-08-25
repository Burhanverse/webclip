import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtQuick.Shapes
import WebClip

Item {
    id: root

    required property var controller
    property var thanosEffect: null

    property string fullPreviewUrl: ""
    property bool fullPreviewVisible: false

    function linkifyText(rawText, linkColor) {
        if (!rawText) return ""
        var escaped = rawText
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")

        var urlPattern = /(https?:\/\/[^\s<]+|www\.[^\s<]+|[a-zA-Z0-9.-]+\.(?:com|org|net|io|dev|app|edu|gov|eu|co|in|me|info|xyz|tech|online|ai|gg)(?:\/[^\s<]*)?)/gi
        return escaped.replace(urlPattern, function(match) {
            var url = match
            if (!/^https?:\/\//i.test(url)) {
                url = "https://" + url
            }
            return '<a href="' + url + '" style="color: ' + linkColor + '; text-decoration: underline;">' + match + '</a>'
        }).replace(/\n/g, "<br>")
    }

    function extractFirstUrl(rawText) {
        if (!rawText) return ""
        var urlPattern = /(https?:\/\/[^\s<]+|www\.[^\s<]+|[a-zA-Z0-9.-]+\.(?:com|org|net|io|dev|app|edu|gov|eu|co|in|me|info|xyz|tech|online|ai|gg)(?:\/[^\s<]*)?)/i
        var match = rawText.match(urlPattern)
        if (match && match[0]) {
            var url = match[0]
            if (!/^https?:\/\//i.test(url)) {
                url = "https://" + url
            }
            return url
        }
        return ""
    }

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
                reuseItems: true
                cacheBuffer: 100

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
                    height: bubbleCard.height + 8
                    implicitHeight: height

                    readonly property bool isFromPhone: model.source === "phone"
                    readonly property bool isImageClip: model.isImage
                    property bool expanded: false
                    readonly property bool isLong: !isImageClip && (model.charCount > 350 || (model.text && model.text.split('\n').length > 5))

                    HoverHandler {
                        id: hoverHandler
                    }

                    // Seamless Chat Bubble Container with Vector Tail
                    Item {
                        id: bubbleCard
                        anchors.left: isFromPhone ? parent.left : undefined
                        anchors.right: !isFromPhone ? parent.right : undefined
                        anchors.leftMargin: isFromPhone ? 8 : 0
                        anchors.rightMargin: !isFromPhone ? 8 : 0

                        readonly property color bubbleColor: isFromPhone
                            ? MD3Theme.secondaryContainer
                            : MD3Theme.primaryContainer

                        readonly property real horizPad: isFromPhone ? 28 : 20
                        readonly property real textNeededWidth: sizingText.implicitWidth + horizPad
                        readonly property real metaNeededWidth: timeLabel.implicitWidth + actionIconsRow.implicitWidth + 24 + horizPad

                        width: isImageClip
                            ? Math.min(listView.width * 0.84, 320)
                            : Math.min(listView.width * 0.84, Math.max(130, Math.max(textNeededWidth, metaNeededWidth)))
                        height: bubbleInnerCol.implicitHeight + 16
                        implicitHeight: height

                        Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                        Text {
                            id: sizingText
                            visible: false
                            font: MD3Theme.bodyMedium
                            text: !delegateItem.isImageClip ? (model.text || "") : ""
                        }

                        Canvas {
                            id: bubbleCanvas
                            anchors.fill: parent
                            property color bubbleColor: bubbleCard.bubbleColor

                            onBubbleColorChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                var w = width
                                var h = height
                                var r = 18
                                var tw = 6
                                var th = 15

                                ctx.beginPath()
                                if (!isFromPhone) {
                                    // Outgoing right bubble: smooth rounded corner & tail on bottom-right
                                    ctx.moveTo(r, 0)
                                    ctx.lineTo(w - tw - r, 0)
                                    ctx.arcTo(w - tw, 0, w - tw, r, r)
                                    ctx.lineTo(w - tw, h - th)
                                    ctx.bezierCurveTo(w - tw, h - 7, w - 2, h - 0.5, w, h)
                                    ctx.lineTo(r, h)
                                    ctx.arcTo(0, h, 0, h - r, r)
                                    ctx.lineTo(0, r)
                                    ctx.arcTo(0, 0, r, 0, r)
                                } else {
                                    // Incoming left bubble: smooth rounded corner & tail on bottom-left
                                    ctx.moveTo(tw + r, 0)
                                    ctx.lineTo(w - r, 0)
                                    ctx.arcTo(w, 0, w, r, r)
                                    ctx.lineTo(w, h - r)
                                    ctx.arcTo(w, h, w - r, h, r)
                                    ctx.lineTo(tw, h)
                                    ctx.bezierCurveTo(2, h - 0.5, tw, h - 7, tw, h - th)
                                    ctx.lineTo(tw, r)
                                    ctx.arcTo(tw, 0, tw + r, 0, r)
                                }
                                ctx.closePath()
                                ctx.fillStyle = bubbleColor
                                ctx.fill()
                            }
                        }

                        ColumnLayout {
                            id: bubbleInnerCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: isFromPhone ? 14 : 10
                            anchors.rightMargin: !isFromPhone ? 14 : 10
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 4

                            // Image Content Area
                            Rectangle {
                                id: imageContainer
                                visible: delegateItem.isImageClip
                                Layout.fillWidth: true
                                Layout.preferredHeight: 220
                                radius: 12
                                color: isFromPhone ? MD3Theme.surfaceContainerHigh : MD3Theme.secondaryContainer
                                clip: true

                                Image {
                                    id: imgPreview
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: delegateItem.isImageClip ? model.imageData : ""
                                    fillMode: Image.PreserveAspectFit
                                    sourceSize.width: 480
                                    sourceSize.height: 480
                                    asynchronous: true
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        root.fullPreviewUrl = model.imageData
                                        root.fullPreviewVisible = true
                                    }
                                }
                            }

                            // Text Content Area (clipped to 85px with gradient when collapsed)
                            Item {
                                id: textClipContainer
                                visible: !delegateItem.isImageClip
                                Layout.fillWidth: true
                                height: delegateItem.isLong && !delegateItem.expanded
                                    ? 85
                                    : bubbleContent.implicitHeight
                                implicitHeight: height
                                clip: true

                                Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                                TextEdit {
                                    id: bubbleContent
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    textFormat: TextEdit.RichText
                                    text: !delegateItem.isImageClip
                                        ? root.linkifyText(model.text, isFromPhone ? MD3Theme.primary : (MD3Theme.isDark ? "#8AB4F8" : "#1A73E8"))
                                        : ""
                                    font: MD3Theme.bodyMedium
                                    color: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                    wrapMode: Text.WrapAnywhere
                                    readOnly: true
                                    selectByMouse: true
                                    selectionColor: MD3Theme.primary
                                    selectedTextColor: MD3Theme.onPrimary

                                    onLinkActivated: (link) => {
                                        Qt.openUrlExternally(link)
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: bubbleContent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                                        acceptedButtons: Qt.NoButton
                                    }
                                }

                                // Fade gradient at the bottom of preview text
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 24
                                    visible: delegateItem.isLong && !delegateItem.expanded
                                    gradient: Gradient {
                                        orientation: Gradient.Vertical
                                        GradientStop { position: 0.0; color: "transparent" }
                                        GradientStop {
                                            position: 1.0
                                            color: bubbleCard.bubbleColor
                                        }
                                    }
                                }
                            }

                            // Centered "Show full clip" Pill Button
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                visible: delegateItem.isLong && !delegateItem.expanded
                                width: showText.implicitWidth + 24
                                height: 24
                                radius: 12
                                color: isFromPhone
                                    ? MD3Theme.surfaceContainerHigh
                                    : MD3Theme.surfaceContainerLowest

                                Text {
                                    id: showText
                                    anchors.centerIn: parent
                                    text: I18n.tr("chat.show_full_clip")
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: MD3Theme.primary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: delegateItem.expanded = true
                                }
                            }

                            // Bottom Metadata & Actions Row
                            RowLayout {
                                id: metadataRow
                                Layout.fillWidth: true
                                Layout.topMargin: 2
                                spacing: 8

                                // Timestamp
                                Text {
                                    id: timeLabel
                                    Layout.alignment: Qt.AlignVCenter
                                    text: model.timeFormatted
                                    font: MD3Theme.labelSmall
                                    color: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                    opacity: 0.65
                                }

                                Item { Layout.fillWidth: true }

                                // Interactive Inline Actions Container
                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredHeight: 24
                                    Layout.preferredWidth: actionIconsRow.implicitWidth + 8
                                    radius: 12
                                    color: isFromPhone
                                        ? (MD3Theme.isDark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0, 0, 0, 0.06))
                                        : (MD3Theme.isDark ? Qt.rgba(0, 0, 0, 0.12) : Qt.rgba(0, 0, 0, 0.06))

                                    Row {
                                        id: actionIconsRow
                                        anchors.centerIn: parent
                                        spacing: 2

                                        MD3IconButton {
                                            visible: !delegateItem.isImageClip && root.extractFirstUrl(model.text) !== ""
                                            iconName: "link"
                                            iconColor: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                            size: 20
                                            iconSize: 13
                                            onClicked: {
                                                var url = root.extractFirstUrl(model.text)
                                                if (url) Qt.openUrlExternally(url)
                                            }
                                        }

                                        MD3IconButton {
                                            iconName: "copy"
                                            iconColor: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                            size: 20
                                            iconSize: 13
                                            onClicked: {
                                                if (delegateItem.isImageClip) {
                                                    controller.copyImageToClipboard(index)
                                                } else {
                                                    controller.copyToClipboard(model.text)
                                                }
                                            }
                                        }

                                        MD3IconButton {
                                            visible: delegateItem.isImageClip
                                            iconName: "download"
                                            iconColor: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                            size: 20
                                            iconSize: 13
                                            onClicked: {
                                                saveImageDialog.targetClipIndex = index
                                                saveImageDialog.open()
                                            }
                                        }

                                        MD3IconButton {
                                            visible: controller.connected && !isFromPhone
                                            iconName: "send"
                                            iconColor: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                            size: 20
                                            iconSize: 13
                                            onClicked: {
                                                if (delegateItem.isImageClip) {
                                                    controller.pushImage(model.imageData)
                                                } else {
                                                    controller.pushClipboard(model.text)
                                                }
                                            }
                                        }

                                        MD3IconButton {
                                            iconName: "delete"
                                            iconColor: isFromPhone ? MD3Theme.onSecondaryContainer : MD3Theme.onPrimaryContainer
                                            size: 20
                                            iconSize: 13
                                            onClicked: {
                                                var clipId = model.id
                                                if (controller.thanosSnapEnabled && root.thanosEffect && bubbleCard) {
                                                    var pos = bubbleCard.mapToItem(root.thanosEffect, 0, 0)
                                                    var w = bubbleCard.width
                                                    var h = bubbleCard.height > 0 ? bubbleCard.height : bubbleCard.implicitHeight
                                                    bubbleCard.grabToImage(function(result) {
                                                        if (result && result.image) {
                                                            root.thanosEffect.snapImage(result.image, Qt.rect(pos.x, pos.y, w, h))
                                                        }
                                                        controller.clipModel.removeClipById(clipId)
                                                    })
                                                } else {
                                                    controller.clipModel.removeClipById(clipId)
                                                }
                                            }
                                        }
                                    }
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

                // Attach Image Button
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

                // Rounded Input Pill Container
                Rectangle {
                    id: inputPill
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 24
                    color: MD3Theme.surfaceContainerHigh
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
                            onClicked: {
                                if (controller.pushCurrentClipboard()) {
                                    msgInput.text = ""
                                } else {
                                    msgInput.paste()
                                }
                            }
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

    // Full-Screen Image Preview Modal
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

        // Close button top right
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
