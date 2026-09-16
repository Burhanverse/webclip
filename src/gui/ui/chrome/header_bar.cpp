#include "header_bar.hpp"
#include "../basic/painter_helpers.hpp"
#include "../md3/icon_loader.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../util/display_scale.hpp"
#include "../../controllers/webclip_controller.hpp"

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QTouchEvent>
#include <QtGui/QWindow>

namespace Ui {

HeaderBar::HeaderBar(QWidget* parent, webclip::WebClipController* controller)
    : RpWidget(parent)
    , controller_(controller) {
    setFixedHeight(webclip::scale::px(58));

    themeBtn_ = new Md3IconButton(this, QStringLiteral("dark_mode"), webclip::scale::px(34), webclip::scale::px(18));
    settingsBtn_ = new Md3IconButton(this, QStringLiteral("settings"), webclip::scale::px(34), webclip::scale::px(18));


    themeBtn_->addClickHandler([this] {
        if (controller_) {
            const int nextMode = (controller_->themeMode() + 1) % 4;
            controller_->setThemeMode(nextMode);
            updateButtons();
        }
    });

    settingsBtn_->addClickHandler([this] {
        emit openSettingsRequested();
    });

    updateLayout();

    if (controller_) {
        setController(controller_);
    }
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::setController(webclip::WebClipController* controller) {
    controller_ = controller;
    if (!controller_) return;

    connect(controller_, &webclip::WebClipController::connectedChanged, this, [this] {
        updateButtons();
        update();
    });
    connect(controller_, &webclip::WebClipController::connectingChanged, this, [this] {
        if (controller_->connecting()) {
            pulseAnim_.start(
                [this](double v) {
                    pulseOpacity_ = v;
                    update();
                },
                0.3,
                1.0,
                500,
                anim::sineInOut
            );
        } else {
            pulseAnim_.stop();
            pulseOpacity_ = 1.0;
        }
        updateButtons();
        update();
    });
    connect(controller_, &webclip::WebClipController::themeModeChanged, this, [this] {
        updateButtons();
        update();
    });

    updateButtons();
    update();
}

void HeaderBar::updateButtons() {
    if (!controller_) return;
    auto* theme = webclip::MD3Theme::instance();

    switch (controller_->themeMode()) {
    case 0:
        themeBtn_->setIconName(QStringLiteral("sync"));
        break;
    case 1:
        themeBtn_->setIconName(QStringLiteral("light_mode"));
        break;
    case 2:
        themeBtn_->setIconName(QStringLiteral("dark_mode"));
        break;
    case 3:
        themeBtn_->setIconName(QStringLiteral("moon"));
        break;
    default:
        themeBtn_->setIconName(QStringLiteral("dark_mode"));
        break;
    }
    themeBtn_->setIconColor(theme->onSurfaceVariant());
    settingsBtn_->setIconColor(theme->onSurfaceVariant());
}

void HeaderBar::touchEvent(QTouchEvent* e) {
    if (e->type() == QEvent::TouchBegin && e->points().size() > 0) {
        const QPointF pt = e->points().first().position();
        const int64_t now = QDateTime::currentMSecsSinceEpoch();
        const bool isSameArea = (pt - lastTouchPoint_).manhattanLength() < kDoubleTapDist;
        const bool isQuick = (now - lastTouchMs_ < kDoubleTapMs);
        lastTouchPoint_ = pt.toPoint();
        lastTouchMs_ = now;

        if (isSameArea && isQuick) {
            // Double-tap on empty area -> minimize
            if (!themeBtn_->geometry().contains(pt.toPoint()) &&
                !settingsBtn_->geometry().contains(pt.toPoint())) {
                const QRect connArea(webclip::scale::px(16), webclip::scale::px(6), webclip::scale::px(200), webclip::scale::px(46));
                if (!connArea.contains(pt.toPoint())) {
                    if (window()) {
                        window()->showMinimized();
                    }
                    e->accept();
                    return;
                }
            }
        }
    }
    e->ignore();
}

void HeaderBar::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (!themeBtn_->geometry().contains(e->pos()) &&
            !settingsBtn_->geometry().contains(e->pos())) {
            const QRect connArea(webclip::scale::px(16), webclip::scale::px(6), webclip::scale::px(200), webclip::scale::px(46));
            if (!connArea.contains(e->pos())) {
                if (window()) {
                    window()->showMinimized();
                }
                e->accept();
                return;
            }
        }
    }
    RpWidget::mouseDoubleClickEvent(e);
}

void HeaderBar::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void HeaderBar::updateLayout() {
    const int btnSize = webclip::scale::px(34);
    const int spacing = webclip::scale::px(4);
    const int rightMargin = webclip::scale::px(12);
    const int y = (height() - btnSize) / 2;

    int rightX = width() - rightMargin - btnSize;
    settingsBtn_->setGeometry(rightX, y, btnSize, btnSize);
    rightX -= (btnSize + spacing);
    themeBtn_->setGeometry(rightX, y, btnSize, btnSize);
    rightX -= (btnSize + spacing);
}

void HeaderBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (settingsBtn_->geometry().contains(e->pos()) ||
            themeBtn_->geometry().contains(e->pos())) {
            e->accept();
            return;
        }

        const QRect connArea(webclip::scale::px(16), webclip::scale::px(6), webclip::scale::px(200), webclip::scale::px(46));
        if (connArea.contains(e->pos()) && controller_) {
            e->accept();
            controller_->toggleConnection();
            return;
        }

        if (window() && window()->windowHandle()) {
            e->accept();
            window()->windowHandle()->startSystemMove();
            return;
        }
    }
    RpWidget::mousePressEvent(e);
}

void HeaderBar::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const QRectF avatarRect(webclip::scale::pxF(16), webclip::scale::pxF(10), webclip::scale::pxF(38), webclip::scale::pxF(38));
    IconLoader::paint(p, QStringLiteral("webclip"), avatarRect);

    const QRectF dotRect(webclip::scale::pxF(44), webclip::scale::pxF(38), webclip::scale::pxF(10), webclip::scale::pxF(10));
    QColor dotColor;
    if (controller_ && controller_->connected()) {
        dotColor = QColor(QStringLiteral("#4CAF50"));
    } else if (controller_ && controller_->connecting()) {
        dotColor = QColor(QStringLiteral("#FF9800"));
    } else {
        dotColor = QColor(QStringLiteral("#F44336"));
    }

    if (controller_ && controller_->connecting()) {
        ScopedPainterOpacity op(p, pulseOpacity_);
        p.setPen(QPen(theme->surface(), webclip::scale::pxF(1.5)));
        p.setBrush(dotColor);
        p.drawEllipse(dotRect);
    } else {
        p.setPen(QPen(theme->surface(), webclip::scale::pxF(1.5)));
        p.setBrush(dotColor);
        p.drawEllipse(dotRect);
    }

    p.setFont(theme->titleSmall());
    p.setPen(theme->onSurface());
    p.drawText(QPointF(webclip::scale::pxF(66), webclip::scale::pxF(24)), webclip::I18n::instance()->tr(QStringLiteral("app.header_title")));

    QString statusText;
    QColor statusColor;
    if (controller_ && controller_->connected()) {
        statusText = webclip::I18n::instance()->tr(QStringLiteral("app.status_connected"));
        statusColor = QColor(QStringLiteral("#4CAF50"));
    } else if (controller_ && controller_->connecting()) {
        statusText = webclip::I18n::instance()->tr(QStringLiteral("app.status_connecting"));
        statusColor = theme->onSurfaceVariant();
    } else {
        statusText = webclip::I18n::instance()->tr(QStringLiteral("app.status_offline"));
        statusColor = theme->onSurfaceVariant();
    }

    p.setFont(theme->labelSmall());
    p.setPen(statusColor);
    p.drawText(QPointF(webclip::scale::pxF(66), webclip::scale::pxF(42)), statusText);

    p.setPen(theme->outlineVariant());
    p.drawLine(0, height() - 1, width(), height() - 1);
}

} // namespace Ui
