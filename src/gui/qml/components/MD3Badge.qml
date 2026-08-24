import QtQuick
import WebClip

Rectangle {
    id: root

    property string text: ""
    property string iconName: ""
    property color badgeColor: MD3Theme.secondaryContainer
    property color textColor: MD3Theme.onSecondaryContainer

    implicitWidth: row.implicitWidth + 20
    implicitHeight: 28
    radius: 14
    color: badgeColor

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        MD3Icon {
            visible: root.iconName !== ""
            name: root.iconName
            color: root.textColor
            size: 14
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            font: MD3Theme.labelSmall
            color: root.textColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
