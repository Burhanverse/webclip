#include "md3_kinetic_scroller.hpp"
#include <QWheelEvent>
#include <QDateTime>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

namespace webclip {

MD3KineticScroller::MD3KineticScroller(QQuickItem* parent)
    : QQuickItem(parent) {
    frameTimer_ = new QTimer(this);
    frameTimer_->setSingleShot(false);
    frameTimer_->setTimerType(Qt::PreciseTimer);
    frameTimer_->setInterval(8);
    connect(frameTimer_, &QTimer::timeout, this, &MD3KineticScroller::onFrameTick);

    touchpadTimer_ = new QTimer(this);
    touchpadTimer_->setSingleShot(true);
    touchpadTimer_->setInterval(70);
    connect(touchpadTimer_, &QTimer::timeout, this, &MD3KineticScroller::onTouchpadTimeout);
}

MD3KineticScroller::~MD3KineticScroller() {
    if (target_) {
        target_->removeEventFilter(this);
    }
    if (frameTimer_) {
        frameTimer_->stop();
    }
    if (touchpadTimer_) {
        touchpadTimer_->stop();
    }
}

void MD3KineticScroller::setTarget(QQuickItem* item) {
    if (target_ != item) {
        if (target_) {
            target_->removeEventFilter(this);
        }
        stop();
        target_ = item;
        if (target_) {
            target_->installEventFilter(this);
        }
        setActive(false);
        emit targetChanged();
    }
}

void MD3KineticScroller::setSpeedMultiplier(qreal mult) {
    if (!qFuzzyCompare(speedMultiplier_, mult)) {
        speedMultiplier_ = mult;
        emit speedMultiplierChanged();
    }
}

void MD3KineticScroller::setWheelStep(qreal step) {
    if (!qFuzzyCompare(wheelStep_, step)) {
        wheelStep_ = step;
        emit wheelStepChanged();
    }
}

void MD3KineticScroller::setTouchpadGain(qreal gain) {
    if (!qFuzzyCompare(touchpadGain_, gain)) {
        touchpadGain_ = gain;
        emit touchpadGainChanged();
    }
}

void MD3KineticScroller::setFriction(qreal f) {
    if (!qFuzzyCompare(friction_, f)) {
        friction_ = f;
        emit frictionChanged();
    }
}

void MD3KineticScroller::setMaxVelocity(qreal vel) {
    if (!qFuzzyCompare(maxVelocity_, vel)) {
        maxVelocity_ = vel;
        emit maxVelocityChanged();
    }
}

void MD3KineticScroller::setActive(bool a) {
    if (active_ != a) {
        active_ = a;
        emit activeChanged();
    }
}

qreal MD3KineticScroller::minY() const {
    return target_ ? target_->property("originY").toReal() : 0.0;
}

qreal MD3KineticScroller::maxY() const {
    if (!target_) return 0.0;
    const qreal originY = target_->property("originY").toReal();
    const qreal contentHeight = target_->property("contentHeight").toReal();
    const qreal viewportHeight = target_->height();
    return std::max(originY, originY + contentHeight - viewportHeight);
}

qreal MD3KineticScroller::clampY(qreal y) const {
    return std::max(minY(), std::min(maxY(), y));
}

bool MD3KineticScroller::eventFilter(QObject* watched, QEvent* event) {
    if (watched == target_ && event->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(event);
        handleWheel(we->angleDelta().y(), we->pixelDelta().y());
        we->accept();
        return true;
    }
    return QQuickItem::eventFilter(watched, event);
}

void MD3KineticScroller::handleWheel(qreal angleDeltaY, qreal pixelDeltaY) {
    if (!target_) return;

    const qreal contentHeight = target_->property("contentHeight").toReal();
    const qreal viewportHeight = target_->height();
    if (contentHeight <= viewportHeight) return;

    if (std::abs(pixelDeltaY) > 0.001) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        qreal dt = (lastTouchpadMs_ > 0) ? (now - lastTouchpadMs_) / 1000.0 : 0.016;
        dt = std::max(0.004, std::min(0.06, dt));
        lastTouchpadMs_ = now;

        const qreal delta = pixelDeltaY * speedMultiplier_ * touchpadGain_;
        if (std::abs(delta) < 0.001) return;

        const qreal currentY = target_->property("contentY").toReal();
        const qreal nextY = clampY(currentY - delta);
        target_->setProperty("contentY", nextY);

        const qreal instVel = -delta / dt;
        if (!inTouchpadGesture_) {
            inTouchpadGesture_ = true;
            smoothTouchpadVel_ = instVel;
            stop();
            setActive(true);
        } else {
            smoothTouchpadVel_ = smoothTouchpadVel_ * 0.5 + instVel * 0.5;
        }

        touchpadTimer_->start(70);
        return;
    }

    if (inTouchpadGesture_) {
        onTouchpadTimeout();
    }

    if (std::abs(angleDeltaY) > 0.001) {
        const qreal notches = angleDeltaY / 120.0;
        const qreal impulseVel = -notches * (wheelStep_ * speedMultiplier_ * 6.0);

        qreal newVel = (velocity_ * 0.3) + impulseVel;
        newVel = std::clamp(newVel, -maxVelocity_, maxVelocity_);
        startGlide(newVel);
    }
}

void MD3KineticScroller::onTouchpadTimeout() {
    if (inTouchpadGesture_) {
        inTouchpadGesture_ = false;
        touchpadTimer_->stop();
        lastTouchpadMs_ = 0;

        if (std::abs(smoothTouchpadVel_) > 60.0) {
            const qreal cappedVel = std::clamp(smoothTouchpadVel_, -maxVelocity_, maxVelocity_);
            startGlide(cappedVel);
        } else {
            setActive(false);
        }
    }
}

void MD3KineticScroller::startGlide(qreal initialVelocity) {
    velocity_ = initialVelocity;
    frameClock_.restart();
    setActive(true);
    if (!frameTimer_->isActive()) {
        frameTimer_->start();
    }
}

void MD3KineticScroller::onFrameTick() {
    if (!active_ || !target_) {
        frameTimer_->stop();
        return;
    }

    qreal dt = frameClock_.restart() / 1000.0;
    if (dt <= 0.001 || dt > 0.05) dt = 0.016;

    velocity_ *= std::exp(-friction_ * dt);

    if (std::abs(velocity_) < 20.0) {
        stop();
        return;
    }

    const qreal currentY = target_->property("contentY").toReal();
    const qreal step = velocity_ * dt;
    const qreal newY = clampY(currentY + step);

    if (qFuzzyCompare(newY, currentY) || newY <= minY() || newY >= maxY()) {
        target_->setProperty("contentY", newY);
        stop();
        return;
    }

    target_->setProperty("contentY", newY);
}

void MD3KineticScroller::scrollTo(qreal y, int durationMs) {
    if (!target_) return;
    const qreal currentY = target_->property("contentY").toReal();
    const qreal diff = clampY(y) - currentY;
    if (std::abs(diff) < 1.0) return;

    const qreal durationSec = std::max(0.05, durationMs / 1000.0);
    const qreal neededVel = diff * friction_ / (1.0 - std::exp(-friction_ * durationSec));
    startGlide(neededVel);
}

void MD3KineticScroller::scrollBy(qreal deltaY, int durationMs) {
    if (!target_) return;
    const qreal currentY = target_->property("contentY").toReal();
    scrollTo(currentY + deltaY, durationMs);
}

void MD3KineticScroller::stop() {
    if (inTouchpadGesture_) {
        inTouchpadGesture_ = false;
        if (touchpadTimer_) {
            touchpadTimer_->stop();
        }
    }
    if (frameTimer_) {
        frameTimer_->stop();
    }
    velocity_ = 0.0;
    setActive(false);
}

} // namespace webclip
