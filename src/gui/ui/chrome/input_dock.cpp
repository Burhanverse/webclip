#include "input_dock.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../theme/md3_theme.hpp"
#include "../../util/i18n.hpp"
#include "../../util/display_scale.hpp"
#include "../../controllers/webclip_controller.hpp"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

namespace Ui {

InputDock::InputDock(QWidget* parent, webclip::WebClipController* controller)
    : RpWidget(parent)
    , controller_(controller) {
    setFixedHeight(webclip::scale::px(64));
    setAcceptDrops(false);

    attachBtn_ = new Md3IconButton(this, QStringLiteral("image"), webclip::scale::px(38), webclip::scale::px(20));
    attachBtn_->setRoundSquare(true);
    attachBtn_->setAcceptDrops(false);
    attachBtn_->setToolTip(webclip::I18n::instance()->tr(QStringLiteral("chat.attach_image_tooltip")));
    attachBtn_->addClickHandler([this] {
        emit attachImageRequested();
    });

    attachFilesBtn_ = new Md3IconButton(this, QStringLiteral("attach_file"), webclip::scale::px(38), webclip::scale::px(20));
    attachFilesBtn_->setRoundSquare(true);
    attachFilesBtn_->setAcceptDrops(false);
    attachFilesBtn_->setToolTip(webclip::I18n::instance()->tr(QStringLiteral("chat.send_files_tooltip")));
    attachFilesBtn_->addClickHandler([this] {
        emit attachFilesRequested();
    });

    pasteBtn_ = new Md3IconButton(this, QStringLiteral("paste"), webclip::scale::px(32), webclip::scale::px(16));
    pasteBtn_->setAcceptDrops(false);
    pasteBtn_->setIconColor(webclip::MD3Theme::instance()->onSurfaceVariant());
    pasteBtn_->addClickHandler([this] {
        if (controller_ && controller_->pushCurrentClipboard()) {
            lineEdit_->clear();
        } else {
            lineEdit_->paste();
        }
    });

    sendBtn_ = new Md3IconButton(this, QStringLiteral("send"), webclip::scale::px(44), webclip::scale::px(20));
    sendBtn_->setAcceptDrops(false);
    sendBtn_->addClickHandler([this] {
        const QString text = lineEdit_->text().trimmed();
        if (!text.isEmpty()) {
            emit sendRequested(text);
            lineEdit_->clear();
        }
    });

    lineEdit_ = new QLineEdit(this);
    lineEdit_->setFrame(false);
    lineEdit_->setAcceptDrops(false);
    lineEdit_->setPlaceholderText(webclip::I18n::instance()->tr(QStringLiteral("chat.message_placeholder")));

    updateTheme();

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
        attachBtn_->setCustomBgColor(theme->primaryContainer());
        attachBtn_->setIconColor(theme->onPrimaryContainer());
        attachFilesBtn_->setCustomBgColor(theme->primaryContainer());
        attachFilesBtn_->setIconColor(theme->onPrimaryContainer());
        pasteBtn_->setIconColor(theme->onSurfaceVariant());
        updateTheme();
        updateSendButtonState();
        update();
    });

    auto* theme = webclip::MD3Theme::instance();
    attachBtn_->setCustomBgColor(theme->primaryContainer());
    attachBtn_->setIconColor(theme->onPrimaryContainer());
    attachFilesBtn_->setCustomBgColor(theme->primaryContainer());
    attachFilesBtn_->setIconColor(theme->onPrimaryContainer());
}

void InputDock::updateTheme() {
    auto* theme = webclip::MD3Theme::instance();
    if (lineEdit_) {
        lineEdit_->setFont(theme->bodyLarge());
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
    const int margin = webclip::scale::px(8);
    const int btnSpacing = webclip::scale::px(4);
    const int attachSize = webclip::scale::px(38);
    const int sendSize = webclip::scale::px(44);
    const int pillHeight = webclip::scale::px(46);

    attachBtn_->resize(attachSize, attachSize);
    attachBtn_->move(margin, (height() - attachSize) / 2);

    attachFilesBtn_->resize(attachSize, attachSize);
    attachFilesBtn_->move(margin + attachSize + btnSpacing, (height() - attachSize) / 2);

    sendBtn_->resize(sendSize, sendSize);
    sendBtn_->move(width() - margin - sendSize, (height() - sendSize) / 2);

    const int pillX = margin + attachSize + btnSpacing + attachSize + margin;
    const int pillW = (width() - margin - sendSize - margin) - pillX;
    const int pillY = (height() - pillHeight) / 2;

    const int pasteSize = webclip::scale::px(32);
    pasteBtn_->resize(pasteSize, pasteSize);
    pasteBtn_->move(pillX + pillW - pasteSize - webclip::scale::px(6), pillY + (pillHeight - pasteSize) / 2);

    const int inputX = pillX + webclip::scale::px(14);
    const int inputW = (pasteBtn_->x() - webclip::scale::px(6)) - inputX;
    const int inputH = webclip::scale::px(30);
    lineEdit_->setGeometry(inputX, pillY + (pillHeight - inputH) / 2, inputW, inputH);
}

void InputDock::paintEvent(QPaintEvent*) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const int margin = webclip::scale::px(8);
    const int btnSpacing = webclip::scale::px(4);
    const int attachSize = webclip::scale::px(38);
    const int sendSize = webclip::scale::px(44);
    const int pillHeight = webclip::scale::px(46);

    const int pillX = margin + attachSize + btnSpacing + attachSize + margin;
    const int pillW = (width() - margin - sendSize - margin) - pillX;
    const int pillY = (height() - pillHeight) / 2;
    const QRectF pillRect(pillX + 0.5, pillY + 0.5, pillW - 1.0, pillHeight - 1.0);

    p.setPen(Qt::NoPen);
    p.setBrush(theme->surfaceContainerHigh());
    p.drawRoundedRect(pillRect, pillHeight / 2.0, pillHeight / 2.0);

    if (lineEdit_->hasFocus()) {
        p.setPen(QPen(theme->primary(), webclip::scale::pxF(1.5)));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(pillRect, pillHeight / 2.0, pillHeight / 2.0);
    }
}

}
