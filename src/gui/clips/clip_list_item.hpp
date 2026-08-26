#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>
#include <deque>
#include <memory>
#include <vector>

#include "clip_element.hpp"

namespace webclip {

class ClipboardHistoryModel;
class WebClipController;

// Port of Telegram Desktop's history ListWidget applied to clipboard clips:
// one custom-painted surface owns every element, keeps them sorted by cached
// _y, paints only the items intersecting the damaged region (binary search),
// and implements its own scroll physics, hit testing and overlay scrollbar.
// There are no per-message scene graph objects at all.
class ClipListItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* controller READ controller WRITE setController NOTIFY
                   controllerChanged)
    Q_PROPERTY(QQuickItem* thanosTarget READ thanosTarget WRITE setThanosTarget
                   NOTIFY thanosTargetChanged)

public:
    explicit ClipListItem(QQuickItem* parent = nullptr);
    ~ClipListItem() override;

    QObject* controller() const { return controller_.data(); }
    void setController(QObject* controller);

    QQuickItem* thanosTarget() const { return thanosTarget_.data(); }
    void setThanosTarget(QQuickItem* target);

    ClipboardHistoryModel* model() const { return model_; }

    // Current scroll offset in content coordinates.
    int currentScrollY() const { return scrollY_; }

    // Bubble rectangle (item coords) for a given clip id; useful for
    // tooling/accessibility. Returns an invalid rect if not found.
    Q_INVOKABLE QRectF bubbleSceneRect(const QString& clipId) const;

signals:
    void controllerChanged();
    void thanosTargetChanged();
    void fullPreviewRequested(const QString& imageDataUrl);
    void saveImageRequested(int index);
    void snapRequested(const QImage& image, const QRectF& rect,
                       const QString& clipId);

protected:
    void paint(QPainter* painter) override;
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;
    void timerEvent(QTimerEvent* event) override;

    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

    // --- Model synchronization (refreshRows/enforceViewForItem port) ---
    struct Anchor {
        int index = -1;
        int offset = 0;
        bool valid = false;
    };

    void attachModel();
    void rebuildElements();
    Anchor captureAnchor() const;
    void restoreAnchor(const Anchor& anchor);
    void onRowsInserted(const QModelIndex& parent, int first, int last);
    void onRowsRemoved(const QModelIndex& parent, int first, int last);
    void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                       const QVector<int>& roles);
    void fixRowsFrom(int index);

    // --- Layout (ListWidget::resizeGetHeight / viewHeightAdjusted port) ---
    void layoutAll();
    void layoutElement(int index);
    void repositionFrom(int index);
    int contentHeight() const;
    int maxScroll() const;

    // --- Visible-range bookkeeping (_visibleTop/_visibleBottom port) ---
    int findElementIndexByY(int y) const;
    ClipElement* elementAt(int y) const;
    QRectF elementSceneRect(const ClipElement* el) const;
    void repaintElement(const ClipElement* el);

    // --- Scrolling ---
    void applyScroll(int newY, bool clampHard = true);
    void scrollToBottom(bool instant = true);
    void startWheelAnimation();
    void startFlick(qreal velocityPxPerSec);
    void startSettleBack();
    void stopAnimations();
    void tickAnimations();

    // --- Interaction (mouseActionUpdate port) ---
    enum class Gesture { None, Pressing, DraggingScroll, SelectingText };
    struct HitContext {
        int elementIndex = -1;
        ClipElement::Hit hit;
    };
    HitContext hitContextAt(QPointF itemPos) const;
    void updateHover(QPointF itemPos);
    void activateZone(const HitContext& at);
    void handleDelete(ClipElement* el);
    void selectWordAt(ClipElement* el, int position);
    QString firstLinkOf(ClipElement* el) const;
    void releasePressVisuals();
    void evictDistantImages();

    void paintScrollbar(QPainter* p);
    bool scrollbarContains(QPointF pos) const;
    QRectF scrollbarThumbRect() const;
    void restartScrollbarFade();

    QPointer<QObject> controller_;
    QPointer<QQuickItem> thanosTarget_;
    ClipboardHistoryModel* model_ = nullptr;

    std::vector<std::unique_ptr<ClipElement>> elements_;
    Anchor pendingAnchor_;

    int scrollY_ = 0;
    int viewportHeightCached_ = 0;
    bool stickBottom_ = true;

    constexpr static int kSpacing = 10;
    constexpr static int kTopMargin = 12;
    constexpr static int kBottomMargin = 12;

    // Scroll animation state (wheel easing / flick inertia / rubber band).
    QTimer animationTimer_;
    enum class Anim { None, Wheel, Flick, Settle };
    Anim animState_ = Anim::None;
    int animTarget_ = 0;
    qreal flickVelocity_ = 0.0;  // px/sec
    QElapsedTimer animClock_;

    // Gesture state.
    Gesture gesture_ = Gesture::None;
    QPointF pressPos_;
    int pressScrollY_ = 0;
    int pressedElementIndex_ = -1;
    ClipElement::Zone pressedZone_ = ClipElement::Zone::None;
    QString pressedLink_;
    std::deque<std::pair<qint64, qreal>> dragSamples_;

    // Scrollbar state.
    enum class ScrollbarGesture { None, Dragging };
    ScrollbarGesture scrollbarGesture_ = ScrollbarGesture::None;
    qreal scrollbarGrabOffset_ = 0;
    qreal scrollbarOpacity_ = 0.0;  // 0..1 animated toward target
    qreal scrollbarTargetOpacity_ = 0.0;
    qint64 lastScrollbarInteractionMs_ = 0;
    int scrollbarAnimTimerId_ = 0;

    // Distant-image eviction throttle.
    int evictionCounter_ = 0;

    // Hover state.
    int hoveredElementIndex_ = -1;
    ClipElement::Zone hoveredZone_ = ClipElement::Zone::None;

    friend class ClipElement;
    void onElementImageDecoded(const QString& clipId, const QString& sourceKey,
                               const QImage& image);
};

}  // namespace webclip
