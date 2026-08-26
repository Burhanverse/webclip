#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QTextLayout>
#include <QVector>
#include <memory>
#include <vector>

namespace webclip {

struct LinkRange {
    int start = 0;
    int length = 0;
    QString url;
};

class ClipTextLayout {
public:
    ClipTextLayout() = default;
    ClipTextLayout(ClipTextLayout&&) = default;
    ClipTextLayout& operator=(ClipTextLayout&&) = default;

    void setText(const QString& rawText,
                 const QFont& baseFont,
                 const QColor& textColor,
                 const QColor& linkColor);

    const QString& text() const { return text_; }
    bool isEmpty() const { return paragraphs_.empty(); }

    void layout(int wrapWidth) const;

    qreal naturalWidth() const;
    int heightAt(int width) const;
    int lineCount() const;

    struct DrawArgs {
        QPainter* painter = nullptr;
        QPointF topLeft;
        QRectF clip;
        QColor selectionColor;
        int selectionStart = -1;
        int selectionEnd = -1;
    };

    void draw(const DrawArgs& args) const;
    int positionAt(const QPointF& pos, int wrapWidth) const;
    QString urlAt(const QPointF& pos, int wrapWidth) const;

    static QVector<LinkRange> detectLinks(const QString& text);
    const QVector<LinkRange>& links() const { return links_; }

private:
    struct Paragraph {
        std::unique_ptr<QTextLayout> layout =
            std::make_unique<QTextLayout>();
        int globalOffset = 0;
        int trailingNewline = 0;
    };

    void rebuildParagraphs();
    void applyFormats(QTextLayout& lay, int globalOffset) const;
    qreal paragraphAdvance(const Paragraph& par) const;

    QString text_;
    QFont font_;
    QColor textColor_;
    QColor linkColor_;
    QVector<LinkRange> links_;

    mutable std::vector<std::unique_ptr<Paragraph>> paragraphs_;
    mutable int laidOutWidth_ = -1;
    mutable qreal naturalWidth_ = -1.0;
    mutable int totalHeight_ = 0;

    Q_DISABLE_COPY(ClipTextLayout)
};

}  // namespace webclip
