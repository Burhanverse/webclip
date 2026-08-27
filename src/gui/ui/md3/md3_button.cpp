#include "md3_button.hpp"
#include "icon_loader.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace Ui {

Md3Button::Md3Button(QWidget* parent, const QString& text, ButtonVariant variant)
    : RippleButton(parent)
    , text_(text)
    , variant_(variant) {
    setFont(webclip::MD3Theme::instance()->labelLarge());
    setFixedHeight(40);
}

Md3Button::~Md3Button() = default;

void Md3Button::setText(const QString& text) {
    if (text_ != text) {
        text_ = text;
        updateGeometry();
        update();
    }
}

void Md3Button::setIconName(const QString& iconName) {
    if (iconName_ != iconName) {
        iconName_ = iconName;
        updateGeometry();
        update();
    }
}

void Md3Button::setVariant(ButtonVariant variant) {
    if (variant_ != variant) {
        variant_ = variant;
        update();
    }
}

QColor Md3Button::contentColor() const {
    auto* theme = webclip::MD3Theme::instance();
    if (isDisabled()) {
        auto col = theme->onSurface();
        col.setAlphaF(0.38);
        return col;
    }
    switch (variant_) {
    case ButtonVariant::Filled:
        return theme->onPrimary();
    case ButtonVariant::Tonal:
        return theme->onSecondaryContainer();
    case ButtonVariant::Outlined:
    case ButtonVariant::Text:
        return theme->primary();
    }
    return theme->primary();
}

QColor Md3Button::buttonBgColor() const {
    auto* theme = webclip::MD3Theme::instance();
    if (isDisabled()) {
        if (variant_ == ButtonVariant::Text || variant_ == ButtonVariant::Outlined) {
            return Qt::transparent;
        }
        auto col = theme->onSurface();
        col.setAlphaF(0.12);
        return col;
    }
    switch (variant_) {
    case ButtonVariant::Filled:
        return theme->primary();
    case ButtonVariant::Tonal:
        return theme->secondaryContainer();
    case ButtonVariant::Outlined:
    case ButtonVariant::Text:
        return Qt::transparent;
    }
    return theme->primary();
}

QSize Md3Button::sizeHint() const {
    const QFontMetrics fm(font());
    int contentWidth = 0;

    if (!iconName_.isEmpty()) {
        contentWidth += 18; // icon width
        if (!text_.isEmpty()) {
            contentWidth += 8; // spacing
        }
    }
    if (!text_.isEmpty()) {
        contentWidth += fm.horizontalAdvance(text_);
    }

    const int hPadding = (variant_ == ButtonVariant::Text) ? 24 : 32;
    return QSize(std::max(64, contentWidth + hPadding), 40);
}

void Md3Button::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);

    const auto radius = height() / 2.0;
    const QRectF btnRect(0.5, 0.5, width() - 1.0, height() - 1.0);
    const QColor bgCol = buttonBgColor();
    const QColor contentCol = contentColor();

    // 1. Button background
    if (bgCol != Qt::transparent) {
        p.setPen(Qt::NoPen);
        p.setBrush(bgCol);
        p.drawRoundedRect(btnRect, radius, radius);
    }

    // 2. Outlined border
    if (variant_ == ButtonVariant::Outlined) {
        auto* theme = webclip::MD3Theme::instance();
        const QColor borderColor = isDisabled()
            ? QColor::fromRgbF(theme->onSurface().redF(), theme->onSurface().greenF(), theme->onSurface().blueF(), 0.12)
            : (isDown() ? theme->primary() : theme->outline());
        p.setPen(QPen(borderColor, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(btnRect, radius, radius);
    }

    // 3. State layer overlay
    if (!isDisabled() && (isOver() || isDown())) {
        const int stateAlpha = isDown() ? 36 : 20; // 0.14 vs 0.08
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(contentCol.red(), contentCol.green(), contentCol.blue(), stateAlpha));
        p.drawRoundedRect(btnRect, radius, radius);
    }

    // 4. Click ripple
    paintRipple(p, 0, 0, &contentCol);

    // 5. Content layout (Icon + Text centered)
    const QFontMetrics fm(font());
    const int iconSize = 18;
    const int spacing = (!iconName_.isEmpty() && !text_.isEmpty()) ? 8 : 0;
    const int textWidth = text_.isEmpty() ? 0 : fm.horizontalAdvance(text_);
    const int totalContentWidth = (iconName_.isEmpty() ? 0 : iconSize) + spacing + textWidth;

    const double startX = (width() - totalContentWidth) / 2.0;
    double currentX = startX;

    if (!iconName_.isEmpty()) {
        const double iconY = (height() - iconSize) / 2.0;
        IconLoader::paint(p, iconName_, QRectF(currentX, iconY, iconSize, iconSize), contentCol);
        currentX += iconSize + spacing;
    }

    if (!text_.isEmpty()) {
        p.setFont(font());
        p.setPen(contentCol);
        const double textY = (height() - fm.height()) / 2.0 + fm.ascent();
        p.drawText(QPointF(currentX, textY), text_);
    }
}

QImage Md3Button::prepareRippleMask() const {
    const auto radius = height() / 2.0;
    return RippleAnimation::MaskByDrawer(size(), false, [&](QPainter& p) {
        PainterHighQualityEnabler hq(p);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(0, 0, width(), height()), radius, radius);
    });
}

QPoint Md3Button::prepareRippleStartPosition() const {
    return mapFromGlobal(QCursor::pos());
}

} // namespace Ui
