#include "clip_widget.hpp"
#include "../basic/painter_helpers.hpp"
#include "../md3/icon_loader.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../models/clipboard_history_model.hpp"
#include "../../controllers/webclip_controller.hpp"
#include "../../clips/bubble_corners.hpp"
#include "../../clips/clip_text_layout.hpp"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <cmath>

namespace Ui {

ClipWidget::ClipWidget(
    QWidget* parent,
    webclip::WebClipController* controller,
    webclip::ClipboardHistoryModel* model
)
    : RpWidget(parent)
    , controller_(nullptr)
    , model_(nullptr) {
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    animationTimer_.setInterval(16);
    animationTimer_.setTimerType(Qt::CoarseTimer);
    connect(&animationTimer_, &QTimer::timeout, this, &ClipWidget::tickAnimations);

    scrollbarFadeTimer_.setInterval(16);
    scrollbarFadeTimer_.setTimerType(Qt::CoarseTimer);
    connect(&scrollbarFadeTimer_, &QTimer::timeout, this, [this] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastScrollbarInteractionMs_ > 1500) {
            scrollbarTargetOpacity_ = 0.0;
        }
        if (std::abs(scrollbarOpacity_ - scrollbarTargetOpacity_) > 0.01) {
            scrollbarOpacity_ += (scrollbarTargetOpacity_ - scrollbarOpacity_) * 0.20;
            update();
        } else {
            scrollbarOpacity_ = scrollbarTargetOpacity_;
            if (scrollbarOpacity_ == 0.0 || scrollbarOpacity_ == 1.0) {
                scrollbarFadeTimer_.stop();
            }
        }
    });

    auto* theme = webclip::MD3Theme::instance();
    connect(theme, &webclip::MD3Theme::themeChanged, this, [this] {
        webclip::clearBubblePixmapCache();
        webclip::clearClipElementCaches();
        layoutAll();
        applyScroll(scrollY_);
        update();
    });

    if (controller) {
        setController(controller);
    }
    if (model) {
        setModel(model);
    }
}

ClipWidget::~ClipWidget() {
    animationTimer_.stop();
    scrollbarFadeTimer_.stop();
    if (model_) {
        model_->disconnect(this);
    }
}

void ClipWidget::setController(webclip::WebClipController* controller) {
    if (controller_ && controller_ != controller) {
        controller_->disconnect(this);
    }
    controller_ = controller;

    if (controller_) {
        connect(controller_, &webclip::WebClipController::connectedChanged, this, [this] {
            for (auto& el : elements_) {
                el->setPendingResize(true);
            }
            layoutAll();
            applyScroll(scrollY_);
            update();
        });
        if (controller_->clipModel()) {
            setModel(controller_->clipModel());
        }
    }
}

void ClipWidget::setModel(webclip::ClipboardHistoryModel* model) {
    if (model_ && model_ != model) {
        model_->disconnect(this);
    }
    model_ = model;
    attachModel();
    scrollToBottom(true);
}

void ClipWidget::attachModel() {
    if (!model_) return;

    model_->disconnect(this);
    connect(model_, &QAbstractItemModel::rowsInserted, this, &ClipWidget::onRowsInserted);
    connect(model_, &QAbstractItemModel::rowsRemoved, this, &ClipWidget::onRowsRemoved);
    connect(model_, &QAbstractItemModel::dataChanged, this, &ClipWidget::onDataChanged);
    connect(model_, &QAbstractItemModel::modelReset, this, [this] {
        rebuildElements();
        scrollToBottom(true);
    });

    rebuildElements();
}

void ClipWidget::rebuildElements() {
    elements_.clear();
    if (!model_) return;

    const int n = model_->rowCount();
    elements_.reserve(n);
    for (int i = 0; i < n; ++i) {
        auto el = std::make_unique<webclip::ClipElement>(this);
        el->setRow(i);
        el->refreshContent();
        elements_.push_back(std::move(el));
    }
    layoutAll();
    applyScroll(scrollY_);
    update();
}

ClipWidget::Anchor ClipWidget::captureAnchor() const {
    Anchor a;
    if (elements_.empty() || stickBottom_) return a;

    for (int i = 0; i < static_cast<int>(elements_.size()); ++i) {
        const int elTop = elements_[i]->y();
        const int elBottom = elTop + elements_[i]->height();
        if (elBottom >= scrollY_) {
            a.index = i;
            a.offset = scrollY_ - elTop;
            a.valid = true;
            break;
        }
    }
    return a;
}

void ClipWidget::restoreAnchor(const Anchor& anchor) {
    if (!anchor.valid || anchor.index < 0 || anchor.index >= static_cast<int>(elements_.size())) {
        if (stickBottom_) scrollToBottom(true);
        return;
    }
    const int newScroll = elements_[anchor.index]->y() + anchor.offset;
    applyScroll(newScroll, false);
}

void ClipWidget::onRowsInserted(const QModelIndex&, int first, int last) {
    const bool wasAtBottom = stickBottom_ || (scrollY_ >= maxScroll() - 20);
    const Anchor anchor = captureAnchor();

    const int count = last - first + 1;
    std::vector<std::unique_ptr<webclip::ClipElement>> newElements;
    newElements.reserve(count);
    for (int i = first; i <= last; ++i) {
        auto el = std::make_unique<webclip::ClipElement>(this);
        el->setRow(i);
        el->refreshContent();
        newElements.push_back(std::move(el));
    }

    elements_.insert(
        elements_.begin() + first,
        std::make_move_iterator(newElements.begin()),
        std::make_move_iterator(newElements.end())
    );

    fixRowsFrom(last + 1);
    for (int i = first; i < static_cast<int>(elements_.size()); ++i) {
        layoutElement(i);
    }
    repositionFrom(first);

    if (wasAtBottom) {
        scrollToBottom(true);
    } else {
        restoreAnchor(anchor);
    }
    update();
}

void ClipWidget::onRowsRemoved(const QModelIndex&, int first, int last) {
    const Anchor anchor = captureAnchor();
    elements_.erase(elements_.begin() + first, elements_.begin() + last + 1);
    fixRowsFrom(first);
    repositionFrom(first);

    if (stickBottom_) {
        scrollToBottom(true);
    } else {
        restoreAnchor(anchor);
    }
    update();
}

void ClipWidget::onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>&) {
    for (int r = topLeft.row(); r <= bottomRight.row() && r < static_cast<int>(elements_.size()); ++r) {
        elements_[r]->refreshContent();
        layoutElement(r);
    }
    repositionFrom(topLeft.row());
    applyScroll(scrollY_);
    update();
}

void ClipWidget::fixRowsFrom(int index) {
    for (int i = index; i < static_cast<int>(elements_.size()); ++i) {
        elements_[i]->setRow(i);
    }
}

void ClipWidget::layoutAll() {
    if (width() <= 0) return;
    for (int i = 0; i < static_cast<int>(elements_.size()); ++i) {
        layoutElement(i);
    }
    repositionFrom(0);
}

void ClipWidget::layoutElement(int index) {
    if (index < 0 || index >= static_cast<int>(elements_.size())) return;
    elements_[index]->resizeGetHeight(width());
}

void ClipWidget::repositionFrom(int index) {
    int curY = (index == 0) ? kTopMargin : (elements_[index - 1]->y() + elements_[index - 1]->height() + kSpacing);
    for (int i = index; i < static_cast<int>(elements_.size()); ++i) {
        elements_[i]->setY(curY);
        curY += elements_[i]->height() + kSpacing;
    }
}

int ClipWidget::contentHeight() const {
    if (elements_.empty()) return 0;
    const auto& last = elements_.back();
    return last->y() + last->height() + kBottomMargin;
}

int ClipWidget::maxScroll() const {
    const int ch = contentHeight();
    const int vh = height();
    return std::max(0, ch - vh);
}

int ClipWidget::findElementIndexByY(int y) const {
    if (elements_.empty()) return -1;
    int lo = 0;
    int hi = static_cast<int>(elements_.size()) - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const int elTop = elements_[mid]->y();
        const int elBottom = elTop + elements_[mid]->height();
        if (y < elTop) {
            hi = mid - 1;
        } else if (y > elBottom) {
            lo = mid + 1;
        } else {
            return mid;
        }
    }
    return -1;
}

webclip::ClipElement* ClipWidget::elementAt(int y) const {
    const int idx = findElementIndexByY(y);
    return (idx >= 0 && idx < static_cast<int>(elements_.size())) ? elements_[idx].get() : nullptr;
}

void ClipWidget::applyScroll(int newY, bool clampHard) {
    const int ms = maxScroll();
    if (clampHard) {
        scrollY_ = std::clamp(newY, 0, ms);
    } else {
        scrollY_ = newY;
    }
    stickBottom_ = (scrollY_ >= ms);
    update();
}

void ClipWidget::scrollToBottom(bool instant) {
    const int ms = maxScroll();
    if (instant) {
        stopAnimations();
        applyScroll(ms, true);
    } else {
        animTarget_ = ms;
        animState_ = Anim::Wheel;
        animClock_.restart();
        if (!animationTimer_.isActive()) animationTimer_.start();
    }
}

void ClipWidget::stopAnimations() {
    animState_ = Anim::None;
    flickVelocity_ = 0.0;
    animationTimer_.stop();
}

void ClipWidget::startWheelAnimation() {
    animState_ = Anim::Wheel;
    animClock_.restart();
    if (!animationTimer_.isActive()) animationTimer_.start();
}

void ClipWidget::startFlick(double velocityPxPerSec) {
    flickVelocity_ = velocityPxPerSec;
    animState_ = Anim::Flick;
    animClock_.restart();
    if (!animationTimer_.isActive()) animationTimer_.start();
}

void ClipWidget::startSettleBack() {
    animState_ = Anim::Settle;
    animTarget_ = std::clamp(scrollY_, 0, maxScroll());
    animClock_.restart();
    if (!animationTimer_.isActive()) animationTimer_.start();
}

void ClipWidget::tickAnimations() {
    const double dt = animClock_.restart() / 1000.0;
    if (dt <= 0.0 || dt > 0.1) return;

    if (animState_ == Anim::Wheel) {
        const double diff = animTarget_ - scrollY_;
        if (std::abs(diff) < 0.5) {
            applyScroll(animTarget_, true);
            stopAnimations();
        } else {
            applyScroll(static_cast<int>(std::round(scrollY_ + diff * std::min(1.0, dt * 25.0))), true);
        }
    } else if (animState_ == Anim::Flick) {
        applyScroll(static_cast<int>(std::round(scrollY_ - flickVelocity_ * dt)), false);
        flickVelocity_ *= std::pow(0.15, dt);

        const int ms = maxScroll();
        if (scrollY_ < 0 || scrollY_ > ms) {
            startSettleBack();
        } else if (std::abs(flickVelocity_) < 30.0) {
            stopAnimations();
        }
    } else if (animState_ == Anim::Settle) {
        const double diff = animTarget_ - scrollY_;
        if (std::abs(diff) < 0.5) {
            applyScroll(animTarget_, true);
            stopAnimations();
        } else {
            applyScroll(static_cast<int>(std::round(scrollY_ + diff * std::min(1.0, dt * 25.0))), false);
        }
    } else {
        stopAnimations();
    }
}

ClipWidget::HitContext ClipWidget::hitContextAt(const QPointF& itemPos) const {
    HitContext ctx;
    const int contentY = static_cast<int>(itemPos.y()) + scrollY_;
    ctx.elementIndex = findElementIndexByY(contentY);
    if (ctx.elementIndex >= 0) {
        const auto* el = elements_[ctx.elementIndex].get();
        const QPointF rel(itemPos.x(), contentY - el->y());
        ctx.hit = el->hitTest(rel, width());
    }
    return ctx;
}

void ClipWidget::updateHover(const QPointF& itemPos) {
    const HitContext ctx = hitContextAt(itemPos);
    if (ctx.elementIndex != hoveredElementIndex_ || ctx.hit.zone != hoveredZone_) {
        if (hoveredElementIndex_ >= 0 && hoveredElementIndex_ < static_cast<int>(elements_.size())) {
            elements_[hoveredElementIndex_]->setHover(webclip::ClipElement::Zone::None);
        }
        hoveredElementIndex_ = ctx.elementIndex;
        hoveredZone_ = ctx.hit.zone;
        if (hoveredElementIndex_ >= 0 && hoveredElementIndex_ < static_cast<int>(elements_.size())) {
            elements_[hoveredElementIndex_]->setHover(hoveredZone_);
        }
        update();
    }
}

void ClipWidget::activateZone(const HitContext& at) {
    if (at.elementIndex < 0 || at.elementIndex >= static_cast<int>(elements_.size())) return;
    auto* el = elements_[at.elementIndex].get();

    switch (at.hit.zone) {
    case webclip::ClipElement::Zone::BtnCopy: {
        if (!controller_) break;
        if (el->isImage()) {
            controller_->copyImageToClipboard(el->row());
        } else if (el->hasSelection()) {
            controller_->copyToClipboard(el->selectedText(width()));
        } else if (model_) {
            controller_->copyToClipboard(model_->getClipText(el->row()));
        }
        break;
    }
    case webclip::ClipElement::Zone::BtnSend: {
        if (!controller_) break;
        if (el->isImage()) {
            controller_->pushImage(el->imageSourceKey());
        } else if (model_) {
            controller_->pushClipboard(model_->getClipText(el->row()));
        }
        break;
    }
    case webclip::ClipElement::Zone::BtnLink: {
        if (controller_) {
            const QString link = firstLinkOf(el);
            if (!link.isEmpty()) controller_->openUrl(link);
        }
        break;
    }
    case webclip::ClipElement::Zone::BtnDownload:
        emit saveImageRequested(el->row());
        break;
    case webclip::ClipElement::Zone::BtnDelete:
        handleDelete(el);
        break;
    case webclip::ClipElement::Zone::ExpandChip:
        el->setExpanded(true);
        layoutElement(at.elementIndex);
        repositionFrom(at.elementIndex);
        applyScroll(scrollY_);
        update();
        break;
    case webclip::ClipElement::Zone::Image:
        emit fullPreviewRequested(el->imageSourceKey());
        break;
    default:
        break;
    }
}

void ClipWidget::handleDelete(webclip::ClipElement* el) {
    if (model_ && el) {
        model_->removeClipById(el->clipId());
    }
}

QString ClipWidget::firstLinkOf(webclip::ClipElement* el) const {
    if (!model_ || !el) return QString();
    const QString text = model_->getClipText(el->row());
    const auto links = webclip::ClipTextLayout::detectLinks(text);
    return links.isEmpty() ? QString() : links.first().url;
}

void ClipWidget::releasePressVisuals() {
    if (pressedElementIndex_ >= 0 && pressedElementIndex_ < static_cast<int>(elements_.size())) {
        elements_[pressedElementIndex_]->setPressed(false);
    }
    pressedElementIndex_ = -1;
    pressedZone_ = webclip::ClipElement::Zone::None;
    update();
}

void ClipWidget::evictDistantImages() {
    if (++evictionCounter_ < 20) return;
    evictionCounter_ = 0;
    const int topBound = scrollY_ - height() * 2;
    const int bottomBound = scrollY_ + height() * 3;
    for (auto& el : elements_) {
        if (el->y() + el->height() < topBound || el->y() > bottomBound) {
            el->releaseImage();
        }
    }
}

void ClipWidget::restartScrollbarFade() {
    lastScrollbarInteractionMs_ = QDateTime::currentMSecsSinceEpoch();
    scrollbarTargetOpacity_ = 1.0;
    if (!scrollbarFadeTimer_.isActive()) {
        scrollbarFadeTimer_.start();
    }
}

QRectF ClipWidget::scrollbarThumbRect() const {
    const int ch = contentHeight();
    const int vh = height();
    if (ch <= vh || vh <= 0) return QRectF();

    const double ratio = static_cast<double>(vh) / ch;
    const double thumbH = std::max(24.0, vh * ratio);
    const double maxScrollVal = maxScroll();
    const double scrollRatio = (maxScrollVal > 0) ? std::clamp(static_cast<double>(scrollY_) / maxScrollVal, 0.0, 1.0) : 0.0;
    const double thumbY = scrollRatio * (vh - thumbH);

    return QRectF(width() - 6.0, thumbY, 4.0, thumbH);
}

void ClipWidget::paintScrollbar(QPainter* p) {
    if (scrollbarOpacity_ <= 0.0) return;
    const QRectF r = scrollbarThumbRect();
    if (r.isEmpty()) return;

    ScopedPainterOpacity op(*p, scrollbarOpacity_);
    p->setPen(Qt::NoPen);
    p->setBrush(webclip::MD3Theme::instance()->outline());
    p->drawRoundedRect(r, 2.0, 2.0);
}

void ClipWidget::onElementImageDecoded(const QString& clipId, const QString& sourceKey, const QImage& image) {
    for (auto& el : elements_) {
        if (el->clipId() == clipId && el->setImageResult(sourceKey, image)) {
            update();
            break;
        }
    }
}

void ClipWidget::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    layoutAll();
    applyScroll(scrollY_);
}

void ClipWidget::wheelEvent(QWheelEvent* e) {
    const int delta = e->angleDelta().y();
    if (delta == 0) return;

    if (animState_ != Anim::Wheel) {
        animTarget_ = scrollY_;
    }
    animTarget_ = std::clamp(animTarget_ - delta, 0, maxScroll());
    startWheelAnimation();
    restartScrollbarFade();
}

void ClipWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        stopAnimations();
        pressPos_ = e->pos();
        pressScrollY_ = scrollY_;

        const HitContext ctx = hitContextAt(e->pos());
        pressedElementIndex_ = ctx.elementIndex;
        pressedZone_ = ctx.hit.zone;

        if (pressedElementIndex_ >= 0 && pressedElementIndex_ < static_cast<int>(elements_.size())) {
            elements_[pressedElementIndex_]->setPressed(true);
            update();
        }
    }
    RpWidget::mousePressEvent(e);
}

void ClipWidget::mouseMoveEvent(QMouseEvent* e) {
    updateHover(e->pos());
    if (e->buttons() & Qt::LeftButton) {
        const double dy = e->pos().y() - pressPos_.y();
        if (gesture_ == Gesture::None && std::abs(dy) > 4.0) {
            gesture_ = Gesture::DraggingScroll;
            releasePressVisuals();
        }

        if (gesture_ == Gesture::DraggingScroll) {
            applyScroll(static_cast<int>(std::round(pressScrollY_ - dy)), false);
            dragSamples_.push_back({QDateTime::currentMSecsSinceEpoch(), static_cast<double>(e->pos().y())});
            while (dragSamples_.size() > 10) dragSamples_.pop_front();
            restartScrollbarFade();
        }
    }
    RpWidget::mouseMoveEvent(e);
}

void ClipWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (gesture_ == Gesture::DraggingScroll) {
            gesture_ = Gesture::None;
            if (dragSamples_.size() >= 2) {
                const auto& first = dragSamples_.front();
                const auto& last = dragSamples_.back();
                const double dt = (last.first - first.first) / 1000.0;
                if (dt > 0.01) {
                    const double vel = (last.second - first.second) / dt;
                    startFlick(vel);
                } else {
                    startSettleBack();
                }
            } else {
                startSettleBack();
            }
            dragSamples_.clear();
        } else {
            const HitContext ctx = hitContextAt(e->pos());
            if (ctx.elementIndex == pressedElementIndex_ && ctx.hit.zone == pressedZone_) {
                activateZone(ctx);
            }
            releasePressVisuals();
        }
    }
    RpWidget::mouseReleaseEvent(e);
}

void ClipWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    const HitContext ctx = hitContextAt(e->pos());
    if (ctx.elementIndex >= 0 && ctx.hit.zone == webclip::ClipElement::Zone::Image) {
        emit fullPreviewRequested(elements_[ctx.elementIndex]->imageSourceKey());
    }
    RpWidget::mouseDoubleClickEvent(e);
}

void ClipWidget::leaveEvent(QEvent* e) {
    if (hoveredElementIndex_ >= 0 && hoveredElementIndex_ < static_cast<int>(elements_.size())) {
        elements_[hoveredElementIndex_]->setHover(webclip::ClipElement::Zone::None);
        hoveredElementIndex_ = -1;
        hoveredZone_ = webclip::ClipElement::Zone::None;
        update();
    }
    RpWidget::leaveEvent(e);
}

void ClipWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Down) {
        applyScroll(scrollY_ + 40, true);
    } else if (e->key() == Qt::Key_Up) {
        applyScroll(scrollY_ - 40, true);
    } else if (e->key() == Qt::Key_PageDown) {
        applyScroll(scrollY_ + height() - 40, true);
    } else if (e->key() == Qt::Key_PageUp) {
        applyScroll(scrollY_ - height() + 40, true);
    } else if (e->key() == Qt::Key_Home) {
        applyScroll(0, true);
    } else if (e->key() == Qt::Key_End) {
        scrollToBottom(true);
    } else {
        RpWidget::keyPressEvent(e);
    }
}

void ClipWidget::paintEvent(QPaintEvent* /*e*/) {
    if (width() <= 0 || height() <= 0) return;

    QPainter p(this);
    PainterHighQualityEnabler hq(p);

    if (elements_.empty()) {
        auto* theme = webclip::MD3Theme::instance();
        const int cx = width() / 2;
        const int cy = height() / 2 - 20;

        // 56x56 primary container circle
        const QRectF avatarRect(cx - 28, cy - 40, 56, 56);
        p.setPen(Qt::NoPen);
        p.setBrush(theme->primaryContainer());
        p.drawEllipse(avatarRect);

        IconLoader::paint(p, QStringLiteral("phone"), QRectF(cx - 13, cy - 25, 26, 26), theme->onPrimaryContainer());

        // Empty title
        p.setFont(theme->titleSmall());
        p.setPen(theme->onSurface());
        const QString emptyTitle = webclip::I18n::instance()->tr(QStringLiteral("chat.empty_title"));
        p.drawText(QRectF(16, cy + 28, width() - 32, 24), Qt::AlignCenter, emptyTitle);

        // Empty subtitle
        p.setFont(theme->bodySmall());
        p.setPen(theme->onSurfaceVariant());
        const QString emptySub = webclip::I18n::instance()->tr(QStringLiteral("chat.empty_subtitle"));
        p.drawText(QRectF(16, cy + 52, width() - 32, 20), Qt::AlignCenter, emptySub);
        return;
    }

    const int vh = height();
    const int topBound = scrollY_;
    const int bottomBound = scrollY_ + vh;

    p.save();
    p.translate(0, -scrollY_);

    for (const auto& el : elements_) {
        const int elTop = el->y();
        const int elBottom = elTop + el->height();
        if (elBottom < topBound || elTop > bottomBound) continue;

        if (el->pendingResize() || el->height() <= 0) {
            el->resizeGetHeight(width());
        }
        el->ensureImageLoaded();

        p.save();
        p.translate(0, el->y());
        webclip::ClipElement::PaintContext ctx;
        ctx.painter = &p;
        ctx.clipItemCoords = QRectF(0, 0, width(), el->height());
        ctx.dpr = devicePixelRatioF();
        ctx.withInteractionChrome = true;
        el->paint(ctx);
        p.restore();
    }

    p.restore();
    paintScrollbar(&p);
    evictDistantImages();
}

} // namespace Ui
