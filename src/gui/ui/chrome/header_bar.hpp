#pragma once

#include <QtGui/QTouchEvent>

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../md3/md3_icon_button.hpp"

namespace webclip {
class WebClipController;
}

namespace Ui {

class HeaderBar : public RpWidget {
    Q_OBJECT

public:
    explicit HeaderBar(QWidget* parent = nullptr, webclip::WebClipController* controller = nullptr);
    ~HeaderBar() override;

    void setController(webclip::WebClipController* controller);

    [[nodiscard]] QSize sizeHint() const override {
        return QSize(380, 58);
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        return QSize(320, 58);
    }

    [[nodiscard]] Md3IconButton* themeButton() const noexcept { return themeBtn_; }
    [[nodiscard]] Md3IconButton* settingsButton() const noexcept { return settingsBtn_; }

signals:
    void openSettingsRequested();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void touchEvent(QTouchEvent* e);
    void resizeEvent(QResizeEvent* e) override;

private:
    void updateButtons();
    void updateLayout();

    webclip::WebClipController* controller_ = nullptr;
    Md3IconButton* themeBtn_ = nullptr;
    Md3IconButton* settingsBtn_ = nullptr;

    Ui::Animations::Simple pulseAnim_;
    double pulseOpacity_ = 1.0;

    // Double-tap tracking for touchpads
    QPoint lastTouchPoint_;
    int64_t lastTouchMs_ = 0;
    static constexpr int64_t kDoubleTapMs = 300;
    static constexpr int kDoubleTapDist = 30; // pixels
};

} // namespace Ui
