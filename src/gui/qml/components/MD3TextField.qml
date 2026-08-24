import QtQuick
import WebClip

Item {
    id: root

    property string label: ""
    property string placeholderText: ""
    property alias text: input.text
    property alias echoMode: input.echoMode

    implicitWidth: 220
    implicitHeight: root.label !== "" ? 44 : 36

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 12
        color: MD3Theme.surfaceContainerHighest

        border.color: input.activeFocus ? MD3Theme.primary : MD3Theme.outlineVariant
        border.width: input.activeFocus ? 1.5 : 1
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: root.label !== "" ? 4 : 8
            anchors.bottomMargin: 4
            spacing: 1

            Text {
                visible: root.label !== ""
                text: root.label
                font: MD3Theme.labelSmall
                color: input.activeFocus ? MD3Theme.primary : MD3Theme.onSurfaceVariant
            }

            TextInput {
                id: input
                width: parent.width
                font: MD3Theme.bodyMedium
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
