#include "thanos_effect_item.hpp"
#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGVertexColorMaterial>
#include <QQuickWindow>
#include <QQuickItemGrabResult>
#include <cmath>
#include <algorithm>

namespace webclip {

namespace {

constexpr float kMaxPhaseDuration = 6.0f;
constexpr float kPhaseSpeed = 1.65f;
constexpr float kTimeStepMultiplier = 1.65f;
constexpr float kAccelerationStartPhase = 1.0f;
constexpr float kAccelerationRampPhase = 2.5f;
constexpr float kAccelerationMaxMultiplier = 2.2f;
constexpr float kDisappearStartPhase = kMaxPhaseDuration * 0.15f;
constexpr float kDisappearDuration = kMaxPhaseDuration - kDisappearStartPhase;
constexpr uint32_t kMaxParticleCount = 120000;

uint32_t hashUint(uint32_t x) {
    x ^= x >> 16u;
    x *= 0x45d9f3bu;
    x ^= x >> 16u;
    x *= 0x45d9f3bu;
    x ^= x >> 16u;
    return x;
}

float hashFloat(uint32_t x) {
    return static_cast<float>(hashUint(x)) / static_cast<float>(0xFFFFFFFFu);
}

float AnimationSpeedMultiplier(float phase) {
    if (phase <= kAccelerationStartPhase) {
        return 1.0f;
    }
    const float t = std::clamp(
        (phase - kAccelerationStartPhase) / kAccelerationRampPhase,
        0.0f,
        1.0f);
    const float smooth = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    return 1.0f + ((kAccelerationMaxMultiplier - 1.0f) * smooth);
}

float DisappearProgress(float phase) {
    const float t = std::clamp(
        (phase - kDisappearStartPhase) / kDisappearDuration,
        0.0f,
        1.0f);
    const float oneMinus = 1.0f - t;
    return 1.0f - (oneMinus * oneMinus * oneMinus);
}

float easeInWindow(float fraction, float t) {
    const float windowSize = 0.8f;
    const float windowStart = -windowSize;
    const float windowEnd = 1.0f;
    const float windowPos = (1.0f - fraction) * windowStart + fraction * windowEnd;
    const float windowT = std::clamp((t - windowPos) / windowSize, 0.0f, 1.0f);
    return 1.0f - windowT;
}

}

ThanosEffectItem::ThanosEffectItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::NoButton);
    frameTimer_.start();
}

ThanosEffectItem::~ThanosEffectItem() = default;

void ThanosEffectItem::snapItem(QQuickItem* item) {
    if (!item || !window()) {
        return;
    }

    QSharedPointer<QQuickItemGrabResult> grab = item->grabToImage();
    if (!grab) {
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, grab, item]() {
        if (!item) {
            return;
        }
        const QImage img = grab->image();
        if (img.isNull()) {
            return;
        }
        const QPointF scenePos = this->mapFromItem(item, QPointF(0, 0));
        const QRectF targetRect(scenePos, QSizeF(item->width(), item->height()));
        snapImage(img, targetRect);
    });
}

void ThanosEffectItem::snapImage(const QImage& image, const QRectF& rect) {
    if (image.isNull() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }
    addInstance(image, rect);
}

void ThanosEffectItem::clear() {
    instances_.clear();
    update();
    emit runningChanged();
    emit countChanged();
}

void ThanosEffectItem::addInstance(const QImage& image, const QRectF& sceneRect) {
    const int w = static_cast<int>(sceneRect.width());
    const int h = static_cast<int>(sceneRect.height());
    if (w <= 0 || h <= 0 || image.isNull()) {
        return;
    }

    const uint32_t totalPixels = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
    uint32_t particleCountX = 0;
    uint32_t particleCountY = 0;
    if (totalPixels <= kMaxParticleCount) {
        particleCountX = static_cast<uint32_t>(w);
        particleCountY = static_cast<uint32_t>(h);
    } else {
        const double aspectRatio = static_cast<double>(w) / static_cast<double>(h);
        const double maxParticles = static_cast<double>(kMaxParticleCount);
        particleCountY = std::max(1u, static_cast<uint32_t>(std::sqrt(maxParticles / aspectRatio)));
        particleCountX = std::max(1u, static_cast<uint32_t>(maxParticles / static_cast<double>(particleCountY)));
    }

    ThanosInstance instance;
    instance.targetRect = sceneRect;
    instance.particleCountX = particleCountX;
    instance.particleCountY = particleCountY;
    instance.particleSizeX = static_cast<float>(sceneRect.width()) / static_cast<float>(particleCountX);
    instance.particleSizeY = static_cast<float>(sceneRect.height()) / static_cast<float>(particleCountY);
    instance.phase = 0.0f;
    instance.finished = false;

    const uint32_t seed = seedCounter_++;
    const QImage formatted = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int imgW = formatted.width();
    const int imgH = formatted.height();

    instance.particles.reserve(std::min<size_t>(particleCountX * particleCountY, 60000));

    for (uint32_t pY = 0; pY < particleCountY; ++pY) {
        const int sampleY = std::clamp(static_cast<int>((static_cast<float>(pY) / particleCountY) * imgH), 0, imgH - 1);
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(formatted.constScanLine(sampleY));

        for (uint32_t pX = 0; pX < particleCountX; ++pX) {
            const int sampleX = std::clamp(static_cast<int>((static_cast<float>(pX) / particleCountX) * imgW), 0, imgW - 1);
            const QRgb pixel = scanLine[sampleX];
            const int alpha = qAlpha(pixel);
            if (alpha < 8) {
                continue;
            }

            const uint32_t gid = pY * particleCountX + pX;
            const uint32_t s = gid * 3u + seed;

            const float direction = hashFloat(s) * (3.14159265358979323846f * 2.0f);
            const float speed = (0.1f + hashFloat(s + 1u) * 0.1f) * 320.0f;
            const float lifetime = 1.5f + hashFloat(s + 2u) * 1.5f;

            ThanosParticle p;
            p.offsetX = 0.0f;
            p.offsetY = 0.0f;
            p.vx = std::cos(direction) * speed;
            p.vy = std::sin(direction) * speed;
            p.lifetime = lifetime;
            p.pX = pX;
            p.pY = pY;
            p.r = static_cast<uint8_t>(qRed(pixel));
            p.g = static_cast<uint8_t>(qGreen(pixel));
            p.b = static_cast<uint8_t>(qBlue(pixel));
            p.a = static_cast<uint8_t>(alpha);

            instance.particles.push_back(p);
        }
    }

    const bool wasEmpty = instances_.empty();
    instances_.push_back(std::move(instance));

    if (wasEmpty) {
        lastFrameTimeNs_ = frameTimer_.nsecsElapsed();
        emit runningChanged();
    }
    emit countChanged();
    update();
}

void ThanosEffectItem::onWindowBeforeRendering() {
    if (!instances_.empty()) {
        update();
    }
}

QSGNode* ThanosEffectItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    if (instances_.empty()) {
        delete oldNode;
        return nullptr;
    }

    const qint64 currentNs = frameTimer_.nsecsElapsed();
    const float dt = std::clamp(
        static_cast<float>(currentNs - lastFrameTimeNs_) / 1e9f,
        0.001f,
        0.066f
    );
    lastFrameTimeNs_ = currentNs;

    size_t totalAliveParticles = 0;
    constexpr float easeInDuration = 0.8f;

    for (auto& item : instances_) {
        if (item.phase >= kMaxPhaseDuration) {
            item.finished = true;
            continue;
        }

        const float animationTimeStep = dt * AnimationSpeedMultiplier(item.phase);
        item.phase += animationTimeStep * kPhaseSpeed;
        const float timeStep = animationTimeStep * kTimeStepMultiplier;
        const float effectFraction = std::clamp(item.phase / easeInDuration, 0.0f, 1.0f);
        const float inverseDisappear = float(1.0f - DisappearProgress(item.phase));

        bool anyAlive = false;

        for (auto& p : item.particles) {
            const float particleXFraction = static_cast<float>(p.pX) / static_cast<float>(item.particleCountX);
            const float particleFraction = easeInWindow(effectFraction, particleXFraction);

            p.offsetX += p.vx * timeStep * particleFraction;
            p.offsetY += p.vy * timeStep * particleFraction;

            p.vy -= 80.0f * timeStep * particleFraction;
            p.lifetime = std::max(0.0f, p.lifetime - 0.6f * timeStep * particleFraction);

            const float alpha = std::clamp(p.lifetime / 0.6f, 0.0f, 1.0f) * inverseDisappear;
            if (alpha > 0.001f) {
                anyAlive = true;
                ++totalAliveParticles;
            }
        }

        if (!anyAlive || item.phase >= kMaxPhaseDuration) {
            item.finished = true;
        }
    }

    const size_t beforeSize = instances_.size();
    instances_.erase(
        std::remove_if(instances_.begin(), instances_.end(), [](const ThanosInstance& inst) {
            return inst.finished;
        }),
        instances_.end()
    );

    if (instances_.size() != beforeSize) {
        emit countChanged();
        if (instances_.empty()) {
            emit runningChanged();
            emit finished();
            delete oldNode;
            return nullptr;
        }
    }

    if (totalAliveParticles == 0) {
        instances_.clear();
        emit runningChanged();
        emit countChanged();
        emit finished();
        delete oldNode;
        return nullptr;
    }

    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode();
        auto* material = new QSGVertexColorMaterial();
        node->setMaterial(material);
        node->setFlags(QSGNode::OwnsMaterial | QSGNode::OwnsGeometry);
    }

    const int vertexCount = static_cast<int>(totalAliveParticles * 6);
    auto* geom = node->geometry();
    if (!geom || geom->vertexCount() != vertexCount) {
        geom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), vertexCount);
        geom->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(geom);
    }

    auto* vertices = geom->vertexDataAsColoredPoint2D();
    int vIdx = 0;

    for (const auto& item : instances_) {
        if (item.finished) continue;

        const float inverseDisappear = float(1.0f - DisappearProgress(item.phase));
        const float scaleFactor = inverseDisappear;
        const float halfWidth = item.particleSizeX * 0.5f * scaleFactor;
        const float halfHeight = item.particleSizeY * 0.5f * scaleFactor;

        for (const auto& p : item.particles) {
            const float alpha = std::clamp(p.lifetime / 0.6f, 0.0f, 1.0f) * inverseDisappear;
            if (alpha <= 0.001f) {
                continue;
            }

            const float topLeftX = item.targetRect.x() + (static_cast<float>(p.pX) * item.particleSizeX) + p.offsetX;
            const float topLeftY = item.targetRect.y() + (static_cast<float>(p.pY) * item.particleSizeY) + p.offsetY;

            const float centerX = topLeftX + (item.particleSizeX * 0.5f);
            const float centerY = topLeftY + (item.particleSizeY * 0.5f);

            const float left = centerX - halfWidth;
            const float right = centerX + halfWidth;
            const float top = centerY - halfHeight;
            const float bottom = centerY + halfHeight;

            const uint8_t finalAlpha = static_cast<uint8_t>(std::clamp(p.a * alpha, 0.0f, 255.0f));
            const uint8_t r = static_cast<uint8_t>((p.r * finalAlpha) / 255);
            const uint8_t g = static_cast<uint8_t>((p.g * finalAlpha) / 255);
            const uint8_t b = static_cast<uint8_t>((p.b * finalAlpha) / 255);

            vertices[vIdx].x = left;
            vertices[vIdx].y = top;
            vertices[vIdx].r = r; vertices[vIdx].g = g; vertices[vIdx].b = b; vertices[vIdx].a = finalAlpha;
            ++vIdx;

            vertices[vIdx].x = right;
            vertices[vIdx].y = top;
            vertices[vIdx].r = r; vertices[vIdx].g = g; vertices[vIdx].b = b; vertices[vIdx].a = finalAlpha;
            ++vIdx;

            vertices[vIdx].x = left;
            vertices[vIdx].y = bottom;
            vertices[vIdx].r = r; vertices[vIdx].g = g; vertices[vIdx].b = b; vertices[vIdx].a = finalAlpha;
            ++vIdx;

            vertices[vIdx].x = right;
            vertices[vIdx].y = top;
            vertices[vIdx].r = r; vertices[vIdx].g = g; vertices[vIdx].b = b; vertices[vIdx].a = finalAlpha;
            ++vIdx;

            vertices[vIdx].x = right;
            vertices[vIdx].y = bottom;
            vertices[vIdx].r = r; vertices[vIdx].g = g; vertices[vIdx].b = b; vertices[vIdx].a = finalAlpha;
            ++vIdx;

            vertices[vIdx].x = left;
            vertices[vIdx].y = bottom;
            vertices[vIdx].r = r; vertices[vIdx].g = g; vertices[vIdx].b = b; vertices[vIdx].a = finalAlpha;
            ++vIdx;
        }
    }

    geom->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);

    update();

    return node;
}

}
