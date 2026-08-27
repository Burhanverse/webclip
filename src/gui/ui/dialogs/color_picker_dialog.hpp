#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../md3/md3_button.hpp"

#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <vector>

namespace Ui {

class SatValArea;
class HueBar;
class AlphaBar;
class SwatchesRow;
class FormatPill;

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
    void updateFromHsv();
    void updateFromHex(const QString& hex);
    void updateLayout();

    QColor currentColor_ = QColor(QStringLiteral("#8B5CF6"));
    double hue_ = 258.0;
    double sat_ = 0.63;
    double val_ = 0.96;
    double alpha_ = 1.0;
    bool updating_ = false;

    double progress_ = 0.0;
    Ui::Animations::Simple anim_;

    QWidget* card_ = nullptr;
    SatValArea* satValArea_ = nullptr;
    HueBar* hueBar_ = nullptr;
    AlphaBar* alphaBar_ = nullptr;

    FormatPill* formatPill_ = nullptr;

    QWidget* hexBox_ = nullptr;
    QLineEdit* hexInput_ = nullptr;
    QLabel* opacityLabel_ = nullptr;

    QWidget* savedHeader_ = nullptr;
    QLabel* savedTitle_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    SwatchesRow* swatchesRow_ = nullptr;

    Md3Button* cancelBtn_ = nullptr;
    Md3Button* selectBtn_ = nullptr;

    std::vector<QColor> savedColors_;
};

} // namespace Ui
