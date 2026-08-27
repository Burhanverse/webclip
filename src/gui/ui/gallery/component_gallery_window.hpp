#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QScrollArea>

namespace Ui {

class ComponentGalleryWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ComponentGalleryWindow(QWidget* parent = nullptr);
    ~ComponentGalleryWindow() override;

private:
    void setupUi();
    void applyTheme(int mode);

    QWidget* centralContent_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
};

} // namespace Ui
