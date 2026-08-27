#include "color_picker_dialog.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace Ui {

class SatValArea : public QWidget {
    Q_OBJECT
public:
    explicit SatValArea(QWidget* parent = nullptr)
        : QWidget(parent) {
        setCursor(Qt::CrossCursor);
        setMouseTracking(true);
    }

    void setHue(double hue) {
        hue_ = hue;
        update();
    }

    void setSatVal(double sat, double val) {
        sat_ = std::clamp(sat, 0.0, 1.0);
        val_ = std::clamp(val, 0.0, 1.0);
        update();
    }

signals:
    void satValChanged(double sat, double val);

protected:
    void paintEvent(QPaintEvent* /*e*/) override {
        QPainter p(this);
        PainterHighQualityEnabler hq(p);

        const QRectF r = rect();
        QPainterPath clip;
        clip.addRoundedRect(r, 12.0, 12.0);
        p.setClipPath(clip);

        const QColor pureHue = QColor::fromHsvF(hue_ / 360.0, 1.0, 1.0);
        p.fillRect(rect(), pureHue);

        QLinearGradient hGrad(r.left(), r.top(), r.right(), r.top());
        hGrad.setColorAt(0.0, QColor(255, 255, 255, 255));
        hGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(rect(), hGrad);

        QLinearGradient vGrad(r.left(), r.top(), r.left(), r.bottom());
        vGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
        vGrad.setColorAt(1.0, QColor(0, 0, 0, 255));
        p.fillRect(rect(), vGrad);

        p.setClipping(false);
        const double hX = std::clamp(r.left() + sat_ * r.width(), r.left() + 8.0, r.right() - 8.0);
        const double hY = std::clamp(r.top() + (1.0 - val_) * r.height(), r.top() + 8.0, r.bottom() - 8.0);
        const QPointF center(hX, hY);

        p.setPen(QPen(QColor(0, 0, 0, 140), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, 7.5, 7.5);
        p.setPen(QPen(Qt::white, 2.0));
        p.drawEllipse(center, 7.0, 7.0);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = true;
            updatePos(e->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_) {
            updatePos(e->position());
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = false;
        }
    }

private:
    void updatePos(const QPointF& pos) {
        sat_ = std::clamp(pos.x() / double(width()), 0.0, 1.0);
        val_ = std::clamp(1.0 - (pos.y() / double(height())), 0.0, 1.0);
        emit satValChanged(sat_, val_);
        update();
    }

    double hue_ = 0.0;
    double sat_ = 1.0;
    double val_ = 1.0;
    bool dragging_ = false;
};

class HueBar : public QWidget {
    Q_OBJECT
public:
    explicit HueBar(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedHeight(14);
        setCursor(Qt::PointingHandCursor);
    }

    void setHue(double hue) {
        hue_ = std::clamp(hue, 0.0, 360.0);
        update();
    }

signals:
    void hueChanged(double hue);

protected:
    void paintEvent(QPaintEvent* /*e*/) override {
        QPainter p(this);
        PainterHighQualityEnabler hq(p);

        const QRectF r(0.0, 1.0, width(), height() - 2.0);
        const double radius = r.height() / 2.0;

        QLinearGradient grad(r.left(), r.center().y(), r.right(), r.center().y());
        grad.setColorAt(0.00, QColor(255, 0, 0));
        grad.setColorAt(0.17, QColor(255, 255, 0));
        grad.setColorAt(0.33, QColor(0, 255, 0));
        grad.setColorAt(0.50, QColor(0, 255, 255));
        grad.setColorAt(0.67, QColor(0, 0, 255));
        grad.setColorAt(0.83, QColor(255, 0, 255));
        grad.setColorAt(1.00, QColor(255, 0, 0));

        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRoundedRect(r, radius, radius);

        const double tX = std::clamp(r.left() + (hue_ / 360.0) * r.width(), r.left() + 7.0, r.right() - 7.0);
        const QPointF center(tX, r.center().y());

        p.setPen(QPen(Qt::white, 2.5));
        p.setBrush(QColor::fromHsvF(hue_ / 360.0, 1.0, 1.0));
        p.drawEllipse(center, 6.5, 6.5);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = true;
            updatePos(e->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_) {
            updatePos(e->position());
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = false;
        }
    }

private:
    void updatePos(const QPointF& pos) {
        hue_ = std::clamp(pos.x() / double(width()), 0.0, 0.9999) * 360.0;
        emit hueChanged(hue_);
        update();
    }

    double hue_ = 0.0;
    bool dragging_ = false;
};

// -----------------------------------------------------------------------------
// AlphaBar: Opacity Slider with Checkerboard Background
// -----------------------------------------------------------------------------
class AlphaBar : public QWidget {
    Q_OBJECT
public:
    explicit AlphaBar(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedHeight(14);
        setCursor(Qt::PointingHandCursor);
    }

    void setColorAndAlpha(const QColor& c, double alpha) {
        color_ = c;
        alpha_ = std::clamp(alpha, 0.0, 1.0);
        update();
    }

signals:
    void alphaChanged(double alpha);

protected:
    void paintEvent(QPaintEvent* /*e*/) override {
        QPainter p(this);
        PainterHighQualityEnabler hq(p);

        const QRectF r(0.0, 1.0, width(), height() - 2.0);
        const double radius = r.height() / 2.0;

        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);

        // Checkerboard
        static const QPixmap checker = []() {
            QPixmap pm(12, 12);
            QPainter cp(&pm);
            cp.fillRect(0, 0, 6, 6, QColor(200, 200, 200));
            cp.fillRect(6, 6, 6, 6, QColor(200, 200, 200));
            cp.fillRect(6, 0, 6, 6, QColor(255, 255, 255));
            cp.fillRect(0, 6, 6, 6, QColor(255, 255, 255));
            return pm;
        }();
        p.drawTiledPixmap(r, checker);

        // Alpha gradient
        QLinearGradient grad(r.left(), r.center().y(), r.right(), r.center().y());
        QColor trans = color_;
        trans.setAlpha(0);
        QColor opaque = color_;
        opaque.setAlpha(255);
        grad.setColorAt(0.0, trans);
        grad.setColorAt(1.0, opaque);

        p.fillRect(r, grad);
        p.setClipping(false);

        // Circular thumb
        const double tX = std::clamp(r.left() + alpha_ * r.width(), r.left() + 7.0, r.right() - 7.0);
        const QPointF center(tX, r.center().y());

        p.setPen(QPen(Qt::white, 2.5));
        QColor fill = color_;
        fill.setAlphaF(alpha_);
        p.setBrush(fill);
        p.drawEllipse(center, 6.5, 6.5);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = true;
            updatePos(e->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_) {
            updatePos(e->position());
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = false;
        }
    }

private:
    void updatePos(const QPointF& pos) {
        alpha_ = std::clamp(pos.x() / double(width()), 0.0, 1.0);
        emit alphaChanged(alpha_);
        update();
    }

    QColor color_ = Qt::black;
    double alpha_ = 1.0;
    bool dragging_ = false;
};

// -----------------------------------------------------------------------------
// SwatchesRow: Circular Preset / Saved Color Dots
// -----------------------------------------------------------------------------
class SwatchesRow : public QWidget {
    Q_OBJECT
public:
    explicit SwatchesRow(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedHeight(26);
        setCursor(Qt::PointingHandCursor);
    }

    void setColors(const std::vector<QColor>& colors) {
        colors_ = colors;
        update();
    }

    void setSelectedColor(const QColor& c) {
        selectedColor_ = c;
        update();
    }

signals:
    void colorClicked(const QColor& c);

protected:
    void paintEvent(QPaintEvent* /*e*/) override {
        if (colors_.empty()) return;

        QPainter p(this);
        PainterHighQualityEnabler hq(p);

        const int count = static_cast<int>(colors_.size());
        const double dotD = 22.0;
        const double totalDotsW = count * dotD;
        const double gap = count > 1 ? (width() - totalDotsW) / double(count - 1) : 0.0;

        for (int i = 0; i < count; ++i) {
            const double cx = i * (dotD + gap) + dotD / 2.0;
            const double cy = height() / 2.0;
            const QPointF center(cx, cy);

            const bool isSelected = (colors_[i].name(QColor::HexRgb).toUpper() == selectedColor_.name(QColor::HexRgb).toUpper());

            if (isSelected) {
                p.setPen(QPen(webclip::MD3Theme::instance()->primary(), 2.0));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(center, dotD / 2.0 + 2.0, dotD / 2.0 + 2.0);
            }

            p.setPen(QPen(QColor(0, 0, 0, 30), 1.0));
            p.setBrush(colors_[i]);
            p.drawEllipse(center, dotD / 2.0, dotD / 2.0);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (colors_.empty() || e->button() != Qt::LeftButton) return;

        const int count = static_cast<int>(colors_.size());
        const double dotD = 22.0;
        const double gap = count > 1 ? (width() - count * dotD) / double(count - 1) : 0.0;

        const double clickX = e->position().x();
        for (int i = 0; i < count; ++i) {
            const double dotLeft = i * (dotD + gap);
            if (clickX >= dotLeft - 4.0 && clickX <= dotLeft + dotD + 4.0) {
                emit colorClicked(colors_[i]);
                return;
            }
        }
    }

private:
    std::vector<QColor> colors_;
    QColor selectedColor_;
};

// -----------------------------------------------------------------------------
// FormatPill: "Hex ⌵" Dropdown Button with Vector Chevron
// -----------------------------------------------------------------------------
class FormatPill : public QWidget {
    Q_OBJECT
public:
    explicit FormatPill(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedSize(68, 34);
    }

protected:
    void paintEvent(QPaintEvent* /*e*/) override {
        QPainter p(this);
        PainterHighQualityEnabler hq(p);
        auto* theme = webclip::MD3Theme::instance();

        const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
        p.setPen(QPen(theme->outlineVariant(), 1.0));
        p.setBrush(theme->surface());
        p.drawRoundedRect(r, 8.0, 8.0);

        p.setFont(theme->bodyMedium());
        p.setPen(theme->onSurface());
        p.drawText(QRectF(10, 0, 32, height()), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Hex"));

        // Draw crisp vector down chevron
        p.setPen(QPen(theme->onSurfaceVariant(), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const double cx = 50.0;
        const double cy = height() / 2.0 - 1.0;
        p.drawLine(QPointF(cx - 3.5, cy - 2.0), QPointF(cx, cy + 2.0));
        p.drawLine(QPointF(cx, cy + 2.0), QPointF(cx + 3.5, cy - 2.0));
    }
};

// -----------------------------------------------------------------------------
// ColorPickerDialog Main Implementation
// -----------------------------------------------------------------------------
ColorPickerDialog::ColorPickerDialog(QWidget* parent)
    : RpWidget(parent) {
    hide();
    setFocusPolicy(Qt::StrongFocus);

    savedColors_ = {
        QColor(QStringLiteral("#10B981")), // Emerald
        QColor(QStringLiteral("#3B82F6")), // Blue
        QColor(QStringLiteral("#6366F1")), // Indigo
        QColor(QStringLiteral("#8B5CF6")), // Violet
        QColor(QStringLiteral("#D946EF")), // Fuchsia
        QColor(QStringLiteral("#EC4899")), // Pink
        QColor(QStringLiteral("#EF4444")), // Red
        QColor(QStringLiteral("#F97316")), // Orange
        QColor(QStringLiteral("#7C3AED"))  // Purple
    };

    card_ = new QWidget(this);

    // 1. 2D Saturation / Value Area
    satValArea_ = new SatValArea(card_);
    connect(satValArea_, &SatValArea::satValChanged, this, [this](double s, double v) {
        if (!updating_) {
            sat_ = s;
            val_ = v;
            updateFromHsv();
        }
    });

    // 2. Rainbow Hue Slider
    hueBar_ = new HueBar(card_);
    connect(hueBar_, &HueBar::hueChanged, this, [this](double h) {
        if (!updating_) {
            hue_ = h;
            satValArea_->setHue(hue_);
            updateFromHsv();
        }
    });

    // 3. Alpha Opacity Slider
    alphaBar_ = new AlphaBar(card_);
    connect(alphaBar_, &AlphaBar::alphaChanged, this, [this](double a) {
        if (!updating_) {
            alpha_ = a;
            updateFromHsv();
        }
    });

    // 4. Input Row
    formatPill_ = new FormatPill(card_);

    // Hex Box + Opacity Box
    hexBox_ = new QWidget(card_);
    hexInput_ = new QLineEdit(hexBox_);
    hexInput_->setFrame(false);
    hexInput_->setMaxLength(7);

    opacityLabel_ = new QLabel(QStringLiteral("100%"), hexBox_);
    opacityLabel_->setAlignment(Qt::AlignCenter);

    connect(hexInput_, &QLineEdit::textChanged, this, [this](const QString& txt) {
        if (!updating_) updateFromHex(txt);
    });

    // 5. Saved Swatches Section
    savedHeader_ = new QWidget(card_);
    savedTitle_ = new QLabel(QStringLiteral("Saved"), savedHeader_);
    addBtn_ = new QPushButton(QStringLiteral("+ Add"), savedHeader_);
    addBtn_->setFlat(true);
    addBtn_->setCursor(Qt::PointingHandCursor);
    connect(addBtn_, &QPushButton::clicked, this, [this] {
        if (savedColors_.size() >= 9) {
            savedColors_.pop_back();
        }
        savedColors_.insert(savedColors_.begin(), currentColor_);
        swatchesRow_->setColors(savedColors_);
        swatchesRow_->setSelectedColor(currentColor_);
    });

    swatchesRow_ = new SwatchesRow(card_);
    swatchesRow_->setColors(savedColors_);
    connect(swatchesRow_, &SwatchesRow::colorClicked, this, [this](const QColor& c) {
        openWithColor(c);
    });

    // 6. Action Buttons
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
    updating_ = true;
    currentColor_ = initialColor;

    hue_ = std::max(0, initialColor.hsvHue());
    if (initialColor.hsvHue() == -1) hue_ = 0.0;
    sat_ = initialColor.hsvSaturationF();
    val_ = initialColor.valueF();
    alpha_ = initialColor.alphaF();

    satValArea_->setHue(hue_);
    satValArea_->setSatVal(sat_, val_);
    hueBar_->setHue(hue_);
    alphaBar_->setColorAndAlpha(currentColor_, alpha_);

    hexInput_->setText(currentColor_.name(QColor::HexRgb).toUpper());
    opacityLabel_->setText(QString::number(static_cast<int>(alpha_ * 100)) + QStringLiteral("%"));

    swatchesRow_->setSelectedColor(currentColor_);
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

void ColorPickerDialog::updateFromHsv() {
    updating_ = true;
    currentColor_ = QColor::fromHsvF(hue_ / 360.0, sat_, val_, alpha_);
    alphaBar_->setColorAndAlpha(currentColor_, alpha_);
    hexInput_->setText(currentColor_.name(QColor::HexRgb).toUpper());
    opacityLabel_->setText(QString::number(static_cast<int>(alpha_ * 100)) + QStringLiteral("%"));
    swatchesRow_->setSelectedColor(currentColor_);
    updating_ = false;
    update();
}

void ColorPickerDialog::updateFromHex(const QString& hex) {
    QColor c(hex);
    if (c.isValid()) {
        updating_ = true;
        currentColor_ = c;
        currentColor_.setAlphaF(alpha_);
        hue_ = std::max(0, c.hsvHue());
        if (c.hsvHue() == -1) hue_ = 0.0;
        sat_ = c.hsvSaturationF();
        val_ = c.valueF();

        satValArea_->setHue(hue_);
        satValArea_->setSatVal(sat_, val_);
        hueBar_->setHue(hue_);
        alphaBar_->setColorAndAlpha(currentColor_, alpha_);
        swatchesRow_->setSelectedColor(currentColor_);
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
    const int cardW = 326;
    const int cardH = 430;
    const int cardX = (width() - cardW) / 2;
    const int cardY = (height() - cardH) / 2;
    card_->setGeometry(cardX, cardY, cardW, cardH);

    const int contentX = 16;
    const int contentW = cardW - 32;
    int curY = 16;

    // 1. 2D Area
    satValArea_->setGeometry(contentX, curY, contentW, 160);
    curY += 160 + 12;

    // 2. Hue Bar
    hueBar_->setGeometry(contentX, curY, contentW, 14);
    curY += 14 + 8;

    // 3. Alpha Bar
    alphaBar_->setGeometry(contentX, curY, contentW, 14);
    curY += 14 + 14;

    // 4. Input Row
    const int pillW = 68;
    const int rowH = 34;
    formatPill_->setGeometry(contentX, curY, pillW, rowH);

    const int hexBoxX = contentX + pillW + 8;
    const int hexBoxW = contentW - pillW - 8;
    hexBox_->setGeometry(hexBoxX, curY, hexBoxW, rowH);
    hexInput_->setGeometry(32, 2, hexBoxW - 80, rowH - 4);
    opacityLabel_->setGeometry(hexBoxW - 46, 2, 42, rowH - 4);
    curY += rowH + 12;

    // 5. Saved Header
    savedHeader_->setGeometry(contentX, curY, contentW, 18);
    savedTitle_->setGeometry(0, 0, 100, 18);
    addBtn_->setGeometry(contentW - 60, 0, 60, 18);
    curY += 18 + 8;

    // 6. Swatches Row
    swatchesRow_->setGeometry(contentX, curY, contentW, 26);
    curY += 26 + 14;

    // 7. Buttons
    cancelBtn_->setGeometry(cardW - 16 - 80 - 8 - 90, curY, 80, 36);
    selectBtn_->setGeometry(cardW - 16 - 90, curY, 90, 36);
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
    p.fillRect(rect(), QColor(0, 0, 0, 125));

    // 2. Card background
    auto* theme = webclip::MD3Theme::instance();
    const QRectF cRect(card_->geometry());

    p.setPen(QPen(theme->outlineVariant(), 1.0));
    p.setBrush(theme->surfaceContainer());
    p.drawRoundedRect(cRect, 20.0, 20.0);

    // 3. Hex Box styling
    const QRectF hbRect(card_->x() + hexBox_->x(), card_->y() + hexBox_->y(), hexBox_->width(), hexBox_->height());
    p.setPen(QPen(theme->outlineVariant(), 1.0));
    p.setBrush(theme->surface());
    p.drawRoundedRect(hbRect, 8.0, 8.0);

    // 5. Circular preview swatch inside hexBox
    const QPointF swatchCenter(hbRect.left() + 18.0, hbRect.center().y());
    p.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
    p.setBrush(currentColor_);
    p.drawEllipse(swatchCenter, 8.0, 8.0);

    // Subtle divider before opacity
    p.setPen(theme->outlineVariant());
    p.drawLine(QPointF(hbRect.right() - 48.0, hbRect.top() + 6.0), QPointF(hbRect.right() - 48.0, hbRect.bottom() - 6.0));

    hexInput_->setStyleSheet(QStringLiteral("QLineEdit { color: %1; background: transparent; border: none; font-family: monospace; font-size: 13px; }").arg(theme->onSurface().name()));
    opacityLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent; font-size: 12px;").arg(theme->onSurfaceVariant().name()));

    savedTitle_->setStyleSheet(QStringLiteral("color: %1; background: transparent; font-weight: 600; font-size: 13px;").arg(theme->onSurface().name()));
    addBtn_->setStyleSheet(QStringLiteral("QPushButton { color: %1; background: transparent; border: none; font-weight: 600; font-size: 13px; text-align: right; padding: 0px; }").arg(theme->primary().name()));
}

} // namespace Ui

#include "color_picker_dialog.moc"
