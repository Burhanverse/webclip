#pragma once

#include <QQuickItem>
#include <QImage>
#include <QRectF>
#include <QElapsedTimer>
#include <QtQml/qqmlregistration.h>
#include <vector>
#include <memory>

namespace webclip {

struct ThanosParticle {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float lifetime = 0.0f;
    uint32_t pX = 0;
    uint32_t pY = 0;
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

struct ThanosInstance {
    QRectF targetRect;
    std::vector<ThanosParticle> particles;
    uint32_t particleCountX = 0;
    uint32_t particleCountY = 0;
    float particleSizeX = 1.0f;
    float particleSizeY = 1.0f;
    float phase = 0.0f;
    bool finished = false;
};

class ThanosEffectItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(ThanosEffect)

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int activeInstancesCount READ activeInstancesCount NOTIFY countChanged)

public:
    explicit ThanosEffectItem(QQuickItem* parent = nullptr);
    ~ThanosEffectItem() override;

    bool isRunning() const { return !instances_.empty(); }
    int activeInstancesCount() const { return static_cast<int>(instances_.size()); }

    Q_INVOKABLE void snapItem(QQuickItem* item);
    Q_INVOKABLE void snapImage(const QImage& image, const QRectF& rect);
    Q_INVOKABLE void clear();

signals:
    void runningChanged();
    void countChanged();
    void finished();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;

private slots:
    void onWindowBeforeRendering();

private:
    void addInstance(const QImage& image, const QRectF& sceneRect);
    void updateAnimation();

    std::vector<ThanosInstance> instances_;
    QElapsedTimer frameTimer_;
    qint64 lastFrameTimeNs_ = 0;
    bool animating_ = false;
    uint32_t seedCounter_ = 12345;
};

} // namespace webclip
