#include "animation.hpp"

#include <QtCore/QBasicTimer>
#include <QtCore/QDateTime>
#include <QtCore/QTimerEvent>
#include <algorithm>
#include <cmath>

namespace anim {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const transition linear = [](double delta, double dt) {
    return delta * dt;
};

const transition sineInOut = [](double delta, double dt) {
    return -(delta / 2.0) * (std::cos(M_PI * dt) - 1.0);
};

const transition halfSine = [](double delta, double dt) {
    return delta * std::sin(M_PI * dt / 2.0);
};

const transition easeOutBack = [](double delta, double dt) {
    constexpr double s = 1.70158;
    const double t = dt - 1.0;
    return delta * (t * t * ((s + 1.0) * t + s) + 1.0);
};

const transition easeInCubic = [](double delta, double dt) {
    return delta * dt * dt * dt;
};

const transition easeOutCubic = [](double delta, double dt) {
    const double t = dt - 1.0;
    return delta * (t * t * t + 1.0);
};

const transition easeOutQuint = [](double delta, double dt) {
    const double t = dt - 1.0;
    const double t2 = t * t;
    return delta * (t2 * t2 * t + 1.0);
};

QColor color(const QColor& a, const QColor& b, double b_ratio) {
    const double ratio = std::clamp(b_ratio, 0.0, 1.0);
    const double a_ratio = 1.0 - ratio;
    return QColor::fromRgbF(
        std::clamp(a.redF() * a_ratio + b.redF() * ratio, 0.0, 1.0),
        std::clamp(a.greenF() * a_ratio + b.greenF() * ratio, 0.0, 1.0),
        std::clamp(a.blueF() * a_ratio + b.blueF() * ratio, 0.0, 1.0),
        std::clamp(a.alphaF() * a_ratio + b.alphaF() * ratio, 0.0, 1.0)
    );
}

} // namespace anim

namespace Ui::Animations {

namespace {

class AnimationManager final : public QObject {
public:
    static AnimationManager& instance() {
        static AnimationManager manager;
        return manager;
    }

    void registerAnimation(Basic* anim) {
        if (!anim) return;
        if (std::find(active_.begin(), active_.end(), anim) == active_.end()) {
            active_.push_back(anim);
            if (!timer_.isActive()) {
                timer_.start(16, Qt::PreciseTimer, this);
            }
        }
    }

    void unregisterAnimation(Basic* anim) {
        auto it = std::find(active_.begin(), active_.end(), anim);
        if (it != active_.end()) {
            active_.erase(it);
            if (active_.empty()) {
                timer_.stop();
            }
        }
    }

protected:
    void timerEvent(QTimerEvent* event) override {
        if (event->timerId() != timer_.timerId()) {
            QObject::timerEvent(event);
            return;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        std::vector<Basic*> current = active_;

        for (auto* anim : current) {
            auto it = std::find(active_.begin(), active_.end(), anim);
            if (it != active_.end()) {
                const bool continueRunning = anim->tick(now);
                if (!continueRunning) {
                    unregisterAnimation(anim);
                }
            }
        }
    }

private:
    AnimationManager() = default;
    QBasicTimer timer_;
    std::vector<Basic*> active_;
};

} // namespace

Simple::~Simple() {
    stop();
}

Simple::Simple(Simple&& other) noexcept
    : active_(other.active_)
    , from_(other.from_)
    , to_(other.to_)
    , current_(other.current_)
    , durationMs_(other.durationMs_)
    , startMs_(other.startMs_)
    , transition_(std::move(other.transition_))
    , callback_(std::move(other.callback_))
    , finishedCallback_(std::move(other.finishedCallback_)) {
    other.stop();
    if (active_) {
        AnimationManager::instance().registerAnimation(this);
    }
}

Simple& Simple::operator=(Simple&& other) noexcept {
    if (this != &other) {
        stop();
        active_ = other.active_;
        from_ = other.from_;
        to_ = other.to_;
        current_ = other.current_;
        durationMs_ = other.durationMs_;
        startMs_ = other.startMs_;
        transition_ = std::move(other.transition_);
        callback_ = std::move(other.callback_);
        finishedCallback_ = std::move(other.finishedCallback_);
        other.stop();
        if (active_) {
            AnimationManager::instance().registerAnimation(this);
        }
    }
    return *this;
}

void Simple::start(
    CallbackFloat callback,
    double from,
    double to,
    int durationMs,
    anim::transition transition
) {
    callback_ = std::move(callback);
    from_ = from;
    to_ = to;
    current_ = from;
    durationMs_ = std::max(1, durationMs);
    transition_ = std::move(transition);
    startMs_ = QDateTime::currentMSecsSinceEpoch();
    active_ = true;

    AnimationManager::instance().registerAnimation(this);

    if (callback_) {
        callback_(current_);
    }
}

void Simple::start(
    CallbackVoid callback,
    double from,
    double to,
    int durationMs,
    anim::transition transition
) {
    start(
        [cb = std::move(callback)](double) {
            if (cb) cb();
        },
        from,
        to,
        durationMs,
        std::move(transition)
    );
}

void Simple::stop() {
    if (active_) {
        active_ = false;
        AnimationManager::instance().unregisterAnimation(this);
    }
}

bool Simple::tick(qint64 nowMs) {
    if (!active_) {
        return false;
    }

    const qint64 elapsed = nowMs - startMs_;
    const double dt = std::clamp(static_cast<double>(elapsed) / durationMs_, 0.0, 1.0);
    const double delta = to_ - from_;

    if (dt >= 1.0) {
        current_ = to_;
        active_ = false;
        if (callback_) {
            callback_(current_);
        }
        if (finishedCallback_) {
            finishedCallback_();
        }
        return false;
    }

    const double progress = transition_ ? transition_(delta, dt) : (delta * dt);
    current_ = from_ + progress;

    if (callback_) {
        callback_(current_);
    }

    return true;
}

} // namespace Ui::Animations
