#include "settings_dialog.hpp"
#include "../basic/painter_helpers.hpp"
#include "../md3/icon_loader.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../util/display_scale.hpp"
#include "../../controllers/webclip_controller.hpp"
#include "../../models/clipboard_history_model.hpp"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGraphicsDropShadowEffect>

namespace Ui {

class AccentPill : public RippleButton {
public:
    AccentPill(
        QWidget* parent,
        const QString& name,
        const QString& label,
        const QColor& color,
        bool isCustom = false
    )
        : RippleButton(parent)
        , name_(name)
        , label_(label)
        , color_(color)
        , isCustom_(isCustom) {
        setFixedHeight(webclip::scale::px(32));
        setFont(webclip::MD3Theme::instance()->labelSmall());
    }

    [[nodiscard]] QString name() const noexcept { return name_; }

    void setSelected(bool sel) {
        if (selected_ != sel) {
            selected_ = sel;
            update();
        }
    }

    void setCustomColor(const QColor& c) {
        if (color_ != c) {
            color_ = c;
            update();
        }
    }

    [[nodiscard]] QSize sizeHint() const override {
        const QFontMetrics fm(font());
        const int textW = fm.horizontalAdvance(label_);
        return QSize(textW + webclip::scale::px(36), webclip::scale::px(32));
    }

protected:
    void paintEvent(QPaintEvent* /*e*/) override {
        QPainter p(this);
        PainterHighQualityEnabler hq(p);
        auto* theme = webclip::MD3Theme::instance();

        const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
        const QColor bg = selected_ ? theme->primary() : theme->surfaceContainerHigh();
        const QColor textCol = selected_ ? theme->onPrimary() : theme->onSurface();

        p.setPen(selected_ ? Qt::NoPen : QPen(theme->outlineVariant(), 1.0));
        p.setBrush(bg);
        p.drawRoundedRect(r, webclip::scale::pxF(16.0), webclip::scale::pxF(16.0));

        // State layer & ripple
        if (!isDisabled() && (isOver() || isDown())) {
            const int stateAlpha = isDown() ? 36 : 20;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(textCol.red(), textCol.green(), textCol.blue(), stateAlpha));
            p.drawRoundedRect(r, webclip::scale::pxF(16.0), webclip::scale::pxF(16.0));
        }
        paintRipple(p, 0, 0, &textCol);

        // Icon or Color dot
        if (isCustom_) {
            IconLoader::paint(p, QStringLiteral("palette"), QRectF(webclip::scale::pxF(10), webclip::scale::pxF(8), webclip::scale::pxF(16), webclip::scale::pxF(16)), textCol);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(color_);
            p.drawEllipse(QRectF(webclip::scale::pxF(11), webclip::scale::pxF(10), webclip::scale::pxF(12), webclip::scale::pxF(12)));
        }

        // Text label
        p.setFont(font());
        p.setPen(textCol);
        const QFontMetrics fm(font());
        const int textY = (height() - fm.height()) / 2 + fm.ascent();
        p.drawText(QPointF(webclip::scale::pxF(30), textY), label_);
    }

    QImage prepareRippleMask() const override {
        return RippleAnimation::RoundRectMask(size(), webclip::scale::px(16));
    }

private:
    QString name_;
    QString label_;
    QColor color_;
    bool isCustom_ = false;
    bool selected_ = false;
};

SettingsDialog::SettingsDialog(QWidget* parent, webclip::WebClipController* controller)
    : RpWidget(parent)
    , controller_(controller) {
    hide();
    setFocusPolicy(Qt::StrongFocus);

    sheet_ = new QWidget(this);
    headerBar_ = new QWidget(sheet_);
    headerBar_->setFixedHeight(webclip::scale::px(56));

    closeBtn_ = new Md3IconButton(headerBar_, QStringLiteral("close"), webclip::scale::px(36), webclip::scale::px(20));
    closeBtn_->addClickHandler([this] {
        hideAnimated();
    });

    scrollArea_ = new QScrollArea(sheet_);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 6px; background: transparent; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(128, 128, 128, 0.4); border-radius: 3px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(128, 128, 128, 0.7); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    ));
    scrollArea_->viewport()->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    scrollContent_ = new QWidget(scrollArea_);
    scrollContent_->setStyleSheet(QStringLiteral("background: transparent;"));
    scrollArea_->setWidget(scrollContent_);

    mainLayout_ = new QVBoxLayout(scrollContent_);
    mainLayout_->setContentsMargins(webclip::scale::px(16), webclip::scale::px(12), webclip::scale::px(16), webclip::scale::px(28));
    mainLayout_->setSpacing(webclip::scale::px(16));

    colorPicker_ = new ColorPickerDialog(this);
    connect(colorPicker_, &ColorPickerDialog::colorSelected, this, [this](const QColor& c) {
        if (controller_) {
            controller_->setCustomColor(c);
            controller_->setAccentPreset(QStringLiteral("custom"));
        }
        if (customAccentPill_) {
            customAccentPill_->setCustomColor(c);
        }
        updateAccentSelection();
    });

    setupContent();

    if (controller_) {
        setController(controller_);
    }

    connect(webclip::MD3Theme::instance(), &webclip::MD3Theme::themeChanged, this, &SettingsDialog::onThemeChanged);
}

SettingsDialog::~SettingsDialog() = default;

QLabel* SettingsDialog::createSectionHeader(const QString& text) {
    auto* theme = webclip::MD3Theme::instance();
    auto* lbl = new QLabel(text, scrollContent_);
    lbl->setFont(theme->labelLarge());
    lbl->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; background: transparent; margin-top: 8px; margin-bottom: 2px;").arg(theme->primary().name()));
    sectionHeaders_.push_back(lbl);
    return lbl;
}

void SettingsDialog::setupContent() {
    auto* theme = webclip::MD3Theme::instance();

    // ==========================================
    // 1. Connection Section
    // ==========================================
    mainLayout_->addWidget(createSectionHeader(webclip::I18n::instance()->tr(QStringLiteral("settings.connection.section_title"))));

    auto* connCard = new CardContainer(scrollContent_);
    auto* connRow = new CardRow(connCard, QString(), QString());
    connRow->setFixedHeight(webclip::scale::px(128));
    auto* connBox = new QVBoxLayout(connRow);
    connBox->setContentsMargins(webclip::scale::px(14), webclip::scale::px(12), webclip::scale::px(14), webclip::scale::px(12));
    connBox->setSpacing(webclip::scale::px(10));

    auto* row1 = new QHBoxLayout();
    row1->setSpacing(webclip::scale::px(8));
    hostInput_ = new Md3TextField(connRow, webclip::I18n::instance()->tr(QStringLiteral("settings.connection.host_label")), QStringLiteral("192.168.1.100"));
    portInput_ = new Md3TextField(connRow, webclip::I18n::instance()->tr(QStringLiteral("settings.connection.port_label")), QStringLiteral("8080"));
    portInput_->setFixedWidth(webclip::scale::px(85));
    row1->addWidget(hostInput_, 1);
    row1->addWidget(portInput_, 0);
    connBox->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    row2->setSpacing(webclip::scale::px(8));
    pinInput_ = new Md3TextField(connRow, webclip::I18n::instance()->tr(QStringLiteral("settings.connection.code_label")), QStringLiteral("4-digit code"));
    connectBtn_ = new Md3Button(connRow, webclip::I18n::instance()->tr(QStringLiteral("settings.connection.btn_connect")), ButtonVariant::Filled);
    connectBtn_->setFixedHeight(webclip::scale::px(44));
    connectBtn_->setIconName(QStringLiteral("sync"));
    connectBtn_->addClickHandler([this] {
        if (controller_) controller_->toggleConnection();
    });
    row2->addWidget(pinInput_, 1);
    row2->addWidget(connectBtn_, 0);
    connBox->addLayout(row2);

    connCard->addRow(connRow);
    mainLayout_->addWidget(connCard);

    // Auto-connect on startup toggle
    auto* startupCard = new CardContainer(scrollContent_);
    autoConnectRow_ = new CardToggleRow(
        startupCard,
        webclip::I18n::instance()->tr(QStringLiteral("settings.connection.autoconnect_title")),
        webclip::I18n::instance()->tr(QStringLiteral("settings.connection.autoconnect_subtitle")),
        QStringLiteral("link"),
        false
    );
    connect(autoConnectRow_, &CardToggleRow::toggled, this, [this](bool val) {
        if (controller_) controller_->setAutoConnect(val);
    });
    startupCard->addRow(autoConnectRow_);
    mainLayout_->addWidget(startupCard);

    // ==========================================
    // 2. Security Section
    // ==========================================
    mainLayout_->addWidget(createSectionHeader(webclip::I18n::instance()->tr(QStringLiteral("settings.security.section_title"))));
    securityCard_ = new CardContainer(scrollContent_);

    httpsRow_ = new CardToggleRow(
        securityCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.security.https_title")),
        webclip::I18n::instance()->tr(QStringLiteral("settings.security.https_subtitle")),
        QStringLiteral("link"),
        false
    );
    connect(httpsRow_, &CardToggleRow::toggled, this, [this](bool val) {
        if (controller_) controller_->setUseHttps(val);
    });

    insecureRow_ = new CardToggleRow(
        securityCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.security.ssl_title")),
        webclip::I18n::instance()->tr(QStringLiteral("settings.security.ssl_subtitle")),
        QStringLiteral("link_off"),
        true
    );
    connect(insecureRow_, &CardToggleRow::toggled, this, [this](bool val) {
        if (controller_) controller_->setInsecure(val);
    });

    securityCard_->addRow(httpsRow_);
    securityCard_->addRow(insecureRow_);
    mainLayout_->addWidget(securityCard_);

    // ==========================================
    // 3. Synchronization Section
    // ==========================================
    mainLayout_->addWidget(createSectionHeader(webclip::I18n::instance()->tr(QStringLiteral("settings.sync.section_title"))));
    syncCard_ = new CardContainer(scrollContent_);

    autoSyncRow_ = new CardToggleRow(
        syncCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.sync.autosync_title")),
        webclip::I18n::instance()->tr(QStringLiteral("settings.sync.autosync_subtitle")),
        QStringLiteral("sync"),
        true
    );
    connect(autoSyncRow_, &CardToggleRow::toggled, this, [this](bool val) {
        if (controller_) controller_->setAutoSync(val);
    });
    syncCard_->addRow(autoSyncRow_);
    mainLayout_->addWidget(syncCard_);

    // Polling Interval Card
    pollingCard_ = new CardContainer(scrollContent_);
    auto* pollRow = new CardRow(pollingCard_, QString(), QString());
    pollRow->setFixedHeight(webclip::scale::px(86));
    auto* pollRowLayout = new QVBoxLayout(pollRow);
    pollRowLayout->setContentsMargins(webclip::scale::px(16), webclip::scale::px(10), webclip::scale::px(16), webclip::scale::px(12));
    pollRowLayout->setSpacing(webclip::scale::px(8));

    auto* pollHeader = new QHBoxLayout();
    pollingLabel_ = new QLabel(pollRow);
    pollingLabel_->setFont(theme->bodyMedium());
    pollingLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme->onSurface().name()));
    pollingLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.sync.polling_title")) + QStringLiteral(": 1.0s"));

    resetPollBtn_ = new Md3IconButton(pollRow, QStringLiteral("sync"), webclip::scale::px(26), webclip::scale::px(16));
    resetPollBtn_->addClickHandler([this] {
        if (controller_) controller_->setPollInterval(1.0);
    });

    pollHeader->addWidget(pollingLabel_, 1);
    pollHeader->addWidget(resetPollBtn_, 0);
    pollRowLayout->addLayout(pollHeader);

    pollSlider_ = new Md3Slider(pollRow);
    pollSlider_->setFixedHeight(webclip::scale::px(32));
    pollSlider_->setRange(0.5, 5.0);
    pollSlider_->setSteps(9);
    pollSlider_->setValue(1.0);
    connect(pollSlider_, &Md3Slider::valueChanged, this, [this](double val) {
        if (controller_) controller_->setPollInterval(val);
        if (pollingLabel_) {
            pollingLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.sync.polling_title")) + QStringLiteral(": ") + QString::number(val, 'f', 1) + QStringLiteral("s"));
        }
    });
    pollRowLayout->addWidget(pollSlider_);

    pollingCard_->addRow(pollRow);
    mainLayout_->addWidget(pollingCard_);

    // ==========================================
    // 4. Appearance Section
    // ==========================================
    mainLayout_->addWidget(createSectionHeader(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.section_title"))));

    auto* themeCard = new CardContainer(scrollContent_);
    auto* themeRow = new CardRow(themeCard, QString(), QString());
    themeRow->setFixedHeight(webclip::scale::px(86));
    auto* themeRowLayout = new QVBoxLayout(themeRow);
    themeRowLayout->setContentsMargins(webclip::scale::px(16), webclip::scale::px(12), webclip::scale::px(16), webclip::scale::px(12));
    themeRowLayout->setSpacing(webclip::scale::px(8));

    auto* themeTitle = new QLabel(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.theme_mode")), themeRow);
    themeTitle->setFont(theme->bodyMedium());
    themeTitle->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme->onSurface().name()));
    themeRowLayout->addWidget(themeTitle);

    auto* modeBox = new QHBoxLayout();
    modeBox->setSpacing(webclip::scale::px(6));
    themeAutoBtn_ = new Md3Button(themeRow, webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.mode_system")), ButtonVariant::Tonal);
    themeLightBtn_ = new Md3Button(themeRow, webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.mode_light")), ButtonVariant::Tonal);
    themeDarkBtn_ = new Md3Button(themeRow, webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.mode_dark")), ButtonVariant::Tonal);
    themePitchBtn_ = new Md3Button(themeRow, webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.mode_pitch_black")), ButtonVariant::Tonal);

    themeAutoBtn_->setFixedHeight(webclip::scale::px(34));
    themeLightBtn_->setFixedHeight(webclip::scale::px(34));
    themeDarkBtn_->setFixedHeight(webclip::scale::px(34));
    themePitchBtn_->setFixedHeight(webclip::scale::px(34));

    themeAutoBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(0); updateThemeSelection(); });
    themeLightBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(1); updateThemeSelection(); });
    themeDarkBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(2); updateThemeSelection(); });
    themePitchBtn_->addClickHandler([this] { if (controller_) controller_->setThemeMode(3); updateThemeSelection(); });

    modeBox->addWidget(themeAutoBtn_, 1);
    modeBox->addWidget(themeLightBtn_, 1);
    modeBox->addWidget(themeDarkBtn_, 1);
    modeBox->addWidget(themePitchBtn_, 1);
    themeRowLayout->addLayout(modeBox);

    themeCard->addRow(themeRow);
    mainLayout_->addWidget(themeCard);

    // Accent Color Card
    auto* accentCard = new CardContainer(scrollContent_);
    auto* accentRow = new CardRow(accentCard, QString(), QString());
    accentRow->setFixedHeight(webclip::scale::px(116));
    auto* accentLayout = new QVBoxLayout(accentRow);
    accentLayout->setContentsMargins(webclip::scale::px(16), webclip::scale::px(12), webclip::scale::px(16), webclip::scale::px(14));
    accentLayout->setSpacing(webclip::scale::px(8));

    auto* accentLabel = new QLabel(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.accent_color")), accentRow);
    accentLabel->setFont(theme->bodyMedium());
    accentLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme->onSurface().name()));
    accentLayout->addWidget(accentLabel);

    auto* flowLayout1 = new QHBoxLayout();
    flowLayout1->setSpacing(webclip::scale::px(6));

    const struct { QString name; QString label; QColor col; } presets[] = {
        { QStringLiteral("purple"), QStringLiteral("Purple"), QColor(QStringLiteral("#6750A4")) },
        { QStringLiteral("blue"), QStringLiteral("Blue"), QColor(QStringLiteral("#2196F3")) },
        { QStringLiteral("teal"), QStringLiteral("Teal"), QColor(QStringLiteral("#009688")) },
        { QStringLiteral("green"), QStringLiteral("Green"), QColor(QStringLiteral("#4CAF50")) },
        { QStringLiteral("orange"), QStringLiteral("Orange"), QColor(QStringLiteral("#FF9800")) },
        { QStringLiteral("red"), QStringLiteral("Red"), QColor(QStringLiteral("#F44336")) },
        { QStringLiteral("pink"), QStringLiteral("Pink"), QColor(QStringLiteral("#E91E63")) }
    };

    auto* flowLayout2 = new QHBoxLayout();
    flowLayout2->setSpacing(webclip::scale::px(6));

    int idx = 0;
    for (const auto& p : presets) {
        auto* pill = new AccentPill(accentRow, p.name, p.label, p.col, false);
        pill->addClickHandler([this, name = p.name] {
            if (controller_) controller_->setAccentPreset(name);
            updateAccentSelection();
        });
        accentPills_.push_back(pill);
        if (idx < 4) {
            flowLayout1->addWidget(pill);
        } else {
            flowLayout2->addWidget(pill);
        }
        idx++;
    }

    customAccentPill_ = new AccentPill(
        accentRow,
        QStringLiteral("custom"),
        webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.accent_custom")),
        controller_ ? controller_->customColor() : QColor(QStringLiteral("#6750A4")),
        true
    );
    customAccentPill_->addClickHandler([this] {
        if (controller_) {
            controller_->setAccentPreset(QStringLiteral("custom"));
            colorPicker_->openWithColor(controller_->customColor());
        }
        updateAccentSelection();
    });
    flowLayout2->addWidget(customAccentPill_);

    accentLayout->addLayout(flowLayout1);
    accentLayout->addLayout(flowLayout2);
    accentCard->addRow(accentRow);
    mainLayout_->addWidget(accentCard);

    // Display Scale Card
    displayScaleCard_ = new CardContainer(scrollContent_);
    auto* displayScaleRow = new CardRow(displayScaleCard_, QString(), QString());
    displayScaleRow->setFixedHeight(webclip::scale::px(86));
    auto* displayScaleRowLayout = new QVBoxLayout(displayScaleRow);
    displayScaleRowLayout->setContentsMargins(webclip::scale::px(16), webclip::scale::px(10), webclip::scale::px(16), webclip::scale::px(12));
    displayScaleRowLayout->setSpacing(webclip::scale::px(8));

    auto* displayScaleHeader = new QHBoxLayout();
    displayScaleLabel_ = new QLabel(displayScaleRow);
    displayScaleLabel_->setFont(theme->bodyMedium());
    displayScaleLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme->onSurface().name()));
    double currentScale = controller_ ? controller_->displayScale() : 0.0;
    if (currentScale <= 0.0) {
        displayScaleLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_title")) +
                               QStringLiteral(": ") + webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_auto")));
    } else {
        displayScaleLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_title")) +
                               QStringLiteral(": ") + QString::number(qRound(currentScale * 100)) + QStringLiteral("%"));
    }

    resetDisplayScaleBtn_ = new Md3IconButton(displayScaleRow, QStringLiteral("refresh"), webclip::scale::px(26), webclip::scale::px(16));
    resetDisplayScaleBtn_->addClickHandler([this] {
        if (controller_) controller_->setDisplayScale(0.0);
    });

    displayScaleHeader->addWidget(displayScaleLabel_, 1);
    displayScaleHeader->addWidget(resetDisplayScaleBtn_, 0);
    displayScaleRowLayout->addLayout(displayScaleHeader);

    displayScaleSlider_ = new Md3Slider(displayScaleRow);
    displayScaleSlider_->setFixedHeight(webclip::scale::px(32));
    displayScaleSlider_->setRange(0.75, 1.40);
    displayScaleSlider_->setSteps(14);
    displayScaleSlider_->setValue(currentScale <= 0.0 ? 1.0 : currentScale);
    connect(displayScaleSlider_, &Md3Slider::valueChanged, this, [this](double val) {
        if (controller_) controller_->setDisplayScale(val);
        if (displayScaleLabel_) {
            displayScaleLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_title")) +
                                   QStringLiteral(": ") + QString::number(qRound(val * 100)) + QStringLiteral("%"));
        }
        if (restartNoticeWidget_) {
            restartNoticeWidget_->setVisible(true);
        }
    });
    displayScaleRowLayout->addWidget(displayScaleSlider_);

    displayScaleCard_->addRow(displayScaleRow);
    mainLayout_->addWidget(displayScaleCard_);

    // Restart notice row
    restartNoticeWidget_ = new QWidget(scrollContent_);
    auto* restartLayout = new QHBoxLayout(restartNoticeWidget_);
    restartLayout->setContentsMargins(webclip::scale::px(4), 0, webclip::scale::px(4), 0);
    restartLayout->setSpacing(webclip::scale::px(8));

    restartNoticeLabel_ = new QLabel(restartNoticeWidget_);
    restartNoticeLabel_->setFont(theme->bodySmall());
    restartNoticeLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme->onSurfaceVariant().name()));
    restartNoticeLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_restart_hint")));

    restartNowBtn_ = new Md3Button(
        restartNoticeWidget_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.btn_restart")),
        ButtonVariant::Tonal
    );
    restartNowBtn_->setFixedHeight(webclip::scale::px(32));
    restartNowBtn_->addClickHandler([this] {
        if (controller_) controller_->restartApplication();
    });

    restartLayout->addWidget(restartNoticeLabel_, 1);
    restartLayout->addWidget(restartNowBtn_, 0);
    restartNoticeWidget_->setVisible(false);
    mainLayout_->addWidget(restartNoticeWidget_);

    // Clear History Button
    clearHistoryBtn_ = new Md3Button(
        scrollContent_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.btn_clear_history")),
        ButtonVariant::Outlined
    );
    clearHistoryBtn_->setIconName(QStringLiteral("delete"));
    clearHistoryBtn_->addClickHandler([this] {
        if (controller_ && controller_->clipModel()) {
            controller_->clipModel()->clear();
        }
    });
    mainLayout_->addWidget(clearHistoryBtn_);

    // ==========================================
    // 5. About WebClip Section
    // ==========================================
    mainLayout_->addWidget(createSectionHeader(webclip::I18n::instance()->tr(QStringLiteral("settings.about.section_title"))));
    aboutCard_ = new CardContainer(scrollContent_);

    const QString verText = controller_ ? QStringLiteral("v") + controller_->appVersion()
                                        : QStringLiteral("v") + QString::fromUtf8(webclip::VERSION_STRING.data(), webclip::VERSION_STRING.size());
    appRow_ = new CardButtonRow(
        aboutCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.app_name")),
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.app_subtitle")),
        QStringLiteral("webclip"),
        verText
    );

    qtRow_ = new CardButtonRow(
        aboutCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.qt_runtime")),
        QString(),
        QString(),
        controller_ ? (QStringLiteral("Qt ") + controller_->qtVersion()) : QStringLiteral("Qt 6.8")
    );

    engineRow_ = new CardButtonRow(
        aboutCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.engine")),
        QString(),
        QString(),
        controller_ ? controller_->clipboardBackend() : QStringLiteral("Native C++")
    );

    licenseRow_ = new CardButtonRow(
        aboutCard_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.license")),
        QString(),
        QString(),
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.license_val"))
    );

    aboutCard_->addRow(appRow_);
    aboutCard_->addRow(qtRow_);
    aboutCard_->addRow(engineRow_);
    aboutCard_->addRow(licenseRow_);
    mainLayout_->addWidget(aboutCard_);

    githubBtn_ = new Md3Button(
        scrollContent_,
        webclip::I18n::instance()->tr(QStringLiteral("settings.about.btn_github")),
        ButtonVariant::Tonal
    );
    githubBtn_->setIconName(QStringLiteral("link"));
    githubBtn_->addClickHandler([this] {
        if (controller_) controller_->openUrl(QStringLiteral("https://github.com/burhanverse/webclip"));
    });
    mainLayout_->addWidget(githubBtn_);

    mainLayout_->addStretch();
}

void SettingsDialog::setController(webclip::WebClipController* controller) {
    controller_ = controller;
    if (!controller_) return;

    hostInput_->setText(controller_->host());
    portInput_->setText(QString::number(controller_->port()));
    pinInput_->setText(controller_->code());

    httpsRow_->setChecked(controller_->useHttps(), anim::type::instant);
    insecureRow_->setChecked(controller_->insecure(), anim::type::instant);
    autoSyncRow_->setChecked(controller_->autoSync(), anim::type::instant);
    autoConnectRow_->setChecked(controller_->autoConnect(), anim::type::instant);

    if (pollSlider_) {
        pollSlider_->setValue(controller_->pollInterval());
    }
    if (pollingLabel_) {
        pollingLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.sync.polling_title")) + QStringLiteral(": ") + QString::number(controller_->pollInterval(), 'f', 1) + QStringLiteral("s"));
    }

    connect(hostInput_, &Md3TextField::textChanged, this, [this](const QString& h) {
        if (controller_) controller_->setHost(h);
    });
    connect(portInput_, &Md3TextField::textChanged, this, [this](const QString& p) {
        bool ok = false;
        const int port = p.toInt(&ok);
        if (ok && controller_) controller_->setPort(port);
    });
    connect(pinInput_, &Md3TextField::textChanged, this, [this](const QString& p) {
        if (controller_) controller_->setCode(p);
    });

    connect(controller_, &webclip::WebClipController::connectedChanged, this, &SettingsDialog::updateConnectionButton);
    connect(controller_, &webclip::WebClipController::connectingChanged, this, &SettingsDialog::updateConnectionButton);

    connect(controller_, &webclip::WebClipController::themeModeChanged, this, &SettingsDialog::updateThemeSelection);
    connect(controller_, &webclip::WebClipController::accentPresetChanged, this, &SettingsDialog::updateAccentSelection);
    connect(controller_, &webclip::WebClipController::customColorChanged, this, [this] {
        if (customAccentPill_ && controller_) {
            customAccentPill_->setCustomColor(controller_->customColor());
        }
        updateAccentSelection();
    });

    connect(controller_, &webclip::WebClipController::displayScaleChanged, this, [this] {
        if (controller_ && displayScaleSlider_) {
            double s = controller_->displayScale();
            displayScaleSlider_->setValue(s <= 0.0 ? 1.0 : s);
        }
        if (displayScaleLabel_) {
            double s = controller_ ? controller_->displayScale() : 0.0;
            if (s <= 0.0) {
                displayScaleLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_title")) +
                                       QStringLiteral(": ") + webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_auto")));
            } else {
                displayScaleLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.appearance.displayscale_title")) +
                                       QStringLiteral(": ") + QString::number(qRound(s * 100)) + QStringLiteral("%"));
            }
        }
        if (restartNoticeWidget_) {
            restartNoticeWidget_->setVisible(true);
        }
    });

    connect(controller_, &webclip::WebClipController::pollIntervalChanged, this, [this] {
        if (controller_ && pollSlider_) {
            pollSlider_->setValue(controller_->pollInterval());
        }
        if (controller_ && pollingLabel_) {
            pollingLabel_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.sync.polling_title")) + QStringLiteral(": ") + QString::number(controller_->pollInterval(), 'f', 1) + QStringLiteral("s"));
        }
    });

    connect(controller_, &webclip::WebClipController::autoSyncChanged, this, [this] {
        if (controller_ && autoSyncRow_) {
            autoSyncRow_->setChecked(controller_->autoSync(), anim::type::instant);
        }
    });

    connect(controller_, &webclip::WebClipController::autoConnectChanged, this, [this] {
        if (controller_ && autoConnectRow_) {
            autoConnectRow_->setChecked(controller_->autoConnect(), anim::type::instant);
        }
    });

    updateThemeSelection();
    updateAccentSelection();
    updateConnectionButton();
}

void SettingsDialog::updateConnectionButton() {
    if (!controller_ || !connectBtn_) return;

    if (controller_->connected()) {
        connectBtn_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.connection.btn_disconnect")));
        connectBtn_->setVariant(ButtonVariant::Tonal);
        connectBtn_->setIconName(QStringLiteral("close"));
    } else if (controller_->connecting()) {
        connectBtn_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.connection.btn_connecting")));
        connectBtn_->setVariant(ButtonVariant::Tonal);
        connectBtn_->setIconName(QStringLiteral("sync"));
    } else {
        connectBtn_->setText(webclip::I18n::instance()->tr(QStringLiteral("settings.connection.btn_connect")));
        connectBtn_->setVariant(ButtonVariant::Filled);
        connectBtn_->setIconName(QStringLiteral("sync"));
    }
}

void SettingsDialog::updateThemeSelection() {
    if (!controller_) return;
    const int mode = controller_->themeMode();

    if (themeAutoBtn_) themeAutoBtn_->setVariant(mode == 0 ? ButtonVariant::Filled : ButtonVariant::Tonal);
    if (themeLightBtn_) themeLightBtn_->setVariant(mode == 1 ? ButtonVariant::Filled : ButtonVariant::Tonal);
    if (themeDarkBtn_) themeDarkBtn_->setVariant(mode == 2 ? ButtonVariant::Filled : ButtonVariant::Tonal);
    if (themePitchBtn_) themePitchBtn_->setVariant(mode == 3 ? ButtonVariant::Filled : ButtonVariant::Tonal);
}

void SettingsDialog::updateAccentSelection() {
    if (!controller_) return;
    const QString currentPreset = controller_->accentPreset();

    for (auto* pill : accentPills_) {
        if (pill) {
            pill->setSelected(pill->name() == currentPreset);
        }
    }
    if (customAccentPill_) {
        customAccentPill_->setSelected(currentPreset == QStringLiteral("custom"));
    }
}

void SettingsDialog::onThemeChanged() {
    auto* theme = webclip::MD3Theme::instance();
    for (auto* hdr : sectionHeaders_) {
        if (hdr) {
            hdr->setFont(theme->labelLarge());
            hdr->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; background: transparent; margin-top: 4px;").arg(theme->primary().name()));
        }
    }
    updateThemeSelection();
    updateAccentSelection();
    updateConnectionButton();
    update();
}

void SettingsDialog::open() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    }
    show();
    raise();
    setFocus();

    anim_.setFinishedCallback(nullptr);
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

    const int headerH = webclip::scale::px(56);
    headerBar_->setGeometry(0, 0, sheet_->width(), headerH);
    closeBtn_->move(sheet_->width() - webclip::scale::px(16) - closeBtn_->width(), (headerH - closeBtn_->height()) / 2);

    scrollArea_->setGeometry(0, headerH, sheet_->width(), sheet_->height() - headerH);

    if (colorPicker_) {
        colorPicker_->setGeometry(rect());
    }
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

    // 2. Sheet background matching MainWindow container
    auto* theme = webclip::MD3Theme::instance();
    const QRectF sheetRect(sheet_->geometry());

    QPainterPath sheetPath;
    sheetPath.addRoundedRect(sheetRect, webclip::scale::pxF(18.0), webclip::scale::pxF(18.0));

    p.setPen(Qt::NoPen);
    p.setBrush(theme->surface());
    p.drawPath(sheetPath);

    // 3. Header title
    p.setFont(theme->titleMedium());
    p.setPen(theme->onSurface());
    p.drawText(QPointF(webclip::scale::pxF(20), sheetRect.top() + webclip::scale::pxF(35)), webclip::I18n::instance()->tr(QStringLiteral("settings.title")));

    // 4. Header bottom divider
    const qreal divY = sheetRect.top() + webclip::scale::pxF(55);
    p.setPen(theme->outlineVariant());
    p.drawLine(0, divY, width(), divY);
}

} // namespace Ui
