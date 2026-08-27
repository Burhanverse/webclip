#include "input_dock.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../controllers/webclip_controller.hpp"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

namespace Ui {

InputDock::InputDock(QWidget* parent, webclip::WebClipController* controller)
    : RpWidget(parent)
    , controller_(controller) {
    setFixedHeight(64);

    attachBtn_ = new Md3IconButton(this, QStringLiteral("image"), 44, 22);
    attachBtn_->setIconColor(webclip::MD3Theme::instance()->primary());
    attachBtn_->addClickHandler([this] {
        emit attachImageRequested();
    });

    pasteBtn_ = new Md3IconButton(this, QStringLiteral("paste"), 36, 18);
    pasteBtn_->setIconColor(webclip::MD3Theme::instance()->onSurfaceVariant());
    pasteBtn_->addClickHandler([this] {
        if (controller_ && controller_->pushCurrentClipboard()) {
            lineEdit_->clear();
        } else {
            lineEdit_->paste();
        }
    });

    sendBtn_ = new Md3IconButton(this, QStringLiteral("send"), 48, 20);
    sendBtn_->addClickHandler([this] {
        const QString text = lineEdit_->text().trimmed();
        if (!text.isEmpty()) {
            emit sendRequested(text);
            lineEdit_->clear();
        }
    });

    lineEdit_ = new QLineEdit(this);
    lineEdit_->setFrame(false);
    lineEdit_->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; padding: 0px; }"));
    lineEdit_->setPlaceholderText(webclip::I18n::instance()->tr(QStringLiteral("chat.message_placeholder")));
    lineEdit_->setFont(webclip::MD3Theme::instance()->bodyLarge());

    connect(lineEdit_, &QLineEdit::textChanged, this, [this] {
        updateSendButtonState();
    });
    connect(lineEdit_, &QLineEdit::returnPressed, this, [this] {
        const QString text = lineEdit_->text().trimmed();
        if (!text.isEmpty()) {
            emit sendRequested(text);
            lineEdit_->clear();
        }
    });

    if (controller_) {
        setController(controller_);
    } else {
        updateSendButtonState();
    }

    connect(webclip::MD3Theme::instance(), &webclip::MD3Theme::themeChanged, this, [this] {
        auto* theme = webclip::MD3Theme::instance();
        attachBtn_->setIconColor(theme->primary());
        pasteBtn_->setIconColor(theme->onSurfaceVariant());
        updateSendButtonState();
        update();
    });
}

InputDock::~InputDock() = default;

void InputDock::setController(webclip::WebClipController* controller) {
    controller_ = controller;
    if (controller_) {
        connect(controller_, &webclip::WebClipController::connectedChanged, this, [this] {
            updateSendButtonState();
        });
    }
    updateSendButtonState();
}

QString InputDock::text() const {
    return lineEdit_->text();
}

void InputDock::clear() {
    lineEdit_->clear();
}

void InputDock::updateSendButtonState() {
    auto* theme = webclip::MD3Theme::instance();
    const bool canSend = !lineEdit_->text().trimmed().isEmpty();

    if (canSend) {
        sendBtn_->setCustomBgColor(theme->primary());
        sendBtn_->setIconColor(theme->onPrimary());
    } else {
        sendBtn_->setCustomBgColor(theme->primaryContainer());
        sendBtn_->setIconColor(theme->onPrimaryContainer());
    }
    update();
}

void InputDock::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void InputDock::updateLayout() {
    const int margin = 10;
    const int attachSize = 44;
    const int sendSize = 48;
    const int pillHeight = 48;

    attachBtn_->move(margin, (height() - attachSize) / 2);
    sendBtn_->move(width() - margin - sendSize, (height() - sendSize) / 2);

    const int pillX = margin + attachSize + margin;
    const int pillW = (width() - margin - sendSize - margin) - pillX;
    const int pillY = (height() - pillHeight) / 2;

    const int pasteSize = 36;
    pasteBtn_->move(pillX + pillW - pasteSize - 6, pillY + (pillHeight - pasteSize) / 2);

    const int inputX = pillX + 18;
    const int inputW = (pasteBtn_->x() - 8) - inputX;
    const int inputH = 32;
    lineEdit_->setGeometry(inputX, pillY + (pillHeight - inputH) / 2, inputW, inputH);
}

void InputDock::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    // 1. Calculate pill bounds
    const int margin = 10;
    const int attachSize = 44;
    const int sendSize = 48;
    const int pillHeight = 48;

    const int pillX = margin + attachSize + margin;
    const int pillW = (width() - margin - sendSize - margin) - pillX;
    const int pillY = (height() - pillHeight) / 2;
    const QRectF pillRect(pillX + 0.5, pillY + 0.5, pillW - 1.0, pillHeight - 1.0);

    // 2. Draw Pill Background
    p.setPen(Qt::NoPen);
    p.setBrush(theme->surfaceContainerHigh());
    p.drawRoundedRect(pillRect, pillHeight / 2.0, pillHeight / 2.0);

    // 3. Focus border
    if (lineEdit_->hasFocus()) {
        p.setPen(QPen(theme->primary(), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(pillRect, pillHeight / 2.0, pillHeight / 2.0);
    }
}

} // namespace Ui
