import QtQuick
import WebClip

Item {
    id: root

    property string label: ""
    property string placeholderText: ""
    property alias text: input.text
    property alias echoMode: input.echoMode

    implicitWidth: 280
    implicitHeight: 56

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: MD3Theme.cornerXS
        color: MD3Theme.surfaceContainerHighest

        border.color: input.activeFocus ? MD3Theme.primary : MD3Theme.outlineVariant
        border.width: input.activeFocus ? 2 : 1
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: root.label !== "" ? 8 : 16
            anchors.bottomMargin: 8
            spacing: 2

            Text {
                visible: root.label !== ""
                text: root.label
                font: MD3Theme.labelSmall
                color: input.activeFocus ? MD3Theme.primary : MD3Theme.onSurfaceVariant
            }

            TextInput {
                id: input
                width: parent.width
                font: MD3Theme.bodyLarge
                color: MD3Theme.onSurface
                selectByMouse: true
                selectionColor: MD3Theme.primaryContainer
                selectedTextColor: MD3Theme.onPrimaryContainer
                clip: true

                Text {
                    anchors.fill: parent
                    visible: !input.text && !input.activeFocus
                    text: root.placeholderText
                    font: input.font
                    color: MD3Theme.onSurfaceVariant
                }
            }
        }
    }
}
