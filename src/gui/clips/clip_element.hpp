#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <utility>
#include <vector>

#include "clip_text_layout.hpp"

class QPainter;

namespace webclip {

class ClipListItem;

void clearClipElementCaches();

class ClipElement {
public:
    enum class Zone {
        None,
        Text,
        Image,
        BtnLink,
        BtnCopy,
        BtnDownload,
        BtnSend,
        BtnDelete,
        ExpandChip,
    };

    explicit ClipElement(ClipListItem* owner);

    int y() const { return y_; }
    void setY(int y) { y_ = y; }
    int height() const { return height_; }
    bool pendingResize() const { return pendingResize_; }
    void setPendingResize(bool on) { pendingResize_ = on; }

    int row() const { return row_; }
    void setRow(int row) { row_ = row; }

    int resizeGetHeight(int containerWidth);

    void refreshContent();
    QString clipId() const { return id_; }
    bool isImage() const { return isImage_; }

    void setExpanded(bool on);
    bool expanded() const { return expanded_; }

    QRectF bubbleRect() const { return bubbleRect_; }

    struct PaintContext {
        QPainter* painter = nullptr;
        QRectF clipItemCoords;  // damaged region in list coordinates
        qreal dpr = 1.0;
        bool withInteractionChrome = true;
    };
    void paint(const PaintContext& context) const;

    QImage snapshotBubble(qreal dpr) const;

    struct Hit {
        Zone zone = Zone::None;
        QString url;
        int textPosition = -1;
    };
    Hit hitTest(QPointF itemPos, int containerWidth) const;

    Zone hoverZone() const { return hoverZone_; }
    bool setHover(Zone zone);
    bool pressed() const { return pressed_; }
    void setPressed(bool on);

    void setSelection(int anchor, int cursor);
    void clearSelection();
    bool hasSelection() const;
    QString selectedText(int containerWidth) const;
    std::pair<int, int> selectionRange() const { return {selAnchor_, selCursor_}; }
    const QString& textForSelection() const { return text_.text(); }

    void ensureImageLoaded();
    bool imageLoading() const { return imageState_ == ImageLoading; }
    QString imageSourceKey() const { return imageStateKey_; }
    bool setImageResult(const QString& sourceKey, const QImage& image);

    void releaseImage();

private:
    struct Metrics {
        qreal maxBubbleWidth = 0;
        qreal minBubbleWidth = 100;
        qreal totalHPad = 0;
        qreal bubbleWidth = 0;
        int textWrapWidth = 0;
        QSizeF imageDisplaySize{0, 0};
    };

    bool isLongText() const;
    Metrics computeMetrics(int containerWidth) const;
    void rebuildText();
    void updateButtonsVisibility();
    void updateButtonRects(qreal contentX, qreal contentW, qreal metaTop);
    qreal actionsPillWidth() const;

    ClipListItem* owner_ = nullptr;
    int row_ = 0;
    int y_ = 0;
    int height_ = 0;
    bool pendingResize_ = true;
    int laidOutWidth_ = -1;
    int lastWrapWidth_ = 0;

    QString id_;
    bool isImage_ = false;
    bool fromPhone_ = false;
    bool connectedFlag_ = false;
    QString headText_;
    QString fullTextCache_;
    bool fullTextLoaded_ = false;
    bool expanded_ = false;
    QString timeString_;
    qint64 charCount_ = 0;
    QString imageDataUrl_;
    QSize nativeDims_{0, 0};
    bool hasUrl_ = false;
    QString firstUrl_;

    ClipTextLayout text_;
    QFont bodyFont_;
    QFont timeFont_;
    QColor textColor_;
    QColor linkColor_;
    QColor bubbleColor_;

    QRectF bubbleRect_;
    QRectF textAreaRect_;
    QRectF imageAreaRect_;
    QRectF expandChipRect_;
    QRectF actionsPillRect_;
    bool showExpandChip_ = false;
    bool collapsedLong_ = false;

    struct ActionButton {
        Zone zone = Zone::None;
        QRectF rect;
        bool visible = false;
    };
    std::vector<ActionButton> buttons_;

    enum ImageState { ImageEmpty, ImageLoading, ImageReady, ImageFailed };
    QString imageStateKey_;
    ImageState imageState_ = ImageEmpty;
    QImage imageScaled_;

    Zone hoverZone_ = Zone::None;
    bool pressed_ = false;
    int selAnchor_ = -1;
    int selCursor_ = -1;
};

}  // namespace webclip
