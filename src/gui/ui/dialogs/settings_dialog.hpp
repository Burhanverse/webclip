#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../md3/md3_button.hpp"
#include "../md3/md3_icon_button.hpp"
#include "../md3/md3_card.hpp"
#include "../md3/md3_text_field.hpp"
#include "../md3/md3_slider.hpp"
#include "color_picker_dialog.hpp"

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <vector>

namespace webclip {
class WebClipController;
}

namespace Ui {

class AccentPill;

class SettingsDialog : public RpWidget {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr, webclip::WebClipController* controller = nullptr);
    ~SettingsDialog() override;

    void setController(webclip::WebClipController* controller);
    void open();
    void hideAnimated();
    ColorPickerDialog* colorPicker() const { return colorPicker_; }

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void setupContent();
    void updateThemeSelection();
    void updateAccentSelection();
    void updateConnectionButton();
    void onThemeChanged();
    void updateLayout();

    QLabel* createSectionHeader(const QString& text);

    webclip::WebClipController* controller_ = nullptr;
    double progress_ = 0.0;
    Ui::Animations::Simple anim_;

    QWidget* sheet_ = nullptr;
    QWidget* headerBar_ = nullptr;
    Md3IconButton* closeBtn_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* scrollContent_ = nullptr;
    QVBoxLayout* mainLayout_ = nullptr;

    std::vector<QLabel*> sectionHeaders_;

    // Connection
    Md3TextField* hostInput_ = nullptr;
    Md3TextField* portInput_ = nullptr;
    Md3TextField* pinInput_ = nullptr;
    Md3Button* connectBtn_ = nullptr;
    CardToggleRow* autoConnectRow_ = nullptr;

    // Security
    CardContainer* securityCard_ = nullptr;
    CardToggleRow* httpsRow_ = nullptr;
    CardToggleRow* insecureRow_ = nullptr;

    // Sync
    CardContainer* syncCard_ = nullptr;
    CardToggleRow* autoSyncRow_ = nullptr;
    CardContainer* pollingCard_ = nullptr;
    QLabel* pollingLabel_ = nullptr;
    Md3IconButton* resetPollBtn_ = nullptr;
    Md3Slider* pollSlider_ = nullptr;

    // Appearance
    Md3Button* themeAutoBtn_ = nullptr;
    Md3Button* themeLightBtn_ = nullptr;
    Md3Button* themeDarkBtn_ = nullptr;
    Md3Button* themePitchBtn_ = nullptr;

    std::vector<AccentPill*> accentPills_;
    AccentPill* customAccentPill_ = nullptr;

    CardContainer* displayScaleCard_ = nullptr;
    QLabel* displayScaleLabel_ = nullptr;
    Md3IconButton* resetDisplayScaleBtn_ = nullptr;
    Md3Slider* displayScaleSlider_ = nullptr;
    QWidget* restartNoticeWidget_ = nullptr;
    QLabel* restartNoticeLabel_ = nullptr;
    Md3Button* restartNowBtn_ = nullptr;

    Md3Button* clearHistoryBtn_ = nullptr;

    // Logs
    CardContainer* logsCard_ = nullptr;
    CardToggleRow* debugLoggingRow_ = nullptr;

    // About
    CardContainer* aboutCard_ = nullptr;
    CardButtonRow* appRow_ = nullptr;
    CardButtonRow* qtRow_ = nullptr;
    CardButtonRow* engineRow_ = nullptr;
    CardButtonRow* licenseRow_ = nullptr;
    Md3Button* githubBtn_ = nullptr;

    ColorPickerDialog* colorPicker_ = nullptr;
};

} // namespace Ui
