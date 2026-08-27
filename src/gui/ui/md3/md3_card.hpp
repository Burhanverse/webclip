#pragma once

#include "../basic/rp_widget.hpp"
#include "../basic/ripple_button.hpp"
#include "md3_switch.hpp"

#include <QtGui/QPainterPath>
#include <QtWidgets/QVBoxLayout>
#include <functional>

namespace Ui {

enum class CardSegmentPosition {
    Single,
    Top,
    Middle,
    Bottom,
};

QPainterPath MakeSegmentPath(
    const QRectF& rect,
    CardSegmentPosition pos,
    double largeRadius = 24.0,
    double smallRadius = 4.0
);

QImage MakeSegmentMask(
    const QSize& size,
    CardSegmentPosition pos,
    double largeRadius = 24.0,
    double smallRadius = 4.0
);

class CardRow : public RippleButton {
    Q_OBJECT

public:
    explicit CardRow(
        QWidget* parent = nullptr,
        const QString& title = QString(),
        const QString& subtitle = QString(),
        const QString& iconName = QString()
    );
    ~CardRow() override;

    [[nodiscard]] QString title() const noexcept {
        return title_;
    }
    void setTitle(const QString& title);

    [[nodiscard]] QString subtitle() const noexcept {
        return subtitle_;
    }
    void setSubtitle(const QString& subtitle);

    [[nodiscard]] QString iconName() const noexcept {
        return iconName_;
    }
    void setIconName(const QString& iconName);

    [[nodiscard]] CardSegmentPosition segmentPosition() const noexcept {
        return segmentPosition_;
    }
    void setSegmentPosition(CardSegmentPosition pos);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override {
        return sizeHint();
    }

protected:
    void paintEvent(QPaintEvent* e) override;
    QImage prepareRippleMask() const override;

    QString title_;
    QString subtitle_;
    QString iconName_;
    CardSegmentPosition segmentPosition_ = CardSegmentPosition::Single;
};

class CardToggleRow : public CardRow {
    Q_OBJECT

public:
    explicit CardToggleRow(
        QWidget* parent = nullptr,
        const QString& title = QString(),
        const QString& subtitle = QString(),
        const QString& iconName = QString(),
        bool checked = false
    );

    [[nodiscard]] bool checked() const;
    void setChecked(bool checked, anim::type animated = anim::type::normal);

signals:
    void toggled(bool checked);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    Md3Switch* switch_ = nullptr;
};

class CardButtonRow : public CardRow {
    Q_OBJECT

public:
    explicit CardButtonRow(
        QWidget* parent = nullptr,
        const QString& title = QString(),
        const QString& subtitle = QString(),
        const QString& iconName = QString(),
        const QString& trailingValue = QString()
    );

    void setTrailingValue(const QString& val);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QString trailingValue_;
};

class CardContainer : public RpWidget {
    Q_OBJECT

public:
    explicit CardContainer(QWidget* parent = nullptr);
    ~CardContainer() override;

    void addRow(CardRow* row);
    void updateSegments();

private:
    QVBoxLayout* layout_ = nullptr;
    std::vector<CardRow*> rows_;
};

} // namespace Ui
