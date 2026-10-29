#include "main_window.hpp"
#include "basic/painter_helpers.hpp"
#include "chrome/header_bar.hpp"
#include "chrome/input_dock.hpp"
#include "chrome/toast_widget.hpp"
#include "clips/clip_widget.hpp"
#include "dialogs/settings_dialog.hpp"
#include "dialogs/image_preview_modal.hpp"
#include "../theme/md3_theme.hpp"
#include "../controllers/webclip_controller.hpp"
#include "../util/display_scale.hpp"

#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtCore/QStandardPaths>
#include <QtCore/QSettings>
#include "../util/debug_logger.hpp"

namespace Ui {

MainWindow::MainWindow(QWidget* parent, webclip::WebClipController* controller)
    : QMainWindow(parent)
    , controller_(controller) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    resize(webclip::scale::px(380), webclip::scale::px(800));
    setMinimumSize(webclip::scale::px(360), webclip::scale::px(680));
    setMaximumSize(webclip::scale::px(420), webclip::scale::px(880));

    setupUi();
    ensureOnScreen();

    auto* theme = webclip::MD3Theme::instance();
    connect(theme, &webclip::MD3Theme::themeChanged, this, [this] {
        if (bannerWidget_) {
            updateBannerStyle();
        }
        update();
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    container_ = new QWidget(this);
    setCentralWidget(container_);

    headerBar_ = new HeaderBar(container_, controller_);
    clipWidget_ = new ClipWidget(container_, controller_, controller_ ? controller_->clipModel() : nullptr);
    inputDock_ = new InputDock(container_, controller_);
    toast_ = new ToastWidget(container_);
    settingsDialog_ = new SettingsDialog(container_, controller_);
    imagePreviewModal_ = new ImagePreviewModal(container_);

    connect(headerBar_, &HeaderBar::openSettingsRequested, this, [this] {
        settingsDialog_->open();
    });

    connect(headerBar_, &HeaderBar::minimizeRequested, this, [this] {
        minimizeToTray();
    });

    connect(inputDock_, &InputDock::sendRequested, this, [this](const QString& text) {
        if (controller_) controller_->pushClipboard(text);
    });

    connect(inputDock_, &InputDock::attachImageRequested, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select Image"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)")
        );
        if (!path.isEmpty() && controller_) {
            controller_->pushImage(path);
        }
    });

    connect(clipWidget_, &ClipWidget::fullPreviewRequested, this, [this](const QString& key) {
        imagePreviewModal_->showImage(key);
    });

    connect(clipWidget_, &ClipWidget::saveImageRequested, this, [this](int row) {
        const QString outPath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save Image"),
            QStringLiteral("clip_image.png"),
            QStringLiteral("PNG Images (*.png);;All Files (*)")
        );
        if (!outPath.isEmpty() && controller_) {
            controller_->copyImageToClipboard(row);
        }
    });

    if (controller_) {
        setController(controller_);
    }
}

void MainWindow::setController(webclip::WebClipController* controller) {
    controller_ = controller;
    if (!controller_) return;

    headerBar_->setController(controller_);
    inputDock_->setController(controller_);
    clipWidget_->setController(controller_);
    settingsDialog_->setController(controller_);
}

void MainWindow::showToast(const QString& message, bool isError) {
    if (toast_) {
        toast_->showMessage(message, isError);
    }
}

void MainWindow::minimizeToTray() {
    hide();
    if (controller_) {
        controller_->notifyMinimizedToTray();
    }
}

void MainWindow::ensureOnScreen() {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) return;

    auto* primary = screens.first();
    const QRect g = primary->geometry();
    move(g.x() + (g.width() - width()) / 2, g.y() + (g.height() - height()) / 2);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    updateLayout();
}

void MainWindow::changeEvent(QEvent* e) {
    if (e->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            QTimer::singleShot(0, this, &MainWindow::minimizeToTray);
        }
    }
    QMainWindow::changeEvent(e);
}

void MainWindow::updateLayout() {
    const int w = width();
    const int h = height();
    const int headerH = webclip::scale::px(58);
    const int dockH = webclip::scale::px(64);
    const int toastH = webclip::scale::px(40);
    const int toastMargin = webclip::scale::px(16);
    const int toastBottomOffset = webclip::scale::px(46);

    headerBar_->setGeometry(0, 0, w, headerH);

    int bannerH = 0;
    if (bannerWidget_ && bannerWidget_->isVisible()) {
        const int bannerMargin = webclip::scale::px(12);
        const int bannerW = w - 2 * bannerMargin;
        const int preferredH = bannerWidget_->sizeHint().height();
        bannerWidget_->setGeometry(bannerMargin, headerH + webclip::scale::px(4), bannerW, preferredH);
        bannerH = preferredH + webclip::scale::px(8);
    }

    inputDock_->setGeometry(0, h - dockH, w, dockH);
    clipWidget_->setGeometry(0, headerH + bannerH, w, h - headerH - dockH - bannerH);

    toast_->setGeometry(toastMargin, h - dockH - toastBottomOffset, w - 2 * toastMargin, toastH);

    if (settingsDialog_) settingsDialog_->setGeometry(rect());
    if (imagePreviewModal_) imagePreviewModal_->setGeometry(rect());
}

void MainWindow::mousePressEvent(QMouseEvent* e) {
    QMainWindow::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent* e) {
    QMainWindow::mouseMoveEvent(e);
}

void MainWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    const QRectF r(0.0, 0.0, width(), height());
    p.setPen(Qt::NoPen);
    p.setBrush(theme->surface());
    p.drawRoundedRect(r, webclip::scale::pxF(18.0), webclip::scale::pxF(18.0));
}

void MainWindow::checkWlClipboardWarning() {
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        return;
    }

    const QString wlPaste = QStandardPaths::findExecutable(QStringLiteral("wl-paste"));
    const QString wlCopy = QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
    if (!wlPaste.isEmpty() && !wlCopy.isEmpty()) {
        return;
    }

    QString missing;
    if (wlPaste.isEmpty() && wlCopy.isEmpty()) {
        missing = QStringLiteral("wl-paste and wl-copy are missing");
    } else if (wlPaste.isEmpty()) {
        missing = QStringLiteral("wl-paste is missing");
    } else {
        missing = QStringLiteral("wl-copy is missing");
    }

    const QString warning = QStringLiteral("wl-clipboard is not installed (%1). Clipboard sync won't work reliably on Wayland. Install it with your package manager (package name: wl-clipboard). Using fallback mode, may miss copies while unfocused.").arg(missing);

    WEBCLIP_LOG(QStringLiteral("[Wayland] ") + warning);

    QSettings settings(QStringLiteral("Burhanverse"), QStringLiteral("WebClip"));
    if (settings.value(QStringLiteral("dismissWlClipboardWarning"), false).toBool()) {
        return;
    }

    showWlClipboardBanner(warning);
}

void MainWindow::updateBannerStyle() {
    if (!bannerWidget_) return;
    auto* theme = webclip::MD3Theme::instance();
    const QString style = QStringLiteral(
        "#warningBanner { background-color: %1; border: 1px solid %2; border-radius: %3px; }"
        "QLabel { color: %4; background: transparent; }"
        "QPushButton { background: transparent; color: %5; font-weight: bold; border: none; padding: %6px %7px; }"
        "QPushButton:hover { background-color: %8; border-radius: %9px; }"
    ).arg(
        theme->errorContainer().name(),
        theme->onErrorContainer().name(),
        QString::number(webclip::scale::px(10)),
        theme->onErrorContainer().name(),
        theme->onErrorContainer().name(),
        QString::number(webclip::scale::px(4)),
        QString::number(webclip::scale::px(8)),
        theme->colorWithAlpha(theme->onErrorContainer(), 0.12).name(QColor::HexArgb),
        QString::number(webclip::scale::px(4))
    );
    bannerWidget_->setStyleSheet(style);
}

void MainWindow::showWlClipboardBanner(const QString& text) {
    if (bannerWidget_) {
        bannerWidget_->show();
        updateLayout();
        return;
    }

    bannerWidget_ = new QFrame(container_);
    bannerWidget_->setObjectName(QStringLiteral("warningBanner"));
    updateBannerStyle();

    auto* layout = new QVBoxLayout(bannerWidget_);
    layout->setContentsMargins(
        webclip::scale::px(12),
        webclip::scale::px(10),
        webclip::scale::px(12),
        webclip::scale::px(10)
    );
    layout->setSpacing(webclip::scale::px(8));

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(webclip::scale::px(8));

    auto* theme = webclip::MD3Theme::instance();
    auto* label = new QLabel(text, bannerWidget_);
    label->setFont(theme->bodySmall());
    label->setWordWrap(true);
    topRow->addWidget(label, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("✕"), bannerWidget_);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(webclip::scale::px(24), webclip::scale::px(24));
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        bannerWidget_->hide();
        updateLayout();
    });
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    layout->addLayout(topRow);

    auto* actionsRow = new QHBoxLayout();
    actionsRow->setContentsMargins(0, 0, 0, 0);
    actionsRow->setSpacing(webclip::scale::px(8));
    actionsRow->addStretch(1);

    auto* dontShowBtn = new QPushButton(QStringLiteral("Don't show again"), bannerWidget_);
    dontShowBtn->setFont(theme->labelSmall());
    dontShowBtn->setCursor(Qt::PointingHandCursor);
    connect(dontShowBtn, &QPushButton::clicked, this, [this]() {
        QSettings settings(QStringLiteral("Burhanverse"), QStringLiteral("WebClip"));
        settings.setValue(QStringLiteral("dismissWlClipboardWarning"), true);
        bannerWidget_->hide();
        updateLayout();
    });
    actionsRow->addWidget(dontShowBtn);

    auto* dismissBtn = new QPushButton(QStringLiteral("Dismiss"), bannerWidget_);
    dismissBtn->setFont(theme->labelSmall());
    dismissBtn->setCursor(Qt::PointingHandCursor);
    connect(dismissBtn, &QPushButton::clicked, this, [this]() {
        bannerWidget_->hide();
        updateLayout();
    });
    actionsRow->addWidget(dismissBtn);

    layout->addLayout(actionsRow);

    bannerWidget_->show();
    updateLayout();
}

}
