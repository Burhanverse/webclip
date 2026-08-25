import QtQuick

Item {
    id: root

    required property Flickable target

    property real wheelStep: 140

    property real repeatBoost: 1.25

    property real wheelFollowRate: 18
    property real touchpadFollowRate: 30

    property real maxSpeed: 6000

    property real touchpadGain: 1.0

    property real _targetY: 0
    property real _followRate: wheelFollowRate
    property real _lastWheelTs: 0

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

    function syncToContent() {
        if (target && !follower.running) {
            _targetY = target.contentY
        }
    }

    function scrollTo(y) {
        if (!target || _maxY() <= _minY()) return
        target.cancelFlick()
        _targetY = _clamp(y)
        follower.start()
    }

    FrameAnimation {
        id: follower

        onTriggered: {
            var f = root.target
            if (!f) {
                follower.stop()
                return
            }

            if (f.dragging || f.flicking) {
                follower.stop()
                root._targetY = f.contentY
                return
            }

            root._targetY = root._clamp(root._targetY)

            var diff = root._targetY - f.contentY
            if (Math.abs(diff) < 0.5) {
                f.contentY = root._targetY
                follower.stop()
                return
            }

            var step = diff * (1.0 - Math.exp(-root._followRate * frameTime))

            var maxStep = root.maxSpeed * frameTime
            if (step > maxStep) step = maxStep
            else if (step < -maxStep) step = -maxStep

            var newY = f.contentY + step
            if ((step > 0 && newY > root._targetY) || (step < 0 && newY < root._targetY)) {
                newY = root._targetY
            }
            f.contentY = newY
        }
    }

    WheelHandler {
        target: root.target
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier
        activeTimeout: 0.25

        onWheel: (event) => {
            var f = root.target
            if (!f) return
            if (f.contentHeight <= f.height) return

            var ts = Date.now()
            var chaining = follower.running && (ts - root._lastWheelTs) < 200
            root._lastWheelTs = ts

            if (event.pixelDelta.y !== 0) {
                root._followRate = root.touchpadFollowRate
                root.scrollTo(root._targetY - event.pixelDelta.y * root.touchpadGain)
            } else if (event.angleDelta.y !== 0) {
                var notches = event.angleDelta.y / 120.0
                root._followRate = root.wheelFollowRate
                root.scrollTo(root._targetY - notches * root.wheelStep * (chaining ? root.repeatBoost : 1.0))
            }
        }
    }

    Connections {
        target: root.target

        function onContentHeightChanged() {
            if (root.target) {
                root._targetY = root._clamp(root._targetY)
            }
        }

        function onContentYChanged() {
            root.syncToContent()
        }
    }
}
