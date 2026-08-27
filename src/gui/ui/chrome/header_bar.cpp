#include "header_bar.hpp"
#include "../basic/painter_helpers.hpp"
#include "../md3/icon_loader.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../controllers/webclip_controller.hpp"

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QWindow>

namespace Ui {

HeaderBar::HeaderBar(QWidget* parent, webclip::WebClipController* controller)
    : RpWidget(parent)
    , controller_(controller) {
    setFixedHeight(58);

    syncBtn_ = new Md3IconButton(this, QStringLiteral("sync"), 34, 18);
    themeBtn_ = new Md3IconButton(this, QStringLiteral("dark_mode"), 34, 18);
    settingsBtn_ = new Md3IconButton(this, QStringLiteral("settings"), 34, 18);

    syncBtn_->addClickHandler([this] {
        if (controller_) controller_->toggleConnection();
    });

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

    if (controller_->connected()) {
        syncBtn_->setIconName(QStringLiteral("sync"));
        syncBtn_->setIconColor(theme->primary());
    } else {
        syncBtn_->setIconName(QStringLiteral("link_off"));
        syncBtn_->setIconColor(theme->onSurfaceVariant());
    }

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

void HeaderBar::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void HeaderBar::updateLayout() {
    const int btnSize = 34;
    const int spacing = 4;
    const int rightMargin = 12;
    const int y = (height() - btnSize) / 2;

    int rightX = width() - rightMargin - btnSize;
    settingsBtn_->setGeometry(rightX, y, btnSize, btnSize);
    rightX -= (btnSize + spacing);
    themeBtn_->setGeometry(rightX, y, btnSize, btnSize);
    rightX -= (btnSize + spacing);
    syncBtn_->setGeometry(rightX, y, btnSize, btnSize);
}

void HeaderBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (settingsBtn_->geometry().contains(e->pos()) ||
            themeBtn_->geometry().contains(e->pos()) ||
            syncBtn_->geometry().contains(e->pos())) {
            e->accept();
            return;
        }

        const QRect connArea(16, 6, 200, 46);
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

    const QRectF avatarRect(16, 10, 38, 38);
    IconLoader::paint(p, QStringLiteral("webclip"), avatarRect);

    const QRectF dotRect(44, 38, 10, 10);
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
        p.setPen(QPen(theme->surface(), 1.5));
        p.setBrush(dotColor);
        p.drawEllipse(dotRect);
    } else {
        p.setPen(QPen(theme->surface(), 1.5));
        p.setBrush(dotColor);
        p.drawEllipse(dotRect);
    }

    p.setFont(theme->titleSmall());
    p.setPen(theme->onSurface());
    p.drawText(QPointF(66, 24), webclip::I18n::instance()->tr(QStringLiteral("app.header_title")));

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
    p.drawText(QPointF(66, 42), statusText);

    p.setPen(theme->outlineVariant());
    p.drawLine(0, height() - 1, width(), height() - 1);
}

} // namespace Ui
