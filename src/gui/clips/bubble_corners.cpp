#include "bubble_corners.hpp"
#include "../util/display_scale.hpp"

#include <QHash>
#include <QPainterPath>
#include <QtMath>

namespace webclip {

namespace {

constexpr int kTopLeft = 0;
constexpr int kTopRight = 1;
constexpr int kBottomLeft = 2;
constexpr int kBottomRight = 3;

inline qreal tailWidth() { return scale::pxF(6.0); }
inline qreal tailHeight() { return scale::pxF(15.0); }

struct TailKey {
    int side;
    quint32 argb;
    qreal dpr;
    bool operator==(const TailKey& o) const {
        return side == o.side && argb == o.argb && dpr == o.dpr;
    }
};

struct CornerKey {
    qreal radius;
    quint32 argb;
    qreal dpr;
    bool operator==(const CornerKey& o) const {
        return radius == o.radius && argb == o.argb && dpr == o.dpr;
    }
};

using ::qHash;

size_t qHash(const CornerKey& k, size_t seed = 0) {
    return qHash(static_cast<quint32>(qRound64(k.radius * 64.0)),
                 qHash(k.argb, qHash(static_cast<quint32>(qRound64(k.dpr * 64.0)), seed)));
}

size_t qHash(const TailKey& k, size_t seed = 0) {
    return qHash(k.side, qHash(k.argb, qHash(static_cast<quint32>(qRound64(k.dpr * 64.0)), seed)));
}

}  // namespace

namespace {

QImage renderRoundedSquare(qreal radius, const QColor& color, qreal dpr) {
    const int r = qCeil(radius * dpr);
    QImage img(r * 2, r * 2, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(dpr, dpr);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(QRectF(0, 0, radius * 2, radius * 2), radius, radius);
    p.end();
    return img;
}

QPixmap sliceCorner(const QImage& square, int idx, qreal radius, qreal dpr) {
    const int r = qCeil(radius * dpr);
    int x = 0, y = 0;
    switch (idx) {
        case kTopLeft: x = 0; y = 0; break;
        case kTopRight: x = r; y = 0; break;
        case kBottomLeft: x = 0; y = r; break;
        default: x = r; y = r; break;
    }
    return QPixmap::fromImage(square.copy(x, y, r, r));
}

QPixmap makeTailPixmap(BubbleTail side, const QColor& color, qreal dpr) {
    const qreal tw = tailWidth();
    const qreal th = tailHeight();
    const int w = qCeil(tw * dpr);
    const int h = qCeil(th * dpr);
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(dpr, dpr);

    QPainterPath path;
    if (side == BubbleTail::Right) {
        path.moveTo(0, 0);
        path.cubicTo(0, scale::pxF(8.0), scale::pxF(4.0), th - 0.5, tw, th);
        path.lineTo(0, th);
    } else {
        path.moveTo(tw, 0);
        path.cubicTo(tw, scale::pxF(8.0), tw - scale::pxF(4.0), th - 0.5, 0, th);
        path.lineTo(tw, th);
    }
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(path);
    p.end();
    return QPixmap::fromImage(img);
}

QHash<CornerKey, CornerPixmaps>& cornerCache() {
    static QHash<CornerKey, CornerPixmaps> cache;
    return cache;
}

QHash<TailKey, QPixmap>& tailCache() {
    static QHash<TailKey, QPixmap> cache;
    return cache;
}

}  // namespace

const CornerPixmaps& bubbleCornerPixmaps(qreal radius, const QColor& color, qreal dpr) {
    CornerKey key{radius, color.rgba(), dpr};
    auto it = cornerCache().find(key);
    if (it != cornerCache().end()) return it.value();

    const QImage square = renderRoundedSquare(radius, color, dpr);
    CornerPixmaps result;
    result.radius = radius;
    result.color = color;
    for (int i = 0; i < 4; ++i) {
        result.corners[i] = sliceCorner(square, i, radius, dpr);
    }
    return cornerCache().insert(key, result).value();
}

const QPixmap& bubbleTailPixmap(BubbleTail side, const QColor& color, qreal dpr) {
    TailKey key{static_cast<int>(side), color.rgba(), dpr};
    auto it = tailCache().find(key);
    if (it != tailCache().end()) return it.value();
    return tailCache().insert(key, makeTailPixmap(side, color, dpr)).value();
}

void clearBubblePixmapCache() {
    cornerCache().clear();
    tailCache().clear();
}

void paintBubble(QPainter* p, const QRectF& rectIncludingTail, qreal radius,
                 const QColor& color, BubbleTail tail, qreal dpr) {
    Q_UNUSED(dpr);
    if (rectIncludingTail.width() <= 0 || rectIncludingTail.height() <= 0) return;

    const qreal tw = tailWidth();
    QRectF body = rectIncludingTail;
    if (tail == BubbleTail::Right) {
        body.setRight(rectIncludingTail.right() - tw);
    } else if (tail == BubbleTail::Left) {
        body.setLeft(rectIncludingTail.left() + tw);
    }

    QPainterPath path;
    path.addRoundedRect(body, radius, radius);

    if (tail == BubbleTail::Right) {
        const qreal bx = body.right();
        const qreal by = body.bottom();
        QPainterPath tailPath;
        tailPath.moveTo(bx - radius, by);
        tailPath.lineTo(rectIncludingTail.right(), by);
        tailPath.cubicTo(rectIncludingTail.right() - scale::pxF(4.0), by - scale::pxF(4.0), bx, by - scale::pxF(12.0), bx, by - scale::pxF(14.0));
        tailPath.closeSubpath();
        path = path.united(tailPath);
    } else if (tail == BubbleTail::Left) {
        const qreal bx = body.left();
        const qreal by = body.bottom();
        QPainterPath tailPath;
        tailPath.moveTo(bx + radius, by);
        tailPath.lineTo(rectIncludingTail.left(), by);
        tailPath.cubicTo(rectIncludingTail.left() + scale::pxF(4.0), by - scale::pxF(4.0), bx, by - scale::pxF(12.0), bx, by - scale::pxF(14.0));
        tailPath.closeSubpath();
        path = path.united(tailPath);
    }

    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPath(path);
}

}  // namespace webclip
