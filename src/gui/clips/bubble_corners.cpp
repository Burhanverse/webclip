#include "bubble_corners.hpp"

#include <QHash>
#include <QPainterPath>
#include <QtMath>

namespace webclip {

namespace {

constexpr int kTopLeft = 0;
constexpr int kTopRight = 1;
constexpr int kBottomLeft = 2;
constexpr int kBottomRight = 3;

constexpr qreal kTailWidth = 6.0;
constexpr qreal kTailHeight = 15.0;

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
    const int w = qCeil(kTailWidth * dpr);
    const int h = qCeil(kTailHeight * dpr);
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(dpr, dpr);

    QPainterPath path;
    if (side == BubbleTail::Right) {
        path.moveTo(0, 0);
        path.cubicTo(0, 8.0, 4.0, kTailHeight - 0.5, kTailWidth, kTailHeight);
        path.lineTo(0, kTailHeight);
    } else {
        path.moveTo(kTailWidth, 0);
        path.cubicTo(kTailWidth, 8.0, kTailWidth - 4.0, kTailHeight - 0.5, 0, kTailHeight);
        path.lineTo(kTailWidth, kTailHeight);
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
    if (rectIncludingTail.width() <= 0 || rectIncludingTail.height() <= 0) return;

    const CornerPixmaps& corners = bubbleCornerPixmaps(radius, color, dpr);
    const qreal r = qMax<qreal>(1.0, corners.corners[0].width() / dpr);
    const qreal left = rectIncludingTail.left();
    const qreal right = rectIncludingTail.right();
    const qreal bottom = rectIncludingTail.bottom();

    QRectF body = rectIncludingTail;
    QPointF tailPos = rectIncludingTail.topLeft();
    bool squareBL = false;
    bool squareBR = false;

    if (tail == BubbleTail::Right) {
        body.setRight(right - kTailWidth);
        tailPos = QPointF(right - kTailWidth, bottom - kTailHeight);
        squareBR = true;
    } else if (tail == BubbleTail::Left) {
        body.setLeft(left + kTailWidth);
        tailPos = QPointF(left, bottom - kTailHeight);
        squareBL = true;
    }

    const qreal cx1 = body.left() + r;
    const qreal cx2 = body.right() - r;
    const qreal cy1 = body.top() + r;
    const qreal cy2 = body.bottom() - r;

    p->fillRect(QRectF(body.left(), cy1 - 0.5, body.width(),
                       qMax(0.0, cy2 - cy1 + 1.0)),
                color);
    p->fillRect(QRectF(cx1 - 0.5, body.top(), qMax(0.0, cx2 - cx1 + 1.0),
                       qMax(0.0, cy1 - body.top())),
                color);
    p->fillRect(QRectF(cx1 - 0.5, cy2, qMax(0.0, cx2 - cx1 + 1.0),
                       qMax(0.0, body.bottom() - cy2)),
                color);

    const auto drawPiece = [&](const QPixmap& pm, const QRectF& at) {
        p->drawPixmap(
            QRectF(at.left(), at.top(), pm.width() / dpr, pm.height() / dpr),
            pm, QRectF(0, 0, pm.width(), pm.height()));
    };

    drawPiece(corners.corners[kTopLeft], QRectF(body.left(), body.top(), r, r));
    drawPiece(corners.corners[kTopRight], QRectF(cx2, body.top(), r, r));
    if (squareBL) {
        p->fillRect(QRectF(body.left() - 0.5, cy2, r + 0.5, r), color);
    } else {
        drawPiece(corners.corners[kBottomLeft],
                  QRectF(body.left(), cy2, r, r));
    }
    if (squareBR) {
        p->fillRect(QRectF(cx2, cy2, r + 0.5, r), color);
    } else {
        drawPiece(corners.corners[kBottomRight],
                  QRectF(cx2, cy2, r, r));
    }

    if (tail != BubbleTail::None) {
        const QPixmap& tailPm = bubbleTailPixmap(tail, color, dpr);
        drawPiece(tailPm, QRectF(tailPos, QSizeF(kTailWidth, kTailHeight)));
    }
}

}  // namespace webclip
