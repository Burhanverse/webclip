#pragma once

#include <QtGui/QPainter>

namespace Ui {

class PainterHighQualityEnabler final {
public:
    explicit PainterHighQualityEnabler(QPainter& p)
        : painter_(p) {
        static constexpr QPainter::RenderHint kHints[] = {
            QPainter::Antialiasing,
            QPainter::SmoothPixmapTransform,
            QPainter::TextAntialiasing
        };

        const auto hints = painter_.renderHints();
        for (const auto hint : kHints) {
            if (!(hints & hint)) {
                addedHints_ |= hint;
            }
        }
        if (addedHints_) {
            painter_.setRenderHints(addedHints_, true);
        }
    }

    PainterHighQualityEnabler(const PainterHighQualityEnabler&) = delete;
    PainterHighQualityEnabler& operator=(const PainterHighQualityEnabler&) = delete;

    ~PainterHighQualityEnabler() {
        if (addedHints_ && painter_.isActive()) {
            painter_.setRenderHints(addedHints_, false);
        }
    }

private:
    QPainter& painter_;
    QPainter::RenderHints addedHints_;
};

class ScopedPainterOpacity final {
public:
    ScopedPainterOpacity(QPainter& p, double newOpacity)
        : painter_(p)
        , oldOpacity_(p.opacity()) {
        if (oldOpacity_ != newOpacity) {
            painter_.setOpacity(newOpacity);
        }
    }

    ScopedPainterOpacity(const ScopedPainterOpacity&) = delete;
    ScopedPainterOpacity& operator=(const ScopedPainterOpacity&) = delete;

    ~ScopedPainterOpacity() {
        if (painter_.isActive() && painter_.opacity() != oldOpacity_) {
            painter_.setOpacity(oldOpacity_);
        }
    }

private:
    QPainter& painter_;
    double oldOpacity_ = 1.0;
};

} // namespace Ui
