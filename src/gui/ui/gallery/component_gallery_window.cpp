#include "component_gallery_window.hpp"
#include "../md3/md3_button.hpp"
#include "../md3/md3_icon_button.hpp"
#include "../md3/md3_switch.hpp"
#include "../md3/md3_badge.hpp"
#include "../md3/md3_text_field.hpp"
#include "../md3/md3_card.hpp"
#include "../md3/md3_slider.hpp"
#include "../chrome/toast_widget.hpp"
#include "../chrome/popup_menu.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace Ui {

ComponentGalleryWindow::ComponentGalleryWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("WebClip MD3 Component Gallery (--test-widgets)"));
    resize(860, 940);
    setupUi();
}

ComponentGalleryWindow::~ComponentGalleryWindow() = default;

void ComponentGalleryWindow::applyTheme(int mode) {
    auto* theme = webclip::MD3Theme::instance();
    theme->setThemeMode(mode);

    // Refresh background
    centralContent_->setStyleSheet(QStringLiteral("background-color: %1;").arg(theme->surface().name()));
    centralContent_->update();
}

void ComponentGalleryWindow::setupUi() {
    auto* theme = webclip::MD3Theme::instance();

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    setCentralWidget(scrollArea_);

    centralContent_ = new QWidget();
    centralContent_->setStyleSheet(QStringLiteral("background-color: %1;").arg(theme->surface().name()));
    scrollArea_->setWidget(centralContent_);

    auto* mainLayout = new QVBoxLayout(centralContent_);
    mainLayout->setContentsMargins(32, 24, 32, 32);
    mainLayout->setSpacing(24);

    // 0. Header Toolbar
    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel(QStringLiteral("MD3 Native C++ Component Gallery"));
    titleLabel->setFont(theme->titleMedium());
    titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(theme->onSurface().name()));
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    auto* lightBtn = new Md3Button(this, QStringLiteral("Light"), ButtonVariant::Tonal);
    auto* darkBtn = new Md3Button(this, QStringLiteral("Dark"), ButtonVariant::Tonal);
    auto* pitchBtn = new Md3Button(this, QStringLiteral("Pitch Black"), ButtonVariant::Tonal);

    lightBtn->addClickHandler([this] { applyTheme(1); });
    darkBtn->addClickHandler([this] { applyTheme(2); });
    pitchBtn->addClickHandler([this] { applyTheme(3); });

    headerLayout->addWidget(lightBtn);
    headerLayout->addWidget(darkBtn);
    headerLayout->addWidget(pitchBtn);
    mainLayout->addLayout(headerLayout);

    // Section Helper Lambda
    auto addSectionHeader = [&](const QString& title) {
        auto* lbl = new QLabel(title);
        lbl->setFont(theme->titleSmall());
        lbl->setStyleSheet(QStringLiteral("color: %1; margin-top: 8px;").arg(theme->primary().name()));
        mainLayout->addWidget(lbl);
    };

    // 1. Buttons
    addSectionHeader(QStringLiteral("1. Buttons (Md3Button)"));
    auto* btnRow1 = new QHBoxLayout();
    btnRow1->addWidget(new Md3Button(centralContent_, QStringLiteral("Filled Button"), ButtonVariant::Filled));
    btnRow1->addWidget(new Md3Button(centralContent_, QStringLiteral("Tonal Button"), ButtonVariant::Tonal));
    btnRow1->addWidget(new Md3Button(centralContent_, QStringLiteral("Outlined Button"), ButtonVariant::Outlined));
    btnRow1->addWidget(new Md3Button(centralContent_, QStringLiteral("Text Button"), ButtonVariant::Text));
    btnRow1->addStretch();
    mainLayout->addLayout(btnRow1);

    auto* btnRow2 = new QHBoxLayout();
    auto* iconBtn1 = new Md3Button(centralContent_, QStringLiteral("With Icon"), ButtonVariant::Filled);
    iconBtn1->setIconName(QStringLiteral("sync"));
    auto* disabledBtn = new Md3Button(centralContent_, QStringLiteral("Disabled"), ButtonVariant::Filled);
    disabledBtn->setEnabled(false);
    btnRow2->addWidget(iconBtn1);
    btnRow2->addWidget(disabledBtn);
    btnRow2->addStretch();
    mainLayout->addLayout(btnRow2);

    // 2. Icon Buttons
    addSectionHeader(QStringLiteral("2. Icon Buttons (Md3IconButton)"));
    auto* iconRow = new QHBoxLayout();
    const QStringList iconNames = {
        QStringLiteral("sync"), QStringLiteral("dark_mode"), QStringLiteral("settings"),
        QStringLiteral("delete"), QStringLiteral("check"), QStringLiteral("image"),
        QStringLiteral("paste"), QStringLiteral("send")
    };
    for (const auto& name : iconNames) {
        iconRow->addWidget(new Md3IconButton(centralContent_, name, 40, 20));
    }
    iconRow->addStretch();
    mainLayout->addLayout(iconRow);

    // 3. Switches
    addSectionHeader(QStringLiteral("3. Switches (Md3Switch)"));
    auto* switchRow = new QHBoxLayout();
    switchRow->addWidget(new Md3Switch(centralContent_, true));
    switchRow->addWidget(new Md3Switch(centralContent_, false));
    auto* disSwitch = new Md3Switch(centralContent_, true);
    disSwitch->setEnabled(false);
    switchRow->addWidget(disSwitch);
    switchRow->addStretch();
    mainLayout->addLayout(switchRow);

    // 4. Badges
    addSectionHeader(QStringLiteral("4. Badges (Md3Badge)"));
    auto* badgeRow = new QHBoxLayout();
    badgeRow->addWidget(new Md3Badge(centralContent_, QStringLiteral("Connected"), QStringLiteral("check")));
    badgeRow->addWidget(new Md3Badge(centralContent_, QStringLiteral("Syncing..."), QStringLiteral("sync")));
    badgeRow->addStretch();
    mainLayout->addLayout(badgeRow);

    // 5. Text Fields
    addSectionHeader(QStringLiteral("5. Text Fields (Md3TextField)"));
    auto* tfRow = new QHBoxLayout();
    tfRow->addWidget(new Md3TextField(centralContent_, QStringLiteral("Server Host"), QStringLiteral("e.g. 192.168.1.10")));
    tfRow->addWidget(new Md3TextField(centralContent_, QStringLiteral("PIN Code"), QStringLiteral("1234")));
    mainLayout->addLayout(tfRow);

    // 6. Sliders
    addSectionHeader(QStringLiteral("6. Sliders (Md3Slider)"));
    auto* sliderCont = new Md3Slider(centralContent_);
    sliderCont->setRange(0, 100);
    sliderCont->setValue(45);

    auto* sliderDisc = new Md3Slider(centralContent_);
    sliderDisc->setRange(0, 100);
    sliderDisc->setSteps(5);
    sliderDisc->setValue(50);

    mainLayout->addWidget(sliderCont);
    mainLayout->addWidget(sliderDisc);

    // 7. Cards & Rows
    addSectionHeader(QStringLiteral("7. Segmented Card Container (fa_cards.cpp pattern)"));
    auto* card = new CardContainer(centralContent_);
    card->addRow(new CardToggleRow(card, QStringLiteral("Auto Sync"), QStringLiteral("Sync clips automatically across devices"), QStringLiteral("sync"), true));
    card->addRow(new CardButtonRow(card, QStringLiteral("Accent Color"), QStringLiteral("Customize application palette"), QStringLiteral("palette"), QStringLiteral("Purple")));
    card->addRow(new CardButtonRow(card, QStringLiteral("Connection Settings"), QStringLiteral("Port and discovery options"), QStringLiteral("settings"), QString()));
    mainLayout->addWidget(card);

    // 8. Dialog Triggers
    addSectionHeader(QStringLiteral("8. Dialogs & Modals"));
    auto* dialogRow = new QHBoxLayout();
    auto* toastTrigger = new Md3Button(centralContent_, QStringLiteral("Show Toast Notification"), ButtonVariant::Filled);
    auto* popupTrigger = new Md3Button(centralContent_, QStringLiteral("Show Popup Menu"), ButtonVariant::Tonal);

    auto* toast = new ToastWidget(this);

    toastTrigger->addClickHandler([toast] {
        toast->showMessage(QStringLiteral("Item copied to clipboard!"));
    });

    popupTrigger->addClickHandler([this, popupTrigger] {
        auto* menu = new PopupMenu(this);
        menu->addAction(QStringLiteral("sync"), QStringLiteral("Sync Now"), [] {});
        menu->addAction(QStringLiteral("palette"), QStringLiteral("Change Theme"), [] {});
        menu->addSeparator();
        menu->addAction(QStringLiteral("delete"), QStringLiteral("Clear History"), [] {});
        const QPoint globalPos = popupTrigger->mapToGlobal(QPoint(0, popupTrigger->height()));
        menu->popup(globalPos);
    });

    dialogRow->addWidget(toastTrigger);
    dialogRow->addWidget(popupTrigger);
    dialogRow->addStretch();
    mainLayout->addLayout(dialogRow);

    mainLayout->addStretch();
}

} // namespace Ui
