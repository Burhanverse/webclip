#include "image_preview_modal.hpp"
#include "../basic/painter_helpers.hpp"
#include "../../util/display_scale.hpp"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

namespace Ui {

ImagePreviewModal::ImagePreviewModal(QWidget* parent)
    : RpWidget(parent) {
    hide();
    setFocusPolicy(Qt::StrongFocus);

    closeBtn_ = new Md3IconButton(this, QStringLiteral("close"), webclip::scale::px(40), webclip::scale::px(22));
    closeBtn_->setCustomBgColor(QColor(255, 255, 255, 38));
    closeBtn_->setIconColor(Qt::white);
    closeBtn_->addClickHandler([this] {
        hideAnimated();
    });
}

ImagePreviewModal::~ImagePreviewModal() = default;

void ImagePreviewModal::showImage(const QPixmap& pixmap) {
    pixmap_ = pixmap;
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    }
    show();
    raise();
    setFocus();

    anim_.start(
        [this](double progress) {
            opacity_ = progress;
            update();
        },
        opacity_,
        1.0,
        180,
        anim::easeOutCubic
    );
}

void ImagePreviewModal::showImage(const QString& filePath) {
    QPixmap p;
    if (p.load(filePath)) {
        showImage(p);
    }
}

void ImagePreviewModal::hideAnimated() {
    anim_.start(
        [this](double progress) {
            opacity_ = progress;
            update();
        },
        opacity_,
        0.0,
        180,
        anim::easeOutCubic
    );
    anim_.setFinishedCallback([this] {
        hide();
    });
}

void ImagePreviewModal::resizeEvent(QResizeEvent* e) {
    RpWidget::resizeEvent(e);
    updateLayout();
}

void ImagePreviewModal::updateLayout() {
    closeBtn_->move(width() - webclip::scale::px(16) - closeBtn_->width(), webclip::scale::px(16));
}

void ImagePreviewModal::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        hideAnimated();
    }
    RpWidget::mousePressEvent(e);
}

void ImagePreviewModal::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hideAnimated();
    } else {
        RpWidget::keyPressEvent(e);
    }
}

void ImagePreviewModal::paintEvent(QPaintEvent* /*e*/) {
    if (opacity_ <= 0.0) return;

    QPainter p(this);
    PainterHighQualityEnabler hq(p);
    ScopedPainterOpacity op(p, opacity_);

    // 1. Fullscreen dark backdrop (85% black)
    p.fillRect(rect(), QColor(0, 0, 0, 217));

    // 2. Centered image preserved aspect ratio
    if (!pixmap_.isNull()) {
        const int maxW = width() - webclip::scale::px(48);
        const int maxH = height() - webclip::scale::px(80);
        const QSize scaledSize = pixmap_.size().scaled(maxW, maxH, Qt::KeepAspectRatio);

        const int imgX = (width() - scaledSize.width()) / 2;
        const int imgY = (height() - scaledSize.height()) / 2;
        const QRectF imgRect(imgX, imgY, scaledSize.width(), scaledSize.height());

        p.drawPixmap(imgRect.toRect(), pixmap_);
    }
}

} // namespace Ui
