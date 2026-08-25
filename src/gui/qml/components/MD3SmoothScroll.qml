import QtQuick

Item {
    id: root

    required property Flickable target

    property real wheelStep: 120
    property real followRate: 20

    property real _targetY: 0
    property bool _animating: false

    function _minY() {
        return target ? target.originY : 0
    }

    function _maxY() {
        if (!target) return 0
        return Math.max(_minY(), target.contentHeight - target.height)
    }

    function _clamp(y) {
        return Math.max(_minY(), Math.min(_maxY(), y))
    }

    function _beginWheel() {
        var f = target
        if (!f) return
        if (f.dragging || f.flicking) f.cancelFlick()
        if (!root._animating) {
            root._targetY = f.contentY
            root._animating = true
        }
    }

    FrameAnimation {
        running: root._animating

        onTriggered: {
            var f = root.target
            if (!f || f.dragging || f.flicking) {
                root._animating = false
                return
            }

            var goal = root._clamp(root._targetY)
            var diff = goal - f.contentY
            if (Math.abs(diff) < 0.5) {
                f.contentY = goal
                root._animating = false
                return
            }
            f.contentY += diff * (1.0 - Math.exp(-root.followRate * frameTime))
        }
    }

    WheelHandler {
        target: root.target
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier

        onWheel: (event) => {
            var f = root.target
            if (!f || f.contentHeight <= f.height) {
                event.accepted = false
                return
            }

            if (event.pixelDelta.y !== 0) {
                event.accepted = true
                root._animating = false
                if (f.flicking) f.cancelFlick()
                f.contentY = root._clamp(f.contentY - event.pixelDelta.y)
            } else if (event.angleDelta.y !== 0) {
                event.accepted = true
                root._beginWheel()
                root._targetY -= (event.angleDelta.y / 120.0) * root.wheelStep
            }
        }
    }

    Connections {
        target: root.target

        function onContentHeightChanged() {
            if (root._animating)
                root._targetY = root._clamp(root._targetY)
        }
    }
}
