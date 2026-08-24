import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import WebClip

Item {
    id: root

    property color selectedColor: "#6750A4"
    signal colorSelected(color col)

    // Internal HSL state
    property real currentHue: 0.74      // 0.0 - 1.0
    property real currentSat: 0.50      // 0.0 - 1.0
    property real currentLight: 0.48    // 0.0 - 1.0
    property bool updatingInternally: false

    implicitWidth: 380
    implicitHeight: mainLayout.implicitHeight

    function updateFromColor(col) {
        if (updatingInternally) return
        updatingInternally = true
        var c = Qt.color(col)
        if (c.r !== undefined) {
            currentHue = c.hslHue >= 0 ? c.hslHue : 0.0
            currentSat = c.hslSaturation >= 0 ? c.hslSaturation : 0.5
            currentLight = c.hslLightness >= 0 ? c.hslLightness : 0.5
            hexInput.text = colorToHex(c)
        }
        updatingInternally = false
    }

    function emitColor() {
        if (updatingInternally) return
        updatingInternally = true
        var col = Qt.hsla(currentHue, currentSat, currentLight, 1.0)
        selectedColor = col
        hexInput.text = colorToHex(col)
        colorSelected(col)
        updatingInternally = false
    }

    function colorToHex(c) {
        var r = Math.round(c.r * 255).toString(16).padStart(2, '0')
        var g = Math.round(c.g * 255).toString(16).padStart(2, '0')
        var b = Math.round(c.b * 255).toString(16).padStart(2, '0')
        return ("#" + r + g + b).toUpperCase()
    }

    onSelectedColorChanged: {
        updateFromColor(selectedColor)
    }

    Component.onCompleted: {
        updateFromColor(selectedColor)
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 16

        // Live preview & Hex Input Bar
        Rectangle {
            Layout.fillWidth: true
            height: 56
            radius: MD3Theme.cornerM
            color: MD3Theme.surfaceContainerHighest
            border.color: MD3Theme.outlineVariant
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12

                // Large Preview Swatch
                Rectangle {
                    width: 40
                    height: 40
                    radius: MD3Theme.cornerS
                    color: root.selectedColor
                    border.color: MD3Theme.outlineVariant
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 100 } }

                    MD3Icon {
                        anchors.centerIn: parent
                        name: "palette"
                        size: 20
                        color: root.currentLight > 0.6 ? "#1A1A1A" : "#FFFFFF"
                    }
                }

                // Hex input
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        text: "HEX COLOR"
                        font: MD3Theme.labelSmall
                        color: MD3Theme.onSurfaceVariant
                    }

                    TextInput {
                        id: hexInput
                        Layout.fillWidth: true
                        text: colorToHex(root.selectedColor)
                        font: MD3Theme.titleSmall
                        color: MD3Theme.onSurface
                        selectByMouse: true
                        maximumLength: 7

                        onTextChanged: {
                            if (root.updatingInternally) return
                            var str = text.trim()
                            if (!str.startsWith("#")) str = "#" + str
                            if (/^#[0-9A-Fa-f]{6}$/.test(str)) {
                                var col = Qt.color(str)
                                root.updatingInternally = true
                                root.selectedColor = col
                                root.currentHue = col.hslHue >= 0 ? col.hslHue : 0.0
                                root.currentSat = col.hslSaturation >= 0 ? col.hslSaturation : 0.5
                                root.currentLight = col.hslLightness >= 0 ? col.hslLightness : 0.5
                                root.colorSelected(col)
                                root.updatingInternally = false
                            }
                        }
                    }
                }

                // Active RGB / HSL stats badge
                Rectangle {
                    radius: MD3Theme.cornerXS
                    color: MD3Theme.surfaceContainer
                    implicitHeight: 32
                    implicitWidth: statCol.implicitWidth + 16

                    ColumnLayout {
                        id: statCol
                        anchors.centerIn: parent
                        spacing: 0

                        Text {
                            text: "H: " + Math.round(root.currentHue * 360) + "°  S: " + Math.round(root.currentSat * 100) + "%"
                            font: MD3Theme.labelSmall
                            color: MD3Theme.onSurfaceVariant
                        }
                    }
                }
            }
        }

        // Hue Slider Track
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Hue"
                    font: MD3Theme.labelMedium
                    color: MD3Theme.onSurface
                    Layout.fillWidth: true
                }
                Text {
                    text: Math.round(root.currentHue * 360) + "°"
                    font: MD3Theme.labelMedium
                    color: MD3Theme.primary
                }
            }

            Rectangle {
                id: hueTrack
                Layout.fillWidth: true
                height: 24
                radius: 12
                clip: true

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

                border.color: MD3Theme.outlineVariant
                border.width: 1

                // Draggable thumb
                Rectangle {
                    x: Math.max(0, Math.min(hueTrack.width - width, (root.currentHue * (hueTrack.width - width))))
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    radius: 11
                    color: Qt.hsla(root.currentHue, 1.0, 0.5, 1.0)
                    border.color: "#FFFFFF"
                    border.width: 2.5

                    Rectangle {
                        anchors.fill: parent
                        radius: 11
                        color: "transparent"
                        border.color: "rgba(0,0,0,0.3)"
                        border.width: 1
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    function updateHue(mouse) {
                        var h = Math.max(0, Math.min(1.0, mouse.x / hueTrack.width))
                        root.currentHue = h
                        root.emitColor()
                    }

                    onPressed: (mouse) => updateHue(mouse)
                    onPositionChanged: (mouse) => {
                        if (pressed) updateHue(mouse)
                    }
                }
            }
        }

        // Saturation Slider Track
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Saturation"
                    font: MD3Theme.labelMedium
                    color: MD3Theme.onSurface
                    Layout.fillWidth: true
                }
                Text {
                    text: Math.round(root.currentSat * 100) + "%"
                    font: MD3Theme.labelMedium
                    color: MD3Theme.primary
                }
            }

            Rectangle {
                id: satTrack
                Layout.fillWidth: true
                height: 20
                radius: 10
                clip: true

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.hsla(root.currentHue, 0.0, root.currentLight, 1.0) }
                    GradientStop { position: 1.0; color: Qt.hsla(root.currentHue, 1.0, root.currentLight, 1.0) }
                }

                border.color: MD3Theme.outlineVariant
                border.width: 1

                // Draggable thumb
                Rectangle {
                    x: Math.max(0, Math.min(satTrack.width - width, (root.currentSat * (satTrack.width - width))))
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    radius: 9
                    color: Qt.hsla(root.currentHue, root.currentSat, root.currentLight, 1.0)
                    border.color: "#FFFFFF"
                    border.width: 2

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: "transparent"
                        border.color: "rgba(0,0,0,0.3)"
                        border.width: 1
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    function updateSat(mouse) {
                        var s = Math.max(0, Math.min(1.0, mouse.x / satTrack.width))
                        root.currentSat = s
                        root.emitColor()
                    }

                    onPressed: (mouse) => updateSat(mouse)
                    onPositionChanged: (mouse) => {
                        if (pressed) updateSat(mouse)
                    }
                }
            }
        }

        // Lightness / Tone Slider Track
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Lightness"
                    font: MD3Theme.labelMedium
                    color: MD3Theme.onSurface
                    Layout.fillWidth: true
                }
                Text {
                    text: Math.round(root.currentLight * 100) + "%"
                    font: MD3Theme.labelMedium
                    color: MD3Theme.primary
                }
            }

            Rectangle {
                id: lightTrack
                Layout.fillWidth: true
                height: 20
                radius: 10
                clip: true

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#000000" }
                    GradientStop { position: 0.5; color: Qt.hsla(root.currentHue, root.currentSat, 0.5, 1.0) }
                    GradientStop { position: 1.0; color: "#FFFFFF" }
                }

                border.color: MD3Theme.outlineVariant
                border.width: 1

                // Draggable thumb
                Rectangle {
                    x: Math.max(0, Math.min(lightTrack.width - width, (root.currentLight * (lightTrack.width - width))))
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    radius: 9
                    color: Qt.hsla(root.currentHue, root.currentSat, root.currentLight, 1.0)
                    border.color: "#FFFFFF"
                    border.width: 2

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: "transparent"
                        border.color: "rgba(0,0,0,0.3)"
                        border.width: 1
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    function updateLight(mouse) {
                        var l = Math.max(0.1, Math.min(0.9, mouse.x / lightTrack.width))
                        root.currentLight = l
                        root.emitColor()
                    }

                    onPressed: (mouse) => updateLight(mouse)
                    onPositionChanged: (mouse) => {
                        if (pressed) updateLight(mouse)
                    }
                }
            }
        }

        // Curated Material Design 3 Extended Palette Swatches
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Curated Swatches"
                font: MD3Theme.labelMedium
                color: MD3Theme.onSurfaceVariant
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { name: "Deep Purple", hex: "#6750A4" },
                        { name: "Violet", hex: "#7C4DFF" },
                        { name: "Indigo", hex: "#3F51B5" },
                        { name: "Royal Blue", hex: "#2563EB" },
                        { name: "Sky Blue", hex: "#0288D1" },
                        { name: "Cyan", hex: "#00BCD4" },
                        { name: "Teal", hex: "#009688" },
                        { name: "Mint", hex: "#10B981" },
                        { name: "Green", hex: "#4CAF50" },
                        { name: "Lime", hex: "#84CC16" },
                        { name: "Amber", hex: "#FFB300" },
                        { name: "Orange", hex: "#FF9800" },
                        { name: "Deep Orange", hex: "#FF5722" },
                        { name: "Red", hex: "#F44336" },
                        { name: "Rose", hex: "#E11D48" },
                        { name: "Pink", hex: "#E91E63" },
                        { name: "Fuchsia", hex: "#D946EF" },
                        { name: "Slate", hex: "#64748B" }
                    ]

                    Rectangle {
                        id: swatchChip
                        width: 32
                        height: 32
                        radius: 16
                        color: modelData.hex
                        border.color: root.colorToHex(root.selectedColor) === modelData.hex ? MD3Theme.primary : MD3Theme.outlineVariant
                        border.width: root.colorToHex(root.selectedColor) === modelData.hex ? 2.5 : 1

                        scale: swatchMa.containsMouse ? 1.15 : (swatchMa.pressed ? 0.95 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 120 } }

                        MD3Icon {
                            anchors.centerIn: parent
                            name: "check"
                            size: 16
                            color: "#FFFFFF"
                            visible: root.colorToHex(root.selectedColor) === modelData.hex
                        }

                        MouseArea {
                            id: swatchMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var col = Qt.color(modelData.hex)
                                root.updatingInternally = true
                                root.selectedColor = col
                                root.currentHue = col.hslHue >= 0 ? col.hslHue : 0.0
                                root.currentSat = col.hslSaturation >= 0 ? col.hslSaturation : 0.5
                                root.currentLight = col.hslLightness >= 0 ? col.hslLightness : 0.5
                                root.hexInput.text = modelData.hex
                                root.colorSelected(col)
                                root.updatingInternally = false
                            }
                        }
                    }
                }
            }
        }
    }
}
