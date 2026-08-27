#pragma once

#include "../basic/rp_widget.hpp"

namespace Ui {

class Md3Badge : public RpWidget {
    Q_OBJECT

public:
    explicit Md3Badge(
        QWidget* parent = nullptr,
        const QString& text = QString(),
        const QString& iconName = QString()
    );
    ~Md3Badge() override;

    [[nodiscard]] QString text() const noexcept {
        return text_;
    }
    void setText(const QString& text);

    [[nodiscard]] QString iconName() const noexcept {
        return iconName_;
    }
    void setIconName(const QString& iconName);

    [[nodiscard]] QColor badgeColor() const noexcept {
        return badgeColor_;
    }
    void setBadgeColor(const QColor& color);

    [[nodiscard]] QColor textColor() const noexcept {
        return textColor_;
    }
    void setTextColor(const QColor& color);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override {
        return sizeHint();
    }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QString text_;
    QString iconName_;
    QColor badgeColor_;
    QColor textColor_;
};

} // namespace Ui
