#include "popup_menu.hpp"
#include "../basic/painter_helpers.hpp"
#include "../md3/icon_loader.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

namespace Ui {

PopupMenu::PopupMenu(QWidget* parent)
    : RpWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setFont(webclip::MD3Theme::instance()->bodyMedium());
}

PopupMenu::~PopupMenu() = default;

QAction* PopupMenu::addAction(const QString& text, std::function<void()> handler) {
    return addAction(QString(), text, std::move(handler));
}

QAction* PopupMenu::addAction(const QString& iconName, const QString& text, std::function<void()> handler) {
    auto* action = new QAction(text, this);
    if (handler) {
        connect(action, &QAction::triggered, this, std::move(handler));
    }

    Item item;
    item.action = action;
    item.iconName = iconName;
    item.text = text;
    item.isSeparator = false;
    items_.push_back(std::move(item));

    updateGeometryAndMask();
    return action;
}

void PopupMenu::addSeparator() {
    Item item;
    item.isSeparator = true;
    items_.push_back(std::move(item));
    updateGeometryAndMask();
}

void PopupMenu::updateGeometryAndMask() {
    const QFontMetrics fm(font());
    int maxTextWidth = 140;
    for (const auto& item : items_) {
        if (!item.isSeparator) {
            maxTextWidth = std::max(maxTextWidth, fm.horizontalAdvance(item.text) + 60);
        }
    }

    const int contentWidth = maxTextWidth;
    int currentY = kShadowMargin + 8;

    for (auto& item : items_) {
        if (item.isSeparator) {
            item.rect = QRect(kShadowMargin + 8, currentY + 4, contentWidth - 16, 1);
            currentY += 9;
        } else {
            item.rect = QRect(kShadowMargin + 4, currentY, contentWidth - 8, kItemHeight);
            if (!item.ripple) {
                RippleConfig cfg;
                cfg.color = webclip::MD3Theme::instance()->primary();
                cfg.showDuration = 180;
                cfg.hideDuration = 250;
                item.ripple = std::make_unique<RippleAnimation>(
                    cfg,
                    RippleAnimation::RoundRectMask(item.rect.size(), 6),
                    [this] { update(); }
                );
            }
            currentY += kItemHeight;
        }
    }

    currentY += 8; // bottom padding
    const int totalWidth = contentWidth + 2 * kShadowMargin;
    const int totalHeight = currentY + kShadowMargin;
    resize(totalWidth, totalHeight);
}

int PopupMenu::itemUnderPoint(const QPoint& pos) const {
    for (size_t i = 0; i < items_.size(); ++i) {
        if (!items_[i].isSeparator && items_[i].rect.contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PopupMenu::popup(const QPoint& globalPos) {
    updateGeometryAndMask();
    move(globalPos.x() - kShadowMargin, globalPos.y() - kShadowMargin);
    show();

    openAnimation_.start(
        [this](double v) {
            openProgress_ = v;
            update();
        },
        0.0,
        1.0,
        150,
        anim::easeOutCubic
    );
}

void PopupMenu::mouseMoveEvent(QMouseEvent* e) {
    const int idx = itemUnderPoint(e->pos());
    if (idx != hoveredIndex_) {
        hoveredIndex_ = idx;
        update();
    }
    RpWidget::mouseMoveEvent(e);
}

void PopupMenu::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        pressedIndex_ = itemUnderPoint(e->pos());
        if (pressedIndex_ >= 0 && items_[pressedIndex_].ripple) {
            const QPoint rel = e->pos() - items_[pressedIndex_].rect.topLeft();
            items_[pressedIndex_].ripple->add(rel);
            update();
        }
    }
    RpWidget::mousePressEvent(e);
}

void PopupMenu::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && pressedIndex_ >= 0) {
        const int clicked = itemUnderPoint(e->pos());
        if (clicked == pressedIndex_ && items_[pressedIndex_].action) {
            auto* act = items_[pressedIndex_].action;
            close();
            act->trigger();
            pressedIndex_ = -1;
            return;
        }
        if (items_[pressedIndex_].ripple) {
            items_[pressedIndex_].ripple->lastStop();
        }
        pressedIndex_ = -1;
        update();
    }
    RpWidget::mouseReleaseEvent(e);
}

void PopupMenu::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        close();
    } else {
        RpWidget::keyPressEvent(e);
    }
}

void PopupMenu::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    ScopedPainterOpacity op(p, openProgress_);

    auto* theme = webclip::MD3Theme::instance();
    const QRectF innerRect(
        kShadowMargin,
        kShadowMargin,
        width() - 2 * kShadowMargin,
        height() - 2 * kShadowMargin
    );

    // 1. Soft client-side ambient drop shadow
    for (int i = 0; i < kShadowMargin; ++i) {
        const double shadowOpacity = 0.015 * (kShadowMargin - i);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, static_cast<int>(shadowOpacity * 255)));
        p.drawRoundedRect(
            innerRect.adjusted(-i, -i, i, i),
            kCornerRadius + i,
            kCornerRadius + i
        );
    }

    // 2. Container background & 1px outline
    p.setPen(QPen(theme->outlineVariant(), 1.0));
    p.setBrush(theme->surfaceContainer());
    p.drawRoundedRect(innerRect, kCornerRadius, kCornerRadius);

    // 3. Menu items
    const QFontMetrics fm(font());
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        if (item.isSeparator) {
            p.setPen(theme->outlineVariant());
            p.drawLine(item.rect.left(), item.rect.top(), item.rect.right(), item.rect.top());
            continue;
        }

        // Hover state layer
        if (static_cast<int>(i) == hoveredIndex_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(theme->primary().red(), theme->primary().green(), theme->primary().blue(), 20));
            p.drawRoundedRect(item.rect, 6, 6);
        }

        // Ripple
        if (item.ripple) {
            const QColor ripCol = theme->primary();
            item.ripple->paint(p, item.rect.x(), item.rect.y(), item.rect.width(), &ripCol);
        }

        // Icon
        int textX = item.rect.x() + 12;
        if (!item.iconName.isEmpty()) {
            const int iconSize = 18;
            const int iconY = item.rect.y() + (item.rect.height() - iconSize) / 2;
            IconLoader::paint(p, item.iconName, QRectF(textX, iconY, iconSize, iconSize), theme->onSurfaceVariant());
            textX += iconSize + 10;
        }

        // Text
        p.setFont(font());
        p.setPen(theme->onSurface());
        const int textY = item.rect.y() + (item.rect.height() - fm.height()) / 2 + fm.ascent();
        p.drawText(QPointF(textX, textY), item.text);
    }
}

} // namespace Ui
