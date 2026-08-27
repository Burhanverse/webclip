#include "md3_badge.hpp"
#include "icon_loader.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace Ui {

Md3Badge::Md3Badge(QWidget* parent, const QString& text, const QString& iconName)
    : RpWidget(parent)
    , text_(text)
    , iconName_(iconName) {
    setFont(webclip::MD3Theme::instance()->labelSmall());
    setFixedHeight(28);
}

Md3Badge::~Md3Badge() = default;

void Md3Badge::setText(const QString& text) {
    if (text_ != text) {
        text_ = text;
        updateGeometry();
        update();
    }
}

void Md3Badge::setIconName(const QString& iconName) {
    if (iconName_ != iconName) {
        iconName_ = iconName;
        updateGeometry();
        update();
    }
}

void Md3Badge::setBadgeColor(const QColor& color) {
    if (badgeColor_ != color) {
        badgeColor_ = color;
        update();
    }
}

void Md3Badge::setTextColor(const QColor& color) {
    if (textColor_ != color) {
        textColor_ = color;
        update();
    }
}

QSize Md3Badge::sizeHint() const {
    const QFontMetrics fm(font());
    int contentW = 0;
    if (!iconName_.isEmpty()) {
        contentW += 14;
        if (!text_.isEmpty()) contentW += 6;
    }
    if (!text_.isEmpty()) {
        contentW += fm.horizontalAdvance(text_);
    }
    return QSize(contentW + 20, 28);
}

void Md3Badge::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const QColor bg = badgeColor_.isValid() ? badgeColor_ : theme->secondaryContainer();
    const QColor fg = textColor_.isValid() ? textColor_ : theme->onSecondaryContainer();

    // 1. Draw capsule background
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), 14.0, 14.0);

    // 2. Centered content (icon + text)
    const QFontMetrics fm(font());
    const int iconSize = 14;
    const int spacing = (!iconName_.isEmpty() && !text_.isEmpty()) ? 6 : 0;
    const int textW = text_.isEmpty() ? 0 : fm.horizontalAdvance(text_);
    const int totalW = (iconName_.isEmpty() ? 0 : iconSize) + spacing + textW;

    double curX = (width() - totalW) / 2.0;

    if (!iconName_.isEmpty()) {
        const double iconY = (height() - iconSize) / 2.0;
        IconLoader::paint(p, iconName_, QRectF(curX, iconY, iconSize, iconSize), fg);
        curX += iconSize + spacing;
    }

    if (!text_.isEmpty()) {
        p.setFont(font());
        p.setPen(fg);
        const double textY = (height() - fm.height()) / 2.0 + fm.ascent();
        p.drawText(QPointF(curX, textY), text_);
    }
}

} // namespace Ui
