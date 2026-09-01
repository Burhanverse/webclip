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

    resize(webclip::scale::px(380), webclip::scale::px(800));
    setMinimumSize(webclip::scale::px(360), webclip::scale::px(680));
    setMaximumSize(webclip::scale::px(420), webclip::scale::px(880));

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
    const int headerH = webclip::scale::px(58);
    const int dockH = webclip::scale::px(64);
    const int toastH = webclip::scale::px(40);
    const int toastMargin = webclip::scale::px(16);
    const int toastBottomOffset = webclip::scale::px(46);

    headerBar_->setGeometry(0, 0, w, headerH);
    inputDock_->setGeometry(0, h - dockH, w, dockH);
    clipWidget_->setGeometry(0, headerH, w, h - headerH - dockH);

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

void MainWindow::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    auto* theme = webclip::MD3Theme::instance();

    // Rounded window container without outer outline
    const QRectF r(0.0, 0.0, width(), height());
    p.setPen(Qt::NoPen);
    p.setBrush(theme->surface());
    p.drawRoundedRect(r, webclip::scale::pxF(18.0), webclip::scale::pxF(18.0));
}

} // namespace Ui
