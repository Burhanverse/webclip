#pragma once

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
    [[nodiscard]] Md3IconButton* minimizeButton() const noexcept { return minimizeBtn_; }

signals:
    void openSettingsRequested();
    void minimizeRequested();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void updateButtons();
    void updateLayout();

    webclip::WebClipController* controller_ = nullptr;
    Md3IconButton* themeBtn_ = nullptr;
    Md3IconButton* settingsBtn_ = nullptr;
    Md3IconButton* minimizeBtn_ = nullptr;

    Ui::Animations::Simple pulseAnim_;
    double pulseOpacity_ = 1.0;
};

}
