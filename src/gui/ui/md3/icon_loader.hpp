#pragma once

#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

namespace Ui {

class IconLoader {
public:
    static QPixmap loadPixmap(
        const QString& name,
        int size,
        const QColor& tintColor = QColor(),
        double dpr = 1.0
    );

    static void paint(
        QPainter& p,
        const QString& name,
        const QRectF& targetRect,
        const QColor& tintColor = QColor()
    );

    static void paint(
        QPainter& p,
        const QString& name,
        const QPointF& topLeft,
        double size,
        const QColor& tintColor = QColor()
    );

    static void clearCache();
};

} // namespace Ui
