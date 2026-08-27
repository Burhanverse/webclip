#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/animation.hpp"
#include "../md3/md3_icon_button.hpp"

#include <QtGui/QPixmap>

namespace Ui {

class ImagePreviewModal : public RpWidget {
    Q_OBJECT

public:
    explicit ImagePreviewModal(QWidget* parent = nullptr);
    ~ImagePreviewModal() override;

    void showImage(const QPixmap& pixmap);
    void showImage(const QString& filePath);
    void hideAnimated();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void updateLayout();

    QPixmap pixmap_;
    double opacity_ = 0.0;
    Ui::Animations::Simple anim_;
    Md3IconButton* closeBtn_ = nullptr;
};

} // namespace Ui
