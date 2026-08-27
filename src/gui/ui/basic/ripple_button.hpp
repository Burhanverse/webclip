#pragma once

#include "rp_widget.hpp"
#include "ripple_animation.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace Ui {

class RippleButton : public RpWidget {
public:
    explicit RippleButton(QWidget* parent = nullptr, RippleConfig config = {});
    ~RippleButton() override;

    void addClickHandler(std::function<void()> handler) {
        clickHandlers_.push_back(std::move(handler));
    }

    void setRippleConfig(const RippleConfig& config);
    [[nodiscard]] const RippleConfig& rippleConfig() const noexcept {
        return config_;
    }

    [[nodiscard]] bool isOver() const noexcept {
        return isOver_;
    }
    [[nodiscard]] bool isDown() const noexcept {
        return isDown_;
    }
    [[nodiscard]] bool isDisabled() const noexcept {
        return !isEnabled();
    }

    void paintRipple(
        QPainter& p,
        int x = 0,
        int y = 0,
        const QColor* colorOverride = nullptr
    );

protected:
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void changeEvent(QEvent* e) override;

    virtual QImage prepareRippleMask() const;
    virtual QPoint prepareRippleStartPosition() const;

private:
    void ensureRipple();

    RippleConfig config_;
    std::unique_ptr<RippleAnimation> ripple_;
    bool isOver_ = false;
    bool isDown_ = false;
    std::vector<std::function<void()>> clickHandlers_;
};

} // namespace Ui
