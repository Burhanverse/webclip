#include "md3_icon_button.hpp"
#include "icon_loader.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QPainter>

namespace Ui {

Md3IconButton::Md3IconButton(
    QWidget* parent,
    const QString& iconName,
    int buttonSize,
    int iconSize
)
    : RippleButton(parent)
    , iconName_(iconName)
    , buttonSize_(buttonSize)
    , iconSize_(iconSize) {
    resize(sizeHint());
}

Md3IconButton::~Md3IconButton() = default;

void Md3IconButton::setIconName(const QString& iconName) {
    if (iconName_ != iconName) {
        iconName_ = iconName;
        update();
    }
}

void Md3IconButton::setIconColor(const QColor& color) {
    if (iconColor_ != color) {
        iconColor_ = color;
        update();
    }
}

void Md3IconButton::setCustomBgColor(const QColor& color) {
    if (customBgColor_ != color) {
        customBgColor_ = color;
        update();
    }
}

void Md3IconButton::setButtonSize(int buttonSize) {
    if (buttonSize_ != buttonSize) {
        buttonSize_ = buttonSize;
        resize(sizeHint());
        updateGeometry();
        update();
    }
}

void Md3IconButton::setIconSize(int iconSize) {
    if (iconSize_ != iconSize) {
        iconSize_ = iconSize;
        update();
    }
}

QColor Md3IconButton::effectiveIconColor() const {
    auto* theme = webclip::MD3Theme::instance();
    if (isDisabled()) {
        auto col = theme->onSurface();
        col.setAlphaF(0.38);
        return col;
    }
    if (iconColor_.isValid()) {
        return iconColor_;
    }
    return theme->onSurfaceVariant();
}

void Md3IconButton::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);

    const auto color = effectiveIconColor();
    const QRectF btnRect(0, 0, width(), height());

    // 1. Custom background
    if (customBgColor_ != Qt::transparent) {
        p.setPen(Qt::NoPen);
        p.setBrush(customBgColor_);
        p.drawEllipse(btnRect);
    }

    // 2. State layer
    if (!isDisabled() && (isOver() || isDown())) {
        const int stateAlpha = isDown() ? 36 : 20; // 0.14 vs 0.08
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(color.red(), color.green(), color.blue(), stateAlpha));
        p.drawEllipse(btnRect);
    }

    // 3. Ripple
    paintRipple(p, 0, 0, &color);

    // 4. Centered Icon
    if (!iconName_.isEmpty()) {
        const double x = (width() - iconSize_) / 2.0;
        const double y = (height() - iconSize_) / 2.0;
        IconLoader::paint(p, iconName_, QRectF(x, y, iconSize_, iconSize_), color);
    }
}

QImage Md3IconButton::prepareRippleMask() const {
    return RippleAnimation::EllipseMask(size());
}

QPoint Md3IconButton::prepareRippleStartPosition() const {
    return mapFromGlobal(QCursor::pos());
}

} // namespace Ui
