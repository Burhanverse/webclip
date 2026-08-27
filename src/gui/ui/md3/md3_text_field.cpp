#include "md3_text_field.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"

#include <QtGui/QPainter>

namespace Ui {

Md3TextField::Md3TextField(QWidget* parent, const QString& label, const QString& placeholder)
    : RpWidget(parent)
    , label_(label) {
    auto* theme = webclip::MD3Theme::instance();
    setFont(theme->bodyMedium());

    lineEdit_ = new QLineEdit(this);
    lineEdit_->setFrame(false);
    lineEdit_->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; padding: 0px; }"));
    lineEdit_->setPlaceholderText(placeholder);
    lineEdit_->setFont(theme->bodyMedium());

    connect(lineEdit_, &QLineEdit::textChanged, this, [this](const QString& txt) {
        emit textChanged(txt);
        update();
    });
    connect(lineEdit_, &QLineEdit::returnPressed, this, &Md3TextField::returnPressed);

    setFixedHeight(label_.isEmpty() ? 36 : 44);
}

Md3TextField::~Md3TextField() = default;

QString Md3TextField::text() const {
    return lineEdit_->text();
}

void Md3TextField::setText(const QString& text) {
    lineEdit_->setText(text);
}

void Md3TextField::setLabel(const QString& label) {
    if (label_ != label) {
        label_ = label;
        setFixedHeight(label_.isEmpty() ? 36 : 44);
        updateGeometry();
        updateLayout();
        update();
    }
}

QString Md3TextField::placeholder() const {
    return lineEdit_->placeholderText();
}

void Md3TextField::setPlaceholder(const QString& placeholder) {
    lineEdit_->setPlaceholderText(placeholder);
}

void Md3TextField::setEchoMode(QLineEdit::EchoMode mode) {
    lineEdit_->setEchoMode(mode);
}

QLineEdit::EchoMode Md3TextField::echoMode() const {
    return lineEdit_->echoMode();
}

QSize Md3TextField::sizeHint() const {
    return QSize(220, label_.isEmpty() ? 36 : 44);
}

void Md3TextField::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void Md3TextField::updateLayout() {
    const int hMargin = 12;
    const int w = width() - 2 * hMargin;

    if (label_.isEmpty()) {
        const int inputH = 24;
        lineEdit_->setGeometry(hMargin, (height() - inputH) / 2, w, inputH);
    } else {
        const int inputH = 22;
        lineEdit_->setGeometry(hMargin, height() - inputH - 4, w, inputH);
    }
}

void Md3TextField::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const QRectF boxRect(0.5, 0.5, width() - 1.0, height() - 1.0);

    // 1. Background
    p.setPen(Qt::NoPen);
    p.setBrush(theme->surfaceContainerHighest());
    p.drawRoundedRect(boxRect, 12.0, 12.0);

    // 2. Focus border
    if (lineEdit_->hasFocus()) {
        p.setPen(QPen(theme->primary(), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(boxRect, 12.0, 12.0);
    }

    // 3. Label
    if (!label_.isEmpty()) {
        p.setFont(theme->labelSmall());
        p.setPen(lineEdit_->hasFocus() ? theme->primary() : theme->onSurfaceVariant());
        p.drawText(QPointF(12, 14), label_);
    }
}

} // namespace Ui
