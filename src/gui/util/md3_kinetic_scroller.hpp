#pragma once

#include <QQuickItem>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

namespace webclip {

class MD3KineticScroller : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(MD3KineticScroller)

    Q_PROPERTY(QQuickItem* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(qreal speedMultiplier READ speedMultiplier WRITE setSpeedMultiplier NOTIFY speedMultiplierChanged)
    Q_PROPERTY(qreal wheelStep READ wheelStep WRITE setWheelStep NOTIFY wheelStepChanged)
    Q_PROPERTY(qreal touchpadGain READ touchpadGain WRITE setTouchpadGain NOTIFY touchpadGainChanged)
    Q_PROPERTY(qreal friction READ friction WRITE setFriction NOTIFY frictionChanged)
    Q_PROPERTY(qreal maxVelocity READ maxVelocity WRITE setMaxVelocity NOTIFY maxVelocityChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)

public:
    explicit MD3KineticScroller(QQuickItem* parent = nullptr);
    ~MD3KineticScroller() override;

    QQuickItem* target() const { return target_.data(); }
    void setTarget(QQuickItem* item);

    qreal speedMultiplier() const { return speedMultiplier_; }
    void setSpeedMultiplier(qreal mult);

    qreal wheelStep() const { return wheelStep_; }
    void setWheelStep(qreal step);

    qreal touchpadGain() const { return touchpadGain_; }
    void setTouchpadGain(qreal gain);

    qreal friction() const { return friction_; }
    void setFriction(qreal f);

    qreal maxVelocity() const { return maxVelocity_; }
    void setMaxVelocity(qreal vel);

    bool isActive() const { return active_; }

    Q_INVOKABLE void handleWheel(qreal angleDeltaY, qreal pixelDeltaY);
    Q_INVOKABLE void scrollTo(qreal y, int durationMs = 250);
    Q_INVOKABLE void scrollBy(qreal deltaY, int durationMs = 150);
    Q_INVOKABLE void stop();

signals:
    void targetChanged();
    void speedMultiplierChanged();
    void wheelStepChanged();
    void touchpadGainChanged();
    void frictionChanged();
    void maxVelocityChanged();
    void activeChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTouchpadTimeout();
    void onFrameTick();

private:
    void startGlide(qreal initialVelocity);
    void setActive(bool a);
    qreal minY() const;
    qreal maxY() const;
    qreal clampY(qreal y) const;

    QPointer<QQuickItem> target_;
    QTimer* frameTimer_ = nullptr;
    QTimer* touchpadTimer_ = nullptr;
    QElapsedTimer frameClock_;

    qreal speedMultiplier_ = 2.0;
    qreal wheelStep_ = 120.0;
    qreal touchpadGain_ = 1.0;
    qreal friction_ = 4.2;
    qreal maxVelocity_ = 10000.0;
    bool active_ = false;

    qreal velocity_ = 0.0;
    bool inTouchpadGesture_ = false;
    qint64 lastTouchpadMs_ = 0;
    qreal smoothTouchpadVel_ = 0.0;
};

} // namespace webclip
