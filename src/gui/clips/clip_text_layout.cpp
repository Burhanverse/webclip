#include "clip_text_layout.hpp"

#include <QPainter>
#include <QRegularExpression>
#include <QtGlobal>

namespace webclip {

namespace {

const QRegularExpression& urlRegex() {
    static const QRegularExpression re(
        QStringLiteral(
            "(https?:\\/\\/[^\\s<]+|www\\.[^\\s<]+|[a-zA-Z0-9.-]+\\."
            "(?:com|org|net|io|dev|app|edu|gov|eu|co|in|me|info|xyz|tech"
            "|online|ai|gg)(?:\\/[^\\s<]*)?)"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

QString normalizeUrl(QString url) {
    if (!url.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) &&
        !url.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        url.prepend(QLatin1String("https://"));
    }
    return url;
}

}  // namespace

QVector<LinkRange> ClipTextLayout::detectLinks(const QString& text) {
    QVector<LinkRange> result;
    auto it = urlRegex().globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        LinkRange range;
        range.start = match.capturedStart();
        range.length = match.capturedLength();
        range.url = normalizeUrl(match.captured());
        result.append(range);
    }
    return result;
}

void ClipTextLayout::setText(const QString& rawText, const QFont& baseFont,
                             const QColor& textColor, const QColor& linkColor) {
    if (text_ == rawText && font_ == baseFont && textColor_ == textColor &&
        linkColor_ == linkColor) {
        return;
    }

    if (text_ == rawText && font_ == baseFont) {
        textColor_ = textColor;
        if (linkColor_ != linkColor) {
            linkColor_ = linkColor;
            for (const auto& par : paragraphs_) {
                applyFormats(*par->layout, par->globalOffset);
            }
            if (laidOutWidth_ > 0) {
                int oldWidth = laidOutWidth_;
                laidOutWidth_ = -1;
                layout(oldWidth);
            }
        }
        return;
    }

    text_ = rawText;
    font_ = baseFont;
    textColor_ = textColor;
    linkColor_ = linkColor;
    links_ = detectLinks(rawText);
    laidOutWidth_ = -1;
    naturalWidth_ = -1.0;
    totalHeight_ = 0;
    rebuildParagraphs();
}

void ClipTextLayout::rebuildParagraphs() {
    paragraphs_.clear();
    if (text_.isEmpty()) return;

    int offset = 0;
    const QStringList parts = text_.split(QLatin1Char('\n'));
    paragraphs_.reserve(parts.size());
    for (int i = 0; i < parts.size(); ++i) {
        auto par = std::make_unique<Paragraph>();
        par->globalOffset = offset;
        par->trailingNewline = (i + 1 < parts.size()) ? 1 : 0;

        QTextLayout* lay = par->layout.get();
        lay->setFont(font_);
        lay->setText(parts.at(i));

        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lay->setTextOption(option);

        applyFormats(*lay, par->globalOffset);

        offset += parts.at(i).length() + 1;  // + '\n'
        paragraphs_.push_back(std::move(par));
    }
}

void ClipTextLayout::applyFormats(QTextLayout& lay, int globalOffset) const {
    if (links_.isEmpty()) return;

    const int paragraphLength = lay.text().length();
    QVector<QTextLayout::FormatRange> formats;
    for (const LinkRange& link : links_) {
        const int start = link.start - globalOffset;
        const int end = start + link.length;
        if (end <= 0 || start >= paragraphLength) continue;
        QTextCharFormat fmt;
        fmt.setForeground(linkColor_);
        fmt.setFontUnderline(true);
        QTextLayout::FormatRange range;
        range.start = qMax(0, start);
        range.length = qMin(end, paragraphLength) - range.start;
        range.format = fmt;
        formats.append(range);
    }
    if (!formats.isEmpty()) lay.setFormats(formats);
}

void ClipTextLayout::layout(int wrapWidth) const {
    if (paragraphs_.empty()) return;
    const bool hasLines = paragraphs_.front()->layout->lineCount() > 0;
    if (laidOutWidth_ == wrapWidth && naturalWidth_ >= 0 && hasLines) return;

    QFontMetricsF fm(font_);
    qreal maxWidth = 0.0;
    totalHeight_ = 0;

    for (const std::unique_ptr<Paragraph>& parPtr : paragraphs_) {
        Paragraph& par = *parPtr;
        QTextLayout& lay = *par.layout;

        lay.beginLayout();
        qreal parNatural = 0.0;
        forever {
            QTextLine line = lay.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(qreal(INT_MAX));
            parNatural = qMax(parNatural, line.naturalTextWidth());
        }
        lay.endLayout();
        maxWidth = qMax(maxWidth, parNatural);

        lay.clearLayout();
        lay.beginLayout();
        qreal y = 0.0;
        forever {
            QTextLine line = lay.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(qMax(1, wrapWidth));
            line.setPosition(QPointF(0, y));
            y += line.height();
        }
        lay.endLayout();

        totalHeight_ += qCeil(y) + par.trailingNewline * qCeil(fm.height());
    }

    laidOutWidth_ = wrapWidth;
    naturalWidth_ = maxWidth;
}

qreal ClipTextLayout::naturalWidth() const {
    if (naturalWidth_ < 0) layout(INT_MAX);
    return naturalWidth_;
}

int ClipTextLayout::heightAt(int width) const {
    layout(width);
    return totalHeight_;
}

int ClipTextLayout::lineCount() const {
    int count = 0;
    for (const auto& par : paragraphs_) count += par->layout->lineCount();
    return count;
}

qreal ClipTextLayout::paragraphAdvance(const Paragraph& par) const {
    return par.layout->boundingRect().height() +
           par.trailingNewline * QFontMetricsF(font_).height();
}

void ClipTextLayout::draw(const DrawArgs& args) const {
    QPainter* p = args.painter;
    if (!p || paragraphs_.empty()) return;

    if (laidOutWidth_ <= 0) {
        int wrap = args.clip.isValid() && args.clip.width() > 0
                       ? qCeil(args.clip.width())
                       : qCeil(naturalWidth());
        layout(wrap > 0 ? wrap : 400);
    }

    QPointF origin = args.topLeft;
    for (const auto& parPtr : paragraphs_) {
        const Paragraph& par = *parPtr;
        if (par.layout->lineCount() == 0 && laidOutWidth_ > 0) {
            layout(laidOutWidth_);
        }
        const qreal advance = paragraphAdvance(par);
        const QRectF bound(origin, QSizeF(laidOutWidth_, advance));
        const bool culled =
            args.clip.isValid() && !args.clip.intersects(bound);

        if (!culled) {
            if (args.selectionStart >= 0 && args.selectionEnd > args.selectionStart) {
                const int selStart = args.selectionStart - par.globalOffset;
                const int selEnd = args.selectionEnd - par.globalOffset;
                if (selEnd > 0 && selStart < par.layout->text().length()) {
                    p->save();
                    p->setPen(Qt::NoPen);
                    p->setBrush(args.selectionColor);
                    for (int i = 0; i < par.layout->lineCount(); ++i) {
                        const QTextLine line = par.layout->lineAt(i);
                        const int lineStart = line.textStart();
                        const int lineEnd = lineStart + line.textLength();
                        const int a = qMax(selStart, lineStart);
                        const int b = qMin(selEnd, lineEnd);
                        if (b <= a) continue;
                        const qreal x1 = line.cursorToX(a);
                        const qreal x2 = line.cursorToX(b);
                        p->drawRect(QRectF(origin.x() + x1, origin.y() + line.y(),
                                           x2 - x1, line.height()));
                    }
                    p->restore();
                }
            }
            par.layout->draw(p, origin);
        }

        origin.ry() += advance;
    }
}

int ClipTextLayout::positionAt(const QPointF& pos, int wrapWidth) const {
    layout(wrapWidth);
    qreal y = 0.0;
    for (const auto& parPtr : paragraphs_) {
        const Paragraph& par = *parPtr;
        const qreal advance = paragraphAdvance(par);
        const bool lastPar = parPtr.get() == paragraphs_.back().get();
        if (pos.y() < y + advance || lastPar) {
            QPointF local(pos.x(), pos.y() - y);
            const QTextLayout& lay = *par.layout;

            QTextLine hitLine;
            for (int i = 0; i < lay.lineCount(); ++i) {
                const QTextLine line = lay.lineAt(i);
                if (local.y() >= line.y() && local.y() <= line.y() + line.height()) {
                    hitLine = line;
                    break;
                }
            }
            if (!hitLine.isValid()) {
                hitLine = lay.lineCount() > 0
                              ? lay.lineAt(lay.lineCount() - 1)
                              : QTextLine();
            }
            if (!hitLine.isValid()) return par.globalOffset;

            const qreal x = qBound<qreal>(0.0, local.x(), hitLine.width());
            const int cursorPos =
                hitLine.xToCursor(x, QTextLine::CursorOnCharacter);
            if (cursorPos < 0 || hitLine.textLength() == 0) {
                return par.globalOffset;
            }
            return par.globalOffset +
                   qBound(hitLine.textStart(), cursorPos,
                          hitLine.textStart() + hitLine.textLength());
        }
        y += advance;
    }
    return 0;
}

QString ClipTextLayout::urlAt(const QPointF& pos, int wrapWidth) const {
    const int charPos = positionAt(pos, wrapWidth);
    for (const LinkRange& link : links_) {
        if (charPos >= link.start && charPos < link.start + link.length) {
            return link.url;
        }
    }
    return QString();
}

}  // namespace webclip
