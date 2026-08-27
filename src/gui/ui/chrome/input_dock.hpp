#pragma once

#include "../basic/rp_widget.hpp"
#include "../md3/md3_icon_button.hpp"

#include <QtWidgets/QLineEdit>

namespace webclip {
class WebClipController;
}

namespace Ui {

class InputDock : public RpWidget {
    Q_OBJECT

public:
    explicit InputDock(QWidget* parent = nullptr, webclip::WebClipController* controller = nullptr);
    ~InputDock() override;

    void setController(webclip::WebClipController* controller);

    [[nodiscard]] QString text() const;
    void clear();

    [[nodiscard]] QSize sizeHint() const override {
        return QSize(380, 64);
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        return QSize(320, 64);
    }

signals:
    void sendRequested(const QString& text);
    void attachImageRequested();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void updateSendButtonState();
    void updateLayout();

    webclip::WebClipController* controller_ = nullptr;
    Md3IconButton* attachBtn_ = nullptr;
    Md3IconButton* pasteBtn_ = nullptr;
    Md3IconButton* sendBtn_ = nullptr;
    QLineEdit* lineEdit_ = nullptr;
};

} // namespace Ui
