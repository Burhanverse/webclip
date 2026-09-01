#include "toast_widget.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/display_scale.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace Ui {

ToastWidget::ToastWidget(QWidget* parent)
    : RpWidget(parent) {
    setFont(webclip::MD3Theme::instance()->bodyMedium());
    setFixedHeight(webclip::scale::px(40));
    hide();

    hideTimer_.setSingleShot(true);
    connect(&hideTimer_, &QTimer::timeout, this, [this] {
        hideAnimated();
    });
}

ToastWidget::~ToastWidget() = default;

void ToastWidget::showMessage(const QString& message, bool isError) {
    message_ = message;
    isError_ = isError;

    const QFontMetrics fm(font());
    const int textW = fm.horizontalAdvance(message_);
    const int maxW = parentWidget() ? (parentWidget()->width() - webclip::scale::px(48)) : webclip::scale::px(320);
    const int w = std::min(maxW, textW + webclip::scale::px(32));

    resize(w, webclip::scale::px(40));
    if (parentWidget()) {
        const int x = (parentWidget()->width() - w) / 2;
        const int y = parentWidget()->height() - webclip::scale::px(40) - webclip::scale::px(24);
        move(x, y);
    }

    show();
    raise();

    hideTimer_.stop();
    anim_.start(
        [this](double progress) {
            opacity_ = progress;
            slideOffset_ = webclip::scale::pxF(16.0) * (1.0 - progress);
            update();
        },
        opacity_,
        1.0,
        220,
        anim::easeOutCubic
    );
    anim_.setFinishedCallback([this] {
        hideTimer_.start(3000);
    });
}

void ToastWidget::hideAnimated() {
    anim_.start(
        [this](double progress) {
            opacity_ = progress;
            slideOffset_ = webclip::scale::pxF(16.0) * (1.0 - progress);
            update();
        },
        opacity_,
        0.0,
        200,
        anim::easeOutCubic
    );
    anim_.setFinishedCallback([this] {
        hide();
    });
}

void ToastWidget::paintEvent(QPaintEvent* /*e*/) {
    if (opacity_ <= 0.0) return;

    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    ScopedPainterOpacity op(p, opacity_);

    auto* theme = webclip::MD3Theme::instance();
    const QColor bgCol = isError_ ? theme->errorContainer() : theme->surfaceContainerHighest();
    const QColor textCol = isError_ ? theme->onErrorContainer() : theme->onSurface();

    const QRectF pillRect(0, slideOffset_, width(), height() - slideOffset_);
    p.setPen(Qt::NoPen);
    p.setBrush(bgCol);
    p.drawRoundedRect(pillRect, webclip::scale::pxF(20.0), webclip::scale::pxF(20.0));

    p.setFont(font());
    p.setPen(textCol);
    const QFontMetrics fm(font());
    const QString elidedText = fm.elidedText(message_, Qt::ElideRight, width() - webclip::scale::px(32));
    p.drawText(pillRect, Qt::AlignCenter, elidedText);
}

} // namespace Ui
