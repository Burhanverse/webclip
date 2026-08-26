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

class ClipListItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* controller READ controller WRITE setController NOTIFY
                   controllerChanged)

public:
    explicit ClipListItem(QQuickItem* parent = nullptr);
    ~ClipListItem() override;

    QObject* controller() const { return controller_.data(); }
    void setController(QObject* controller);

    ClipboardHistoryModel* model() const { return model_; }

    int currentScrollY() const { return scrollY_; }

    Q_INVOKABLE QRectF bubbleSceneRect(const QString& clipId) const;

signals:
    void controllerChanged();
    void fullPreviewRequested(const QString& imageDataUrl);
    void saveImageRequested(int index);

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

    void layoutAll();
    void layoutElement(int index);
    void repositionFrom(int index);
    int contentHeight() const;
    int maxScroll() const;

    int findElementIndexByY(int y) const;
    ClipElement* elementAt(int y) const;
    QRectF elementSceneRect(const ClipElement* el) const;
    void repaintElement(const ClipElement* el);
    void applyScroll(int newY, bool clampHard = true);
    void scrollToBottom(bool instant = true);
    void startWheelAnimation();
    void startFlick(qreal velocityPxPerSec);
    void startSettleBack();
    void stopAnimations();
    void tickAnimations();

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
    ClipboardHistoryModel* model_ = nullptr;

    std::vector<std::unique_ptr<ClipElement>> elements_;
    Anchor pendingAnchor_;

    int scrollY_ = 0;
    int viewportHeightCached_ = 0;
    bool stickBottom_ = true;

    constexpr static int kSpacing = 10;
    constexpr static int kTopMargin = 12;
    constexpr static int kBottomMargin = 12;

    QTimer animationTimer_;
    enum class Anim { None, Wheel, Flick, Settle };
    Anim animState_ = Anim::None;
    int animTarget_ = 0;
    qreal flickVelocity_ = 0.0;
    QElapsedTimer animClock_;

    Gesture gesture_ = Gesture::None;
    QPointF pressPos_;
    int pressScrollY_ = 0;
    int pressedElementIndex_ = -1;
    ClipElement::Zone pressedZone_ = ClipElement::Zone::None;
    QString pressedLink_;
    std::deque<std::pair<qint64, qreal>> dragSamples_;

    enum class ScrollbarGesture { None, Dragging };
    ScrollbarGesture scrollbarGesture_ = ScrollbarGesture::None;
    qreal scrollbarGrabOffset_ = 0;
    qreal scrollbarOpacity_ = 0.0;
    qreal scrollbarTargetOpacity_ = 0.0;
    qint64 lastScrollbarInteractionMs_ = 0;
    int scrollbarAnimTimerId_ = 0;

    int evictionCounter_ = 0;

    int hoveredElementIndex_ = -1;
    ClipElement::Zone hoveredZone_ = ClipElement::Zone::None;

    QMetaObject::Connection windowScreenConn_;
    QMetaObject::Connection screenDprConn_;
    void followWindowDpr(QQuickWindow* window);

    friend class ClipElement;
    void onElementImageDecoded(const QString& clipId, const QString& sourceKey,
                               const QImage& image);
};

}  // namespace webclip
