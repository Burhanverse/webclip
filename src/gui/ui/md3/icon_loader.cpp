#include "icon_loader.hpp"
#include "../basic/painter_helpers.hpp"

#include <QtCore/QCache>
#include <QtCore/QFile>
#include <QtSvg/QSvgRenderer>

namespace Ui {

namespace {

QCache<QString, QPixmap>& getCache() {
    static QCache<QString, QPixmap> cache(1000);
    return cache;
}

QString findIconPath(const QString& name) {
    const QStringList possiblePaths = {
        QStringLiteral(":/qt/qml/src/gui/resources/icons/%1.svg").arg(name),
        QStringLiteral(":/src/gui/resources/icons/%1.svg").arg(name),
        QStringLiteral(":/gui/resources/icons/%1.svg").arg(name),
        QStringLiteral(":/icons/%1.svg").arg(name),
        QStringLiteral(":/qt/qml/WebClip/src/gui/resources/icons/%1.svg").arg(name)
    };
    for (const auto& path : possiblePaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

} // namespace

QPixmap IconLoader::loadPixmap(
    const QString& name,
    int size,
    const QColor& tintColor,
    double dpr
) {
    const int effectiveDpr = std::max(1.0, dpr);
    const int pixelSize = std::max(8, static_cast<int>(std::round(size * effectiveDpr)));
    const QString cacheKey = QStringLiteral("%1_%2_%3_%4")
        .arg(name)
        .arg(pixelSize)
        .arg(tintColor.isValid() ? tintColor.rgba() : 0)
        .arg(static_cast<int>(effectiveDpr * 100));

    if (auto* cached = getCache().object(cacheKey)) {
        return *cached;
    }

    const QString path = findIconPath(name);
    if (path.isEmpty()) {
        QPixmap empty(size, size);
        empty.fill(Qt::transparent);
        return empty;
    }

    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        QPixmap empty(size, size);
        empty.fill(Qt::transparent);
        return empty;
    }

    QImage img(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        PainterHighQualityEnabler hq(p);
        renderer.render(&p, QRectF(0, 0, pixelSize, pixelSize));

        if (tintColor.isValid()) {
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(img.rect(), tintColor);
        }
    }
    img.setDevicePixelRatio(effectiveDpr);

    auto result = std::make_unique<QPixmap>(QPixmap::fromImage(img));
    QPixmap ret = *result;
    getCache().insert(cacheKey, result.release());
    return ret;
}

void IconLoader::paint(
    QPainter& p,
    const QString& name,
    const QRectF& targetRect,
    const QColor& tintColor
) {
    const auto dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    const int size = std::max(1, static_cast<int>(std::round(std::max(targetRect.width(), targetRect.height()))));
    const QPixmap pix = loadPixmap(name, size, tintColor, dpr);

    PainterHighQualityEnabler hq(p);
    p.drawPixmap(targetRect.toRect(), pix);
}

void IconLoader::paint(
    QPainter& p,
    const QString& name,
    const QPointF& topLeft,
    double size,
    const QColor& tintColor
) {
    paint(p, name, QRectF(topLeft.x(), topLeft.y(), size, size), tintColor);
}

void IconLoader::clearCache() {
    getCache().clear();
}

} // namespace Ui
