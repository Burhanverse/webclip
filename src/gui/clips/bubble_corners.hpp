#pragma once

#include <QColor>
#include <QPainter>
#include <QPixmap>

namespace webclip {

enum class BubbleTail { None, Left, Right };

struct CornerPixmaps {
    QPixmap corners[4];
    qreal radius = 0.0;
    QColor color;
};

const CornerPixmaps& bubbleCornerPixmaps(qreal radius, const QColor& color, qreal dpr);
const QPixmap& bubbleTailPixmap(BubbleTail side, const QColor& color, qreal dpr);
void clearBubblePixmapCache();

void paintBubble(QPainter* p, const QRectF& rectIncludingTail, qreal radius,
                 const QColor& color, BubbleTail tail, qreal dpr);

}  // namespace webclip
