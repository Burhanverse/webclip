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

#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtWidgets/QFileDialog>

namespace Ui {

MainWindow::MainWindow(QWidget* parent, webclip::WebClipController* controller)
    : QMainWindow(parent)
    , controller_(controller) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    resize(380, 800);
    setMinimumSize(360, 680);
    setMaximumSize(420, 880);

    setupUi();
    ensureOnScreen();

    auto* theme = webclip::MD3Theme::instance();
    connect(theme, &webclip::MD3Theme::themeChanged, this, [this] {
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

    // Header actions
    connect(headerBar_, &HeaderBar::openSettingsRequested, this, [this] {
        settingsDialog_->open();
    });

    // Input actions
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

    // Timeline actions
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

void MainWindow::updateLayout() {
    const int w = width();
    const int h = height();

    headerBar_->setGeometry(0, 0, w, 58);
    inputDock_->setGeometry(0, h - 64, w, 64);
    clipWidget_->setGeometry(0, 58, w, h - 58 - 64);

    toast_->setGeometry(16, h - 64 - 46, w - 32, 40);

    if (settingsDialog_) settingsDialog_->setGeometry(rect());
    if (imagePreviewModal_) imagePreviewModal_->setGeometry(rect());
}

void MainWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (e->pos().y() < 58 && windowHandle()) {
            windowHandle()->startSystemMove();
        }
    }
    QMainWindow::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent* e) {
    QMainWindow::mouseMoveEvent(e);
}

void MainWindow::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    // 28px rounded window container
    const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
    p.setPen(QPen(theme->outlineVariant(), 1.0));
    p.setBrush(theme->surface());
    p.drawRoundedRect(r, 28.0, 28.0);
}

} // namespace Ui
