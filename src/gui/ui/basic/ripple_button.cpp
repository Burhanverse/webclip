#include "ripple_button.hpp"

#include <QtGui/QCursor>
#include <QtGui/QMouseEvent>

namespace Ui {

RippleButton::RippleButton(QWidget* parent, RippleConfig config)
    : RpWidget(parent)
    , config_(std::move(config)) {
    setCursor(Qt::PointingHandCursor);
}

RippleButton::~RippleButton() = default;

void RippleButton::setRippleConfig(const RippleConfig& config) {
    config_ = config;
    ripple_.reset();
}

void RippleButton::ensureRipple() {
    if (!ripple_) {
        ripple_ = std::make_unique<RippleAnimation>(
            config_,
            prepareRippleMask(),
            [this] { update(); }
        );
    }
}

QImage RippleButton::prepareRippleMask() const {
    return RippleAnimation::RoundRectMask(size(), 8);
}

QPoint RippleButton::prepareRippleStartPosition() const {
    return mapFromGlobal(QCursor::pos());
}

void RippleButton::paintRipple(
    QPainter& p,
    int x,
    int y,
    const QColor* colorOverride
) {
    if (ripple_) {
        ripple_->paint(p, x, y, width(), colorOverride);
    }
}

void RippleButton::enterEvent(QEnterEvent* e) {
    RpWidget::enterEvent(e);
    if (!isDisabled()) {
        isOver_ = true;
        update();
    }
}

void RippleButton::leaveEvent(QEvent* e) {
    RpWidget::leaveEvent(e);
    if (isOver_ || isDown_) {
        isOver_ = false;
        isDown_ = false;
        if (ripple_) {
            ripple_->lastStop();
        }
        update();
    }
}

void RippleButton::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && !isDisabled()) {
        e->accept();
        isDown_ = true;
        ensureRipple();
        ripple_->add(prepareRippleStartPosition());
        update();
        return;
    }
    RpWidget::mousePressEvent(e);
}

void RippleButton::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && isDown_) {
        e->accept();
        isDown_ = false;
        if (ripple_) {
            ripple_->lastStop();
        }
        update();

        if (rect().contains(e->pos()) && !isDisabled()) {
            for (const auto& handler : clickHandlers_) {
                if (handler) {
                    handler();
                }
            }
        }
        return;
    }
    RpWidget::mouseReleaseEvent(e);
}

void RippleButton::changeEvent(QEvent* e) {
    RpWidget::changeEvent(e);
    if (e->type() == QEvent::EnabledChange) {
        if (isDisabled()) {
            isOver_ = false;
            isDown_ = false;
            if (ripple_) {
                ripple_->lastFinish();
            }
        }
        update();
    }
}

} // namespace Ui
