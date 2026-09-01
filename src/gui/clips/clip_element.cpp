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
#include "../ui/md3/icon_loader.hpp"
#include "../util/i18n.hpp"
#include "../util/display_scale.hpp"
#include "bubble_corners.hpp"

namespace webclip {

namespace {

inline qreal kBubbleRadius() { return scale::pxF(18.0); }
inline qreal kTailSpace() { return scale::pxF(6.0); }
inline qreal kContentHPad() { return scale::pxF(12.0); }
inline qreal kVPad() { return scale::pxF(8.0); }
inline qreal kSectionSpacing() { return scale::pxF(4.0); }
inline qreal kMetaRowHeight() { return scale::pxF(22.0); }
inline qreal kButtonSize() { return scale::pxF(22.0); }
inline qreal kIconSize() { return scale::pxF(14.0); }
inline qreal kButtonSpacing() { return scale::pxF(4.0); }
inline qreal kChipHeight() { return scale::pxF(24.0); }
inline qreal kImageMaxH() { return scale::pxF(260.0); }
inline qreal kCollapsedTextHeight() { return scale::pxF(85.0); }

QPixmap iconPixmap(const QString& name, const QColor& color, qreal dpr) {
    return Ui::IconLoader::loadPixmap(name, static_cast<int>(kIconSize()), color, dpr);
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

void drawRoundedImage(QPainter* p, const QPixmap& img, const QRectF& rect,
                      qreal radius) {
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);
    p->save();
    const QSizeF target = rect.size();
    const qreal dpr = img.devicePixelRatio();
    const QSizeF src = dpr > 0.0
        ? QSizeF(img.width() / dpr, img.height() / dpr)
        : QSizeF(img.width(), img.height());
    // Only enable expensive smooth filtering when the cached pixmap is actually
    // rescaled vs the target; skip it for identical (pixel-aligned) blits.
    static const bool g_forceSmooth = qEnvironmentVariableIsSet("WEBCLIP_FORCE_SMOOTH");
    if (g_forceSmooth ||
        target.width() != src.width() || target.height() != src.height()) {
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    p->setClipPath(clipPath);
    p->drawPixmap(rect.toAlignedRect(), img,
                  QRect(0, 0, img.width(), img.height()));
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
    Ui::IconLoader::clearCache();
}

ClipElement::ClipElement(IClipViewHost* owner) : owner_(owner) {}

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

    fullTextLoaded_ = false;
    fullTextCache_.clear();
    expanded_ = false;
    imageState_ = ImageEmpty;
    imageStateKey_ = isImage_ ? imageDataUrl_ : QString();
    imageScaled_ = QPixmap();
    rebuildText();

    hasUrl_ = !text_.links().isEmpty();
    firstUrl_ = hasUrl_ ? text_.links().first().url : QString();
}

void ClipElement::rebuildText() {
    MD3Theme* theme = MD3Theme::instance();
    bodyFont_ = theme->bodyMedium();
    timeFont_ = theme->labelSmall();

    textColor_ =
        fromPhone_ ? theme->onSecondaryContainer() : theme->onPrimaryContainer();
    linkColor_ = theme->primary();
    bubbleColor_ =
        fromPhone_ ? theme->secondaryContainer() : theme->primaryContainer();

    if (isImage_) {
        text_ = ClipTextLayout();
        return;
    }
    const QString& body = expanded_ ? fullTextCache_ : headText_;
    text_.setText(body, bodyFont_, textColor_, linkColor_);
}

void ClipElement::refreshTheme() {
    pendingResize_ = true;
    laidOutWidth_ = -1;
    rebuildText();
    if (lastWrapWidth_ > 0 && !isImage_) {
        text_.layout(lastWrapWidth_);
    }
}

qreal ClipElement::actionsPillWidth() const {
    qreal w = scale::pxF(8.0);
    for (const ActionButton& b : buttons_) {
        if (b.visible) w += kButtonSize() + kButtonSpacing();
    }
    return qMax<qreal>(scale::pxF(8.0), w - kButtonSpacing());
}

ClipElement::Metrics ClipElement::computeMetrics(int containerWidth) const {
    Metrics m;
    const qreal avail = containerWidth - scale::pxF(32.0);  // old page margins (16 each side)
    m.maxBubbleWidth = avail * 0.84;
    m.totalHPad = kTailSpace() + kContentHPad() * 2;

    QFontMetricsF timeFm(timeFont_);
    const qreal pillWidth = actionsPillWidth();
    const qreal metaNeeded =
        timeFm.horizontalAdvance(timeString_) + pillWidth + scale::pxF(24.0) + m.totalHPad;

    if (isImage_) {
        qreal natW = nativeDims_.width();
        qreal natH = nativeDims_.height();
        if ((natW <= 0 || natH <= 0) && imageState_ == ImageReady) {
            natW = imageScaled_.width();
            natH = imageScaled_.height();
        }
        if (natW <= 0 || natH <= 0) {
            m.bubbleWidth = qMin(m.maxBubbleWidth, scale::pxF(260.0));
            return m;
        }

        const qreal imgMaxW = m.maxBubbleWidth - m.totalHPad;
        qreal scaleRatio = qMin(imgMaxW / natW, kImageMaxH() / natH);
        const qreal minDispW = qMin(scale::pxF(160.0), imgMaxW);
        if (natW * scaleRatio < minDispW) scaleRatio = minDispW / natW;
        const qreal dispW = std::max<qreal>(1.0, qRound(natW * scaleRatio));

        const qreal needed =
            std::max(dispW, metaNeeded - m.totalHPad) + m.totalHPad;
        m.bubbleWidth = qMin(m.maxBubbleWidth, needed);

        const qreal w2 = m.bubbleWidth - m.totalHPad;
        qreal s2 = qMin(w2 / natW, kImageMaxH() / natH);
        const qreal minDisp2 = qMin(scale::pxF(160.0), w2);
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

    const qreal leftInset = scale::pxF(24.0);
    const qreal x = fromPhone_ ? leftInset
                               : containerWidth - leftInset - m.bubbleWidth;
    bubbleRect_ = QRectF(qRound(x), 0, qRound(m.bubbleWidth), 0);
    lastWrapWidth_ = qMax(scale::px(40), qRound(m.bubbleWidth) -
                                  qCeil(m.totalHPad));

    const qreal contentX = bubbleRect_.left() +
                           (fromPhone_ ? kTailSpace() + kContentHPad() : kContentHPad());
    const qreal contentW = bubbleRect_.width() - m.totalHPad;

    qreal cy = kVPad();
    qreal totalH = kVPad();

    if (isImage_) {
        if (m.imageDisplaySize.width() > 1) {
            const qreal imgW = qRound(
                std::min<qreal>(m.imageDisplaySize.width(), contentW));
            const qreal imgH = qRound(m.imageDisplaySize.height());
            imageAreaRect_ =
                QRectF(qRound(contentX + (contentW - imgW) / 2.0), cy, imgW,
                       imgH);
            cy += imageAreaRect_.height() + kSectionSpacing();
            totalH += imageAreaRect_.height() + kSectionSpacing();
        } else {
            imageAreaRect_ = QRectF(contentX, cy, contentW, scale::pxF(220.0));
            cy += scale::pxF(220.0) + kSectionSpacing();
            totalH += scale::pxF(220.0) + kSectionSpacing();
        }
        textAreaRect_ = QRectF();
        collapsedLong_ = false;
    } else {
        collapsedLong_ = isLongText() && !expanded_;
        int textH;
        if (collapsedLong_) {
            text_.heightAt(lastWrapWidth_);
            textH = qCeil(kCollapsedTextHeight());
        } else {
            textH = std::max(text_.heightAt(lastWrapWidth_), 1);
        }
        textAreaRect_ = QRectF(contentX, cy, contentW, textH);
        cy += textH + kSectionSpacing();
        totalH += textH + kSectionSpacing();
        imageAreaRect_ = QRectF();
    }

    showExpandChip_ = collapsedLong_;
    if (showExpandChip_) {
        QFont chipFont = timeFont_;
        chipFont.setPixelSize(scale::px(11));
        chipFont.setBold(true);
        const QString label =
            I18n::instance()->tr(QStringLiteral("chat.show_full_clip"));
        const qreal chipW = QFontMetricsF(chipFont).horizontalAdvance(label) + scale::pxF(24.0);
        expandChipRect_ =
            QRectF(qRound(bubbleRect_.center().x() - chipW / 2.0), cy,
                   qCeil(chipW), kChipHeight());
        cy += kChipHeight() + kSectionSpacing();
        totalH += kChipHeight() + kSectionSpacing();
    } else {
        expandChipRect_ = QRectF();
    }

    const qreal metaTop = cy + scale::pxF(2.0);
    totalH += scale::pxF(2.0) + kMetaRowHeight() + kVPad();

    bubbleRect_.setBottom(totalH);
    updateButtonRects(contentX, contentW, metaTop);

    height_ = qCeil(totalH);
    pendingResize_ = false;
    laidOutWidth_ = containerWidth;
    return height_;
}

void ClipElement::updateButtonsVisibility() {
    auto* ctrl = owner_ ? owner_->controller() : nullptr;
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
    actionsPillRect_ = QRectF(pillX, metaTop, pillW, kMetaRowHeight());

    qreal bx = pillX + scale::pxF(4.0);
    const qreal by = metaTop + (kMetaRowHeight() - kButtonSize()) / 2.0;
    for (ActionButton& b : buttons_) {
        if (!b.visible) continue;
        b.rect = QRectF(bx, by, kButtonSize(), kButtonSize());
        bx += kButtonSize() + kButtonSpacing();
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
    paintBubble(p, bubbleRect_, kBubbleRadius(), bubbleColor_, tail, context.dpr);

    MD3Theme* theme = MD3Theme::instance();

    if (isImage_ && imageAreaRect_.isValid()) {
        const QColor containerBg =
            fromPhone_ ? theme->surfaceContainerHigh() : theme->secondaryContainer();
        QPainterPath bgPath;
        bgPath.addRoundedRect(imageAreaRect_, scale::pxF(12.0), scale::pxF(12.0));
        p->fillPath(bgPath, containerBg);
        if (imageState_ == ImageReady) {
            drawRoundedImage(p, imageScaled_, imageAreaRect_, scale::pxF(12.0));
        }
    }

    if (!isImage_ && textAreaRect_.isValid()) {
        if (lastWrapWidth_ > 0) {
            text_.layout(lastWrapWidth_);
        }
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
            const qreal fadeH = scale::pxF(24.0);
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
        p->drawRoundedRect(expandChipRect_, scale::pxF(12.0), scale::pxF(12.0));

        QFont chipFont = timeFont_;
        chipFont.setPixelSize(scale::px(11));
        chipFont.setBold(true);
        p->setFont(chipFont);
        p->setPen(theme->primary());
        p->drawText(expandChipRect_,
                    Qt::AlignCenter,
                    I18n::instance()->tr(QStringLiteral("chat.show_full_clip")));
    }

    const qreal metaTop = actionsPillRect_.top();
    const qreal contentX = bubbleRect_.left() +
                           (fromPhone_ ? kTailSpace() + kContentHPad() : kContentHPad());
    const QRectF metaRow(contentX, metaTop, bubbleRect_.width(), kMetaRowHeight());
    if (context.clipItemCoords.intersects(metaRow)) {
        QColor timeColor = textColor_;
        timeColor.setAlphaF(timeColor.alphaF() * 0.65);
        p->setFont(timeFont_);
        p->setPen(timeColor);
        const QRectF timeRect(
            contentX, metaTop,
            qMax<qreal>(0.0, actionsPillRect_.left() - contentX - scale::pxF(8.0)),
            kMetaRowHeight());
        p->drawText(timeRect, Qt::AlignVCenter | Qt::AlignLeft, timeString_);

        const bool dark = theme->isDark();
        const QColor pillBg = fromPhone_
            ? overlayColor(dark, dark ? 0.09 : 0.06)
            : overlayColor(dark, dark ? 0.12 : 0.06);
        p->setPen(Qt::NoPen);
        p->setBrush(pillBg);
        p->drawRoundedRect(actionsPillRect_, scale::pxF(12.0), scale::pxF(12.0));

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
            QRectF iconRect(b.rect.center().x() - kIconSize() / 2.0,
                            b.rect.center().y() - kIconSize() / 2.0, kIconSize(),
                            kIconSize());
            p->drawPixmap(iconRect.toRect(), glyph,
                          QRect(0, 0, glyph.width(), glyph.height()));
        }
    }
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
        if (!text_.links().isEmpty()) {
            const QPointF rel = localPos - textAreaRect_.topLeft();
            hit.url = text_.urlAt(rel, lastWrapWidth_);
        }
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

    const QPointer<QObject> guard(owner_ ? owner_->asQObject() : nullptr);
    IClipViewHost* ownerPtr = owner_;
    const QString clipId = id_;
    const QString key = imageStateKey_;
    const QSize native = nativeDims_;

    QThreadPool::globalInstance()->start([guard, ownerPtr, clipId, key, native]() {
        const QImage img = decodeScaledImage(key, native);
        if (!guard) return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, ownerPtr, clipId, key, img]() {
                if (guard && ownerPtr) ownerPtr->onElementImageDecoded(clipId, key, img);
            },
            Qt::QueuedConnection
        );
    });
}

bool ClipElement::setImageResult(const QString& sourceKey, const QImage& image) {
    if (sourceKey != imageStateKey_) return false;
    if (image.isNull()) {
        imageState_ = ImageFailed;
    } else {
        imageScaled_ = QPixmap::fromImage(image);
        imageScaled_.setDevicePixelRatio(image.devicePixelRatio());
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
        imageScaled_ = QPixmap();
        imageState_ = ImageEmpty;
    }
}

}  // namespace webclip
