import QtQuick
import WebClip

Item {
    id: control

    property bool checked: false
    signal toggled()

    implicitWidth: 52
    implicitHeight: 32

    Rectangle {
        id: track
        anchors.fill: parent
        radius: 16
        color: control.checked ? MD3Theme.primary : MD3Theme.surfaceContainerHighest
        border.color: control.checked ? MD3Theme.primary : MD3Theme.outline
        border.width: control.checked ? 0 : 2
        Behavior on color { ColorAnimation { duration: 180 } }

        Rectangle {
            id: thumb
            width: control.checked ? 24 : 16
            height: width
            radius: width / 2
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? (parent.width - width - 4) : 8
            color: control.checked ? MD3Theme.onPrimary : MD3Theme.outline
            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: 180 } }
            Behavior on color { ColorAnimation { duration: 180 } }

            MD3Icon {
                anchors.centerIn: parent
                visible: control.checked
                name: "check"
                color: MD3Theme.primary
                size: 14
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.checked = !control.checked
            control.toggled()
        }
    }
}
