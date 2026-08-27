#pragma once

#include <QtWidgets/QMainWindow>
#include <QtCore/QPointer>

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

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

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
};

} // namespace Ui
