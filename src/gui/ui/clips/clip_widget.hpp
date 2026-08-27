#pragma once

#include "../basic/rp_widget.hpp"
#include "../../clips/clip_element.hpp"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <deque>
#include <memory>
#include <vector>

namespace webclip {
class ClipboardHistoryModel;
class WebClipController;
}

namespace Ui {

class ClipWidget : public RpWidget, public webclip::IClipViewHost {
    Q_OBJECT

public:
    explicit ClipWidget(
        QWidget* parent = nullptr,
        webclip::WebClipController* controller = nullptr,
        webclip::ClipboardHistoryModel* model = nullptr
    );
    ~ClipWidget() override;

    QObject* asQObject() override { return this; }
    [[nodiscard]] webclip::ClipboardHistoryModel* model() const override { return model_; }
    [[nodiscard]] webclip::WebClipController* controller() const override { return controller_; }

    void setController(webclip::WebClipController* controller);
    void setModel(webclip::ClipboardHistoryModel* model);

    [[nodiscard]] int currentScrollY() const noexcept { return scrollY_; }

    void onElementImageDecoded(
        const QString& clipId,
        const QString& sourceKey,
        const QImage& image
    ) override;

signals:
    void fullPreviewRequested(const QString& imageDataUrl);
    void saveImageRequested(int index);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
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
    void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);
    void fixRowsFrom(int index);

    void layoutAll();
    void layoutElement(int index);
    void repositionFrom(int index);
    int contentHeight() const;
    int maxScroll() const;

    int findElementIndexByY(int y) const;
    webclip::ClipElement* elementAt(int y) const;
    void applyScroll(int newY, bool clampHard = true);
    void scrollToBottom(bool instant = true);
    void startWheelAnimation();
    void startFlick(double velocityPxPerSec);
    void startSettleBack();
    void stopAnimations();
    void tickAnimations();

    enum class Gesture { None, Pressing, DraggingScroll, SelectingText };
    struct HitContext {
        int elementIndex = -1;
        webclip::ClipElement::Hit hit;
    };
    HitContext hitContextAt(const QPointF& itemPos) const;
    void updateHover(const QPointF& itemPos);
    void activateZone(const HitContext& at);
    void handleDelete(webclip::ClipElement* el);
    void selectWordAt(webclip::ClipElement* el, int position);
    QString firstLinkOf(webclip::ClipElement* el) const;
    void releasePressVisuals();
    void evictDistantImages();

    void paintScrollbar(QPainter* p);
    bool scrollbarContains(const QPointF& pos) const;
    QRectF scrollbarThumbRect() const;
    void restartScrollbarFade();

    webclip::WebClipController* controller_ = nullptr;
    webclip::ClipboardHistoryModel* model_ = nullptr;

    std::vector<std::unique_ptr<webclip::ClipElement>> elements_;
    Anchor pendingAnchor_;

    int scrollY_ = 0;
    int viewportHeightCached_ = 0;
    bool stickBottom_ = true;

    static constexpr int kSpacing = 10;
    static constexpr int kTopMargin = 12;
    static constexpr int kBottomMargin = 12;

    QTimer animationTimer_;
    enum class Anim { None, Wheel, Flick, Settle };
    Anim animState_ = Anim::None;
    int animTarget_ = 0;
    double flickVelocity_ = 0.0;
    QElapsedTimer animClock_;

    Gesture gesture_ = Gesture::None;
    QPointF pressPos_;
    int pressScrollY_ = 0;
    int pressedElementIndex_ = -1;
    webclip::ClipElement::Zone pressedZone_ = webclip::ClipElement::Zone::None;
    QString pressedLink_;
    std::deque<std::pair<qint64, double>> dragSamples_;

    enum class ScrollbarGesture { None, Dragging };
    ScrollbarGesture scrollbarGesture_ = ScrollbarGesture::None;
    double scrollbarGrabOffset_ = 0.0;
    double scrollbarOpacity_ = 0.0;
    double scrollbarTargetOpacity_ = 0.0;
    qint64 lastScrollbarInteractionMs_ = 0;
    QTimer scrollbarFadeTimer_;

    int evictionCounter_ = 0;
    int hoveredElementIndex_ = -1;
    webclip::ClipElement::Zone hoveredZone_ = webclip::ClipElement::Zone::None;
};

} // namespace Ui
