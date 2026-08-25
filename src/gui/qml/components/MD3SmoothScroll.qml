import QtQuick
import WebClip

MD3KineticScroller {
    id: root

    speedMultiplier: 2.0

    Connections {
        target: root.target

        function onDraggingChanged() {
            if (root.target && root.target.dragging) {
                root.stop()
            }
        }
    }
}
