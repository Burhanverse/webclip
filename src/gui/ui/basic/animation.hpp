#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QRectF>
#include <QtGui/QColor>
#include <functional>
#include <memory>
#include <vector>

namespace anim {

enum class type : unsigned char {
    normal,
    instant,
};

using transition = std::function<double(double delta, double dt)>;

extern const transition linear;
extern const transition sineInOut;
extern const transition halfSine;
extern const transition easeOutBack;
extern const transition easeOutCubic;
extern const transition easeInCubic;
extern const transition easeOutQuint;

inline double interpolateF(double a, double b, double b_ratio) {
    return a + (b - a) * b_ratio;
}

inline QRectF interpolatedRectF(const QRectF& r1, const QRectF& r2, double ratio) {
    return QRectF(
        interpolateF(r1.x(), r2.x(), ratio),
        interpolateF(r1.y(), r2.y(), ratio),
        interpolateF(r1.width(), r2.width(), ratio),
        interpolateF(r1.height(), r2.height(), ratio)
    );
}

QColor color(const QColor& a, const QColor& b, double b_ratio);

} // namespace anim

namespace Ui::Animations {

class Basic {
public:
    virtual ~Basic() = default;
    virtual bool tick(qint64 nowMs) = 0;
};

class Simple final : public Basic {
public:
    using CallbackFloat = std::function<void(double)>;
    using CallbackVoid = std::function<void()>;

    Simple() = default;
    ~Simple() override;

    Simple(const Simple&) = delete;
    Simple& operator=(const Simple&) = delete;
    Simple(Simple&& other) noexcept;
    Simple& operator=(Simple&& other) noexcept;

    void start(
        CallbackFloat callback,
        double from,
        double to,
        int durationMs,
        anim::transition transition = anim::easeOutCubic
    );

    void start(
        CallbackVoid callback,
        double from,
        double to,
        int durationMs,
        anim::transition transition = anim::easeOutCubic
    );

    void stop();

    [[nodiscard]] bool animating() const noexcept {
        return active_;
    }

    [[nodiscard]] double value(double fallback) const noexcept {
        return active_ ? current_ : fallback;
    }

    void setFinishedCallback(std::function<void()> callback) {
        finishedCallback_ = std::move(callback);
    }

    bool tick(qint64 nowMs) override;

private:
    bool active_ = false;
    double from_ = 0.0;
    double to_ = 0.0;
    double current_ = 0.0;
    int durationMs_ = 0;
    qint64 startMs_ = 0;
    anim::transition transition_;
    CallbackFloat callback_;
    std::function<void()> finishedCallback_;
};

} // namespace Ui::Animations
