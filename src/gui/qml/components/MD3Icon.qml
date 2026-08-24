import QtQuick
import Qt5Compat.GraphicalEffects
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
        sourceSize: Qt.size(Math.max(48, root.size * 3), Math.max(48, root.size * 3))
        source: root.name !== "" ? ("qrc:/qt/qml/src/gui/resources/icons/" + root.name + ".svg") : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: false
    }

    ColorOverlay {
        anchors.fill: iconImg
        source: iconImg
        color: root.color
    }
}
