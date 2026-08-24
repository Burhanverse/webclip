import QtQuick
import WebClip

Item {
    id: control

    property string iconName: ""
    property color iconColor: MD3Theme.onSurfaceVariant
    property color customBgColor: "transparent"
    property real size: 40
    property real iconSize: 20
    signal clicked()

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: width / 2
        color: control.customBgColor

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.iconColor
            opacity: mouseArea.pressed ? 0.14 : (mouseArea.containsMouse ? 0.08 : 0.0)
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }
    }

    MD3Icon {
        anchors.centerIn: parent
        name: control.iconName
        color: control.enabled ? control.iconColor : MD3Theme.colorWithAlpha(MD3Theme.onSurface, 0.38)
        size: control.iconSize
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: control.enabled
        hoverEnabled: control.enabled
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.clicked()
    }
}
