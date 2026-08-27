#pragma once

#include <QtWidgets/QWidget>
#include "rpl_lite.hpp"

namespace Ui {

class RpWidget : public QWidget {
public:
    explicit RpWidget(QWidget* parent = nullptr);
    ~RpWidget() override;

    [[nodiscard]] rpl::lifetime& lifetime() noexcept {
        return lifetime_;
    }

    virtual int resizeGetHeight(int newWidth);
    void resizeToWidth(int newWidth);

    [[nodiscard]] rpl::producer<int> widthValue() const;
    [[nodiscard]] rpl::producer<int> heightValue() const;
    [[nodiscard]] rpl::producer<QSize> sizeValue() const;
    [[nodiscard]] rpl::producer<QRect> geometryValue() const;
    [[nodiscard]] rpl::producer<bool> shownValue() const;
    [[nodiscard]] rpl::producer<QRect> paintRequest() const;

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    rpl::lifetime lifetime_;
    mutable rpl::event_stream<QRect> geometryStream_;
    mutable rpl::event_stream<bool> shownStream_;
    mutable rpl::event_stream<QRect> paintStream_;
};

} // namespace Ui
