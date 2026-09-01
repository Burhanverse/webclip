#pragma once

#include "../basic/animation.hpp"
#include "../basic/ripple_button.hpp"
#include "../basic/rpl_lite.hpp"
#include "../../util/display_scale.hpp"

class QPainter;

namespace Ui {

void PaintMd3Switch(
    QPainter& p,
    double x,
    double y,
    double toggled,
    double switchWidth = 52.0,
    double switchHeight = 32.0,
    bool enabled = true
);

class Md3Switch : public RippleButton {
    Q_OBJECT

public:
    explicit Md3Switch(QWidget* parent = nullptr, bool checked = false);
    ~Md3Switch() override;

    [[nodiscard]] bool checked() const noexcept {
        return checked_;
    }
    void setChecked(bool checked, anim::type animated = anim::type::normal);

    [[nodiscard]] rpl::producer<bool> checkedChanges() const {
        return checkedChanges_.events();
    }
    [[nodiscard]] rpl::producer<bool> checkedValue() const {
        return checkedChanges_.events_starting_with_copy(checked_);
    }

    [[nodiscard]] QSize sizeHint() const override {
        return QSize(webclip::scale::px(52), webclip::scale::px(32));
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        return sizeHint();
    }

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* e) override;
    QImage prepareRippleMask() const override;
    QPoint prepareRippleStartPosition() const override;

private:
    bool checked_ = false;
    Ui::Animations::Simple animation_;
    rpl::event_stream<bool> checkedChanges_;
};

} // namespace Ui
