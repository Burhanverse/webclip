#include "clip_element.hpp"

#include <QBuffer>
#include <QImageReader>
#include <QLinearGradient>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSvgRenderer>
#include <QThreadPool>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>

#include "../controllers/webclip_controller.hpp"
#include "../models/clipboard_history_model.hpp"
#include "../theme/md3_theme.hpp"
#include "../util/i18n.hpp"
#include "bubble_corners.hpp"
#include "clip_list_item.hpp"

namespace webclip {

namespace {

constexpr qreal kBubbleRadius = 18.0;
constexpr qreal kTailSpace = 6.0;
constexpr qreal kContentHPad = 12.0;
constexpr qreal kVPad = 8.0;
constexpr qreal kSectionSpacing = 4.0;
constexpr qreal kImageMaxH = 360.0;
constexpr qreal kMetaRowHeight = 24.0;
constexpr qreal kChipHeight = 24.0;
constexpr qreal kButtonSize = 20.0;
constexpr qreal kButtonSpacing = 2.0;
constexpr qreal kIconSize = 13.0;
constexpr qreal kCollapsedTextHeight = 85.0;

struct IconKey {
    QString name;
    quint32 argb;
    int sizePx;
    qreal dpr;
    bool operator==(const IconKey& o) const {
        return sizePx == o.sizePx && dpr == o.dpr && argb == o.argb &&
               name == o.name;
    }
};

using ::qHash;

size_t qHash(const IconKey& k, size_t seed = 0) {
    return qHash(k.name, qHash(k.argb, qHash(k.sizePx, qHash(static_cast<quint32>(qRound64(k.dpr * 64.0)), seed))));
}

QHash<IconKey, QPixmap>& iconCache() {
    static QHash<IconKey, QPixmap> cache;
    return cache;
}

const QPixmap& iconPixmap(const QString& name, const QColor& color, qreal dpr) {
    const int px = qCeil(kIconSize * dpr);
    IconKey key{name, color.rgba(), px, dpr};
    auto it = iconCache().find(key);
    if (it != iconCache().end()) return it.value();

    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QString candidates[] = {
        QStringLiteral(":/qt/qml/src/gui/resources/icons/%1.svg").arg(name),
        QStringLiteral(":/src/gui/resources/icons/%1.svg").arg(name),
    };
    for (const QString& path : candidates) {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            renderer.render(&p, QRectF(0, 0, kIconSize, kIconSize));
            break;
        }
    }
    if (color.alpha() > 0) {
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(QRectF(0, 0, kIconSize, kIconSize), color);
    }
    p.end();
    return iconCache().insert(key, pm).value();
}

QString actionIconName(ClipElement::Zone zone) {
    switch (zone) {
        case ClipElement::Zone::BtnLink: return QStringLiteral("link");
        case ClipElement::Zone::BtnCopy: return QStringLiteral("copy");
        case ClipElement::Zone::BtnDownload: return QStringLiteral("download");
        case ClipElement::Zone::BtnSend: return QStringLiteral("send");
        case ClipElement::Zone::BtnDelete: return QStringLiteral("delete");
        default: return QString();
    }
}

QColor overlayColor(bool darkBase, qreal alphaFraction) {
    return darkBase ? QColor(255, 255, 255, qRound(alphaFraction * 255))
                    : QColor(0, 0, 0, qRound(alphaFraction * 255));
}

void drawRoundedImage(QPainter* p, const QImage& img, const QRectF& rect,
                      qreal radius) {
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);
    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setClipPath(clipPath);
    p->drawImage(rect, img);
    p->restore();
}

QImage decodeScaledImage(const QString& sourceUrl, const QSize& native) {
    QImageReader reader;
    QByteArray b64;
    if (sourceUrl.startsWith(QLatin1String("data:"))) {
        const int comma = sourceUrl.indexOf(QLatin1Char(','));
        if (comma < 0) return {};
        b64 = QByteArray::fromBase64(sourceUrl.mid(comma + 1).toLatin1());
        QBuffer buf(&b64);
        buf.open(QIODevice::ReadOnly);
        reader.setDevice(&buf);
    } else {
        const QString local = sourceUrl.startsWith(QLatin1String("file://"))
                                  ? QUrl(sourceUrl).toLocalFile()
                                  : sourceUrl;
        reader.setFileName(local);
    }
    QSize n = (native.width() > 0 && native.height() > 0) ? native : reader.size();
    if (!n.isEmpty()) {
        QSize scaled = n;
        scaled.scale(QSize(512, 512), Qt::KeepAspectRatio);
        if (scaled.width() > 0 && scaled.height() > 0) reader.setScaledSize(scaled);
    }
    return reader.read();
}

}  // namespace

void clearClipElementCaches() {
    iconCache().clear();
}

ClipElement::ClipElement(ClipListItem* owner) : owner_(owner) {}

bool ClipElement::isLongText() const {
    if (isImage_) return false;
    if (charCount_ > 350) return true;
    return headText_.count(QLatin1Char('\n')) + 1 > 5;
}

void ClipElement::refreshContent() {
    const auto* model = owner_->model();
    const ClipItem* item = model ? model->getItem(row_) : nullptr;
    laidOutWidth_ = -1;
    pendingResize_ = true;
    selAnchor_ = selCursor_ = -1;
    hoverZone_ = Zone::None;
    pressed_ = false;

    if (!item) {
        id_.clear();
        isImage_ = false;
        headText_.clear();
        height_ = 0;
        bubbleRect_ = QRectF();
        return;
    }

    id_ = item->id;
    isImage_ = item->isImage;
    fromPhone_ = (item->source == QLatin1String("phone"));
    headText_ = item->text;
    timeString_ = item->formattedTime();
    charCount_ = item->charCount();
    imageDataUrl_ = item->imageData;
    nativeDims_ = QSize(item->imgWidth, item->imgHeight);

    const auto links = ClipTextLayout::detectLinks(headText_);
    hasUrl_ = !links.isEmpty();
    firstUrl_ = hasUrl_ ? links.first().url : QString();

    fullTextLoaded_ = false;
    fullTextCache_.clear();
    expanded_ = false;
    imageState_ = ImageEmpty;
    imageStateKey_ = isImage_ ? imageDataUrl_ : QString();
    imageScaled_ = QImage();
    rebuildText();
}

void ClipElement::rebuildText() {
    MD3Theme* theme = MD3Theme::instance();
    bodyFont_ = theme->bodyMedium();
    timeFont_ = theme->labelSmall();

    textColor_ =
        fromPhone_ ? theme->onSecondaryContainer() : theme->onPrimaryContainer();
    linkColor_ = fromPhone_ ? theme->primary()
                            : QColor(theme->isDark() ? "#8AB4F8" : "#1A73E8");
    bubbleColor_ =
        fromPhone_ ? theme->secondaryContainer() : theme->primaryContainer();

    if (isImage_) {
        text_ = ClipTextLayout();
        return;
    }
    const QString& body = expanded_ ? fullTextCache_ : headText_;
    text_.setText(body, bodyFont_, textColor_, linkColor_);
}

qreal ClipElement::actionsPillWidth() const {
    qreal w = 8.0;
    for (const ActionButton& b : buttons_) {
        if (b.visible) w += kButtonSize + kButtonSpacing;
    }
    return qMax<qreal>(8.0, w - kButtonSpacing);
}

ClipElement::Metrics ClipElement::computeMetrics(int containerWidth) const {
    Metrics m;
    const qreal avail = containerWidth - 32;  // old page margins (16 each side)
    m.maxBubbleWidth = avail * 0.84;
    m.totalHPad = kTailSpace + kContentHPad * 2;

    QFontMetricsF timeFm(timeFont_);
    const qreal pillWidth = actionsPillWidth();
    const qreal metaNeeded =
        timeFm.horizontalAdvance(timeString_) + pillWidth + 24.0 + m.totalHPad;

    if (isImage_) {
        qreal natW = nativeDims_.width();
        qreal natH = nativeDims_.height();
        if ((natW <= 0 || natH <= 0) && imageState_ == ImageReady) {
            natW = imageScaled_.width();
            natH = imageScaled_.height();
        }
        if (natW <= 0 || natH <= 0) {
            m.bubbleWidth = qMin(m.maxBubbleWidth, 260.0);
            return m;
        }

        const qreal imgMaxW = m.maxBubbleWidth - m.totalHPad;
        qreal scale = qMin(imgMaxW / natW, kImageMaxH / natH);
        const qreal minDispW = qMin(160.0, imgMaxW);
        if (natW * scale < minDispW) scale = minDispW / natW;
        const qreal dispW = std::max<qreal>(1.0, qRound(natW * scale));

        const qreal needed =
            std::max(dispW, metaNeeded - m.totalHPad) + m.totalHPad;
        m.bubbleWidth = qMin(m.maxBubbleWidth, needed);

        const qreal w2 = m.bubbleWidth - m.totalHPad;
        qreal s2 = qMin(w2 / natW, kImageMaxH / natH);
        const qreal minDisp2 = qMin(160.0, w2);
        if (natW * s2 < minDisp2) s2 = minDisp2 / natW;
        m.imageDisplaySize = QSizeF(std::max<qreal>(1.0, qRound(natW * s2)),
                                    std::max<qreal>(1.0, qRound(natH * s2)));
        return m;
    }

    const qreal textNeeded = text_.naturalWidth() + m.totalHPad;
    const qreal needed = std::max({m.minBubbleWidth, textNeeded, metaNeeded});
    m.bubbleWidth = qMin(m.maxBubbleWidth, needed);
    m.textWrapWidth = qFloor(m.bubbleWidth - m.totalHPad);
    return m;
}

int ClipElement::resizeGetHeight(int containerWidth) {
    if (!pendingResize_ && containerWidth == laidOutWidth_) return height_;
    if (qEnvironmentVariableIsSet("WEBCLIP_DEBUG_LAYOUT")) {
        fprintf(stderr, "[layout] row=%d containerWidth=%d laidOut=%d pending=%d\n",
                row_, containerWidth, laidOutWidth_, pendingResize_ ? 1 : 0);
    }

    if (expanded_ && !isImage_ && !fullTextLoaded_) {
        fullTextCache_ = owner_->model()->getClipText(row_);
        fullTextLoaded_ = true;
    }
    rebuildText();

    updateButtonsVisibility();
    const Metrics m = computeMetrics(containerWidth);

    const qreal leftInset = 16 + 8;
    const qreal x = fromPhone_ ? leftInset
                               : containerWidth - leftInset - m.bubbleWidth;
    bubbleRect_ = QRectF(qRound(x), 0, qRound(m.bubbleWidth), 0);
    lastWrapWidth_ = qMax(40, qRound(m.bubbleWidth) -
                                  qCeil(m.totalHPad));

    const qreal contentX = bubbleRect_.left() +
                           (fromPhone_ ? kTailSpace + kContentHPad : kContentHPad);
    const qreal contentW = bubbleRect_.width() - m.totalHPad;

    qreal cy = kVPad;
    qreal totalH = kVPad;

    if (isImage_) {
        if (m.imageDisplaySize.width() > 1) {
            const qreal imgW = qRound(
                std::min<qreal>(m.imageDisplaySize.width(), contentW));
            const qreal imgH = qRound(m.imageDisplaySize.height());
            imageAreaRect_ =
                QRectF(qRound(contentX + (contentW - imgW) / 2.0), cy, imgW,
                       imgH);
            cy += imageAreaRect_.height() + kSectionSpacing;
            totalH += imageAreaRect_.height() + kSectionSpacing;
        } else {
            imageAreaRect_ = QRectF(contentX, cy, contentW, 220);
            cy += 220 + kSectionSpacing;
            totalH += 220 + kSectionSpacing;
        }
        textAreaRect_ = QRectF();
        collapsedLong_ = false;
    } else {
        collapsedLong_ = isLongText() && !expanded_;
        int textH;
        if (collapsedLong_) {
            text_.heightAt(lastWrapWidth_);
            textH = qCeil(kCollapsedTextHeight);
        } else {
            textH = std::max(text_.heightAt(lastWrapWidth_), 1);
        }
        textAreaRect_ = QRectF(contentX, cy, contentW, textH);
        cy += textH + kSectionSpacing;
        totalH += textH + kSectionSpacing;
        imageAreaRect_ = QRectF();
    }

    showExpandChip_ = collapsedLong_;
    if (showExpandChip_) {
        QFont chipFont = timeFont_;
        chipFont.setPixelSize(11);
        chipFont.setBold(true);
        const QString label =
            I18n::instance()->tr(QStringLiteral("chat.show_full_clip"));
        const qreal chipW = QFontMetricsF(chipFont).horizontalAdvance(label) + 24.0;
        expandChipRect_ =
            QRectF(qRound(bubbleRect_.center().x() - chipW / 2.0), cy,
                   qCeil(chipW), kChipHeight);
        cy += kChipHeight + kSectionSpacing;
        totalH += kChipHeight + kSectionSpacing;
    } else {
        expandChipRect_ = QRectF();
    }

    const qreal metaTop = cy + 2.0;
    totalH += 2.0 + kMetaRowHeight + kVPad;

    bubbleRect_.setBottom(totalH);
    updateButtonRects(contentX, contentW, metaTop);

    height_ = qCeil(totalH);
    pendingResize_ = false;
    laidOutWidth_ = containerWidth;
    return height_;
}

void ClipElement::updateButtonsVisibility() {
    auto* ctrl = qobject_cast<WebClipController*>(owner_->controller());
    connectedFlag_ = ctrl && ctrl->connected();
    buttons_.clear();
    if (hasUrl_ && !isImage_) buttons_.push_back({Zone::BtnLink, QRectF(), true});
    buttons_.push_back({Zone::BtnCopy, QRectF(), true});
    if (isImage_) buttons_.push_back({Zone::BtnDownload, QRectF(), true});
    if (connectedFlag_ && !fromPhone_)
        buttons_.push_back({Zone::BtnSend, QRectF(), true});
    buttons_.push_back({Zone::BtnDelete, QRectF(), true});
}

void ClipElement::updateButtonRects(qreal contentX, qreal contentW, qreal metaTop) {
    const qreal pillW = actionsPillWidth();
    const qreal pillX = contentX + contentW - pillW;
    actionsPillRect_ = QRectF(pillX, metaTop, pillW, kMetaRowHeight);

    qreal bx = pillX + 4.0;
    const qreal by = metaTop + (kMetaRowHeight - kButtonSize) / 2.0;
    for (ActionButton& b : buttons_) {
        if (!b.visible) continue;
        b.rect = QRectF(bx, by, kButtonSize, kButtonSize);
        bx += kButtonSize + kButtonSpacing;
    }
}

void ClipElement::setExpanded(bool on) {
    if (expanded_ == on) return;
    expanded_ = on;
    pendingResize_ = true;
    laidOutWidth_ = -1;
    clearSelection();
}

void ClipElement::paint(const PaintContext& context) const {
    QPainter* p = context.painter;
    if (!p || !bubbleRect_.isValid()) return;

    if (!context.clipItemCoords.intersects(bubbleRect_)) return;

    const BubbleTail tail = fromPhone_ ? BubbleTail::Left : BubbleTail::Right;
    paintBubble(p, bubbleRect_, kBubbleRadius, bubbleColor_, tail, context.dpr);

    MD3Theme* theme = MD3Theme::instance();

    if (isImage_ && imageAreaRect_.isValid()) {
        const QColor containerBg =
            fromPhone_ ? theme->surfaceContainerHigh() : theme->secondaryContainer();
        QPainterPath bgPath;
        bgPath.addRoundedRect(imageAreaRect_, 12, 12);
        p->fillPath(bgPath, containerBg);
        if (imageState_ == ImageReady) {
            drawRoundedImage(p, imageScaled_, imageAreaRect_, 12);
        }
    }

    if (!isImage_ && textAreaRect_.isValid()) {
        ClipTextLayout::DrawArgs da;
        da.painter = p;
        da.topLeft = textAreaRect_.topLeft();
        da.clip = context.clipItemCoords;
        if (context.withInteractionChrome && hasSelection()) {
            da.selectionColor = theme->primary();
            da.selectionStart = std::min(selAnchor_, selCursor_);
            da.selectionEnd = std::max(selAnchor_, selCursor_);
        }

        p->save();
        p->setPen(textColor_);
        if (collapsedLong_) {
            p->setClipRect(textAreaRect_);
            text_.draw(da);
            const qreal fadeH = 24.0;
            QRectF fadeRect(textAreaRect_.left(),
                            textAreaRect_.bottom() - fadeH,
                            textAreaRect_.width(), fadeH);
            fadeRect = fadeRect.intersected(context.clipItemCoords);
            if (fadeRect.isValid()) {
                QLinearGradient grad(0, fadeRect.top(), 0, fadeRect.bottom());
                QColor transparent = bubbleColor_;
                transparent.setAlpha(0);
                grad.setColorAt(0, transparent);
                grad.setColorAt(1, bubbleColor_);
                p->fillRect(fadeRect, grad);
            }
        } else {
            text_.draw(da);
        }
        p->restore();
    }

    if (showExpandChip_ && expandChipRect_.isValid() &&
        context.clipItemCoords.intersects(expandChipRect_)) {
        const QColor chipBg = fromPhone_ ? theme->surfaceContainerHigh()
                                         : theme->surfaceContainerLowest();
        p->setPen(Qt::NoPen);
        p->setBrush(chipBg);
        p->drawRoundedRect(expandChipRect_, 12, 12);

        QFont chipFont = timeFont_;
        chipFont.setPixelSize(11);
        chipFont.setBold(true);
        p->setFont(chipFont);
        p->setPen(theme->primary());
        p->drawText(expandChipRect_,
                    Qt::AlignCenter,
                    I18n::instance()->tr(QStringLiteral("chat.show_full_clip")));
    }

    const qreal metaTop = actionsPillRect_.top();
    const qreal contentX = bubbleRect_.left() +
                           (fromPhone_ ? kTailSpace + kContentHPad : kContentHPad);
    const QRectF metaRow(contentX, metaTop, bubbleRect_.width(), kMetaRowHeight);
    if (context.clipItemCoords.intersects(metaRow)) {
        QColor timeColor = textColor_;
        timeColor.setAlphaF(timeColor.alphaF() * 0.65);
        p->setFont(timeFont_);
        p->setPen(timeColor);
        const QRectF timeRect(
            contentX, metaTop,
            qMax<qreal>(0.0, actionsPillRect_.left() - contentX - 8.0),
            kMetaRowHeight);
        p->drawText(timeRect, Qt::AlignVCenter | Qt::AlignLeft, timeString_);

        const bool dark = theme->isDark();
        const QColor pillBg = fromPhone_
            ? overlayColor(dark, dark ? 0.09 : 0.06)
            : overlayColor(dark, dark ? 0.12 : 0.06);
        p->setPen(Qt::NoPen);
        p->setBrush(pillBg);
        p->drawRoundedRect(actionsPillRect_, 12, 12);

        for (const ActionButton& b : buttons_) {
            if (!b.visible) continue;
            if (!context.clipItemCoords.intersects(b.rect)) continue;
            if (context.withInteractionChrome &&
                (hoverZone_ == b.zone || (pressed_ && hoverZone_ == b.zone))) {
                QColor tint = textColor_;
                tint.setAlphaF(pressed_ ? 0.14 : 0.08);
                p->setPen(Qt::NoPen);
                p->setBrush(tint);
                p->drawEllipse(b.rect);
            }
            const QPixmap glyph = iconPixmap(actionIconName(b.zone), textColor_,
                                             context.dpr);
            QRectF iconRect(b.rect.center().x() - kIconSize / 2.0,
                            b.rect.center().y() - kIconSize / 2.0, kIconSize,
                            kIconSize);
            p->drawPixmap(iconRect.toRect(), glyph,
                          QRect(0, 0, glyph.width(), glyph.height()));
        }
    }
}

QImage ClipElement::snapshotBubble(qreal dpr) const {
    if (!bubbleRect_.isValid()) return {};
    const QSize sz(qCeil(bubbleRect_.width() * dpr),
                   qCeil(bubbleRect_.height() * dpr));
    QImage img(sz, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(dpr, dpr);
    PaintContext ctx;
    ctx.painter = &p;
    ctx.clipItemCoords = QRectF(QPointF(0, 0), bubbleRect_.size());
    ctx.dpr = dpr;
    ctx.withInteractionChrome = false;
    paint(ctx);
    p.end();
    return img;
}

ClipElement::Hit ClipElement::hitTest(QPointF localPos, int containerWidth) const {
    Q_UNUSED(containerWidth);
    Hit hit;
    for (const ActionButton& b : buttons_) {
        if (b.visible && b.rect.contains(localPos)) {
            hit.zone = b.zone;
            return hit;
        }
    }
    if (showExpandChip_ && expandChipRect_.contains(localPos)) {
        hit.zone = Zone::ExpandChip;
        return hit;
    }
    if (isImage_ && imageAreaRect_.contains(localPos)) {
        hit.zone = Zone::Image;
        return hit;
    }
    if (!isImage_ && textAreaRect_.contains(localPos)) {
        hit.zone = Zone::Text;
        const QPointF rel = localPos - textAreaRect_.topLeft();
        hit.url = text_.urlAt(rel, lastWrapWidth_);
        hit.textPosition = text_.positionAt(rel, lastWrapWidth_);
        return hit;
    }
    return hit;
}

bool ClipElement::setHover(Zone zone) {
    if (hoverZone_ == zone) return false;
    hoverZone_ = zone;
    return true;
}

void ClipElement::setPressed(bool on) {
    pressed_ = on;
}

void ClipElement::setSelection(int anchor, int cursor) {
    selAnchor_ = anchor;
    selCursor_ = cursor;
}

void ClipElement::clearSelection() {
    selAnchor_ = selCursor_ = -1;
}

bool ClipElement::hasSelection() const {
    return selAnchor_ >= 0 && selCursor_ >= 0 && selAnchor_ != selCursor_;
}

QString ClipElement::selectedText(int containerWidth) const {
    Q_UNUSED(containerWidth);
    if (!hasSelection()) return {};
    const int a = qMin(selAnchor_, selCursor_);
    const int b = qMax(selAnchor_, selCursor_);
    return text_.text().mid(a, b - a);
}

void ClipElement::ensureImageLoaded() {
    if (!isImage_ || imageState_ != ImageEmpty || imageDataUrl_.isEmpty()) return;
    imageState_ = ImageLoading;

    const QPointer<ClipListItem> guard(owner_);
    const QString clipId = id_;
    const QString key = imageStateKey_;
    const QSize native = nativeDims_;

    QThreadPool::globalInstance()->start([guard, clipId, key, native]() {
        const QImage img = decodeScaledImage(key, native);
        if (!guard) return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, clipId, key, img]() {
                if (guard) guard->onElementImageDecoded(clipId, key, img);
            },
            Qt::QueuedConnection);
    });
}

bool ClipElement::setImageResult(const QString& sourceKey, const QImage& image) {
    if (sourceKey != imageStateKey_) return false;
    if (image.isNull()) {
        imageState_ = ImageFailed;
    } else {
        imageScaled_ = image;
        imageState_ = ImageReady;
        if (nativeDims_.isEmpty()) {
            pendingResize_ = true;
            laidOutWidth_ = -1;
        }
    }
    return true;
}

void ClipElement::releaseImage() {
    if (imageState_ == ImageReady) {
        imageScaled_ = QImage();
        imageState_ = ImageEmpty;
    }
}

}  // namespace webclip
