#include "md3_slider.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/display_scale.hpp"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <algorithm>
#include <cmath>

namespace Ui {

Md3Slider::Md3Slider(QWidget* parent)
    : RpWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(webclip::scale::px(36));
}

Md3Slider::~Md3Slider() = default;

void Md3Slider::setValue(double val) {
    const double clamped = std::clamp(val, min_, max_);
    if (std::abs(value_ - clamped) > 1e-6) {
        value_ = clamped;
        valueChanges_.fire_copy(value_);
        emit valueChanged(value_);
        update();
    }
}

void Md3Slider::setRange(double min, double max) {
    if (min < max) {
        min_ = min;
        max_ = max;
        setValue(value_);
    }
}

void Md3Slider::setSteps(int steps) {
    steps_ = std::max(0, steps);
    update();
}

void Md3Slider::updateValueFromPos(int x) {
    const double trackMargin = webclip::scale::pxF(4.0);
    const double trackRadius = webclip::scale::pxF(8.0);
    const double trackW = width() - 2.0 * trackMargin;
    const double travelStart = trackMargin + trackRadius;
    const double travelW = trackW - 2.0 * trackRadius;
    if (travelW <= 0.0) return;

    const double ratio = std::clamp((static_cast<double>(x) - travelStart) / travelW, 0.0, 1.0);
    double newVal = min_ + ratio * (max_ - min_);

    if (steps_ > 1) {
        const double stepSize = (max_ - min_) / (steps_ - 1);
        newVal = min_ + std::round((newVal - min_) / stepSize) * stepSize;
    }

    setValue(newVal);
}

void Md3Slider::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        isDown_ = true;
        updateValueFromPos(e->pos().x());
    }
    RpWidget::mousePressEvent(e);
}

void Md3Slider::mouseMoveEvent(QMouseEvent* e) {
    if (isDown_) {
        updateValueFromPos(e->pos().x());
    }
    RpWidget::mouseMoveEvent(e);
}

void Md3Slider::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && isDown_) {
        isDown_ = false;
        update();
    }
    RpWidget::mouseReleaseEvent(e);
}

void Md3Slider::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const double trackMargin = webclip::scale::pxF(4.0);
    const double trackH = webclip::scale::pxF(16.0);
    const double trackRadius = webclip::scale::pxF(8.0);
    const double trackY = (height() - trackH) / 2.0;
    const double trackW = width() - 2.0 * trackMargin;
    const QRectF trackRect(trackMargin, trackY, trackW, trackH);

    const double travelStart = trackMargin + trackRadius;
    const double travelW = trackW - 2.0 * trackRadius;
    const double ratio = (max_ > min_) ? std::clamp((value_ - min_) / (max_ - min_), 0.0, 1.0) : 0.0;
    const double thumbX = travelStart + ratio * travelW;

    p.setPen(Qt::NoPen);
    p.setBrush(theme->secondaryContainer());
    p.drawRoundedRect(trackRect, trackRadius, trackRadius);

    if (thumbX > trackMargin) {
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(trackRect, trackRadius, trackRadius);
        p.setClipPath(clip);
        p.setBrush(theme->primary());
        p.drawRect(QRectF(trackMargin, trackY, thumbX - trackMargin, trackH));
        p.restore();
    }

    if (steps_ > 1) {
        const double dotRadius = webclip::scale::pxF(1.75);
        for (int i = 0; i < steps_; ++i) {
            const double dotRatio = static_cast<double>(i) / (steps_ - 1);
            const double dotX = travelStart + dotRatio * travelW;
            if (std::abs(dotX - thumbX) < webclip::scale::pxF(6.0)) continue;
            const bool isActive = (dotX <= thumbX);
            p.setBrush(isActive ? theme->onPrimary() : theme->onSecondaryContainer());
            p.drawEllipse(QPointF(dotX, height() / 2.0), dotRadius, dotRadius);
        }
    }

    const double thumbW = webclip::scale::pxF(6.0);
    const double thumbH = webclip::scale::pxF(26.0);
    const double thumbRadius = webclip::scale::pxF(3.0);
    const double thumbY = (height() - thumbH) / 2.0;
    const QRectF thumbRect(thumbX - thumbW / 2.0, thumbY, thumbW, thumbH);

    const QColor thumbCol = isDown_ ? theme->onPrimary() : (theme->isDark() ? QColor(QStringLiteral("#FFFFFF")) : theme->onSurface());
    p.setPen(Qt::NoPen);
    p.setBrush(thumbCol);
    p.drawRoundedRect(thumbRect, thumbRadius, thumbRadius);
}

} // namespace Ui
