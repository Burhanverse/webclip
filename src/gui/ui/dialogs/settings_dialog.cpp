#include "settings_dialog.hpp"
#include "../basic/painter_helpers.hpp"
#include "../md3/icon_loader.hpp"
#include "../md3/md3_badge.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../controllers/webclip_controller.hpp"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

namespace Ui {

SettingsDialog::SettingsDialog(QWidget* parent, webclip::WebClipController* controller)
    : RpWidget(parent)
    , controller_(controller) {
    hide();
    setFocusPolicy(Qt::StrongFocus);

    sheet_ = new QWidget(this);

    headerBar_ = new QWidget(sheet_);
    headerBar_->setFixedHeight(56);

    closeBtn_ = new Md3IconButton(headerBar_, QStringLiteral("close"), 36, 20);
    closeBtn_->addClickHandler([this] {
        hideAnimated();
    });

    scrollArea_ = new QScrollArea(sheet_);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);

    scrollContent_ = new QWidget();
    scrollArea_->setWidget(scrollContent_);

    colorPicker_ = new ColorPickerDialog(this);
    connect(colorPicker_, &ColorPickerDialog::colorSelected, this, [this](const QColor& c) {
        if (controller_) {
            controller_->setCustomColor(c);
            controller_->setAccentPreset(QStringLiteral("custom"));
        }
    });

    setupContent();
    if (controller_) {
        setController(controller_);
    }
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupContent() {
    auto* theme = webclip::MD3Theme::instance();
    auto* mainLayout = new QVBoxLayout(scrollContent_);
    mainLayout->setContentsMargins(16, 12, 16, 24);
    mainLayout->setSpacing(20);

    auto addSectionTitle = [&](const QString& title) {
        auto* lbl = new QLabel(title);
        lbl->setFont(theme->titleSmall());
        lbl->setStyleSheet(QStringLiteral("color: %1; margin-top: 4px;").arg(theme->primary().name()));
        mainLayout->addWidget(lbl);
    };

    // 1. Connection Section
    addSectionTitle(webclip::I18n::instance()->tr(QStringLiteral("settings.connection.section_title")));
    auto* connCard = new CardContainer(scrollContent_);

    hostInput_ = new Md3TextField(connCard, QStringLiteral("Server Host"), QStringLiteral("123.45.67.89:8080"));
    pinInput_ = new Md3TextField(connCard, QStringLiteral("PIN Code"), QStringLiteral("4-digit PIN"));
    pinInput_->setEchoMode(QLineEdit::Password);

    autoConnectRow_ = new CardToggleRow(
        connCard,
        QStringLiteral("Auto Sync"),
        QStringLiteral("Automatically push clipboard updates"),
        QStringLiteral("sync"),
        true
    );
    connect(autoConnectRow_, &CardToggleRow::toggled, this, [this](bool val) {
        if (controller_) controller_->setAutoSync(val);
    });

    connCard->addRow(autoConnectRow_);
    mainLayout->addWidget(hostInput_);
    mainLayout->addWidget(pinInput_);
    mainLayout->addWidget(connCard);

    // 2. Appearance Section
    addSectionTitle(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.section_title")));

    auto* themeBox = new QHBoxLayout();
    themeAutoBtn_ = new Md3Button(scrollContent_, QStringLiteral("Auto"), ButtonVariant::Tonal);
    themeLightBtn_ = new Md3Button(scrollContent_, QStringLiteral("Light"), ButtonVariant::Tonal);
    themeDarkBtn_ = new Md3Button(scrollContent_, QStringLiteral("Dark"), ButtonVariant::Tonal);
    themePitchBtn_ = new Md3Button(scrollContent_, QStringLiteral("Black"), ButtonVariant::Tonal);

    themeAutoBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(0); updateThemeSelection(); });
    themeLightBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(1); updateThemeSelection(); });
    themeDarkBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(2); updateThemeSelection(); });
    themePitchBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(3); updateThemeSelection(); });

    themeBox->addWidget(themeAutoBtn_);
    themeBox->addWidget(themeLightBtn_);
    themeBox->addWidget(themeDarkBtn_);
    themeBox->addWidget(themePitchBtn_);
    mainLayout->addLayout(themeBox);

    auto* clearHistoryBtn = new Md3Button(
        scrollContent_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.btn_clear_history")),
        ButtonVariant::Outlined
    );
    clearHistoryBtn->setIconName(QStringLiteral("delete"));
    clearHistoryBtn->addClickHandler([this] {
        if (controller_ && controller_->clipModel()) {
            controller_->clipModel()->clear();
        }
    });
    mainLayout->addWidget(clearHistoryBtn);

    // 3. About Section
    addSectionTitle(webclip::I18n::instance()->tr(QStringLiteral("settings.about.section_title")));
    auto* aboutCard = new CardContainer(scrollContent_);

    const QString verText = controller_ ? QStringLiteral("v") + controller_->appVersion() : QStringLiteral("v1.3.0");
    auto* appRow = new CardButtonRow(aboutCard, QStringLiteral("WebClip Sync"), QStringLiteral("Cross-platform clipboard utility"), QStringLiteral("webclip"), verText);
    auto* gitRow = new CardButtonRow(aboutCard, QStringLiteral("GitHub Repository"), QStringLiteral("Open source code & releases"), QStringLiteral("link"), QStringLiteral("github.com"));
    gitRow->addClickHandler([this] {
        if (controller_) controller_->openUrl(QStringLiteral("https://github.com/burhanverse/webclip"));
    });

    aboutCard->addRow(appRow);
    aboutCard->addRow(gitRow);
    mainLayout->addWidget(aboutCard);

    mainLayout->addStretch();
}

void SettingsDialog::setController(webclip::WebClipController* controller) {
    controller_ = controller;
    if (!controller_) return;

    hostInput_->setText(controller_->host());
    pinInput_->setText(controller_->code());
    autoConnectRow_->setChecked(controller_->autoSync(), anim::type::instant);

    connect(hostInput_, &Md3TextField::textChanged, this, [this](const QString& h) {
        if (controller_) controller_->setHost(h);
    });
    connect(pinInput_, &Md3TextField::textChanged, this, [this](const QString& p) {
        if (controller_) controller_->setCode(p);
    });

    connect(controller_, &webclip::WebClipController::themeModeChanged, this, &SettingsDialog::updateThemeSelection);
    updateThemeSelection();
}

void SettingsDialog::updateThemeSelection() {
    if (!controller_) return;
    const int mode = controller_->themeMode();

    themeAutoBtn_->setVariant(mode == 0 ? ButtonVariant::Filled : ButtonVariant::Tonal);
    themeLightBtn_->setVariant(mode == 1 ? ButtonVariant::Filled : ButtonVariant::Tonal);
    themeDarkBtn_->setVariant(mode == 2 ? ButtonVariant::Filled : ButtonVariant::Tonal);
    themePitchBtn_->setVariant(mode == 3 ? ButtonVariant::Filled : ButtonVariant::Tonal);
}

void SettingsDialog::open() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    }
    show();
    raise();
    setFocus();

    anim_.start(
        [this](double progress) {
            progress_ = progress;
            updateLayout();
            update();
        },
        progress_,
        1.0,
        220,
        anim::easeOutCubic
    );
}

void SettingsDialog::hideAnimated() {
    anim_.start(
        [this](double progress) {
            progress_ = progress;
            updateLayout();
            update();
        },
        progress_,
        0.0,
        180,
        anim::easeOutCubic
    );
    anim_.setFinishedCallback([this] {
        hide();
    });
}

void SettingsDialog::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void SettingsDialog::updateLayout() {
    const int sheetH = static_cast<int>(height() * 0.85);
    const int sheetY = height() - static_cast<int>(sheetH * progress_);
    sheet_->setGeometry(0, sheetY, width(), sheetH);

    headerBar_->setGeometry(0, 0, sheet_->width(), 56);
    closeBtn_->move(sheet_->width() - 16 - closeBtn_->width(), (56 - closeBtn_->height()) / 2);

    scrollArea_->setGeometry(0, 56, sheet_->width(), sheet_->height() - 56);
}

void SettingsDialog::mousePressEvent(QMouseEvent* e) {
    if (e->pos().y() < sheet_->y()) {
        hideAnimated();
        return;
    }
    RpWidget::mousePressEvent(e);
}

void SettingsDialog::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hideAnimated();
    } else {
        RpWidget::keyPressEvent(e);
    }
}

void SettingsDialog::paintEvent(QPaintEvent* /*e*/) {
    if (progress_ <= 0.0) return;

    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    ScopedPainterOpacity op(p, progress_);

    // 1. Dimmed backdrop
    p.fillRect(rect(), QColor(0, 0, 0, 115));

    // 2. Sheet background with 24px rounded top corners
    auto* theme = webclip::MD3Theme::instance();
    const QRectF sheetRect(sheet_->geometry());

    QPainterPath sheetPath;
    sheetPath.moveTo(sheetRect.left(), sheetRect.bottom());
    sheetPath.lineTo(sheetRect.left(), sheetRect.top() + 24);
    sheetPath.arcTo(QRectF(sheetRect.left(), sheetRect.top(), 48, 48), 180, -90);
    sheetPath.lineTo(sheetRect.right() - 24, sheetRect.top());
    sheetPath.arcTo(QRectF(sheetRect.right() - 48, sheetRect.top(), 48, 48), 90, -90);
    sheetPath.lineTo(sheetRect.right(), sheetRect.bottom());
    sheetPath.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(theme->surface());
    p.drawPath(sheetPath);

    // 3. Header title
    p.setFont(theme->titleMedium());
    p.setPen(theme->onSurface());
    p.drawText(QPointF(20, sheetRect.top() + 35), webclip::I18n::instance()->tr(QStringLiteral("settings.title")));

    // 4. Header bottom line
    p.setPen(theme->outlineVariant());
    p.drawLine(0, sheetRect.top() + 55, width(), sheetRect.top() + 55);
}

} // namespace Ui
