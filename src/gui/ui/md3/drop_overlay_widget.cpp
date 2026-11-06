#include "drop_overlay_widget.hpp"
#include "icon_loader.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/display_scale.hpp"
#include "../../util/i18n.hpp"

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>

namespace Ui {

DropOverlayWidget::DropOverlayWidget(QWidget* parent)
    : RpWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAcceptDrops(false);
    hide();
}

DropOverlayWidget::~DropOverlayWidget() = default;

void DropOverlayWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    QColor bg = theme->surface();
    bg.setAlpha(230);
    p.fillRect(rect(), bg);

    const qreal margin = webclip::scale::pxF(16.0);
    const QRectF borderRect(margin, margin, width() - 2.0 * margin, height() - 2.0 * margin);
    const qreal radius = webclip::scale::pxF(16.0);

    QPen pen(theme->primary(), webclip::scale::pxF(2.0), Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(borderRect, radius, radius);

    const int iconSize = webclip::scale::px(48);
    const int centerY = height() / 2;
    const QRectF iconRect(
        (width() - iconSize) / 2.0,
        centerY - webclip::scale::px(50),
        iconSize,
        iconSize
    );
    IconLoader::paint(p, QStringLiteral("attach_file"), iconRect, theme->primary());

    p.setFont(theme->titleMedium());
    p.setPen(theme->primary());
    const QString title = webclip::I18n::instance()->tr(QStringLiteral("chat.drop_files_title"));
    const QRectF titleRect(
        margin + webclip::scale::pxF(8.0),
        centerY + webclip::scale::px(10),
        width() - 2.0 * (margin + webclip::scale::pxF(8.0)),
        webclip::scale::px(28)
    );
    p.drawText(titleRect, Qt::AlignCenter, title);

    p.setFont(theme->bodyMedium());
    p.setPen(theme->onSurfaceVariant());
    const QString subtitle = webclip::I18n::instance()->tr(QStringLiteral("chat.drop_files_subtitle"));
    const QRectF subtitleRect(
        margin + webclip::scale::pxF(8.0),
        centerY + webclip::scale::px(38),
        width() - 2.0 * (margin + webclip::scale::pxF(8.0)),
        webclip::scale::px(24)
    );
    p.drawText(subtitleRect, Qt::AlignCenter, subtitle);
}

}

