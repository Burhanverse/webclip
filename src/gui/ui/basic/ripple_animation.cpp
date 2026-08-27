#include "ripple_animation.hpp"
#include "animation.hpp"
#include "painter_helpers.hpp"

#include <QtGui/QPainter>
#include <QtGui/QRadialGradient>
#include <cmath>

namespace Ui {

namespace {

uint32_t fastPrng(uint32_t& state) {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state = x;
}

} // namespace

class RippleAnimation::Ripple {
public:
    Ripple(
        const RippleConfig& config,
        QPoint origin,
        int startRadius,
        const QPixmap& mask,
        std::function<void()> update
    )
        : config_(config)
        , update_(std::move(update))
        , origin_(origin)
        , radiusFrom_(startRadius)
        , frame_(mask.size(), QImage::Format_ARGB32_Premultiplied) {
        frame_.setDevicePixelRatio(mask.devicePixelRatio());

        const auto dpr = mask.devicePixelRatio();
        const QPoint points[] = {
            { 0, 0 },
            { static_cast<int>(frame_.width() / dpr), 0 },
            { static_cast<int>(frame_.width() / dpr), static_cast<int>(frame_.height() / dpr) },
            { 0, static_cast<int>(frame_.height() / dpr) },
        };
        for (const auto& point : points) {
            const auto dx = origin_.x() - point.x();
            const auto dy = origin_.y() - point.y();
            radiusTo_ = std::max(radiusTo_, dx * dx + dy * dy);
        }
        radiusTo_ = static_cast<int>(std::round(std::sqrt(static_cast<double>(radiusTo_)) / 0.55));

        if (config_.useNoiseDither) {
            const auto w = frame_.width();
            const auto h = frame_.height();
            noiseBlockW_ = (w + 1) / 2;
            noiseBlockH_ = (h + 1) / 2;
            noisePattern_.resize(noiseBlockW_ * noiseBlockH_);
            uint32_t seed = 0x85ebca6b ^ static_cast<uint32_t>(origin_.x() * 37 + origin_.y());
            for (auto& val : noisePattern_) {
                val = static_cast<uint8_t>(fastPrng(seed) & 0xFF);
            }
        }

        show_.start(update_, 0.0, 1.0, config_.showDuration, anim::easeOutQuint);
    }

    Ripple(
        const RippleConfig& config,
        const QPixmap& mask,
        std::function<void()> update
    )
        : config_(config)
        , update_(std::move(update))
        , origin_(
            static_cast<int>(mask.width() / (2 * mask.devicePixelRatio())),
            static_cast<int>(mask.height() / (2 * mask.devicePixelRatio()))
        )
        , radiusFrom_(mask.width() + mask.height())
        , frame_(mask.size(), QImage::Format_ARGB32_Premultiplied) {
        frame_.setDevicePixelRatio(mask.devicePixelRatio());
        radiusTo_ = radiusFrom_;
        hide_.start(update_, 0.0, 1.0, config_.hideDuration);
    }

    void stop() {
        if (!hiding_) {
            hiding_ = true;
            hide_.start(update_, 0.0, 1.0, config_.hideDuration);
        }
    }

    void unstop() {
        if (hiding_) {
            hiding_ = false;
            hide_.stop();
        }
    }

    void finish() {
        show_.stop();
        hide_.stop();
        hiding_ = true;
    }

    [[nodiscard]] bool finished() const {
        return hiding_ && !hide_.animating();
    }

    void paint(
        QPainter& p,
        const QPixmap& mask,
        const QColor* colorOverride
    ) {
        const double opacity = hide_.value(hiding_ ? 0.0 : 1.0);
        if (opacity <= 0.0) return;

        if (cache_.isNull() || colorOverride != nullptr) {
            const double shown = show_.value(1.0);
            const double diff = static_cast<double>(radiusTo_ - radiusFrom_);
            const int radius = std::max(1, static_cast<int>(std::round(radiusFrom_ + diff * shown)));

            frame_.fill(Qt::transparent);
            {
                QPainter fp(&frame_);
                fp.setPen(Qt::NoPen);
                const QColor color = colorOverride ? *colorOverride : config_.color;
                const double safeRadius = std::max(radius, 1);
                const double edgeWidth = 56.0;
                const double innerStop = std::max(0.0, 1.0 - edgeWidth / safeRadius);

                QRadialGradient gradient(QPointF(origin_), safeRadius);
                gradient.setColorAt(0.0, color);
                gradient.setColorAt(innerStop, color);
                gradient.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));
                fp.setBrush(gradient);
                {
                    PainterHighQualityEnabler hq(fp);
                    fp.drawEllipse(origin_, radius, radius);
                }

                if (config_.useNoiseDither && !noisePattern_.empty()) {
                    QImage noise(frame_.size(), QImage::Format_ARGB32_Premultiplied);
                    noise.fill(Qt::transparent);
                    noise.setDevicePixelRatio(frame_.devicePixelRatio());

                    const QColor light(
                        color.red() + (255 - color.red()) * 3 / 10,
                        color.green() + (255 - color.green()) * 3 / 10,
                        color.blue() + (255 - color.blue()) * 3 / 10
                    );

                    auto* noisePixels = reinterpret_cast<uint32_t*>(noise.bits());
                    const int bpl = noise.bytesPerLine() / 4;
                    const int w = frame_.width();
                    const int h = frame_.height();

                    for (int by = 0; by < noiseBlockH_; ++by) {
                        for (int bx = 0; bx < noiseBlockW_; ++bx) {
                            const uint8_t random = noisePattern_[by * noiseBlockW_ + bx];
                            if (random > 230) {
                                const int alpha = (random - 230) * 80 / 25;
                                const uint32_t pixel = qPremultiply(qRgba(
                                    light.red(), light.green(), light.blue(), alpha
                                ));
                                const int y0 = by * 2;
                                const int x0 = bx * 2;
                                noisePixels[y0 * bpl + x0] = pixel;
                                if (x0 + 1 < w) noisePixels[y0 * bpl + x0 + 1] = pixel;
                                if (y0 + 1 < h) {
                                    noisePixels[(y0 + 1) * bpl + x0] = pixel;
                                    if (x0 + 1 < w) noisePixels[(y0 + 1) * bpl + x0 + 1] = pixel;
                                }
                            }
                        }
                    }

                    {
                        const double dpr = frame_.devicePixelRatio();
                        const double logicalW = w / dpr;
                        const double logicalH = h / dpr;
                        QPainter noisePainter(&noise);
                        const double ringInward = 16.0;
                        const double ringFade = 8.0;
                        const double ringFullStop = std::max(0.01, innerStop - ringInward / safeRadius);
                        const double ringFadeStop = std::max(0.0, innerStop - (ringInward + ringFade) / safeRadius);

                        QRadialGradient noiseMask(QPointF(origin_), safeRadius);
                        noiseMask.setColorAt(0.0, QColor(0, 0, 0, 0));
                        if (ringFadeStop + 0.001 < ringFullStop) {
                            noiseMask.setColorAt(ringFadeStop, QColor(0, 0, 0, 0));
                        }
                        noiseMask.setColorAt(ringFullStop, QColor(0, 0, 0, 255));
                        noiseMask.setColorAt(1.0, QColor(0, 0, 0, 255));

                        noisePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                        noisePainter.setPen(Qt::NoPen);
                        noisePainter.setBrush(noiseMask);
                        noisePainter.drawRect(QRectF(0, 0, logicalW, logicalH));
                    }

                    fp.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                    fp.drawImage(0, 0, noise);
                }

                fp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                fp.drawPixmap(0, 0, mask);
            }

            if (radius == radiusTo_ && colorOverride == nullptr) {
                cache_ = QPixmap::fromImage(frame_);
            }
        }

        const double savedOpacity = p.opacity();
        if (opacity < 1.0) p.setOpacity(savedOpacity * opacity);
        if (cache_.isNull()) {
            p.drawImage(0, 0, frame_);
        } else {
            p.drawPixmap(0, 0, cache_);
        }
        if (opacity < 1.0) p.setOpacity(savedOpacity);
    }

private:
    const RippleConfig config_;
    std::function<void()> update_;
    QPoint origin_;
    int radiusFrom_ = 0;
    int radiusTo_ = 0;
    bool hiding_ = false;

    Ui::Animations::Simple show_;
    Ui::Animations::Simple hide_;
    QPixmap cache_;
    QImage frame_;
    std::vector<uint8_t> noisePattern_;
    int noiseBlockW_ = 0;
    int noiseBlockH_ = 0;
};

RippleAnimation::RippleAnimation(
    const RippleConfig& config,
    QImage mask,
    std::function<void()> update
)
    : config_(config)
    , mask_(QPixmap::fromImage(std::move(mask)))
    , update_(std::move(update)) {
}

RippleAnimation::~RippleAnimation() = default;

void RippleAnimation::add(QPoint origin, int startRadius) {
    clearFinished();
    ripples_.push_back(std::make_unique<Ripple>(
        config_, origin, startRadius, mask_, update_
    ));
}

void RippleAnimation::addFading() {
    clearFinished();
    ripples_.push_back(std::make_unique<Ripple>(
        config_, mask_, update_
    ));
}

void RippleAnimation::lastStop() {
    if (!ripples_.empty()) {
        ripples_.back()->stop();
    }
}

void RippleAnimation::lastUnstop() {
    if (!ripples_.empty()) {
        ripples_.back()->unstop();
    }
}

void RippleAnimation::lastFinish() {
    if (!ripples_.empty()) {
        ripples_.back()->finish();
    }
}

void RippleAnimation::forceRepaint() {
    if (update_) update_();
}

void RippleAnimation::paint(
    QPainter& p,
    int x,
    int y,
    int outerWidth,
    const QColor* colorOverride
) {
    Q_UNUSED(outerWidth);
    clearFinished();
    if (ripples_.empty()) return;

    p.translate(x, y);
    for (const auto& ripple : ripples_) {
        ripple->paint(p, mask_, colorOverride);
    }
    p.translate(-x, -y);
}

bool RippleAnimation::empty() const {
    return ripples_.empty();
}

void RippleAnimation::clearFinished() {
    while (!ripples_.empty() && ripples_.front()->finished()) {
        ripples_.pop_front();
    }
}

QImage RippleAnimation::MaskByDrawer(
    QSize size,
    bool filled,
    std::function<void(QPainter&)> drawer
) {
    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    result.fill(filled ? Qt::white : Qt::transparent);
    if (drawer) {
        QPainter p(&result);
        drawer(p);
    }
    return result;
}

QImage RippleAnimation::RectMask(QSize size) {
    return MaskByDrawer(size, true, nullptr);
}

QImage RippleAnimation::RoundRectMask(QSize size, int radius) {
    return MaskByDrawer(size, false, [&](QPainter& p) {
        PainterHighQualityEnabler hq(p);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(0, 0, size.width(), size.height()), radius, radius);
    });
}

QImage RippleAnimation::EllipseMask(QSize size) {
    return MaskByDrawer(size, false, [&](QPainter& p) {
        PainterHighQualityEnabler hq(p);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(0, 0, size.width(), size.height()));
    });
}

} // namespace Ui
