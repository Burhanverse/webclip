#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../md3/md3_button.hpp"
#include "../md3/md3_icon_button.hpp"
#include "../md3/md3_card.hpp"
#include "../md3/md3_text_field.hpp"
#include "color_picker_dialog.hpp"

#include <QtWidgets/QScrollArea>

namespace webclip {
class WebClipController;
}

namespace Ui {

class SettingsDialog : public RpWidget {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr, webclip::WebClipController* controller = nullptr);
    ~SettingsDialog() override;

    void setController(webclip::WebClipController* controller);
    void open();
    void hideAnimated();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void setupContent();
    void updateThemeSelection();
    void updateLayout();

    webclip::WebClipController* controller_ = nullptr;
    double progress_ = 0.0;
    Ui::Animations::Simple anim_;

    QWidget* sheet_ = nullptr;
    QWidget* headerBar_ = nullptr;
    Md3IconButton* closeBtn_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* scrollContent_ = nullptr;

    Md3TextField* hostInput_ = nullptr;
    Md3TextField* pinInput_ = nullptr;
    CardToggleRow* autoConnectRow_ = nullptr;

    Md3Button* themeAutoBtn_ = nullptr;
    Md3Button* themeLightBtn_ = nullptr;
    Md3Button* themeDarkBtn_ = nullptr;
    Md3Button* themePitchBtn_ = nullptr;

    ColorPickerDialog* colorPicker_ = nullptr;
};

} // namespace Ui
