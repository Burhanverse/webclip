import QtQuick

Item {
    id: root

    required property Flickable target

    property real stepSize: 320
    property real targetContentY: target ? target.contentY : 0

    NumberAnimation {
        id: scrollAnim
        target: root.target
        property: "contentY"
        duration: 60
        easing.type: Easing.OutQuad
    }

    WheelHandler {
        target: root.target
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            if (!root.target) return
            var flick = root.target
            var minY = flick.originY
            var maxY = Math.max(minY, flick.contentHeight - flick.height)

            if (maxY <= minY) return

            flick.cancelFlick()

            if (event.pixelDelta.y !== 0) {
                // 2x Fast High-Precision Touchpad Tracking
                scrollAnim.stop()
                var newY = Math.max(minY, Math.min(maxY, flick.contentY - (event.pixelDelta.y * 2.0)))
                flick.contentY = newY
                root.targetContentY = newY
            } else if (event.angleDelta.y !== 0) {
                // 2x Fast Mouse Wheel Scrolling
                var notches = event.angleDelta.y / 120.0
                var current = scrollAnim.running ? root.targetContentY : flick.contentY
                var boost = scrollAnim.running ? 1.6 : 1.0
                root.targetContentY = Math.max(minY, Math.min(maxY, current - (notches * root.stepSize * boost)))
                scrollAnim.stop()
                scrollAnim.to = root.targetContentY
                scrollAnim.start()
            }
        }
    }
}
