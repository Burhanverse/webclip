#pragma once

#include "../basic/ripple_button.hpp"

namespace Ui {

class Md3IconButton : public RippleButton {
    Q_OBJECT

public:
    explicit Md3IconButton(
        QWidget* parent = nullptr,
        const QString& iconName = QString(),
        int buttonSize = 40,
        int iconSize = 20
    );
    ~Md3IconButton() override;

    [[nodiscard]] QString iconName() const noexcept {
        return iconName_;
    }
    void setIconName(const QString& iconName);

    [[nodiscard]] QColor iconColor() const noexcept {
        return iconColor_;
    }
    void setIconColor(const QColor& color);

    [[nodiscard]] QColor customBgColor() const noexcept {
        return customBgColor_;
    }
    void setCustomBgColor(const QColor& color);

    [[nodiscard]] bool roundSquare() const noexcept { return roundSquare_; }
    void setRoundSquare(bool on) {
        if (roundSquare_ != on) {
            roundSquare_ = on;
            update();
        }
    }

    void setButtonSize(int buttonSize);
    void setIconSize(int iconSize);

    [[nodiscard]] QSize sizeHint() const override {
        return QSize(buttonSize_, buttonSize_);
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        return sizeHint();
    }

protected:
    void paintEvent(QPaintEvent* e) override;
    QImage prepareRippleMask() const override;
    QPoint prepareRippleStartPosition() const override;

private:
    QString iconName_;
    QColor iconColor_;
    QColor customBgColor_ = Qt::transparent;
    int buttonSize_ = 40;
    int iconSize_ = 20;
    bool roundSquare_ = false;

    QColor effectiveIconColor() const;
};

} // namespace Ui
