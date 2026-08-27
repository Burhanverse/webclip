#include "md3_slider.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <algorithm>
#include <cmath>

namespace Ui {

Md3Slider::Md3Slider(QWidget* parent)
    : RpWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(36);
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
    const int margin = 12;
    const int trackW = width() - 2 * margin;
    if (trackW <= 0) return;

    const double ratio = std::clamp(static_cast<double>(x - margin) / trackW, 0.0, 1.0);
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

    const int margin = 12;
    const int trackH = 16;
    const int trackY = (height() - trackH) / 2;
    const int trackW = width() - 2 * margin;
    const double ratio = (max_ > min_) ? std::clamp((value_ - min_) / (max_ - min_), 0.0, 1.0) : 0.0;
    const double thumbX = margin + ratio * trackW;

    // 1. Inactive track (right side)
    p.setPen(Qt::NoPen);
    p.setBrush(theme->secondaryContainer());
    p.drawRoundedRect(QRectF(margin, trackY, trackW, trackH), 8.0, 8.0);

    // 2. Active track (left side)
    if (thumbX > margin) {
        p.setBrush(theme->primary());
        p.drawRoundedRect(QRectF(margin, trackY, thumbX - margin, trackH), 8.0, 8.0);
    }

    // 3. Discrete notches
    if (steps_ > 1) {
        p.setBrush(theme->onSecondaryContainer());
        for (int i = 0; i < steps_; ++i) {
            const double dotRatio = static_cast<double>(i) / (steps_ - 1);
            const double dotX = margin + dotRatio * trackW;
            // Draw 4px dot
            p.drawEllipse(QPointF(dotX, height() / 2.0), 2.0, 2.0);
        }
    }

    // 4. Thumb handle (4px wide bar, height 44px, radius 2px per MD3 spec)
    const int thumbW = 4;
    const int thumbH = 28;
    const int thumbY = (height() - thumbH) / 2;
    const QRectF thumbRect(thumbX - thumbW / 2.0, thumbY, thumbW, thumbH);

    p.setPen(Qt::NoPen);
    p.setBrush(theme->primary());
    p.drawRoundedRect(thumbRect, 2.0, 2.0);
}

} // namespace Ui
