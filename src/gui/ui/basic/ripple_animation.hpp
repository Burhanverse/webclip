#pragma once

#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <deque>
#include <functional>
#include <memory>

class QPainter;

namespace Ui {

struct RippleConfig {
    int showDuration = 200;
    int hideDuration = 300;
    QColor color;
    bool useNoiseDither = true;
};

class RippleAnimation final {
public:
    RippleAnimation(
        const RippleConfig& config,
        QImage mask,
        std::function<void()> update
    );
    ~RippleAnimation();

    RippleAnimation(const RippleAnimation&) = delete;
    RippleAnimation& operator=(const RippleAnimation&) = delete;

    void add(QPoint origin, int startRadius = 0);
    void addFading();
    void lastStop();
    void lastUnstop();
    void lastFinish();
    void forceRepaint();

    void paint(
        QPainter& p,
        int x,
        int y,
        int outerWidth,
        const QColor* colorOverride = nullptr
    );

    [[nodiscard]] bool empty() const;

    static QImage MaskByDrawer(
        QSize size,
        bool filled,
        std::function<void(QPainter&)> drawer
    );
    static QImage RectMask(QSize size);
    static QImage RoundRectMask(QSize size, int radius);
    static QImage EllipseMask(QSize size);

private:
    void clearFinished();

    const RippleConfig config_;
    QPixmap mask_;
    std::function<void()> update_;

    class Ripple;
    std::deque<std::unique_ptr<Ripple>> ripples_;
};

} // namespace Ui
