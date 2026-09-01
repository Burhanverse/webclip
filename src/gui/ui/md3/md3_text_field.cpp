#include "md3_text_field.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/display_scale.hpp"

#include <QtGui/QPainter>
#include <QtCore/QEvent>

namespace Ui {

Md3TextField::Md3TextField(QWidget* parent, const QString& label, const QString& placeholder)
    : RpWidget(parent)
    , label_(label) {
    lineEdit_ = new QLineEdit(this);
    lineEdit_->setFrame(false);
    lineEdit_->setPlaceholderText(placeholder);
    lineEdit_->installEventFilter(this);

    updateTheme();

    connect(lineEdit_, &QLineEdit::textChanged, this, [this](const QString& txt) {
        emit textChanged(txt);
        update();
    });
    connect(lineEdit_, &QLineEdit::returnPressed, this, &Md3TextField::returnPressed);
    connect(webclip::MD3Theme::instance(), &webclip::MD3Theme::themeChanged, this, &Md3TextField::updateTheme);

    setFixedHeight(label_.isEmpty() ? webclip::scale::px(36) : webclip::scale::px(44));
}

Md3TextField::~Md3TextField() = default;

bool Md3TextField::eventFilter(QObject* obj, QEvent* e) {
    if (obj == lineEdit_ && (e->type() == QEvent::FocusIn || e->type() == QEvent::FocusOut)) {
        update();
    }
    return RpWidget::eventFilter(obj, e);
}

void Md3TextField::updateTheme() {
    auto* theme = webclip::MD3Theme::instance();
    setFont(theme->bodyMedium());
    if (lineEdit_) {
        lineEdit_->setFont(theme->bodyMedium());
        lineEdit_->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0px;"
            "  color: %1;"
            "  selection-background-color: %2;"
            "  selection-color: %3;"
            "}"
        ).arg(theme->onSurface().name(),
              theme->primary().name(),
              theme->onPrimary().name()));

        QPalette pal = lineEdit_->palette();
        pal.setColor(QPalette::Text, theme->onSurface());
        pal.setColor(QPalette::PlaceholderText, theme->onSurfaceVariant());
        pal.setColor(QPalette::Highlight, theme->primary());
        pal.setColor(QPalette::HighlightedText, theme->onPrimary());
        lineEdit_->setPalette(pal);
    }
    update();
}

QString Md3TextField::text() const {
    return lineEdit_->text();
}

void Md3TextField::setText(const QString& text) {
    lineEdit_->setText(text);
}

void Md3TextField::setLabel(const QString& label) {
    if (label_ != label) {
        label_ = label;
        setFixedHeight(label_.isEmpty() ? webclip::scale::px(36) : webclip::scale::px(44));
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
    return QSize(webclip::scale::px(220), label_.isEmpty() ? webclip::scale::px(36) : webclip::scale::px(44));
}

void Md3TextField::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void Md3TextField::updateLayout() {
    const int hMargin = webclip::scale::px(12);
    const int w = width() - 2 * hMargin;

    if (label_.isEmpty()) {
        const int inputH = webclip::scale::px(24);
        lineEdit_->setGeometry(hMargin, (height() - inputH) / 2, w, inputH);
    } else {
        const int inputH = webclip::scale::px(22);
        lineEdit_->setGeometry(hMargin, height() - inputH - webclip::scale::px(4), w, inputH);
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
    p.drawRoundedRect(boxRect, webclip::scale::pxF(12.0), webclip::scale::pxF(12.0));

    // 2. Focus border
    if (lineEdit_->hasFocus()) {
        p.setPen(QPen(theme->primary(), webclip::scale::pxF(1.5)));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(boxRect, webclip::scale::pxF(12.0), webclip::scale::pxF(12.0));
    }

    // 3. Label
    if (!label_.isEmpty()) {
        p.setFont(theme->labelSmall());
        p.setPen(lineEdit_->hasFocus() ? theme->primary() : theme->onSurfaceVariant());
        p.drawText(QPointF(webclip::scale::pxF(12.0), webclip::scale::pxF(14.0)), label_);
    }
}

} // namespace Ui
