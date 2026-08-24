import QtQuick
import WebClip

Item {
    id: root

    property string name: ""
    property color color: MD3Theme.onSurface
    property real size: 20

    implicitWidth: size
    implicitHeight: size

    Image {
        id: iconImg
        anchors.centerIn: parent
        width: root.size
        height: root.size
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: false

        source: root.name !== ""
            ? ("image://icon/" + root.name
                + "?color=" + encodeURIComponent(root.color.toString())
                + "&size=" + Math.max(32, Math.round(root.size * Math.max(1.0, Screen.devicePixelRatio || 1.0))))
            : ""
    }
}
