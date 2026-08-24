import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import WebClip

QQC.Popup {
    id: root

    property color initialColor: "#FF416D"
    property color selectedColor: "#FF416D"

    signal accepted(color col)
    signal rejected()

    // Internal HSL and RGB state
    property real currentHue: 0.96      // 0.0 - 1.0 (e.g. 346 / 360)
    property real currentSat: 1.0       // 0.0 - 1.0 (100%)
    property real currentLight: 0.63    // 0.0 - 1.0 (63%)

    property int currentR: 255
    property int currentG: 65
    property int currentB: 109
    property string currentHex: "ff416d"

    property bool updatingInternally: false

    modal: true
    focus: true
    closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside
    anchors.centerIn: QQC.Overlay.overlay
    padding: 0

    background: Rectangle {
        radius: 24
        color: MD3Theme.isDark ? "#28232D" : "#FFF0F5"
        border.color: MD3Theme.outlineVariant
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: 25
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.15)
            border.width: 1
            z: -1
        }
    }

    QQC.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    function openWithColor(col) {
        initialColor = col
        selectedColor = col
        loadFromColor(col)
        open()
    }

    function updateTextFields() {
        hInput.text = Math.round(currentHue * 360).toString()
        sInput.text = Math.round(currentSat * 100).toString()
        lInput.text = Math.round(currentLight * 100).toString()
        rInput.text = currentR.toString()
        gInput.text = currentG.toString()
        bInput.text = currentB.toString()
        hexTextInput.text = currentHex
    }

    function loadFromColor(col) {
        updatingInternally = true
        var c = Qt.color(col)
        if (c.r !== undefined) {
            currentHue = c.hslHue >= 0 ? c.hslHue : 0.0
            currentSat = c.hslSaturation >= 0 ? c.hslSaturation : 1.0
            currentLight = c.hslLightness >= 0 ? c.hslLightness : 0.5

            currentR = Math.round(c.r * 255)
            currentG = Math.round(c.g * 255)
            currentB = Math.round(c.b * 255)
            currentHex = colorToHexNoHash(c)
            selectedColor = c
            updateTextFields()
        }
        updatingInternally = false
    }

    function syncFromHsl() {
        if (updatingInternally) return
        updatingInternally = true
        var col = Qt.hsla(currentHue, currentSat, currentLight, 1.0)
        selectedColor = col
        currentR = Math.round(col.r * 255)
        currentG = Math.round(col.g * 255)
        currentB = Math.round(col.b * 255)
        currentHex = colorToHexNoHash(col)
        updateTextFields()
        updatingInternally = false
    }

    function syncFromRgb() {
        if (updatingInternally) return
        updatingInternally = true
        var col = Qt.rgba(currentR / 255.0, currentG / 255.0, currentB / 255.0, 1.0)
        selectedColor = col
        currentHue = col.hslHue >= 0 ? col.hslHue : 0.0
        currentSat = col.hslSaturation >= 0 ? col.hslSaturation : 0.0
        currentLight = col.hslLightness >= 0 ? col.hslLightness : 0.5
        currentHex = colorToHexNoHash(col)
        updateTextFields()
        updatingInternally = false
    }

    function syncFromHex(hexStr) {
        if (updatingInternally) return
        var clean = hexStr.trim()
        if (clean.startsWith("#")) clean = clean.substring(1)
        if (/^[0-9A-Fa-f]{6}$/.test(clean)) {
            updatingInternally = true
            var col = Qt.color("#" + clean)
            selectedColor = col
            currentHue = col.hslHue >= 0 ? col.hslHue : 0.0
            currentSat = col.hslSaturation >= 0 ? col.hslSaturation : 0.0
            currentLight = col.hslLightness >= 0 ? col.hslLightness : 0.5
            currentR = Math.round(col.r * 255)
            currentG = Math.round(col.g * 255)
            currentB = Math.round(col.b * 255)
            currentHex = clean.toLowerCase()
            updateTextFields()
            updatingInternally = false
        }
    }

    function colorToHexNoHash(c) {
        var r = Math.round(c.r * 255).toString(16).padStart(2, '0')
        var g = Math.round(c.g * 255).toString(16).padStart(2, '0')
        var b = Math.round(c.b * 255).toString(16).padStart(2, '0')
        return (r + g + b).toLowerCase()
    }

    contentItem: Item {
        implicitWidth: 390
        implicitHeight: dialogLayout.implicitHeight + 40

        ColumnLayout {
            id: dialogLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 16

            // Header
            Text {
                text: "Choose accent color"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: MD3Theme.onSurface
                Layout.fillWidth: true
            }

            // Main Editor Row (2D Spectrum on left, numeric fields on right)
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // Left Column: 2D Spectrum Box + Bottom Lightness Slider
                ColumnLayout {
                    spacing: 12

                    // 2D Hue x Saturation Spectrum Area
                    Rectangle {
                        id: spectrumArea
                        width: 220
                        height: 220
                        radius: 4
                        clip: true

                        // Horizontal rainbow hue gradient
                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.00; color: "#FF0000" }
                                GradientStop { position: 0.17; color: "#FFFF00" }
                                GradientStop { position: 0.33; color: "#00FF00" }
                                GradientStop { position: 0.50; color: "#00FFFF" }
                                GradientStop { position: 0.67; color: "#0000FF" }
                                GradientStop { position: 0.83; color: "#FF00FF" }
                                GradientStop { position: 1.00; color: "#FF0000" }
                            }
                        }

                        // Vertical Saturation overlay (Top: transparent / full color, Bottom: gray)
                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: "transparent" }
                                GradientStop { position: 1.0; color: "#808080" }
                            }
                        }

                        border.color: MD3Theme.outlineVariant
                        border.width: 1

                        // Draggable Pointer Ring
                        Rectangle {
                            id: spectrumHandle
                            x: Math.max(0, Math.min(spectrumArea.width - width, root.currentHue * spectrumArea.width - width / 2))
                            y: Math.max(0, Math.min(spectrumArea.height - height, (1.0 - root.currentSat) * spectrumArea.height - height / 2))
                            width: 16
                            height: 16
                            radius: 8
                            color: "transparent"
                            border.color: "#FFFFFF"
                            border.width: 2

                            Rectangle {
                                anchors.centerIn: parent
                                width: 18
                                height: 18
                                radius: 9
                                color: "transparent"
                                border.color: Qt.rgba(0, 0, 0, 0.4)
                                border.width: 1
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.CrossCursor

                            function updateFromMouse(mouse) {
                                var h = Math.max(0, Math.min(1.0, mouse.x / spectrumArea.width))
                                var s = Math.max(0, Math.min(1.0, 1.0 - (mouse.y / spectrumArea.height)))
                                root.currentHue = h
                                root.currentSat = s
                                root.syncFromHsl()
                            }

                            onPressed: (mouse) => updateFromMouse(mouse)
                            onPositionChanged: (mouse) => {
                                if (pressed) updateFromMouse(mouse)
                            }
                        }
                    }

                    // Bottom Lightness Slider Track
                    Item {
                        id: lightSliderContainer
                        width: 220
                        height: 20

                        Rectangle {
                            id: lightSliderTrack
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 14
                            radius: 2
                            clip: true

                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#000000" }
                                GradientStop { position: 1.0; color: Qt.hsla(root.currentHue, root.currentSat, 0.5, 1.0) }
                            }

                            border.color: MD3Theme.outlineVariant
                            border.width: 1
                        }

                        // Top & Bottom Indicator Arrows
                        Item {
                            x: Math.max(0, Math.min(lightSliderContainer.width, root.currentLight * lightSliderContainer.width)) - 4
                            anchors.fill: parent

                            // Top Arrow indicator pointing down
                            Canvas {
                                width: 8
                                height: 5
                                anchors.top: parent.top
                                anchors.topMargin: 0
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.fillStyle = MD3Theme.onSurface
                                    ctx.beginPath()
                                    ctx.moveTo(0, 0)
                                    ctx.lineTo(8, 0)
                                    ctx.lineTo(4, 5)
                                    ctx.closePath()
                                    ctx.fill()
                                }
                            }

                            // Bottom Arrow indicator pointing up
                            Canvas {
                                width: 8
                                height: 5
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.fillStyle = MD3Theme.onSurface
                                    ctx.beginPath()
                                    ctx.moveTo(4, 0)
                                    ctx.lineTo(0, 5)
                                    ctx.lineTo(8, 5)
                                    ctx.closePath()
                                    ctx.fill()
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor

                            function updateLight(mouse) {
                                var l = Math.max(0.05, Math.min(0.95, mouse.x / lightSliderContainer.width))
                                root.currentLight = l
                                root.syncFromHsl()
                            }

                            onPressed: (mouse) => updateLight(mouse)
                            onPositionChanged: (mouse) => {
                                if (pressed) updateLight(mouse)
                            }
                        }
                    }
                }

                // Right Column: Dual Swatch + HSL, RGB, HEX Inputs
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    // Dual Color Preview Swatch
                    Rectangle {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 64
                        Layout.alignment: Qt.AlignRight
                        radius: 6
                        clip: true
                        border.color: MD3Theme.outlineVariant
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            Rectangle {
                                width: parent.width
                                height: parent.height / 2
                                color: root.selectedColor
                            }
                            Rectangle {
                                width: parent.width
                                height: parent.height / 2
                                color: Qt.hsla(root.currentHue, Math.min(1.0, root.currentSat * 0.7), 0.85, 1.0)
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 4 }

                    // HSL Section
                    // Hue Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "H"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        TextInput {
                            id: hInput
                            Layout.fillWidth: true
                            text: "346"
                            font.pixelSize: 13
                            color: MD3Theme.onSurface
                            selectByMouse: true
                            onTextEdited: {
                                if (root.updatingInternally) return
                                var val = parseInt(text)
                                if (!isNaN(val)) {
                                    root.currentHue = Math.max(0, Math.min(360, val)) / 360.0
                                    root.syncFromHsl()
                                }
                            }
                        }
                        Text { text: "°"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MD3Theme.outlineVariant }

                    // Saturation Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "S"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        TextInput {
                            id: sInput
                            Layout.fillWidth: true
                            text: "100"
                            font.pixelSize: 13
                            color: MD3Theme.onSurface
                            selectByMouse: true
                            onTextEdited: {
                                if (root.updatingInternally) return
                                var val = parseInt(text)
                                if (!isNaN(val)) {
                                    root.currentSat = Math.max(0, Math.min(100, val)) / 100.0
                                    root.syncFromHsl()
                                }
                            }
                        }
                        Text { text: "%"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MD3Theme.outlineVariant }

                    // Lightness Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "L"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        TextInput {
                            id: lInput
                            Layout.fillWidth: true
                            text: "63"
                            font.pixelSize: 13
                            color: MD3Theme.onSurface
                            selectByMouse: true
                            onTextEdited: {
                                if (root.updatingInternally) return
                                var val = parseInt(text)
                                if (!isNaN(val)) {
                                    root.currentLight = Math.max(0, Math.min(100, val)) / 100.0
                                    root.syncFromHsl()
                                }
                            }
                        }
                        Text { text: "%"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MD3Theme.outlineVariant }

                    Item { Layout.preferredHeight: 2 }

                    // RGB Section
                    // R Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "R"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        TextInput {
                            id: rInput
                            Layout.fillWidth: true
                            text: "255"
                            font.pixelSize: 13
                            color: MD3Theme.onSurface
                            selectByMouse: true
                            onTextEdited: {
                                if (root.updatingInternally) return
                                var val = parseInt(text)
                                if (!isNaN(val)) {
                                    root.currentR = Math.max(0, Math.min(255, val))
                                    root.syncFromRgb()
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MD3Theme.outlineVariant }

                    // G Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "G"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        TextInput {
                            id: gInput
                            Layout.fillWidth: true
                            text: "65"
                            font.pixelSize: 13
                            color: MD3Theme.onSurface
                            selectByMouse: true
                            onTextEdited: {
                                if (root.updatingInternally) return
                                var val = parseInt(text)
                                if (!isNaN(val)) {
                                    root.currentG = Math.max(0, Math.min(255, val))
                                    root.syncFromRgb()
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MD3Theme.outlineVariant }

                    // B Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "B"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        TextInput {
                            id: bInput
                            Layout.fillWidth: true
                            text: "109"
                            font.pixelSize: 13
                            color: MD3Theme.onSurface
                            selectByMouse: true
                            onTextEdited: {
                                if (root.updatingInternally) return
                                var val = parseInt(text)
                                if (!isNaN(val)) {
                                    root.currentB = Math.max(0, Math.min(255, val))
                                    root.syncFromRgb()
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MD3Theme.outlineVariant }

                    Item { Layout.preferredHeight: 2 }

                    // Hex Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "#"; font.pixelSize: 13; color: MD3Theme.onSurfaceVariant; Layout.preferredWidth: 14 }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 22
                            color: MD3Theme.isDark ? "#3A3340" : "#E8DCE4"
                            radius: 2

                            TextInput {
                                id: hexTextInput
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                text: "ff416d"
                                font.pixelSize: 13
                                font.family: "monospace"
                                color: MD3Theme.onSurface
                                selectByMouse: true
                                maximumLength: 6
                                verticalAlignment: TextInput.AlignVCenter

                                onTextEdited: {
                                    if (root.updatingInternally) return
                                    root.syncFromHex(text)
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 2; color: MD3Theme.primary }
                }
            }

            // Bottom Actions (Cancel / Save)
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 12

                Item { Layout.fillWidth: true }

                Text {
                    text: "Cancel"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: MD3Theme.primary
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.close()
                            root.rejected()
                        }
                    }
                }

                Item { Layout.preferredWidth: 8 }

                Text {
                    text: "Save"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: MD3Theme.primary
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.accepted(root.selectedColor)
                            root.close()
                        }
                    }
                }
            }
        }
    }
}
