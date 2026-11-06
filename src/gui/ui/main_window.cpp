#include "main_window.hpp"
#include "basic/painter_helpers.hpp"
#include "chrome/header_bar.hpp"
#include "chrome/input_dock.hpp"
#include "chrome/toast_widget.hpp"
#include "clips/clip_widget.hpp"
#include "dialogs/settings_dialog.hpp"
#include "dialogs/image_preview_modal.hpp"
#include "md3/drop_overlay_widget.hpp"
#include "../theme/md3_theme.hpp"
#include "../controllers/webclip_controller.hpp"
#include "../util/display_scale.hpp"
#include "../util/i18n.hpp"

#include <QtCore/QTimer>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDropEvent>
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
    setAcceptDrops(true);

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
    dropOverlay_ = new DropOverlayWidget(container_);

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

    connect(inputDock_, &InputDock::attachFilesRequested, this, [this] {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this,
            webclip::I18n::instance()->tr(QStringLiteral("chat.send_files_dialog_title")),
            QString(),
            QStringLiteral("All Files (*)")
        );
        if (!paths.isEmpty() && controller_) {
            controller_->uploadFiles(paths);
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
    connect(controller_, &webclip::WebClipController::showToast, this, &MainWindow::showToast);
    connect(controller_, &webclip::WebClipController::showOfferBanner, this, &MainWindow::showOfferBanner);
    connect(controller_, &webclip::WebClipController::hideOfferBanner, this, &MainWindow::hideOfferBanner);
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
        bannerH += preferredH + webclip::scale::px(8);
    }
    if (offerBannerWidget_ && offerBannerWidget_->isVisible()) {
        const int bannerMargin = webclip::scale::px(12);
        const int bannerW = w - 2 * bannerMargin;
        const int preferredH = offerBannerWidget_->sizeHint().height();
        offerBannerWidget_->setGeometry(bannerMargin, headerH + bannerH + webclip::scale::px(4), bannerW, preferredH);
        bannerH += preferredH + webclip::scale::px(8);
    }

    inputDock_->setGeometry(0, h - dockH, w, dockH);
    clipWidget_->setGeometry(0, headerH + bannerH, w, h - headerH - dockH - bannerH);

    toast_->setGeometry(toastMargin, h - dockH - toastBottomOffset, w - 2 * toastMargin, toastH);

    if (settingsDialog_) settingsDialog_->setGeometry(rect());
    if (imagePreviewModal_) imagePreviewModal_->setGeometry(rect());
    if (dropOverlay_) dropOverlay_->setGeometry(0, 0, w, h);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if ((settingsDialog_ && settingsDialog_->isVisible()) ||
        (imagePreviewModal_ && imagePreviewModal_->isVisible())) {
        e->ignore();
        return;
    }
    if (!e->mimeData()->hasUrls()) {
        e->ignore();
        return;
    }
    bool hasLocal = false;
    for (const QUrl& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) {
            hasLocal = true;
            break;
        }
    }
    if (!hasLocal) {
        e->ignore();
        return;
    }
    e->acceptProposedAction();
    if (controller_ && controller_->connected()) {
        dropOverlay_->show();
        dropOverlay_->raise();
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent* e) {
    if ((settingsDialog_ && settingsDialog_->isVisible()) ||
        (imagePreviewModal_ && imagePreviewModal_->isVisible())) {
        if (dropOverlay_ && dropOverlay_->isVisible()) {
            dropOverlay_->hide();
        }
        e->ignore();
        return;
    }
    e->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* e) {
    if (dropOverlay_) {
        dropOverlay_->hide();
    }
    QMainWindow::dragLeaveEvent(e);
}

void MainWindow::dropEvent(QDropEvent* e) {
    if (dropOverlay_) {
        dropOverlay_->hide();
    }
    if ((settingsDialog_ && settingsDialog_->isVisible()) ||
        (imagePreviewModal_ && imagePreviewModal_->isVisible())) {
        e->ignore();
        return;
    }
    if (!controller_ || !controller_->connected()) {
        showToast(webclip::I18n::instance()->tr(QStringLiteral("toast.not_connected")), true);
        e->ignore();
        return;
    }
    e->acceptProposedAction();

    const QList<QUrl> urls = e->mimeData()->urls();
    QStringList filePaths;
    bool hasDirectory = false;
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            const QString localPath = url.toLocalFile();
            QFileInfo fi(localPath);
            if (fi.isDir()) {
                hasDirectory = true;
            } else if (fi.isFile()) {
                filePaths.append(localPath);
            }
        }
    }

    if (hasDirectory) {
        showToast(webclip::I18n::instance()->tr(QStringLiteral("toast.directories_not_supported")), true);
    }

    if (!filePaths.isEmpty() && controller_) {
        controller_->uploadFiles(filePaths);
    }
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

void MainWindow::showOfferBanner(const QString& batchId, const QString& title, const QString& details) {
    if (offerBannerWidget_) {
        delete offerBannerWidget_;
        offerBannerWidget_ = nullptr;
    }

    offerBannerWidget_ = new QFrame(container_);
    offerBannerWidget_->setObjectName(QStringLiteral("offerBanner"));
    updateOfferBannerStyle();

    auto* layout = new QVBoxLayout(offerBannerWidget_);
    layout->setContentsMargins(
        webclip::scale::px(12),
        webclip::scale::px(10),
        webclip::scale::px(12),
        webclip::scale::px(10)
    );
    layout->setSpacing(webclip::scale::px(6));

    auto* titleLabel = new QLabel(title, offerBannerWidget_);
    auto* theme = webclip::MD3Theme::instance();
    QFont tFont = theme->titleSmall();
    tFont.setBold(true);
    titleLabel->setFont(tFont);
    layout->addWidget(titleLabel);

    if (!details.isEmpty()) {
        auto* detailsLabel = new QLabel(details, offerBannerWidget_);
        detailsLabel->setFont(theme->bodySmall());
        detailsLabel->setWordWrap(true);
        layout->addWidget(detailsLabel);
    }

    auto* actionsRow = new QHBoxLayout();
    actionsRow->setContentsMargins(0, 0, 0, 0);
    actionsRow->setSpacing(webclip::scale::px(8));
    actionsRow->addStretch(1);

    auto* declineBtn = new QPushButton(QStringLiteral("Decline"), offerBannerWidget_);
    declineBtn->setFont(theme->labelMedium());
    declineBtn->setCursor(Qt::PointingHandCursor);
    connect(declineBtn, &QPushButton::clicked, this, [this, batchId]() {
        if (controller_) {
            controller_->declineFileOffer(batchId);
        }
        hideOfferBanner();
    });
    actionsRow->addWidget(declineBtn);

    auto* acceptBtn = new QPushButton(QStringLiteral("Accept"), offerBannerWidget_);
    acceptBtn->setFont(theme->labelMedium());
    acceptBtn->setCursor(Qt::PointingHandCursor);
    connect(acceptBtn, &QPushButton::clicked, this, [this, batchId]() {
        if (controller_) {
            controller_->acceptFileOffer(batchId);
        }
        hideOfferBanner();
    });
    actionsRow->addWidget(acceptBtn);

    layout->addLayout(actionsRow);

    offerBannerWidget_->show();
    updateLayout();

    if (isHidden() || isMinimized()) {
        if (controller_) {
            emit controller_->showTrayNotification(
                QStringLiteral("Incoming file transfer from Phone"),
                QStringLiteral("Click to view")
            );
        }
    }
}

void MainWindow::hideOfferBanner() {
    if (offerBannerWidget_) {
        offerBannerWidget_->hide();
        offerBannerWidget_->deleteLater();
        offerBannerWidget_ = nullptr;
        updateLayout();
    }
}

void MainWindow::updateOfferBannerStyle() {
    if (!offerBannerWidget_) return;
    auto* theme = webclip::MD3Theme::instance();
    const QString style = QStringLiteral(
        "#offerBanner { background-color: %1; border: 1px solid %2; border-radius: %3px; }"
        "QLabel { color: %4; background: transparent; }"
        "QPushButton { background: %5; color: %6; font-weight: bold; border: none; border-radius: %7px; padding: %8px %9px; }"
        "QPushButton:hover { background-color: %10; }"
    ).arg(
        theme->primaryContainer().name(),
        theme->primary().name(),
        QString::number(webclip::scale::px(10)),
        theme->onPrimaryContainer().name(),
        theme->primary().name(),
        theme->onPrimary().name(),
        QString::number(webclip::scale::px(6)),
        QString::number(webclip::scale::px(4)),
        QString::number(webclip::scale::px(12)),
        theme->colorWithAlpha(theme->primary(), 0.85).name(QColor::HexArgb)
    );
    offerBannerWidget_->setStyleSheet(style);
}

}
