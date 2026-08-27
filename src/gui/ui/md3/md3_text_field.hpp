#pragma once

#include "../basic/rp_widget.hpp"
#include <QtWidgets/QLineEdit>

namespace Ui {

class Md3TextField : public RpWidget {
    Q_OBJECT

public:
    explicit Md3TextField(
        QWidget* parent = nullptr,
        const QString& label = QString(),
        const QString& placeholder = QString()
    );
    ~Md3TextField() override;

    [[nodiscard]] QString text() const;
    void setText(const QString& text);

    [[nodiscard]] QString label() const noexcept {
        return label_;
    }
    void setLabel(const QString& label);

    [[nodiscard]] QString placeholder() const;
    void setPlaceholder(const QString& placeholder);

    void setEchoMode(QLineEdit::EchoMode mode);
    [[nodiscard]] QLineEdit::EchoMode echoMode() const;

    [[nodiscard]] QLineEdit* lineEdit() const noexcept {
        return lineEdit_;
    }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override {
        return sizeHint();
    }

signals:
    void textChanged(const QString& text);
    void returnPressed();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void updateLayout();

    QString label_;
    QLineEdit* lineEdit_ = nullptr;
};

} // namespace Ui
