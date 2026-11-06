#pragma once

#include "../basic/rp_widget.hpp"

namespace Ui {

class DropOverlayWidget : public RpWidget {
    Q_OBJECT

public:
    explicit DropOverlayWidget(QWidget* parent = nullptr);
    ~DropOverlayWidget() override;

protected:
    void paintEvent(QPaintEvent* e) override;
};

}

