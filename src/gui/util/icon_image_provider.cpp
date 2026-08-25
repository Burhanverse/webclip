#include "icon_image_provider.hpp"
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace webclip {

IconImageProvider::IconImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
    cache_.setMaxCost(500);
}

QImage IconImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    if (QImage* cached = cache_.object(id)) {
        if (size) *size = cached->size();
        return *cached;
    }

    QUrl url("icon:///" + id);
    QString name = url.path();
    if (name.startsWith('/')) name = name.mid(1);

    QUrlQuery query(url.query());
    QString colorStr = query.queryItemValue("color");
    QColor tintColor = colorStr.isEmpty() ? QColor() : QColor(colorStr);

    int targetPixelSize = 48;
    QString sizeStr = query.queryItemValue("size");
    if (!sizeStr.isEmpty()) {
        bool ok = false;
        int parsedSize = sizeStr.toInt(&ok);
        if (ok && parsedSize > 0) targetPixelSize = parsedSize;
    }
    if (requestedSize.width() > 0 && requestedSize.height() > 0) {
        targetPixelSize = qMax(requestedSize.width(), requestedSize.height());
    }

    QStringList possiblePaths = {
        QStringLiteral(":/qt/qml/src/gui/resources/icons/%1.svg").arg(name),
        QStringLiteral(":/src/gui/resources/icons/%1.svg").arg(name),
        QStringLiteral(":/icons/%1.svg").arg(name),
        QStringLiteral(":/qt/qml/WebClip/src/gui/resources/icons/%1.svg").arg(name)
    };

    QString foundPath;
    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            foundPath = path;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        QImage emptyImg(targetPixelSize, targetPixelSize, QImage::Format_ARGB32_Premultiplied);
        emptyImg.fill(Qt::transparent);
        if (size) *size = emptyImg.size();
        return emptyImg;
    }

    QSvgRenderer renderer(foundPath);
    if (!renderer.isValid()) {
        QImage emptyImg(targetPixelSize, targetPixelSize, QImage::Format_ARGB32_Premultiplied);
        emptyImg.fill(Qt::transparent);
        if (size) *size = emptyImg.size();
        return emptyImg;
    }

    QImage img(targetPixelSize, targetPixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&p, QRectF(0, 0, targetPixelSize, targetPixelSize));

        if (tintColor.isValid() && tintColor.alpha() > 0) {
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(img.rect(), tintColor);
        }
    }

    if (size) *size = img.size();
    cache_.insert(id, new QImage(img));
    return img;
}

}
