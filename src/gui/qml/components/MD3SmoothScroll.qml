import QtQuick

Item {
    id: root

    required property Flickable target

    property real wheelStep: 120
    property real wheelFollowRate: 20
    property real touchpadGain: 1.5
    property real touchpadFollowRate: 35
    property real inertiaDeceleration: 2000
    property real maxVelocity: 4000
    property real minVelocity: 30
    property int glideDelayMs: 90

    property bool _active: false
    property bool _gliding: false
    property bool _isTouchpad: false
    property real _targetY: 0
    property real _velocity: 0
    property real _followRate: wheelFollowRate
    property double _lastInputMs: 0

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

    function _stop() {
        _active = false
        _gliding = false
        _velocity = 0
        _lastInputMs = 0
    }

    FrameAnimation {
        running: root._active

        onTriggered: {
            var f = root.target
            if (!f || f.dragging || f.flicking) {
                root._stop()
                return
            }

            if (root._gliding) {
                var dt = frameTime
                var step = root._velocity * dt
                root._velocity -= (root._velocity > 0 ? 1 : -1) * root.inertiaDeceleration * dt
                var newY = root._clamp(f.contentY + step)
                var hitEdge = newY <= root._minY() || newY >= root._maxY()
                f.contentY = newY
                if (hitEdge || Math.abs(root._velocity) < root.minVelocity)
                    root._stop()
                return
            }

            if (root._isTouchpad && root._lastInputMs > 0
                    && Date.now() - root._lastInputMs > root.glideDelayMs) {
                if (Math.abs(root._velocity) >= root.minVelocity) {
                    root._gliding = true
                } else {
                    root._stop()
                }
                return
            }

            var goal = root._clamp(root._targetY)
            var diff = goal - f.contentY
            if (Math.abs(diff) < 0.5) {
                f.contentY = goal
                root._stop()
                return
            }
            f.contentY += diff * (1.0 - Math.exp(-root._followRate * frameTime))
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
                var now = Date.now()
                var dt = root._lastInputMs > 0 ? (now - root._lastInputMs) / 1000.0 : 0.016
                root._lastInputMs = now

                var instVel = -event.pixelDelta.y / Math.max(dt, 0.004)
                root._velocity = root._active
                    ? root._velocity * 0.5 + instVel * 0.5
                    : instVel
                root._velocity = Math.max(-root.maxVelocity, Math.min(root.maxVelocity, root._velocity))

                if (f.flicking || f.dragging) f.cancelFlick()
                if (!root._active || root._gliding) {
                    root._gliding = false
                    root._targetY = f.contentY
                    root._active = true
                }
                root._isTouchpad = true
                root._followRate = root.touchpadFollowRate
                root._targetY = root._clamp(root._targetY - event.pixelDelta.y * root.touchpadGain)
            } else if (event.angleDelta.y !== 0) {
                event.accepted = true
                if (f.dragging || f.flicking) f.cancelFlick()
                if (!root._active) {
                    root._targetY = f.contentY
                    root._active = true
                }
                root._isTouchpad = false
                root._gliding = false
                root._followRate = root.wheelFollowRate
                root._targetY -= (event.angleDelta.y / 120.0) * root.wheelStep
            }
        }
    }

    Connections {
        target: root.target

        function onContentHeightChanged() {
            if (root._active)
                root._targetY = root._clamp(root._targetY)
        }
    }
}
