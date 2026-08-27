#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../md3/md3_slider.hpp"
#include "../md3/md3_button.hpp"
#include "../md3/md3_text_field.hpp"

namespace Ui {

class ColorPickerDialog : public RpWidget {
    Q_OBJECT

public:
    explicit ColorPickerDialog(QWidget* parent = nullptr);
    ~ColorPickerDialog() override;

    void openWithColor(const QColor& initialColor);
    void hideAnimated();

signals:
    void colorSelected(const QColor& color);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void updateFromHsl();
    void updateFromHex(const QString& hex);
    void updateLayout();

    QColor currentColor_ = QColor(QStringLiteral("#FF416D"));
    double hue_ = 346.0;
    double sat_ = 100.0;
    double light_ = 63.0;
    bool updating_ = false;

    double progress_ = 0.0;
    Ui::Animations::Simple anim_;

    QWidget* card_ = nullptr;
    Md3TextField* hexInput_ = nullptr;
    Md3Slider* hueSlider_ = nullptr;
    Md3Slider* satSlider_ = nullptr;
    Md3Slider* lightSlider_ = nullptr;
    Md3Button* cancelBtn_ = nullptr;
    Md3Button* selectBtn_ = nullptr;
};

} // namespace Ui
