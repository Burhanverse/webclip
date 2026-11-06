#pragma once

#include <QtWidgets/QMainWindow>
#include <QtCore/QPointer>

class QFrame;

namespace webclip {
class WebClipController;
class ClipboardHistoryModel;
}

namespace Ui {

class HeaderBar;
class ClipWidget;
class InputDock;
class ToastWidget;
class SettingsDialog;
class ImagePreviewModal;
class DropOverlayWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(
        QWidget* parent = nullptr,
        webclip::WebClipController* controller = nullptr
    );
    ~MainWindow() override;

    void setController(webclip::WebClipController* controller);
    void showToast(const QString& message, bool isError = false);
    void showOfferBanner(const QString& batchId, const QString& title, const QString& details);
    void hideOfferBanner();
    void checkWlClipboardWarning();
    void minimizeToTray();

    [[nodiscard]] HeaderBar* headerBar() const noexcept { return headerBar_; }
    [[nodiscard]] SettingsDialog* settingsDialog() const noexcept { return settingsDialog_; }

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void changeEvent(QEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void setupUi();
    void updateLayout();
    void ensureOnScreen();

    webclip::WebClipController* controller_ = nullptr;

    QWidget* container_ = nullptr;
    HeaderBar* headerBar_ = nullptr;
    ClipWidget* clipWidget_ = nullptr;
    InputDock* inputDock_ = nullptr;
    ToastWidget* toast_ = nullptr;
    SettingsDialog* settingsDialog_ = nullptr;
    ImagePreviewModal* imagePreviewModal_ = nullptr;
    DropOverlayWidget* dropOverlay_ = nullptr;
    QFrame* bannerWidget_ = nullptr;
    QFrame* offerBannerWidget_ = nullptr;

    void showWlClipboardBanner(const QString& warning);
    void updateBannerStyle();
    void updateOfferBannerStyle();
};

}
