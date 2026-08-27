#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../basic/ripple_animation.hpp"

#include <QtGui/QAction>
#include <vector>
#include <memory>

namespace Ui {

class PopupMenu : public RpWidget {
    Q_OBJECT

public:
    struct Item {
        QAction* action = nullptr;
        QString iconName;
        QString text;
        bool isSeparator = false;
        QRect rect;
        std::unique_ptr<RippleAnimation> ripple;
    };

    explicit PopupMenu(QWidget* parent = nullptr);
    ~PopupMenu() override;

    QAction* addAction(const QString& text, std::function<void()> handler = nullptr);
    QAction* addAction(const QString& iconName, const QString& text, std::function<void()> handler = nullptr);
    void addSeparator();

    void popup(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void updateGeometryAndMask();
    int itemUnderPoint(const QPoint& pos) const;

    std::vector<Item> items_;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;

    double openProgress_ = 0.0;
    Ui::Animations::Simple openAnimation_;
    static constexpr int kShadowMargin = 12;
    static constexpr int kCornerRadius = 8;
    static constexpr int kItemHeight = 36;
};

} // namespace Ui
