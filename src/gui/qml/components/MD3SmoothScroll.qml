import QtQuick

// Chromium / Telegram-style smooth scroller.
//
// Wheel input moves an internal *target* offset instead of restarting a
// fixed-duration animation. A frame-timed exponential follower drives the
// Flickable's contentY toward that target, which gives:
//   - instant response (no easing ramp-up delay)
//   - natural accumulation when the wheel is spun quickly
//   - a smooth momentum-style settle, independent of refresh rate
//
// Touchpad high-resolution deltas are streamed through the same filter with a
// snappier follow rate, so they track precisely without jitter.

Item {
    id: root

    required property Flickable target

    // Distance scrolled per mouse-wheel notch (px)
    property real wheelStep: 140
    // Extra speed multiplier while consecutive notches chain together
    property real repeatBoost: 1.25
    // Exponential follow rates (1/s). Higher = snappier.
    property real wheelFollowRate: 18
    property real touchpadFollowRate: 30
    // Peak animated speed (px/s); keeps very large jumps readable
    property real maxSpeed: 6000
    // Multiplier for touchpad pixel deltas
    property real touchpadGain: 1.0

    // Internal state
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

    // Keep the internal target aligned when something else moves the view
    // (user drag/flick, programmatic positioning, model changes)
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

            // A manual drag/flick takes priority: hand back control instantly
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

            // Frame-rate-independent exponential approach (Chromium style)
            var step = diff * (1.0 - Math.exp(-root._followRate * frameTime))

            // Cap peak velocity so long jumps stay readable
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
