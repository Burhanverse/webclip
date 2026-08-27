#include "color_picker_dialog.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

namespace Ui {

ColorPickerDialog::ColorPickerDialog(QWidget* parent)
    : RpWidget(parent) {
    hide();
    setFocusPolicy(Qt::StrongFocus);

    card_ = new QWidget(this);

    hexInput_ = new Md3TextField(card_, QStringLiteral("HEX"), QStringLiteral("#RRGGBB"));
    connect(hexInput_, &Md3TextField::textChanged, this, [this](const QString& txt) {
        if (!updating_) updateFromHex(txt);
    });

    hueSlider_ = new Md3Slider(card_);
    hueSlider_->setRange(0, 359);
    connect(hueSlider_, &Md3Slider::valueChanged, this, [this](double val) {
        if (!updating_) {
            hue_ = val;
            updateFromHsl();
        }
    });

    satSlider_ = new Md3Slider(card_);
    satSlider_->setRange(0, 100);
    connect(satSlider_, &Md3Slider::valueChanged, this, [this](double val) {
        if (!updating_) {
            sat_ = val;
            updateFromHsl();
        }
    });

    lightSlider_ = new Md3Slider(card_);
    lightSlider_->setRange(0, 100);
    connect(lightSlider_, &Md3Slider::valueChanged, this, [this](double val) {
        if (!updating_) {
            light_ = val;
            updateFromHsl();
        }
    });

    cancelBtn_ = new Md3Button(card_, QStringLiteral("Cancel"), ButtonVariant::Text);
    cancelBtn_->addClickHandler([this] {
        hideAnimated();
    });

    selectBtn_ = new Md3Button(card_, QStringLiteral("Select"), ButtonVariant::Filled);
    selectBtn_->addClickHandler([this] {
        emit colorSelected(currentColor_);
        hideAnimated();
    });
}

ColorPickerDialog::~ColorPickerDialog() = default;

void ColorPickerDialog::openWithColor(const QColor& initialColor) {
    currentColor_ = initialColor;
    updating_ = true;

    hue_ = std::max(0, initialColor.hslHue());
    sat_ = std::max(0, static_cast<int>(initialColor.hslSaturationF() * 100.0));
    light_ = std::max(0, static_cast<int>(initialColor.lightnessF() * 100.0));

    hueSlider_->setValue(hue_);
    satSlider_->setValue(sat_);
    lightSlider_->setValue(light_);
    hexInput_->setText(currentColor_.name(QColor::HexRgb).toUpper());

    updating_ = false;

    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    }
    show();
    raise();
    setFocus();

    anim_.start(
        [this](double progress) {
            progress_ = progress;
            update();
        },
        progress_,
        1.0,
        200,
        anim::easeOutCubic
    );
}

void ColorPickerDialog::updateFromHsl() {
    updating_ = true;
    currentColor_ = QColor::fromHslF(hue_ / 360.0, sat_ / 100.0, light_ / 100.0);
    hexInput_->setText(currentColor_.name(QColor::HexRgb).toUpper());
    updating_ = false;
    update();
}

void ColorPickerDialog::updateFromHex(const QString& hex) {
    QColor c(hex);
    if (c.isValid()) {
        updating_ = true;
        currentColor_ = c;
        hue_ = std::max(0, c.hslHue());
        sat_ = std::max(0, static_cast<int>(c.hslSaturationF() * 100.0));
        light_ = std::max(0, static_cast<int>(c.lightnessF() * 100.0));
        hueSlider_->setValue(hue_);
        satSlider_->setValue(sat_);
        lightSlider_->setValue(light_);
        updating_ = false;
        update();
    }
}

void ColorPickerDialog::hideAnimated() {
    anim_.start(
        [this](double progress) {
            progress_ = progress;
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

void ColorPickerDialog::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void ColorPickerDialog::updateLayout() {
    const int cardW = 320;
    const int cardH = 390;
    const int cardX = (width() - cardW) / 2;
    const int cardY = (height() - cardH) / 2;
    card_->setGeometry(cardX, cardY, cardW, cardH);

    // Swatch is drawn at top of card (20..84)
    // Hex input: below swatch
    hexInput_->setGeometry(100, 24, 200, 44);

    hueSlider_->setGeometry(20, 90, 280, 36);
    satSlider_->setGeometry(20, 140, 280, 36);
    lightSlider_->setGeometry(20, 190, 280, 36);

    cancelBtn_->setGeometry(120, 330, 80, 40);
    selectBtn_->setGeometry(210, 330, 90, 40);
}

void ColorPickerDialog::mousePressEvent(QMouseEvent* e) {
    if (!card_->geometry().contains(e->pos())) {
        hideAnimated();
        return;
    }
    RpWidget::mousePressEvent(e);
}

void ColorPickerDialog::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hideAnimated();
    } else {
        RpWidget::keyPressEvent(e);
    }
}

void ColorPickerDialog::paintEvent(QPaintEvent* /*e*/) {
    if (progress_ <= 0.0) return;

    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    ScopedPainterOpacity op(p, progress_);

    // 1. Dimmed backdrop
    p.fillRect(rect(), QColor(0, 0, 0, 115));

    // 2. Card background
    auto* theme = webclip::MD3Theme::instance();
    const QRectF cRect(card_->geometry());

    p.setPen(QPen(theme->outlineVariant(), 1.0));
    p.setBrush(theme->surfaceContainer());
    p.drawRoundedRect(cRect, 24.0, 24.0);

    // 3. Color swatch
    const QRectF swatchRect(cRect.left() + 20, cRect.top() + 20, 64, 52);
    p.setPen(Qt::NoPen);
    p.setBrush(currentColor_);
    p.drawRoundedRect(swatchRect, 14.0, 14.0);

    // 4. Slider labels
    p.setFont(theme->labelSmall());
    p.setPen(theme->onSurfaceVariant());
    p.drawText(QPointF(cRect.left() + 20, cRect.top() + 86), QStringLiteral("Hue"));
    p.drawText(QPointF(cRect.left() + 20, cRect.top() + 136), QStringLiteral("Saturation"));
    p.drawText(QPointF(cRect.left() + 20, cRect.top() + 186), QStringLiteral("Lightness"));
}

} // namespace Ui
