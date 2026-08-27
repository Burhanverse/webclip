#include "md3_switch.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <cmath>

namespace Ui {

void PaintMd3Switch(
    QPainter& p,
    double x,
    double y,
    double toggled,
    double switchWidth,
    double switchHeight,
    bool enabled
) {
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const double trackRadius = switchHeight / 2.0;
    const QRectF trackRect(x, y, switchWidth, switchHeight);

    const double baseOpacity = enabled ? 1.0 : 0.38;
    ScopedPainterOpacity baseOpacityScope(p, p.opacity() * baseOpacity);

    // 1. Unchecked track background & outline
    if (toggled < 1.0) {
        ScopedPainterOpacity trackScope(p, p.opacity() * (1.0 - toggled));
        p.setPen(QPen(theme->outline(), 2.0));
        p.setBrush(theme->surfaceContainerHighest());
        p.drawRoundedRect(
            trackRect.adjusted(1.0, 1.0, -1.0, -1.0),
            trackRadius - 1.0,
            trackRadius - 1.0
        );
    }

    // 2. Checked track background
    if (toggled > 0.0) {
        ScopedPainterOpacity trackScope(p, p.opacity() * toggled);
        p.setPen(Qt::NoPen);
        p.setBrush(theme->primary());
        p.drawRoundedRect(trackRect, trackRadius, trackRadius);
    }

    // 3. Thumb geometry & color interpolation
    // Unchecked: diameter 16, centered at margin 8 from left -> x + 8
    // Checked: diameter 24, margin 4 from right -> x + switchWidth - 28
    const double thumbDiameter = anim::interpolateF(16.0, 24.0, toggled);
    const double startX = x + 8.0;
    const double endX = x + switchWidth - 28.0;
    const double thumbX = anim::interpolateF(startX, endX, toggled);
    const double thumbY = y + (switchHeight - thumbDiameter) / 2.0;

    const QColor thumbColor = anim::color(theme->outline(), theme->onPrimary(), toggled);

    p.setPen(Qt::NoPen);
    p.setBrush(thumbColor);
    p.drawEllipse(QRectF(thumbX, thumbY, thumbDiameter, thumbDiameter));

    // 4. Micro checkmark inside checked thumb
    if (toggled > 0.05) {
        ScopedPainterOpacity checkScope(p, p.opacity() * toggled);
        const double cx = thumbX + thumbDiameter / 2.0;
        const double cy = thumbY + thumbDiameter / 2.0;
        const double scale = thumbDiameter / 24.0;

        QPainterPath check;
        check.moveTo(cx - 3.8 * scale, cy + 0.2 * scale);
        check.lineTo(cx - 1.0 * scale, cy + 3.2 * scale);
        check.lineTo(cx + 4.2 * scale, cy - 2.8 * scale);

        p.setPen(QPen(theme->primary(), 2.0 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(check);
    }
}

Md3Switch::Md3Switch(QWidget* parent, bool checked)
    : RippleButton(parent)
    , checked_(checked) {
    addClickHandler([this] {
        setChecked(!checked_);
    });
    resize(sizeHint());
}

Md3Switch::~Md3Switch() = default;

void Md3Switch::setChecked(bool checked, anim::type animated) {
    if (checked_ == checked && !animation_.animating()) {
        return;
    }
    checked_ = checked;
    checkedChanges_.fire_copy(checked_);
    emit toggled(checked_);

    if (animated == anim::type::instant) {
        animation_.stop();
        update();
    } else {
        animation_.start(
            [this] { update(); },
            checked_ ? 0.0 : 1.0,
            checked_ ? 1.0 : 0.0,
            180,
            anim::easeOutCubic
        );
    }
}

void Md3Switch::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    paintRipple(p, 0, 0);

    const double switchW = 52.0;
    const double switchH = 32.0;
    const double x = (width() - switchW) / 2.0;
    const double y = (height() - switchH) / 2.0;
    const double toggledProgress = animation_.value(checked_ ? 1.0 : 0.0);

    PaintMd3Switch(p, x, y, toggledProgress, switchW, switchH, isEnabled());
}

QImage Md3Switch::prepareRippleMask() const {
    const double switchW = 52.0;
    const double switchH = 32.0;
    const double x = (width() - switchW) / 2.0;
    const double y = (height() - switchH) / 2.0;

    return RippleAnimation::MaskByDrawer(size(), false, [&](QPainter& p) {
        PainterHighQualityEnabler hq(p);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(x, y, switchW, switchH), switchH / 2.0, switchH / 2.0);
    });
}

QPoint Md3Switch::prepareRippleStartPosition() const {
    return QPoint(width() / 2, height() / 2);
}

} // namespace Ui
