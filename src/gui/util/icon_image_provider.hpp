#pragma once

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QColor>
#include <QSvgRenderer>
#include <QPainter>
#include <QUrl>
#include <QUrlQuery>

namespace webclip {

class IconImageProvider : public QQuickImageProvider {
public:
    IconImageProvider();
    ~IconImageProvider() override = default;

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    QCache<QString, QImage> cache_;
};

}
