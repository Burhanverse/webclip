#pragma once

#include "../basic/ripple_button.hpp"
#include <QtGui/QIcon>

namespace Ui {

enum class ButtonVariant {
    Filled,
    Tonal,
    Outlined,
    Text,
};

class Md3Button : public RippleButton {
    Q_OBJECT

public:
    explicit Md3Button(
        QWidget* parent = nullptr,
        const QString& text = QString(),
        ButtonVariant variant = ButtonVariant::Filled
    );
    ~Md3Button() override;

    [[nodiscard]] QString text() const noexcept {
        return text_;
    }
    void setText(const QString& text);

    [[nodiscard]] QString iconName() const noexcept {
        return iconName_;
    }
    void setIconName(const QString& iconName);

    [[nodiscard]] ButtonVariant variant() const noexcept {
        return variant_;
    }
    void setVariant(ButtonVariant variant);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override {
        return sizeHint();
    }

protected:
    void paintEvent(QPaintEvent* e) override;
    QImage prepareRippleMask() const override;
    QPoint prepareRippleStartPosition() const override;

private:
    QString text_;
    QString iconName_;
    ButtonVariant variant_ = ButtonVariant::Filled;

    QColor contentColor() const;
    QColor buttonBgColor() const;
};

} // namespace Ui
