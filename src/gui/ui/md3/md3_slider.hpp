#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"

namespace Ui {

class Md3Slider : public RpWidget {
    Q_OBJECT

public:
    explicit Md3Slider(QWidget* parent = nullptr);
    ~Md3Slider() override;

    [[nodiscard]] double value() const noexcept {
        return value_;
    }
    void setValue(double val);

    [[nodiscard]] double minimum() const noexcept {
        return min_;
    }
    [[nodiscard]] double maximum() const noexcept {
        return max_;
    }
    void setRange(double min, double max);

    void setSteps(int steps); // 0 = continuous, >0 = discrete notches

    [[nodiscard]] rpl::producer<double> valueChanges() const {
        return valueChanges_.events();
    }

    [[nodiscard]] QSize sizeHint() const override {
        return QSize(200, 36);
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        return QSize(100, 36);
    }

signals:
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    void updateValueFromPos(int x);

    double min_ = 0.0;
    double max_ = 100.0;
    double value_ = 50.0;
    int steps_ = 0;

    bool isDown_ = false;
    rpl::event_stream<double> valueChanges_;
};

} // namespace Ui
