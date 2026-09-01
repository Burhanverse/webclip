#include "md3_card.hpp"
#include "icon_loader.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/display_scale.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>

namespace Ui {

QPainterPath MakeSegmentPath(
    const QRectF& r,
    CardSegmentPosition pos,
    double largeRadius,
    double smallRadius
) {
    const double rtl = (pos == CardSegmentPosition::Top || pos == CardSegmentPosition::Single)
        ? largeRadius : smallRadius;
    const double rtr = (pos == CardSegmentPosition::Top || pos == CardSegmentPosition::Single)
        ? largeRadius : smallRadius;
    const double rbr = (pos == CardSegmentPosition::Bottom || pos == CardSegmentPosition::Single)
        ? largeRadius : smallRadius;
    const double rbl = (pos == CardSegmentPosition::Bottom || pos == CardSegmentPosition::Single)
        ? largeRadius : smallRadius;

    QPainterPath path;
    path.moveTo(r.left() + rtl, r.top());
    path.lineTo(r.right() - rtr, r.top());
    if (rtr > 0.0) {
        path.arcTo(QRectF(r.right() - 2 * rtr, r.top(), 2 * rtr, 2 * rtr), 90, -90);
    }
    path.lineTo(r.right(), r.bottom() - rbr);
    if (rbr > 0.0) {
        path.arcTo(QRectF(r.right() - 2 * rbr, r.bottom() - 2 * rbr, 2 * rbr, 2 * rbr), 0, -90);
    }
    path.lineTo(r.left() + rbl, r.bottom());
    if (rbl > 0.0) {
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * rbl, 2 * rbl, 2 * rbl), 270, -90);
    }
    path.lineTo(r.left(), r.top() + rtl);
    if (rtl > 0.0) {
        path.arcTo(QRectF(r.left(), r.top(), 2 * rtl, 2 * rtl), 180, -90);
    }
    path.closeSubpath();
    return path;
}

QImage MakeSegmentMask(
    const QSize& size,
    CardSegmentPosition pos,
    double largeRadius,
    double smallRadius
) {
    return RippleAnimation::MaskByDrawer(size, false, [&](QPainter& p) {
        PainterHighQualityEnabler hq(p);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawPath(MakeSegmentPath(QRectF(0, 0, size.width(), size.height()), pos, largeRadius, smallRadius));
    });
}

CardRow::CardRow(
    QWidget* parent,
    const QString& title,
    const QString& subtitle,
    const QString& iconName
)
    : RippleButton(parent)
    , title_(title)
    , subtitle_(subtitle)
    , iconName_(iconName) {
    setFixedHeight(subtitle_.isEmpty() ? webclip::scale::px(56) : webclip::scale::px(72));
}

CardRow::~CardRow() = default;

void CardRow::setTitle(const QString& title) {
    if (title_ != title) {
        title_ = title;
        update();
    }
}

void CardRow::setSubtitle(const QString& subtitle) {
    if (subtitle_ != subtitle) {
        subtitle_ = subtitle;
        setFixedHeight(subtitle_.isEmpty() ? webclip::scale::px(56) : webclip::scale::px(72));
        updateGeometry();
        update();
    }
}

void CardRow::setIconName(const QString& iconName) {
    if (iconName_ != iconName) {
        iconName_ = iconName;
        update();
    }
}

void CardRow::setSegmentPosition(CardSegmentPosition pos) {
    if (segmentPosition_ != pos) {
        segmentPosition_ = pos;
        update();
    }
}

QSize CardRow::sizeHint() const {
    return QSize(webclip::scale::px(340), subtitle_.isEmpty() ? webclip::scale::px(56) : webclip::scale::px(72));
}

QImage CardRow::prepareRippleMask() const {
    return MakeSegmentMask(size(), segmentPosition_, webclip::scale::pxF(20.0), webclip::scale::pxF(4.0));
}

void CardRow::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const QRectF rowRect(0.5, 0.5, width() - 1.0, height() - 1.0);
    const QPainterPath path = MakeSegmentPath(rowRect, segmentPosition_, webclip::scale::pxF(20.0), webclip::scale::pxF(4.0));

    // 1. Background
    p.setPen(Qt::NoPen);
    p.setBrush(theme->surfaceContainerLow());
    p.drawPath(path);

    // 2. State layer
    if (!isDisabled() && (isOver() || isDown())) {
        const int stateAlpha = isDown() ? 36 : 20;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(theme->onSurface().red(), theme->onSurface().green(), theme->onSurface().blue(), stateAlpha));
        p.drawPath(path);
    }

    // 3. Ripple
    const QColor ripCol = theme->primary();
    paintRipple(p, 0, 0, &ripCol);

    // 4. Content (Icon + Title + Subtitle)
    int leftX = (iconName_ == QLatin1String("webclip")) ? webclip::scale::px(16) : webclip::scale::px(14);
    if (!iconName_.isEmpty()) {
        const int iconSize = (iconName_ == QLatin1String("webclip")) ? webclip::scale::px(40) : webclip::scale::px(20);
        const int iconY = (height() - iconSize) / 2;
        if (iconName_ == QLatin1String("webclip")) {
            IconLoader::paint(p, iconName_, QRectF(leftX, iconY, iconSize, iconSize));
            leftX += iconSize + webclip::scale::px(14);
        } else {
            IconLoader::paint(p, iconName_, QRectF(leftX, iconY, iconSize, iconSize), theme->onSurfaceVariant());
            leftX += iconSize + webclip::scale::px(12);
        }
    }

    const int maxTextW = std::max(webclip::scale::px(40), width() - leftX - trailingPadding_);

    if (subtitle_.isEmpty()) {
        p.setFont(theme->bodyLarge());
        p.setPen(theme->onSurface());
        const QFontMetrics fm(p.font());
        const int textY = (height() - fm.height()) / 2 + fm.ascent();
        const QString elidedTitle = fm.elidedText(title_, Qt::ElideRight, maxTextW);
        p.drawText(QPointF(leftX, textY), elidedTitle);
    } else {
        p.setFont(theme->bodyLarge());
        p.setPen(theme->onSurface());
        const QFontMetrics fm(p.font());
        const QString elidedTitle = fm.elidedText(title_, Qt::ElideRight, maxTextW);
        p.drawText(QPointF(leftX, webclip::scale::pxF(28.0)), elidedTitle);

        p.setFont(theme->bodySmall());
        p.setPen(theme->onSurfaceVariant());
        const QFontMetrics fmSub(p.font());
        const QString elidedSub = fmSub.elidedText(subtitle_, Qt::ElideRight, maxTextW);
        p.drawText(QPointF(leftX, webclip::scale::pxF(50.0)), elidedSub);
    }
}

CardToggleRow::CardToggleRow(
    QWidget* parent,
    const QString& title,
    const QString& subtitle,
    const QString& iconName,
    bool checked
)
    : CardRow(parent, title, subtitle, iconName) {
    setTrailingPadding(webclip::scale::px(70));
    switch_ = new Md3Switch(this, checked);
    connect(switch_, &Md3Switch::toggled, this, &CardToggleRow::toggled);

    addClickHandler([this] {
        switch_->setChecked(!switch_->checked());
    });
}

bool CardToggleRow::checked() const {
    return switch_->checked();
}

void CardToggleRow::setChecked(bool checked, anim::type animated) {
    switch_->setChecked(checked, animated);
}

void CardToggleRow::resizeEvent(QResizeEvent* e) {
    CardRow::resizeEvent(e);
    if (switch_) {
        switch_->move(width() - webclip::scale::px(14) - switch_->width(), (height() - switch_->height()) / 2);
    }
}

CardButtonRow::CardButtonRow(
    QWidget* parent,
    const QString& title,
    const QString& subtitle,
    const QString& iconName,
    const QString& trailingValue
)
    : CardRow(parent, title, subtitle, iconName)
    , trailingValue_(trailingValue) {
    setTrailingValue(trailingValue);
}

void CardButtonRow::setTrailingValue(const QString& val) {
    trailingValue_ = val;
    const QFontMetrics fm(webclip::MD3Theme::instance()->bodySmall());
    setTrailingPadding(trailingValue_.isEmpty() ? webclip::scale::px(16) : (fm.horizontalAdvance(trailingValue_) + webclip::scale::px(24)));
    update();
}

void CardButtonRow::paintEvent(QPaintEvent* e) {
    CardRow::paintEvent(e);

    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    // Draw trailing chevron / value
    const int rightX = width() - webclip::scale::px(16);
    if (!trailingValue_.isEmpty()) {
        p.setFont(theme->bodySmall());
        p.setPen(theme->onSurfaceVariant());
        const QFontMetrics fm(p.font());
        const int valW = fm.horizontalAdvance(trailingValue_);
        const int valY = (height() - fm.height()) / 2 + fm.ascent();
        p.drawText(QPointF(rightX - valW, valY), trailingValue_);
    }
}

CardContainer::CardContainer(QWidget* parent)
    : RpWidget(parent) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(1); // 1px gap between segments
}

CardContainer::~CardContainer() = default;

void CardContainer::addRow(CardRow* row) {
    rows_.push_back(row);
    layout_->addWidget(row);
    updateSegments();
}

void CardContainer::updateSegments() {
    if (rows_.empty()) return;

    if (rows_.size() == 1) {
        rows_[0]->setSegmentPosition(CardSegmentPosition::Single);
        return;
    }

    for (size_t i = 0; i < rows_.size(); ++i) {
        if (i == 0) {
            rows_[i]->setSegmentPosition(CardSegmentPosition::Top);
        } else if (i == rows_.size() - 1) {
            rows_[i]->setSegmentPosition(CardSegmentPosition::Bottom);
        } else {
            rows_[i]->setSegmentPosition(CardSegmentPosition::Middle);
        }
    }
}

} // namespace Ui
