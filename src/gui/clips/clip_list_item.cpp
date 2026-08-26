#include "clip_list_item.hpp"

#include <QApplication>
#include <QDateTime>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>

#include "../controllers/webclip_controller.hpp"
#include "../models/clipboard_history_model.hpp"
#include "../theme/md3_theme.hpp"
#include "bubble_corners.hpp"

namespace webclip {

namespace {

constexpr qreal kWheelStepPerNotch = 90.0;   // px per wheel notch
constexpr qreal kFlickDecay = 4.2;           // exponential friction, 1/s
constexpr qreal kFlickStartVelocity = 150;   // px/s
constexpr qreal kFlickCutoffVelocity = 60;   // px/s
constexpr qreal kMaxFlickVelocity = 4500;    // parity with old ListView cap
constexpr qreal kBounceResistance = 0.5;     // drag overscroll resistance
constexpr qreal kWheelEaseRate = 16.0;       // 1/s approach rate for wheel

WebClipController* typedController(const QPointer<QObject>& obj) {
    return qobject_cast<WebClipController*>(obj.data());
}

qreal clampScalar(qreal v, qreal lo, qreal hi) {
    return qBound(lo, v, hi);
}

qint64 nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}

}  // namespace

ClipListItem::ClipListItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setRenderTarget(QQuickPaintedItem::Image);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::ArrowCursor);

    // Drives wheel easing / flick inertia / rubber-band settle-back.
    animationTimer_.setInterval(16);
    animationTimer_.setTimerType(Qt::CoarseTimer);
    connect(&animationTimer_, &QTimer::timeout, this,
            &ClipListItem::tickAnimations);
}

ClipListItem::~ClipListItem() {
    if (scrollbarAnimTimerId_) killTimer(scrollbarAnimTimerId_);
    if (model_) model_->disconnect(this);
}

void ClipListItem::setController(QObject* controller) {
    if (controller_ == controller) return;
    controller_ = controller;
    emit controllerChanged();
    if (isComponentComplete()) {
        attachModel();
        update();
    }
}

void ClipListItem::setThanosTarget(QQuickItem* target) {
    if (thanosTarget_ == target) return;
    thanosTarget_ = target;
    emit thanosTargetChanged();
}

void ClipListItem::componentComplete() {
    QQuickPaintedItem::componentComplete();

    if (window()) {
        setContentsScale(window()->devicePixelRatio());
    }

    attachModel();
    scrollToBottom(true);

    MD3Theme* theme = MD3Theme::instance();
    connect(theme, &MD3Theme::themeChanged, this, [this]() {
        clearBubblePixmapCache();
        clearClipElementCaches();
        layoutAll();
        applyScroll(scrollY_);
        update();
    });

    auto* ctrl = typedController(controller_);
    if (ctrl) {
        connect(ctrl, &WebClipController::connectedChanged, this, [this]() {
            // Send-button visibility depends on the connection state.
            for (auto& el : elements_) el->setPendingResize(true);
            layoutAll();
            applyScroll(scrollY_);
            update();
        });
    }
}

void ClipListItem::itemChange(ItemChange change, const ItemChangeData& data) {
    QQuickPaintedItem::itemChange(change, data);
    if (change == ItemSceneChange && data.window) {
        setContentsScale(data.window->devicePixelRatio());
    }
}

void ClipListItem::geometryChange(const QRectF& newGeometry,
                                  const QRectF& oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    viewportHeightCached_ = qFloor(height());
    if (!elements_.empty()) {
        // Width change invalidates every memoized layout - ListWidget's
        // resizeAllItems path.
        layoutAll();
        applyScroll(scrollY_);
    } else {
        applyScroll(scrollY_);
    }
    update();
}

// ---------------------------------------------------------------------------
// Model synchronization. Surviving elements are reused as-is so their shaped
// text / decoded images / geometry caches survive refreshes.
// ---------------------------------------------------------------------------

void ClipListItem::attachModel() {
    auto* ctrl = typedController(controller_);
    ClipboardHistoryModel* newModel = ctrl ? ctrl->clipModel() : nullptr;
    if (model_ == newModel) return;

    if (model_) model_->disconnect(this);
    model_ = newModel;
    if (!model_) {
        elements_.clear();
        update();
        return;
    }

    connect(model_, &QAbstractItemModel::rowsInserted, this,
            &ClipListItem::onRowsInserted);
    connect(model_, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex&, int, int) { pendingAnchor_ = captureAnchor(); });
    connect(model_, &QAbstractItemModel::rowsRemoved, this,
            &ClipListItem::onRowsRemoved);
    connect(model_, &QAbstractItemModel::modelReset, this, [this]() {
        rebuildElements();
        layoutAll();
        scrollToBottom(true);
        update();
    });
    connect(model_, &QAbstractItemModel::dataChanged, this,
            &ClipListItem::onDataChanged);

    rebuildElements();
    layoutAll();
}

void ClipListItem::rebuildElements() {
    elements_.clear();
    hoveredElementIndex_ = -1;
    pressedElementIndex_ = -1;
    if (!model_) return;
    const int n = model_->rowCount();
    elements_.resize(n);
    for (int i = 0; i < n; ++i) {
        elements_[i] = std::make_unique<ClipElement>(this);
        elements_[i]->setRow(i);
        elements_[i]->refreshContent();
    }
}

ClipListItem::Anchor ClipListItem::captureAnchor() const {
    Anchor a;
    if (elements_.empty()) return a;
    const int idx = findElementIndexByY(scrollY_);
    a.index = idx;
    a.offset = scrollY_ - elements_[idx]->y();
    a.valid = true;
    return a;
}

void ClipListItem::restoreAnchor(const Anchor& anchor) {
    if (!anchor.valid || elements_.empty()) {
        applyScroll(0);
        return;
    }
    const int idx = qBound(0, anchor.index, static_cast<int>(elements_.size()) - 1);
    applyScroll(elements_[idx]->y() + anchor.offset);
}

void ClipListItem::onRowsInserted(const QModelIndex& parent, int first, int last) {
    Q_UNUSED(parent);
    if (first < 0 || last < first || !model_) return;

    const Anchor anchor = captureAnchor();
    const bool wasAtBottom = stickBottom_;

    const int count = last - first + 1;
    for (int i = 0; i < count; ++i) {
        elements_.emplace(elements_.begin() + first);
    }
    for (int row = first; row <= last; ++row) {
        elements_[row] = std::make_unique<ClipElement>(this);
        elements_[row]->setRow(row);
        elements_[row]->refreshContent();
    }
    fixRowsFrom(last + 1);

    for (int row = first; row <= last; ++row) {
        layoutElement(row);
    }
    repositionFrom(first);

    if (wasAtBottom && first >= static_cast<int>(elements_.size()) - count) {
        scrollToBottom(true);  // new clips appended at the bottom
    } else {
        restoreAnchor(anchor);
    }
    update();
}

void ClipListItem::onRowsRemoved(const QModelIndex& parent, int first, int last) {
    Q_UNUSED(parent);
    const Anchor anchor = pendingAnchor_.valid ? pendingAnchor_ : captureAnchor();
    pendingAnchor_ = Anchor{};

    if (first < 0 || last >= static_cast<int>(elements_.size())) return;
    elements_.erase(elements_.begin() + first, elements_.begin() + last + 1);
    fixRowsFrom(first);

    if (hoveredElementIndex_ > last) {
        hoveredElementIndex_ -= (last - first + 1);
    } else if (hoveredElementIndex_ >= first) {
        hoveredElementIndex_ = -1;
        hoveredZone_ = ClipElement::Zone::None;
    }
    if (pressedElementIndex_ >= first && pressedElementIndex_ <= last) {
        pressedElementIndex_ = -1;
    }

    if (elements_.empty()) {
        scrollY_ = 0;
        stickBottom_ = true;
    } else {
        repositionFrom(first);
        restoreAnchor(anchor);
    }
    update();
}

void ClipListItem::onDataChanged(const QModelIndex& topLeft,
                                 const QModelIndex& bottomRight,
                                 const QVector<int>& roles) {
    Q_UNUSED(roles);
    const int first = qMax(0, topLeft.row());
    const int last = qMin(bottomRight.row(),
                          static_cast<int>(elements_.size()) - 1);
    for (int row = first; row <= last; ++row) {
        elements_[row]->refreshContent();
        layoutElement(row);
    }
    repositionFrom(first);
    update();
}

void ClipListItem::fixRowsFrom(int index) {
    for (int i = index; i < static_cast<int>(elements_.size()); ++i) {
        elements_[i]->setRow(i);
    }
}

// ---------------------------------------------------------------------------
// Layout: cumulative y positions with per-element memoized heights. Elements
// only re-measure when dirty or when the width changed.
// ---------------------------------------------------------------------------

void ClipListItem::layoutAll() {
    const int w = qFloor(width());
    for (auto& el : elements_) {
        el->setPendingResize(true);
        el->resizeGetHeight(w);
    }
    repositionFrom(0);
}

void ClipListItem::layoutElement(int index) {
    if (index < 0 || index >= static_cast<int>(elements_.size())) return;
    elements_[index]->resizeGetHeight(qFloor(width()));
}

void ClipListItem::repositionFrom(int index) {
    if (index < 0 || index >= static_cast<int>(elements_.size())) return;
    int y = index == 0
                ? kTopMargin
                : elements_[index - 1]->y() + elements_[index - 1]->height() +
                      kSpacing;
    for (int i = index; i < static_cast<int>(elements_.size()); ++i) {
        elements_[i]->setY(y);
        y += elements_[i]->height() + kSpacing;
    }
}

int ClipListItem::contentHeight() const {
    if (elements_.empty()) return kTopMargin + kBottomMargin;
    const ClipElement* last = elements_.back().get();
    return last->y() + last->height() + kBottomMargin;
}

int ClipListItem::maxScroll() const {
    return qMax(0, contentHeight() - viewportHeightCached_);
}

int ClipListItem::findElementIndexByY(int y) const {
    if (elements_.empty()) return -1;
    auto it = std::lower_bound(
        elements_.begin(), elements_.end(), y,
        [](const std::unique_ptr<ClipElement>& el, int top) {
            return el->y() + el->height() <= top;
        });
    if (it == elements_.end()) return static_cast<int>(elements_.size()) - 1;
    return static_cast<int>(it - elements_.begin());
}

ClipElement* ClipListItem::elementAt(int y) const {
    const int idx = findElementIndexByY(y);
    return idx >= 0 ? elements_[idx].get() : nullptr;
}

QRectF ClipListItem::elementSceneRect(const ClipElement* el) const {
    return QRectF(el->bubbleRect().left(), el->y() + el->bubbleRect().top(),
                  el->bubbleRect().width(), el->bubbleRect().height());
}

QRectF ClipListItem::bubbleSceneRect(const QString& clipId) const {
    for (const auto& el : elements_) {
        if (el->clipId() == clipId) return elementSceneRect(el.get());
    }
    return QRectF();
}

void ClipListItem::repaintElement(const ClipElement* el) {
    // Band-only repaint (ListWidget::repaintItem port).
    update(QRect(0, el->y(), qCeil(width()), qCeil(el->height())));
}

// ---------------------------------------------------------------------------
// Painting: binary-search the visible range from the damaged rect, translate,
// draw only those elements. Identical structure to ListWidget::paintEvent.
// ---------------------------------------------------------------------------

void ClipListItem::paint(QPainter* p) {
    p->setRenderHint(QPainter::Antialiasing, true);
    viewportHeightCached_ = qFloor(height());
    const QRectF damage(0, scrollY_, width(), height());

    p->save();
    p->translate(0, -scrollY_);

    const int count = static_cast<int>(elements_.size());
    if (count > 0) {
        int from = findElementIndexByY(qFloor(damage.top()));
        while (from > 0 && elements_[from]->y() > damage.top()) --from;
        for (int i = from; i < count; ++i) {
            ClipElement* el = elements_[i].get();
            if (el->y() >= damage.bottom()) break;
            if (el->pendingResize() || el->height() <= 0) {
                el->resizeGetHeight(qFloor(width()));
            }
            el->ensureImageLoaded();  // idempotent lazy async decode
            p->save();
            p->translate(0, el->y());
            ClipElement::PaintContext ctx;
            ctx.painter = p;
            ctx.clipItemCoords = damage.translated(0, -el->y());
            ctx.dpr = contentsScale();
            ctx.withInteractionChrome = true;
            el->paint(ctx);
            p->restore();
        }
    }
    p->restore();

    paintScrollbar(p);
}

// ---------------------------------------------------------------------------
// Scrolling: wheel easing, drag with rubber band, flick inertia, settle-back.
// ---------------------------------------------------------------------------

void ClipListItem::applyScroll(int newY, bool clampHard) {
    const int max = maxScroll();
    if (clampHard) {
        newY = qBound(0, newY, max);
    }
    stickBottom_ = (newY >= max);
    scrollY_ = newY;
    restartScrollbarFade();
    if (++evictionCounter_ % 16 == 0) {
        evictDistantImages();  // keep decoded images bounded while scrolling
    }
    update();
}

void ClipListItem::evictDistantImages() {
    const int viewH = qMax(1, viewportHeightCached_);
    const int lo = scrollY_ - viewH * 2;
    const int hi = scrollY_ + viewH * 3;
    for (auto& el : elements_) {
        if (el->y() + el->height() < lo || el->y() > hi) {
            el->releaseImage();
        }
    }
}

void ClipListItem::scrollToBottom(bool instant) {
    const int target = maxScroll();
    if (instant) {
        stopAnimations();
        applyScroll(target);
    } else {
        animState_ = Anim::Wheel;
        animTarget_ = target;
        animClock_.restart();
        if (!animationTimer_.isActive()) animationTimer_.start();
    }
}

void ClipListItem::stopAnimations() {
    animationTimer_.stop();
    animState_ = Anim::None;
    flickVelocity_ = 0;
}

void ClipListItem::startWheelAnimation() {
    animState_ = Anim::Wheel;
    animClock_.restart();
    if (!animationTimer_.isActive()) animationTimer_.start();
}

void ClipListItem::startFlick(qreal velocityPxPerSec) {
    const int max = maxScroll();
    const bool outOfBounds = scrollY_ < 0 || scrollY_ > max;
    if (qAbs(velocityPxPerSec) < kFlickStartVelocity && !outOfBounds) {
        startSettleBack();
        return;
    }
    flickVelocity_ =
        clampScalar(velocityPxPerSec, -kMaxFlickVelocity, kMaxFlickVelocity);
    animState_ = Anim::Flick;
    animClock_.restart();
    if (!animationTimer_.isActive()) animationTimer_.start();
}

void ClipListItem::startSettleBack() {
    const int max = maxScroll();
    if (scrollY_ >= 0 && scrollY_ <= max) {
        stopAnimations();
        return;
    }
    animState_ = Anim::Settle;
    animClock_.restart();
    if (!animationTimer_.isActive()) animationTimer_.start();
}

void ClipListItem::tickAnimations() {
    const qreal dt = qBound(0.001, animClock_.restart() / 1000.0, 0.05);
    switch (animState_) {
        case Anim::Wheel: {
            const qreal diff = animTarget_ - scrollY_;
            if (qAbs(diff) < 0.6) {
                applyScroll(animTarget_);
                stopAnimations();
                break;
            }
            applyScroll(qRound(scrollY_ + diff * std::min(1.0, dt * kWheelEaseRate)));
            break;
        }
        case Anim::Flick: {
            flickVelocity_ *= std::exp(-dt * kFlickDecay);
            const qreal delta = flickVelocity_ * dt;
            const int next = qRound(scrollY_ - delta);
            const int max = maxScroll();
            if ((next < 0 && scrollY_ >= 0) || (next > max && scrollY_ <= max)) {
                // Hit an edge mid-flick: bounce off into settle mode.
                applyScroll(next, false);
                startSettleBack();
                break;
            }
            applyScroll(qBound(-max / 2, next, max + max / 2), false);
            if (qAbs(flickVelocity_) < kFlickCutoffVelocity) {
                startSettleBack();
            }
            break;
        }
        case Anim::Settle: {
            const int max = maxScroll();
            const int bound = scrollY_ < 0 ? 0 : max;
            const qreal diff = bound - scrollY_;
            if (qAbs(diff) < 0.7) {
                applyScroll(bound);
                stopAnimations();
                break;
            }
            applyScroll(qRound(scrollY_ + diff * std::min(1.0, dt * 14.0)));
            break;
        }
        case Anim::None:
            animationTimer_.stop();
            break;
    }
}

// ---------------------------------------------------------------------------
// Interaction: manual hit testing exactly like mouseActionUpdate/pointState.
// ---------------------------------------------------------------------------

ClipListItem::HitContext ClipListItem::hitContextAt(QPointF itemPos) const {
    HitContext ctx;
    if (elements_.empty()) return ctx;
    const QPointF clamped(itemPos.x(), itemPos.y() + scrollY_);
    const int idx = findElementIndexByY(qFloor(clamped.y()));
    if (idx < 0) return ctx;
    ClipElement* el = elements_[idx].get();
    ctx.elementIndex = idx;
    ctx.hit = el->hitTest(QPointF(itemPos.x(), clamped.y() - el->y()),
                          qFloor(width()));
    return ctx;
}

void ClipListItem::updateHover(QPointF itemPos) {
    const HitContext ctx = hitContextAt(itemPos);

    Qt::CursorShape shape = Qt::ArrowCursor;
    switch (ctx.hit.zone) {
        case ClipElement::Zone::BtnLink:
        case ClipElement::Zone::BtnCopy:
        case ClipElement::Zone::BtnDownload:
        case ClipElement::Zone::BtnSend:
        case ClipElement::Zone::BtnDelete:
        case ClipElement::Zone::ExpandChip:
        case ClipElement::Zone::Image:
            shape = Qt::PointingHandCursor;
            break;
        case ClipElement::Zone::Text:
            shape = ctx.hit.url.isEmpty() ? Qt::IBeamCursor
                                          : Qt::PointingHandCursor;
            break;
        default:
            break;
    }
    if (cursor().shape() != shape) setCursor(shape);

    const int newIdx = ctx.elementIndex;
    const ClipElement::Zone newZone =
        ctx.hit.zone == ClipElement::Zone::Text && !ctx.hit.url.isEmpty()
            ? ClipElement::Zone::Text
            : ctx.hit.zone;

    if (newIdx != hoveredElementIndex_) {
        if (hoveredElementIndex_ >= 0 &&
            hoveredElementIndex_ < static_cast<int>(elements_.size())) {
            auto& prev = elements_[hoveredElementIndex_];
            if (prev->setHover(ClipElement::Zone::None)) repaintElement(prev.get());
        }
        hoveredElementIndex_ = newIdx;
        hoveredZone_ = newZone;
        if (newIdx >= 0) {
            auto& cur = elements_[newIdx];
            if (cur->setHover(newZone)) repaintElement(cur.get());
        }
    } else if (hoveredElementIndex_ >= 0) {
        auto& cur = elements_[hoveredElementIndex_];
        if (cur->setHover(newZone)) repaintElement(cur.get());
        hoveredZone_ = newZone;
    }
}

void ClipListItem::activateZone(const HitContext& at) {
    if (at.elementIndex < 0 ||
        at.elementIndex >= static_cast<int>(elements_.size()))
        return;
    ClipElement* el = elements_[at.elementIndex].get();
    auto* ctrl = typedController(controller_);

    switch (at.hit.zone) {
        case ClipElement::Zone::BtnCopy:
            if (!ctrl) break;
            if (el->isImage()) {
                ctrl->copyImageToClipboard(el->row());
            } else if (el->hasSelection()) {
                ctrl->copyToClipboard(el->selectedText(qFloor(width())));
            } else {
                ctrl->copyToClipboard(model_->getClipText(el->row()));
            }
            break;
        case ClipElement::Zone::BtnDelete:
            handleDelete(el);
            break;
        case ClipElement::Zone::BtnSend:
            if (!ctrl) break;
            if (el->isImage()) {
                ctrl->pushImage(el->imageSourceKey());
            } else {
                ctrl->pushClipboard(model_->getClipText(el->row()));
            }
            break;
        case ClipElement::Zone::BtnDownload:
            emit saveImageRequested(el->row());
            break;
        case ClipElement::Zone::BtnLink:
            if (ctrl && !firstLinkOf(el).isEmpty()) ctrl->openUrl(firstLinkOf(el));
            break;
        case ClipElement::Zone::ExpandChip:
            el->setExpanded(true);
            layoutElement(at.elementIndex);
            repositionFrom(at.elementIndex);
            applyScroll(scrollY_);
            update();
            break;
        case ClipElement::Zone::Image:
            emit fullPreviewRequested(el->imageSourceKey());
            break;
        case ClipElement::Zone::Text:
            if (!at.hit.url.isEmpty() && ctrl) ctrl->openUrl(at.hit.url);
            break;
        default:
            break;
    }
}

QString ClipListItem::firstLinkOf(ClipElement* el) const {
    const QString text = model_->getClipText(el->row());
    const auto links = ClipTextLayout::detectLinks(text);
    return links.isEmpty() ? QString() : links.first().url;
}

void ClipListItem::handleDelete(ClipElement* el) {
    auto* ctrl = typedController(controller_);
    if (!ctrl) return;
    if (ctrl->thanosSnapEnabled() && thanosTarget_) {
        // Render the bubble ourselves instead of grabToImage().
        const QImage snap = el->snapshotBubble(contentsScale());
        const QPointF topLeft =
            mapToItem(thanosTarget_.data(), el->bubbleRect().topLeft());
        emit snapRequested(snap,
                           QRectF(topLeft, el->bubbleRect().size()),
                           el->clipId());
        // The QML handler starts the dissolve and removes the row afterwards.
    } else {
        model_->removeClipById(el->clipId());
    }
}

void ClipListItem::selectWordAt(ClipElement* el, int position) {
    const QString& s = el->textForSelection();
    int a = qBound(0, position, s.length());
    int b = a;
    while (a > 0 && !s.at(a - 1).isSpace()) --a;
    while (b < s.length() && !s.at(b).isSpace()) ++b;
    el->setSelection(a, b);
}

// ---------------------------------------------------------------------------
// Input events
// ---------------------------------------------------------------------------

void ClipListItem::wheelEvent(QWheelEvent* event) {
    const double dy = event->angleDelta().y();
    if (dy == 0) {
        event->ignore();
        return;
    }
    event->accept();

    if (animState_ == Anim::Flick || animState_ == Anim::Settle) stopAnimations();
    const int base = animState_ == Anim::Wheel ? animTarget_ : scrollY_;
    animTarget_ = qBound(0, base - qRound(dy / 120.0 * kWheelStepPerNotch),
                         maxScroll());
    stickBottom_ = (animTarget_ >= maxScroll());
    startWheelAnimation();
}

void ClipListItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    forceActiveFocus();
    event->accept();

    stopAnimations();
    pendingAnchor_ = Anchor{};

    const QPointF pos = event->position();

    if (maxScroll() > 0 && scrollbarThumbRect().adjusted(-6, -6, 6, 6)
                               .contains(pos)) {
        scrollbarGesture_ = ScrollbarGesture::Dragging;
        scrollbarGrabOffset_ = pos.y() - scrollbarThumbRect().top();
        restartScrollbarFade();
        return;
    }

    gesture_ = Gesture::Pressing;
    pressPos_ = pos;
    pressScrollY_ = scrollY_;
    dragSamples_.clear();
    dragSamples_.emplace_back(nowMs(), scrollY_);

    const HitContext ctx = hitContextAt(pos);
    pressedElementIndex_ = ctx.elementIndex;
    pressedZone_ = ctx.hit.zone;
    pressedLink_ = ctx.hit.url;

    if (pressedElementIndex_ >= 0) {
        auto& el = elements_[pressedElementIndex_];
        switch (pressedZone_) {
            case ClipElement::Zone::BtnLink:
            case ClipElement::Zone::BtnCopy:
            case ClipElement::Zone::BtnDownload:
            case ClipElement::Zone::BtnSend:
            case ClipElement::Zone::BtnDelete:
                el->setPressed(true);
                repaintElement(el.get());
                break;
            default:
                break;
        }
    }
}

void ClipListItem::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
    const QPointF pos = event->position();

    if (scrollbarGesture_ == ScrollbarGesture::Dragging) {
        const qreal trackH = height() - scrollbarThumbRect().height();
        const qreal thumbY =
            clampScalar(pos.y() - scrollbarGrabOffset_, 0,
                        qMax<qreal>(0, trackH));
        const int max = maxScroll();
        applyScroll(trackH > 0 ? qRound(thumbY / trackH * max) : 0);
        return;
    }

    switch (gesture_) {
        case Gesture::Pressing: {
            if ((pos - pressPos_).manhattanLength() <
                QApplication::startDragDistance() * 2) {
                break;
            }
            if (pressedZone_ == ClipElement::Zone::Text &&
                pressedElementIndex_ >= 0) {
                gesture_ = Gesture::SelectingText;
                auto& el = elements_[pressedElementIndex_];
                const int pos0 = el->hitTest(
                    QPointF(pressPos_.x(), pressPos_.y() + scrollY_ - el->y()),
                    qFloor(width())).textPosition;
                el->setSelection(pos0 < 0 ? 0 : pos0,
                                 pos0 < 0 ? 0 : pos0);
                repaintElement(el.get());
            } else {
                releasePressVisuals();
                gesture_ = Gesture::DraggingScroll;
            }
            dragSamples_.clear();
            dragSamples_.emplace_back(nowMs(), scrollY_);
            break;
        }
        case Gesture::DraggingScroll: {
            const qreal desired = pressScrollY_ + (pressPos_.y() - pos.y());
            const int max = maxScroll();
            qreal applied = desired;
            if (desired < 0) applied = desired * kBounceResistance;
            else if (desired > max) applied = max + (desired - max) * kBounceResistance;
            applyScroll(qRound(applied), false);
            dragSamples_.emplace_back(nowMs(), scrollY_);
            while (dragSamples_.size() > 2 && nowMs() - dragSamples_.front().first > 100) {
                dragSamples_.pop_front();
            }
            break;
        }
        case Gesture::SelectingText: {
            if (pressedElementIndex_ < 0) break;
            auto& el = elements_[pressedElementIndex_];
            const QPointF local(pos.x(), pos.y() + scrollY_ - el->y());
            const int cursorPos = el->hitTest(local, qFloor(width())).textPosition;
            if (cursorPos >= 0) {
                const auto [anchor, cursor] = el->selectionRange();
                if (cursor != cursorPos) {
                    el->setSelection(anchor < 0 ? cursorPos : anchor, cursorPos);
                    repaintElement(el.get());
                }
            }
            break;
        }
        case Gesture::None:
            updateHover(pos);
            break;
    }
}

void ClipListItem::mouseReleaseEvent(QMouseEvent* event) {
    event->accept();
    const QPointF pos = event->position();

    if (scrollbarGesture_ == ScrollbarGesture::Dragging) {
        scrollbarGesture_ = ScrollbarGesture::None;
        restartScrollbarFade();
        return;
    }

    switch (gesture_) {
        case Gesture::DraggingScroll: {
            qreal velocity = 0;
            if (dragSamples_.size() >= 2) {
                const auto [t0, s0] = dragSamples_.front();
                const auto [t1, s1] = dragSamples_.back();
                const qreal dt = qMax<qreal>(1.0, t1 - t0);
                velocity = (s1 - s0) / dt * 1000.0;  // px/s in scroll space
            }
            gesture_ = Gesture::None;
            startFlick(-velocity);
            break;
        }
        case Gesture::SelectingText:
            gesture_ = Gesture::None;
            break;
        case Gesture::Pressing: {
            gesture_ = Gesture::None;
            releasePressVisuals();
            HitContext ctx;
            ctx.elementIndex = pressedElementIndex_;
            ctx.hit.zone = pressedZone_;
            ctx.hit.url = pressedLink_;
            // Re-verify the pointer is still inside the same zone.
            const HitContext now = hitContextAt(pos);
            activateZone(now.elementIndex == ctx.elementIndex &&
                                 now.hit.zone == ctx.hit.zone
                             ? now
                             : HitContext{});
            break;
        }
        case Gesture::None:
            break;
    }
    pressedElementIndex_ = -1;
    pressedZone_ = ClipElement::Zone::None;
    pressedLink_.clear();
}

void ClipListItem::releasePressVisuals() {
    if (pressedElementIndex_ >= 0 &&
        pressedElementIndex_ < static_cast<int>(elements_.size())) {
        auto& el = elements_[pressedElementIndex_];
        el->setPressed(false);
        repaintElement(el.get());
    }
}

void ClipListItem::mouseDoubleClickEvent(QMouseEvent* event) {
    event->accept();
    const HitContext ctx = hitContextAt(event->position());
    if (ctx.elementIndex < 0) return;
    auto& el = elements_[ctx.elementIndex];
    if (ctx.hit.zone == ClipElement::Zone::Text && ctx.hit.textPosition >= 0) {
        selectWordAt(el.get(), ctx.hit.textPosition);
        repaintElement(el.get());
    }
}

void ClipListItem::hoverMoveEvent(QHoverEvent* event) {
    if (gesture_ == Gesture::None && scrollbarGesture_ == ScrollbarGesture::None) {
        updateHover(event->position());
    }
    QQuickPaintedItem::hoverMoveEvent(event);
}

void ClipListItem::hoverLeaveEvent(QHoverEvent* event) {
    if (hoveredElementIndex_ >= 0 &&
        hoveredElementIndex_ < static_cast<int>(elements_.size())) {
        auto& el = elements_[hoveredElementIndex_];
        if (el->setHover(ClipElement::Zone::None)) repaintElement(el.get());
    }
    hoveredElementIndex_ = -1;
    hoveredZone_ = ClipElement::Zone::None;
    setCursor(Qt::ArrowCursor);
    QQuickPaintedItem::hoverLeaveEvent(event);
}

void ClipListItem::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Copy)) {
        for (auto& el : elements_) {
            if (el->hasSelection()) {
                auto* ctrl = typedController(controller_);
                if (ctrl) ctrl->copyToClipboard(el->selectedText(qFloor(width())));
                event->accept();
                return;
            }
        }
    }
    QQuickPaintedItem::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// Scrollbar overlay
// ---------------------------------------------------------------------------

QRectF ClipListItem::scrollbarThumbRect() const {
    const int max = maxScroll();
    if (max <= 0 || height() <= 0) return QRectF();
    const qreal trackH = height();
    const qreal thumbH =
        qMax<qreal>(32.0, trackH * static_cast<qreal>(height()) / contentHeight());
    const qreal maxY = trackH - thumbH;
    const qreal y = maxY > 0 ? static_cast<qreal>(scrollY_) / max * maxY : 0;
    return QRectF(width() - 8, y, 4, thumbH);
}

bool ClipListItem::scrollbarContains(QPointF pos) const {
    return scrollbarThumbRect().contains(pos);
}

void ClipListItem::restartScrollbarFade() {
    scrollbarTargetOpacity_ = 1.0;
    lastScrollbarInteractionMs_ = nowMs();
    if (scrollbarAnimTimerId_ == 0) {
        scrollbarAnimTimerId_ = startTimer(16, Qt::CoarseTimer);
    }
}

void ClipListItem::timerEvent(QTimerEvent* event) {
    if (event->timerId() == scrollbarAnimTimerId_) {
        // Hold fully visible for 1.4s after the last interaction, then fade.
        if (scrollbarOpacity_ >= 1.0 && scrollbarTargetOpacity_ >= 1.0 &&
            nowMs() - lastScrollbarInteractionMs_ >= 1400) {
            scrollbarTargetOpacity_ = 0.0;
        }
        const qreal step = 0.12;
        qreal next = scrollbarOpacity_;
        if (scrollbarOpacity_ < scrollbarTargetOpacity_) {
            next = qMin(scrollbarTargetOpacity_, scrollbarOpacity_ + step);
        } else if (scrollbarOpacity_ > scrollbarTargetOpacity_) {
            next = qMax(scrollbarTargetOpacity_, scrollbarOpacity_ - step);
        }
        if (next != scrollbarOpacity_) {
            scrollbarOpacity_ = next;
            update(QRect(qFloor(width()) - 16, 0, 16, qCeil(height())));
        }
        if (scrollbarOpacity_ == scrollbarTargetOpacity_ &&
            scrollbarTargetOpacity_ <= 0.0) {
            killTimer(scrollbarAnimTimerId_);
            scrollbarAnimTimerId_ = 0;
        }
        return;
    }
    QQuickPaintedItem::timerEvent(event);
}

void ClipListItem::paintScrollbar(QPainter* p) {
    if (scrollbarOpacity_ <= 0.01) return;
    const QRectF thumb = scrollbarThumbRect();
    if (!thumb.isValid()) return;

    auto* theme = MD3Theme::instance();
    QColor color = scrollbarGesture_ == ScrollbarGesture::Dragging
                       ? theme->primary()
                       : theme->onSurfaceVariant();
    color.setAlphaF(color.alphaF() *
                    (0.25 + 0.35 * scrollbarOpacity_));
    if (scrollbarGesture_ == ScrollbarGesture::Dragging) {
        color.setAlphaF(qMin<qreal>(1.0, color.alphaF() + 0.35));
    }
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawRoundedRect(thumb, 2, 2);
}

// ---------------------------------------------------------------------------
// Async image decode callbacks
// ---------------------------------------------------------------------------

void ClipListItem::onElementImageDecoded(const QString& clipId,
                                         const QString& sourceKey,
                                         const QImage& image) {
    for (int i = 0; i < static_cast<int>(elements_.size()); ++i) {
        auto& el = elements_[i];
        if (el->clipId() != clipId) continue;
        if (el->setImageResult(sourceKey, image)) {
            if (el->pendingResize()) {
                layoutElement(i);
                repositionFrom(i);
                applyScroll(scrollY_);
                update();
            } else {
                repaintElement(el.get());
            }
        }
        return;
    }
}

}  // namespace webclip
