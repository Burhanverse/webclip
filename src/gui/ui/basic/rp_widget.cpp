#include "rp_widget.hpp"

#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>

namespace Ui {

RpWidget::RpWidget(QWidget* parent)
    : QWidget(parent) {
}

RpWidget::~RpWidget() {
    lifetime_.destroy();
}

int RpWidget::resizeGetHeight(int /*newWidth*/) {
    return height();
}

void RpWidget::resizeToWidth(int newWidth) {
    resize(newWidth, resizeGetHeight(newWidth));
}

rpl::producer<int> RpWidget::widthValue() const {
    return geometryValue()
        | rpl::map([](const QRect& r) { return r.width(); })
        | rpl::distinct_until_changed();
}

rpl::producer<int> RpWidget::heightValue() const {
    return geometryValue()
        | rpl::map([](const QRect& r) { return r.height(); })
        | rpl::distinct_until_changed();
}

rpl::producer<QSize> RpWidget::sizeValue() const {
    return geometryValue()
        | rpl::map([](const QRect& r) { return r.size(); })
        | rpl::distinct_until_changed();
}

rpl::producer<QRect> RpWidget::geometryValue() const {
    return geometryStream_.events_starting_with_copy(geometry());
}

rpl::producer<bool> RpWidget::shownValue() const {
    return shownStream_.events_starting_with_copy(!isHidden());
}

rpl::producer<QRect> RpWidget::paintRequest() const {
    return paintStream_.events();
}

void RpWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    geometryStream_.fire_copy(geometry());
}

void RpWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    shownStream_.fire_copy(true);
}

void RpWidget::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    shownStream_.fire_copy(false);
}

void RpWidget::paintEvent(QPaintEvent* e) {
    paintStream_.fire_copy(e->rect());
    QWidget::paintEvent(e);
}

} // namespace Ui
