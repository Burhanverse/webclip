#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include <QtCore/QTimer>

namespace Ui {

class ToastWidget : public RpWidget {
    Q_OBJECT

public:
    explicit ToastWidget(QWidget* parent = nullptr);
    ~ToastWidget() override;

    void showMessage(const QString& message, bool isError = false);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    void hideAnimated();

    QString message_;
    bool isError_ = false;
    double opacity_ = 0.0;
    double slideOffset_ = 16.0;

    Ui::Animations::Simple anim_;
    QTimer hideTimer_;
};

} // namespace Ui
