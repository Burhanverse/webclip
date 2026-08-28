#include "webclip_controller.hpp"
#include "../theme/md3_theme.hpp"
#include "../../util/json.hpp"
#include "../../util/base64.hpp"
#include "../../util/cli.hpp"
#include <QMetaObject>
#include <QDesktopServices>
#include <QUrl>
#include <QPointer>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QMimeDatabase>
#include <QMimeData>
#include <QRandomGenerator>
#include <QImageReader>

namespace webclip {

static int maxCachedClipImages() {
    static const int cached = []() {
        int v = qEnvironmentVariableIntValue("WEBCLIP_MAX_IMAGE_CACHE");
        return v > 0 ? v : 200;
    }();
    return cached;
}

static void prune_image_cache(const QString& cacheDir) {
    QDir dir(cacheDir);
    int maxClips = maxCachedClipImages();
    const QStringList fileList = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (fileList.size() <= maxClips) {
        return;
    }
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
    for (int i = maxClips; i < entries.size(); ++i) {
        QFile::remove(entries.at(i).absoluteFilePath());
    }
}

static void trigger_async_cache_prune(const QString& cacheDir) {
    static std::atomic<int> s_writesSinceLastPrune{0};
    if (++s_writesSinceLastPrune >= 15) {
        s_writesSinceLastPrune.store(0);
        std::thread([cacheDir]() {
            prune_image_cache(cacheDir);
        }).detach();
    }
}

static QString saveImageBytesToCache(const QByteArray& bytes, const QString& mimeType) {
    if (bytes.isEmpty()) return QString();
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/clips";
    QDir().mkpath(cacheDir);
    QString clipId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString ext = mimeType.contains("jpeg") ? ".jpg" : (mimeType.contains("webp") ? ".webp" : ".png");
    QString filePath = cacheDir + "/" + clipId + ext;
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);
        file.close();
        trigger_async_cache_prune(cacheDir);
        return QUrl::fromLocalFile(filePath).toString();
    }
    return QString();
}

std::string WebClipController::generateClipId() {
    uint64_t ms = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
    uint32_t r1 = static_cast<uint32_t>(QRandomGenerator::global()->generate());
    uint32_t r2 = static_cast<uint32_t>(QRandomGenerator::global()->generate());
    char buf[64];
    std::snprintf(buf, sizeof(buf), "clip-%llx-%08x%08x", (unsigned long long)ms, r1, r2);
    return std::string(buf);
}

bool WebClipController::isClipIdHandled(const std::string& clipId) {
    if (clipId.empty()) return false;
    std::lock_guard<std::mutex> guard(syncLock_);
    return handledClipIdSet_.find(clipId) != handledClipIdSet_.end();
}

void WebClipController::markClipIdHandled(const std::string& clipId) {
    if (clipId.empty()) return;
    std::lock_guard<std::mutex> guard(syncLock_);
    if (handledClipIdSet_.insert(clipId).second) {
        handledClipIds_.push_back(clipId);
        while (handledClipIds_.size() > 128) {
            handledClipIdSet_.erase(handledClipIds_.front());
            handledClipIds_.pop_front();
        }
    }
}

QString WebClipController::computeImageHash(const QByteArray& data) {
    const int64_t len = data.size();
    if (len == 0) return "";
    uint64_t hash = 14695981039346656037ULL;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());

    if (len > 65536) {
        // Fast sampled hash for large images:
        // Sample head 8KB
        const int64_t headLen = 8192;
        for (int64_t i = 0; i < headLen; i += 8) {
            uint64_t w;
            std::memcpy(&w, ptr + i, 8);
            hash ^= w;
            hash *= 1099511628211ULL;
        }
        // Sample tail 8KB
        const int64_t tailStart = len - 8192;
        for (int64_t i = tailStart; i + 8 <= len; i += 8) {
            uint64_t w;
            std::memcpy(&w, ptr + i, 8);
            hash ^= w;
            hash *= 1099511628211ULL;
        }
        // Sample body across middle in strides
        const int64_t midSpan = tailStart - headLen;
        if (midSpan > 0) {
            const int64_t step = std::max<int64_t>(8, (midSpan / 1024) & ~7ULL);
            for (int64_t i = headLen; i + 8 <= tailStart; i += step) {
                uint64_t w;
                std::memcpy(&w, ptr + i, 8);
                hash ^= w;
                hash *= 1099511628211ULL;
            }
        }
    } else {
        int64_t i = 0;
        for (; i + 8 <= len; i += 8) {
            uint64_t w;
            std::memcpy(&w, ptr + i, 8);
            hash ^= w;
            hash *= 1099511628211ULL;
        }
        for (; i < len; ++i) {
            hash ^= ptr[i];
            hash *= 1099511628211ULL;
        }
    }
    return QString::number(hash, 16) + "-" + QString::number(len);
}

QString WebClipController::computePixelFingerprint(const QImage& img) {
    if (img.isNull()) return "";
    constexpr int kMaxDim = 256;
    QSize target = img.size();
    if (target.width() > kMaxDim || target.height() > kMaxDim) {
        target.scale(kMaxDim, kMaxDim, Qt::KeepAspectRatio);
    }
    const QImage sampled = (target == img.size())
        ? img
        : img.scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    const QImage converted = sampled.convertToFormat(QImage::Format_RGBA8888);
    uint64_t hash = 14695981039346656037ULL;
    hash ^= static_cast<uint64_t>(img.width());
    hash *= 1099511628211ULL;
    hash ^= static_cast<uint64_t>(img.height());
    hash *= 1099511628211ULL;

    const uint8_t* bits = converted.constBits();
    int64_t byteCount = converted.sizeInBytes();
    int64_t step = std::max<int64_t>(1, byteCount / 16384);
    for (int64_t i = 0; i < byteCount; i += step) {
        hash ^= bits[i];
        hash *= 1099511628211ULL;
    }

    return "px:" + QString::number(img.width()) + "x" + QString::number(img.height()) + "-" + QString::number(hash, 16);
}

QString WebClipController::computePixelFingerprint(const QByteArray& data) {
    if (data.isEmpty()) return "";
    QImageReader reader;
    reader.setAutoTransform(true);
    QBuffer buf;
    buf.setBuffer(const_cast<QByteArray*>(&data));
    buf.open(QIODevice::ReadOnly);
    reader.setDevice(&buf);
    return computePixelFingerprint(reader.read());
}

bool WebClipController::shouldSuppressText(const QString& text, int64_t nowMs, int64_t windowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return true;
    if (trimmed == lastLocalText_.trimmed() || trimmed == lastRemoteText_.trimmed()) {
        return true;
    }
    if ((nowMs - lastTextTimeMs_) < windowMs) {
        return true;
    }
    return false;
}

void WebClipController::markTextApplied(const QString& text, int64_t nowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    lastLocalText_ = text;
    lastRemoteText_ = text;
    lastTextTimeMs_ = nowMs;
}

bool WebClipController::shouldSuppressImage(const QString& hash, const QString& pixelFp, int64_t nowMs, int64_t windowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    if (!hash.isEmpty()) {
        if (hash == lastLocalImgHash_ || hash == lastRemoteImgHash_) return true;
    }
    if (!pixelFp.isEmpty()) {
        if (pixelFp == lastLocalPixelFp_ || pixelFp == lastRemotePixelFp_) return true;
    }
    if ((nowMs - lastImgTimeMs_) < windowMs) {
        return true;
    }
    return false;
}

void WebClipController::markImageApplied(const QString& hash, const QString& pixelFp, int64_t nowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    lastLocalImgHash_ = hash;
    lastRemoteImgHash_ = hash;
    lastLocalPixelFp_ = pixelFp;
    lastRemotePixelFp_ = pixelFp;
    lastImgTimeMs_ = nowMs;
}

WebClipController::WebClipController(QObject* parent)
    : QObject(parent) {
    nativeClipboard_ = create_clipboard();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &WebClipController::onPollTimer);

    if (QGuiApplication::clipboard()) {
        connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &WebClipController::onClipboardDataChanged);
    }

    loadSettings();

    if (QGuiApplication::clipboard()) {
        QImage img = QGuiApplication::clipboard()->image();
        if (!img.isNull()) {
            QByteArray ba;
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            lastLocalImgHash_ = computeImageHash(ba);
            QString fileUrl = saveImageBytesToCache(ba, "image/png");
            clipModel_.addClipImage(fileUrl, "image/png", ba.size(), "local");
        } else {
            QString current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
            if (current.isEmpty() && nativeClipboard_) {
                current = QString::fromStdString(nativeClipboard_->get_text());
            }
            if (!current.isEmpty()) {
                lastLocalText_ = current;
                clipModel_.addClip(lastLocalText_, "local");
            }
        }
    }
}

WebClipController::~WebClipController() {
    blockSignals(true);
    disconnectFromPortal();
    saveSettings();
}

QString WebClipController::clipboardBackend() const {
    return nativeClipboard_ ? QString::fromStdString(nativeClipboard_->get_backend_name()) : "Qt Internal";
}

void WebClipController::setHost(const QString& host) {
    if (host_ != host) {
        host_ = host;
        saveSettings();
        emit hostChanged();
    }
}

void WebClipController::setPort(int port) {
    if (port_ != port) {
        port_ = port;
        saveSettings();
        emit portChanged();
    }
}

void WebClipController::setCode(const QString& code) {
    if (code_ != code) {
        code_ = code;
        saveSettings();
        emit codeChanged();
    }
}

void WebClipController::setUseHttps(bool useHttps) {
    if (useHttps_ != useHttps) {
        useHttps_ = useHttps;
        saveSettings();
        emit useHttpsChanged();
    }
}

void WebClipController::setInsecure(bool insecure) {
    if (insecure_ != insecure) {
        insecure_ = insecure;
        saveSettings();
        emit insecureChanged();
    }
}

void WebClipController::setAutoConnect(bool autoConnect) {
    if (autoConnect_ != autoConnect) {
        autoConnect_ = autoConnect;
        saveSettings();
        emit autoConnectChanged();
    }
}

void WebClipController::setAutoSync(bool autoSync) {
    if (autoSync_ != autoSync) {
        autoSync_ = autoSync;
        saveSettings();
        emit autoSyncChanged();
        if (connected_) {
            if (autoSync_) {
                pollTimer_->start(static_cast<int>(pollInterval_ * 1000));
            } else {
                pollTimer_->stop();
            }
        }
    }
}

void WebClipController::setPollInterval(double interval) {
    if (interval < 0.2) interval = 0.2;
    if (interval > 10.0) interval = 10.0;
    if (pollInterval_ != interval) {
        pollInterval_ = interval;
        saveSettings();
        emit pollIntervalChanged();
        if (pollTimer_->isActive()) {
            pollTimer_->setInterval(static_cast<int>(pollInterval_ * 1000));
        }
    }
}

void WebClipController::setThemeMode(int mode) {
    if (themeMode_ != mode) {
        themeMode_ = mode;
        MD3Theme::instance()->setThemeMode(mode);
        saveSettings();
        emit themeModeChanged();
    }
}

void WebClipController::setAccentPreset(const QString& preset) {
    if (accentPreset_ != preset) {
        accentPreset_ = preset;
        MD3Theme::instance()->setAccentPreset(preset);
        saveSettings();
        emit accentPresetChanged();
    }
}

void WebClipController::setCustomColor(const QColor& color) {
    if (customColor_ != color && color.isValid()) {
        customColor_ = color;
        accentPreset_ = "custom";
        MD3Theme::instance()->setCustomColor(color);
        saveSettings();
        emit customColorChanged();
        emit accentPresetChanged();
    }
}

void WebClipController::setConnected(bool c) {
    if (connected_ != c) {
        connected_ = c;
        emit connectedChanged();
    }
}

void WebClipController::setConnecting(bool c) {
    if (connecting_ != c) {
        connecting_ = c;
        emit connectingChanged();
    }
}

void WebClipController::sanitizeHostInput() {
    SyncConfig config;
    config.host = host_.toStdString();
    config.port = port_;
    config.use_https = useHttps_;
    config.insecure = insecure_;

    sanitize_host_and_port(config, port_ != 8080 && port_ != 8081);

    host_ = QString::fromStdString(config.host);
    port_ = config.port;
    useHttps_ = config.use_https;
    insecure_ = config.insecure;

    emit hostChanged();
    emit portChanged();
    emit useHttpsChanged();
    emit insecureChanged();
}

void WebClipController::autoConnectOnStartup() {
    if (!autoConnect_) return;
    if (connected_ || connecting_) return;
    if (host_.trimmed().isEmpty() || code_.trimmed().isEmpty()) return;
    connectToPortal();
}

void WebClipController::connectToPortal() {
    if (connected_ || connecting_) return;

    sanitizeHostInput();

    if (host_.trimmed().isEmpty()) {
        emit showToast("Please enter a valid phone IP or hostname", true);
        return;
    }

    setConnecting(true);

    clientId_ = generate_random_client_id();
    auto client = std::make_shared<HttpClient>(
        host_.toStdString(),
        port_,
        code_.toStdString(),
        useHttps_,
        insecure_,
        clientId_
    );
    httpClient_ = client;

    QPointer<WebClipController> self(this);
    std::thread([self, client]() {
        HttpResponse stateResp = client->get_state();
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, stateResp, client]() {
            if (!self) return;
            if (stateResp.status_code == 200) {
                JsonValue stateJson = JsonValue::parse(stateResp.body);
                std::string type = stateJson.get_string("type");
                bool hasImage = (type == "image") || stateJson.get_bool("hasImage");

                if (hasImage) {
                    std::string imageUrl = stateJson.get_string("imageUrl");
                    std::string mimeType = stateJson.get_string("mimeType");
                    if (mimeType.empty()) mimeType = "image/png";

                    std::thread([self, client, imageUrl, mimeType]() {
                        HttpResponse imgResp = client->get_image(imageUrl);
                        if (!self) return;
                        if (imgResp.status_code != 200 || imgResp.binary_body.empty()) return;
                        QByteArray bytes(reinterpret_cast<const char*>(imgResp.binary_body.data()), static_cast<int>(imgResp.binary_body.size()));
                        std::vector<uint8_t>().swap(imgResp.binary_body);

                        QImage qimg;
                        qimg.loadFromData(bytes);
                        QString pixelFp = computePixelFingerprint(qimg);
                        QString hash = computeImageHash(bytes);
                        int byteSize = bytes.size();
                        QString fileUrl = saveImageBytesToCache(bytes, QString::fromStdString(mimeType));
                        QString mimeQ = QString::fromStdString(mimeType);
                        QByteArray().swap(bytes);

                        int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
                        self->markImageApplied(hash, pixelFp, nowMs);

                        QMetaObject::invokeMethod(self.data(), [self, fileUrl, qimg, mimeQ, byteSize]() {
                            if (!self) return;
                            if (!qimg.isNull()) {
                                self->suppressNextLocalChange_.store(true);
                                if (QGuiApplication::clipboard()) {
                                    QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
                                } else if (self->nativeClipboard_) {
                                    QString localPath = QUrl(fileUrl).toLocalFile();
                                    QFile f(localPath);
                                    QByteArray diskBytes;
                                    if (f.open(QIODevice::ReadOnly)) {
                                        diskBytes = f.readAll();
                                        f.close();
                                    }
                                    if (!diskBytes.isEmpty()) {
                                        std::vector<uint8_t> stdBytes(diskBytes.begin(), diskBytes.end());
                                        self->nativeClipboard_->set_image(stdBytes, mimeQ.toStdString());
                                    }
                                }
                            }
                            self->clipModel_.addClipImage(fileUrl, mimeQ, byteSize, "phone");
                        });
                    }).detach();
                } else {
                    std::string remoteText = stateJson.get_string("text");
                    if (!remoteText.empty()) {
                        QString qRemoteText = QString::fromStdString(remoteText);
                        int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
                        self->markTextApplied(qRemoteText, nowMs);

                        self->suppressNextLocalChange_.store(true);
                        if (QGuiApplication::clipboard()) {
                            QGuiApplication::clipboard()->setText(qRemoteText, QClipboard::Clipboard);
                        } else if (self->nativeClipboard_) {
                            self->nativeClipboard_->set_text(remoteText);
                        }
                        self->clipModel_.addClip(qRemoteText, "phone");
                    }
                }

                self->setConnecting(false);
                self->setConnected(true);
                emit self->showToast("Connected to Gboard Web Clipboard", false);

                if (self->autoSync_) {
                    self->pollTimer_->start(static_cast<int>(self->pollInterval_ * 1000));
                }

                self->startSseListener();
            } else {
                self->setConnecting(false);
                self->setConnected(false);
                QString err = stateResp.status_code == 401
                    ? "Invalid pairing code"
                    : (stateResp.error.empty() ? ("HTTP " + QString::number(stateResp.status_code)) : QString::fromStdString(stateResp.error));
                emit self->showToast("Failed to connect: " + err, true);
            }
        });
    }).detach();
}

void WebClipController::disconnectFromPortal() {
    stopSseListener();
    pollTimer_->stop();
    setConnecting(false);
    setConnected(false);
}

void WebClipController::toggleConnection() {
    if (connected_ || connecting_) {
        disconnectFromPortal();
    } else {
        connectToPortal();
    }
}

void WebClipController::startSseListener() {
    stopSseListener();
    sseStopFlag_->store(false);

    QPointer<WebClipController> self(this);
    auto client = httpClient_;
    auto stopFlag = sseStopFlag_;

    sseThread_ = std::make_unique<std::thread>([self, client, stopFlag]() {
        if (!client || !stopFlag) return;
        client->stream_events(
            [self, client](const SseEvent& ev) {
                WebClipController* c = self.data();
                if (!c) return;
                if (ev.event == "clipboard") {
                    JsonValue data = JsonValue::parse(ev.data);
                    std::string type = data.get_string("type");
                    std::string source = data.get_string("source");
                    std::string evClientId = data.get_string("clientId");
                    std::string clipId = data.get_string("clipId");

                    if (!clipId.empty() && c->isClipIdHandled(clipId)) {
                        return;
                    }
                    if (!clipId.empty()) {
                        c->markClipIdHandled(clipId);
                    }

                    if (source == "web" || (!evClientId.empty() && evClientId == c->clientId_)) {
                        return;
                    }

                    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();

                    if (type == "image") {
                        std::string mimeType = data.get_string("mimeType");
                        if (mimeType.empty()) mimeType = "image/png";
                        std::string inlineData = data.take_string("data");

                        if (!inlineData.empty()) {
                            static const bool s_perfLog = (std::getenv("WEBCLIP_PERF") != nullptr || std::getenv("WEBCLIP_DEBUG_PERF") != nullptr);
                            auto tSseDispatch = std::chrono::steady_clock::now();

                            std::thread([self, inlineData = std::move(inlineData), mimeType = std::move(mimeType), source = std::move(source), nowMs]() mutable {
                                auto tStart = std::chrono::steady_clock::now();
                                WebClipController* c = self.data();
                                if (!c) return;

                                QByteArray bytes;
                                size_t commaIdx = inlineData.find(',');
                                if (commaIdx != std::string::npos) {
                                    bytes = QByteArray::fromBase64(QByteArray::fromRawData(inlineData.data() + commaIdx + 1, static_cast<int>(inlineData.size() - commaIdx - 1)));
                                } else {
                                    bytes = QByteArray::fromBase64(QByteArray::fromRawData(inlineData.data(), static_cast<int>(inlineData.size())));
                                }
                                std::string().swap(inlineData);

                                if (bytes.isEmpty()) return;

                                QImage qimg;
                                qimg.loadFromData(bytes);
                                QString pixelFp = computePixelFingerprint(qimg);
                                QString hash = computeImageHash(bytes);
                                c->markImageApplied(hash, pixelFp, nowMs);

                                QString fileUrl = saveImageBytesToCache(bytes, QString::fromStdString(mimeType));
                                int byteSize = bytes.size();
                                QString mimeQ = QString::fromStdString(mimeType);
                                QString sourceQ = QString::fromStdString(source.empty() ? "phone" : source);
                                QByteArray().swap(bytes);

                                static const bool s_perfLog = (std::getenv("WEBCLIP_PERF") != nullptr || std::getenv("WEBCLIP_DEBUG_PERF") != nullptr);
                                if (s_perfLog) {
                                    auto tEnd = std::chrono::steady_clock::now();
                                    auto durMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
                                    std::cerr << "[PERF] Incoming inline image decode & cache took " << durMs << "ms (size=" << byteSize << " bytes)\n";
                                }

                                QMetaObject::invokeMethod(c, [self, fileUrl, qimg = std::move(qimg), mimeQ, sourceQ, byteSize]() {
                                    WebClipController* c = self.data();
                                    if (!c) return;
                                    if (!qimg.isNull()) {
                                        c->suppressNextLocalChange_.store(true);
                                        if (QGuiApplication::clipboard()) {
                                            QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
                                        } else if (c->nativeClipboard_) {
                                            QString localPath = QUrl(fileUrl).toLocalFile();
                                            QFile f(localPath);
                                            QByteArray diskBytes;
                                            if (f.open(QIODevice::ReadOnly)) {
                                                diskBytes = f.readAll();
                                                f.close();
                                            }
                                            if (!diskBytes.isEmpty()) {
                                                std::vector<uint8_t> stdBytes(diskBytes.begin(), diskBytes.end());
                                                c->nativeClipboard_->set_image(stdBytes, mimeQ.toStdString());
                                            }
                                        }
                                    }

                                    c->clipModel_.addClipImage(fileUrl, mimeQ, byteSize, sourceQ);
                                    emit c->clipReceived("[Image]", sourceQ);
                                });
                            }).detach();

                            if (s_perfLog) {
                                auto tSseNow = std::chrono::steady_clock::now();
                                auto durUs = std::chrono::duration_cast<std::chrono::microseconds>(tSseNow - tSseDispatch).count();
                                std::cerr << "[PERF] SSE stream dispatched inline image in " << durUs << "us\n";
                            }
                        } else {
                            std::string imageUrl = data.get_string("imageUrl");
                            std::thread([self, client, imageUrl, mimeType, source, nowMs]() {
                                HttpResponse imgResp = client->get_image(imageUrl);
                                WebClipController* c = self.data();
                                if (!c) return;
                                if (imgResp.status_code != 200 || imgResp.binary_body.empty()) return;
                                QByteArray bytes(reinterpret_cast<const char*>(imgResp.binary_body.data()), static_cast<int>(imgResp.binary_body.size()));
                                std::vector<uint8_t>().swap(imgResp.binary_body);

                                QImage qimg;
                                qimg.loadFromData(bytes);
                                QString pixelFp = computePixelFingerprint(qimg);
                                QString hash = computeImageHash(bytes);
                                c->markImageApplied(hash, pixelFp, nowMs);

                                QString fileUrl = saveImageBytesToCache(bytes, QString::fromStdString(mimeType));
                                int byteSize = bytes.size();
                                QString mimeQ = QString::fromStdString(mimeType);
                                QString sourceQ = QString::fromStdString(source.empty() ? "phone" : source);
                                QByteArray().swap(bytes);

                                QMetaObject::invokeMethod(c, [self, fileUrl, qimg, mimeQ, sourceQ, byteSize]() {
                                    WebClipController* c = self.data();
                                    if (!c) return;
                                    if (!qimg.isNull()) {
                                        c->suppressNextLocalChange_.store(true);
                                        if (QGuiApplication::clipboard()) {
                                            QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
                                        } else if (c->nativeClipboard_) {
                                            QString localPath = QUrl(fileUrl).toLocalFile();
                                            QFile f(localPath);
                                            QByteArray diskBytes;
                                            if (f.open(QIODevice::ReadOnly)) {
                                                diskBytes = f.readAll();
                                                f.close();
                                            }
                                            if (!diskBytes.isEmpty()) {
                                                std::vector<uint8_t> stdBytes(diskBytes.begin(), diskBytes.end());
                                                c->nativeClipboard_->set_image(stdBytes, mimeQ.toStdString());
                                            }
                                        }
                                    }

                                    c->clipModel_.addClipImage(fileUrl, mimeQ, byteSize, sourceQ);
                                    emit c->clipReceived("[Image]", sourceQ);
                                });
                            }).detach();
                        }
                        return;
                    }

                    QString text = QString::fromStdString(data.get_string("text"));
                    if (text.trimmed().isEmpty()) return;
                    c->markTextApplied(text, nowMs);

                    QMetaObject::invokeMethod(c, [self, text, source]() {
                        WebClipController* c = self.data();
                        if (!c) return;
                        c->suppressNextLocalChange_.store(true);
                        if (QGuiApplication::clipboard()) {
                            QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
                        } else if (c->nativeClipboard_) {
                            c->nativeClipboard_->set_text(text.toStdString());
                        }

                        c->clipModel_.addClip(text, QString::fromStdString(source.empty() ? "phone" : source));
                        emit c->clipReceived(text, QString::fromStdString(source.empty() ? "phone" : source));
                    });
                }
            },
            [](const std::string&) {

            },
            *stopFlag
        );
    });
}

void WebClipController::stopSseListener() {
    if (sseStopFlag_->exchange(true) == false) {
        if (sseThread_ && sseThread_->joinable()) {

            std::shared_ptr<std::thread> stale(sseThread_.release());
            std::thread([stale]() {
                if (stale && stale->joinable()) {
                    stale->join();
                }
            }).detach();
        } else {
            sseThread_.reset();
        }
    }
}

void WebClipController::onClipboardDataChanged() {
    if (!connected_ || !autoSync_) return;

    if (suppressNextLocalChange_.exchange(false)) {
        return;
    }

    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();

    if (QGuiApplication::clipboard()) {
        const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
        if (mimeData) {
            if (mimeData->hasImage() || mimeData->hasFormat("image/png") || mimeData->hasFormat("image/jpeg") || mimeData->hasFormat("image/webp")) {
                QByteArray ba;
                QString mime = "image/png";
                QImage directImg;
                if (mimeData->hasFormat("image/png")) {
                    ba = mimeData->data("image/png");
                    mime = "image/png";
                } else if (mimeData->hasFormat("image/jpeg")) {
                    ba = mimeData->data("image/jpeg");
                    mime = "image/jpeg";
                } else if (mimeData->hasFormat("image/webp")) {
                    ba = mimeData->data("image/webp");
                    mime = "image/webp";
                } else {
                    directImg = qvariant_cast<QImage>(mimeData->imageData());
                }

                if (!ba.isEmpty() || !directImg.isNull()) {
                    if (ba.isEmpty() && !directImg.isNull()) {
                        QBuffer buf(&ba);
                        buf.open(QIODevice::WriteOnly);
                        directImg.save(&buf, "PNG");
                        mime = "image/png";
                    }
                    if (ba.isEmpty()) return;

                    QString hash = computeImageHash(ba);
                    QString pixelFp = !directImg.isNull()
                        ? computePixelFingerprint(directImg)
                        : computePixelFingerprint(ba);

                    if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) {
                        return;
                    }

                    {
                        std::lock_guard<std::mutex> guard(syncLock_);
                        lastLocalImgHash_ = hash;
                        lastLocalPixelFp_ = pixelFp;
                        lastImgTimeMs_ = nowMs;
                    }

                    pushImageBytes(ba, mime);
                    return;
                }
            }
        }
    }

    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (!current.isEmpty()) {
        handleNativeText(current);
        return;
    }

    if (nativeClipboard_) {
        QPointer<WebClipController> self(this);
        std::thread([self]() {
            auto cb = webclip::create_clipboard();
            if (!cb) return;
            PollReadResult r;
            if (cb->has_image()) {
                ClipboardImage img = cb->get_image();
                if (img.valid && !img.data.empty()) {
                    r.ok = true;
                    r.hasImage = true;
                    r.image = std::move(img.data);
                    r.mime = img.mime_type.empty() ? "image/png" : img.mime_type;
                    QMetaObject::invokeMethod(self.data(), [self, r]() {
                        if (self) self->processPollReadResult(std::move(r));
                    });
                    return;
                }
            }
            std::string t = cb->get_text();
            if (!t.empty()) {
                r.ok = true;
                r.text = std::move(t);
            }
            QMetaObject::invokeMethod(self.data(), [self, r]() {
                if (self) self->processPollReadResult(std::move(r));
            });
        }).detach();
    }
}

void WebClipController::handleNativeImage(ClipboardImage img) {
    if (!img.valid || img.data.empty()) return;
    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    QByteArray ba(reinterpret_cast<const char*>(img.data.data()), static_cast<int>(img.data.size()));
    QString hash = computeImageHash(ba);
    QString pixelFp = computePixelFingerprint(ba);

    if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) {
        return;
    }

    markImageApplied(hash, pixelFp, nowMs);
    pushImageBytes(ba, QString::fromStdString(img.mime_type.empty() ? "image/png" : img.mime_type));
}

void WebClipController::handleNativeText(QString current) {
    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    if (current.trimmed().isEmpty()) return;

    QByteArray rawBytes = current.toUtf8();
    if (rawBytes.size() >= 4) {
        const uint8_t* u = reinterpret_cast<const uint8_t*>(rawBytes.constData());
        bool isPng = (u[0] == 0x89 && u[1] == 0x50 && u[2] == 0x4E && u[3] == 0x47);
        bool isJpg = (u[0] == 0xFF && u[1] == 0xD8 && u[2] == 0xFF);
        bool isGif = (u[0] == 0x47 && u[1] == 0x49 && u[2] == 0x46 && u[3] == 0x38);
        bool isWebp = (rawBytes.size() >= 12 && u[0] == 'R' && u[1] == 'I' && u[2] == 'F' && u[3] == 'F' && u[8] == 'W' && u[9] == 'E' && u[10] == 'B' && u[11] == 'P');

        if (isPng || isJpg || isGif || isWebp) {
            QString mime = isPng ? "image/png" : (isJpg ? "image/jpeg" : (isGif ? "image/gif" : "image/webp"));
            QString hash = computeImageHash(rawBytes);
            QString pixelFp = computePixelFingerprint(rawBytes);

            if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) {
                return;
            }

            markImageApplied(hash, pixelFp, nowMs);
            pushImageBytes(rawBytes, mime);
            return;
        }
    }

    if (shouldSuppressText(current, nowMs, 2000)) {
        return;
    }

    markTextApplied(current, nowMs);
    pushClipboard(current);
}

void WebClipController::onPollTimer() {
    if (!connected_ || !autoSync_) return;

    QPointer<WebClipController> self(this);
    std::thread([self]() {
        auto cb = webclip::create_clipboard();
        if (!cb) return;
        PollReadResult r;
        if (cb->has_image()) {
            ClipboardImage img = cb->get_image();
            if (img.valid && !img.data.empty()) {
                r.ok = true;
                r.hasImage = true;
                r.image = std::move(img.data);
                r.mime = img.mime_type.empty() ? "image/png" : img.mime_type;
                QMetaObject::invokeMethod(self.data(), [self, r]() {
                    if (self) self->processPollReadResult(std::move(r));
                });
                return;
            }
        }
        std::string t = cb->get_text();
        if (!t.empty()) {
            r.ok = true;
            r.text = std::move(t);
        }
        QMetaObject::invokeMethod(self.data(), [self, r]() {
            if (self) self->processPollReadResult(std::move(r));
        });
    }).detach();
}

void WebClipController::processPollReadResult(PollReadResult result) {
    if (!connected_ || !autoSync_) return;
    if (!result.ok) return;
    if (result.hasImage) {
        ClipboardImage img;
        img.data = std::move(result.image);
        img.mime_type = std::move(result.mime);
        img.valid = true;
        handleNativeImage(std::move(img));
    } else if (!result.text.empty()) {
        handleNativeText(QString::fromStdString(result.text));
    }
}

bool WebClipController::pushClipboard(const QString& text, const QString& clipId) {
    if (!httpClient_ || !connected_) {
        emit showToast("Not connected to phone", true);
        return false;
    }

    std::string cId = clipId.isEmpty() ? generateClipId() : clipId.toStdString();
    markClipIdHandled(cId);

    auto client = httpClient_;
    std::string textStd = text.toStdString();
    QPointer<WebClipController> self(this);
    std::thread([self, client, text, textStd, cId]() {
        if (!client) return;
        HttpResponse resp = client->push_clipboard(textStd, cId);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, text, resp]() {
            if (!self) return;
            if (resp.status_code == 200) {
                self->clipModel_.addClip(text, "local");
                emit self->showToast("Pushed " + QString::number(text.length()) + " chars to phone", false);
            } else {
                emit self->showToast("Push failed (HTTP " + QString::number(resp.status_code) + ")", true);
            }
        });
    }).detach();

    return true;
}

bool WebClipController::pushImage(const QString& filePathOrDataUrl) {
    if (!httpClient_ || !connected_) {
        emit showToast("Not connected to phone", true);
        return false;
    }

    QString src = filePathOrDataUrl.trimmed();
    if (src.startsWith("file://")) {
        src = QUrl(src).toLocalFile();
    }

    if (src.startsWith("data:image/")) {
        int commaIdx = src.indexOf(',');
        if (commaIdx > 0) {
            QString header = src.left(commaIdx);
            QString base64Data = src.mid(commaIdx + 1);
            QString mimeType = "image/png";
            int semiIdx = header.indexOf(';');
            if (semiIdx > 5) {
                mimeType = header.mid(5, semiIdx - 5);
            }
            QByteArray bytes = QByteArray::fromBase64(base64Data.toLatin1());
            if (!bytes.isEmpty()) {
                return pushImageBytes(bytes, mimeType);
            }
        }
    }

    QFile file(src);
    if (!file.open(QIODevice::ReadOnly)) {
        emit showToast("Failed to open image file: " + file.errorString(), true);
        return false;
    }

    QByteArray bytes = file.readAll();
    file.close();

    if (bytes.isEmpty()) {
        emit showToast("Image file is empty", true);
        return false;
    }

    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(src);
    QString mime = mimeType.isValid() ? mimeType.name() : "image/png";
    if (!mime.startsWith("image/")) {
        mime = "image/png";
    }

    return pushImageBytes(bytes, mime);
}

bool WebClipController::pushImageBytes(const QByteArray& bytes, const QString& mimeType, const QString& clipId) {
    if (!httpClient_ || !connected_) {
        emit showToast("Not connected to phone", true);
        return false;
    }

    if (bytes.isEmpty()) {
        emit showToast("No image data to push", true);
        return false;
    }

    std::string cId = clipId.isEmpty() ? generateClipId() : clipId.toStdString();
    markClipIdHandled(cId);

    auto client = httpClient_;
    std::string mime = mimeType.isEmpty() ? "image/png" : mimeType.toStdString();
    int byteSize = bytes.size();

    QPointer<WebClipController> self(this);
    std::thread([self, client, bytes, mime, byteSize, cId]() {
        if (!client) return;
        auto tStart = std::chrono::steady_clock::now();

        QString fileUrl = saveImageBytesToCache(bytes, QString::fromStdString(mime));

        HttpResponse resp = client->push_image(
            reinterpret_cast<const uint8_t*>(bytes.constData()),
            static_cast<size_t>(bytes.size()),
            mime,
            cId);

        static const bool s_perfLog = (std::getenv("WEBCLIP_PERF") != nullptr || std::getenv("WEBCLIP_DEBUG_PERF") != nullptr);
        if (s_perfLog) {
            auto tEnd = std::chrono::steady_clock::now();
            auto durMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
            std::cerr << "[PERF] pushImageBytes background total: " << durMs << "ms (size=" << byteSize << " bytes, status=" << resp.status_code << ")\n";
        }

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, fileUrl, byteSize, mime, resp]() {
            if (!self) return;
            if (resp.status_code == 200) {
                self->clipModel_.addClipImage(fileUrl, QString::fromStdString(mime), byteSize, "local");
                emit self->showToast("Pushed image (" + QString::number(byteSize / 1024) + " KB) to phone", false);
            } else {
                emit self->showToast("Push image failed (HTTP " + QString::number(resp.status_code) + ")", true);
            }
        });
    }).detach();

    return true;
}

bool WebClipController::pushCurrentClipboard() {
    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();

    if (QGuiApplication::clipboard()) {
        const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
        if (mimeData) {
            if (mimeData->hasImage() || mimeData->hasFormat("image/png") || mimeData->hasFormat("image/jpeg") || mimeData->hasFormat("image/webp")) {
                QByteArray ba;
                QString mime = "image/png";
                QImage directImg;
                if (mimeData->hasFormat("image/png")) {
                    ba = mimeData->data("image/png");
                    mime = "image/png";
                } else if (mimeData->hasFormat("image/jpeg")) {
                    ba = mimeData->data("image/jpeg");
                    mime = "image/jpeg";
                } else if (mimeData->hasFormat("image/webp")) {
                    ba = mimeData->data("image/webp");
                    mime = "image/webp";
                } else {
                    directImg = qvariant_cast<QImage>(mimeData->imageData());
                }
                if (!ba.isEmpty() || !directImg.isNull()) {
                    if (ba.isEmpty() && !directImg.isNull()) {
                        QBuffer buf(&ba);
                        buf.open(QIODevice::WriteOnly);
                        directImg.save(&buf, "PNG");
                        mime = "image/png";
                    }
                    if (ba.isEmpty()) return false;
                    QString hash = computeImageHash(ba);
                    QString pixelFp = !directImg.isNull() ? computePixelFingerprint(directImg) : computePixelFingerprint(ba);

                    if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) return false;

                    {
                        std::lock_guard<std::mutex> guard(syncLock_);
                        lastLocalImgHash_ = hash;
                        lastLocalPixelFp_ = pixelFp;
                        lastImgTimeMs_ = nowMs;
                    }

                    return pushImageBytes(ba, mime);
                }
            }
        }
    }

    if (nativeClipboard_ && nativeClipboard_->has_image()) {
        ClipboardImage local_img = nativeClipboard_->get_image();
        if (local_img.valid && !local_img.data.empty()) {
            QByteArray ba(reinterpret_cast<const char*>(local_img.data.data()), static_cast<int>(local_img.data.size()));
            QString hash = computeImageHash(ba);
            QString pixelFp = computePixelFingerprint(ba);

            if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) return false;

            {
                std::lock_guard<std::mutex> guard(syncLock_);
                lastLocalImgHash_ = hash;
                lastLocalPixelFp_ = pixelFp;
                lastImgTimeMs_ = nowMs;
            }

            return pushImageBytes(ba, QString::fromStdString(local_img.mime_type.empty() ? "image/png" : local_img.mime_type));
        }
    }

    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }

    if (current.trimmed().isEmpty()) return false;

    QByteArray rawBytes = current.toUtf8();
    if (rawBytes.size() >= 4) {
        const uint8_t* u = reinterpret_cast<const uint8_t*>(rawBytes.constData());
        bool isPng = (u[0] == 0x89 && u[1] == 0x50 && u[2] == 0x4E && u[3] == 0x47);
        bool isJpg = (u[0] == 0xFF && u[1] == 0xD8 && u[2] == 0xFF);
        bool isGif = (u[0] == 0x47 && u[1] == 0x49 && u[2] == 0x46 && u[3] == 0x38);
        bool isWebp = (rawBytes.size() >= 12 && u[0] == 'R' && u[1] == 'I' && u[2] == 'F' && u[3] == 'F' && u[8] == 'W' && u[9] == 'E' && u[10] == 'B' && u[11] == 'P');
        if (isPng || isJpg || isGif || isWebp) {
            QString mime = isPng ? "image/png" : (isJpg ? "image/jpeg" : (isGif ? "image/gif" : "image/webp"));
            QString hash = computeImageHash(rawBytes);
            QString pixelFp = computePixelFingerprint(rawBytes);

            if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) return false;

            {
                std::lock_guard<std::mutex> guard(syncLock_);
                lastLocalImgHash_ = hash;
                lastLocalPixelFp_ = pixelFp;
                lastImgTimeMs_ = nowMs;
            }

            return pushImageBytes(rawBytes, mime);
        }
    }

    if (shouldSuppressText(current, nowMs, 2000)) return false;

    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalText_ = current;
        lastTextTimeMs_ = nowMs;
    }
    return pushClipboard(current);
}

void WebClipController::copyToClipboard(const QString& text) {
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalText_ = text;
        lastLocalImgHash_.clear();
        lastLocalPixelFp_.clear();
    }
    suppressNextLocalChange_.store(true);
    if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
    } else if (nativeClipboard_) {
        nativeClipboard_->set_text(text.toStdString());
    }
    emit showToast("Copied to clipboard", false);
}

void WebClipController::copyImageToClipboard(int index) {
    QString dataUrl = clipModel_.getClipImageData(index);
    if (dataUrl.isEmpty()) return;

    QByteArray bytes;
    if (dataUrl.startsWith("file://") || dataUrl.startsWith("/")) {
        QString localPath = dataUrl.startsWith("file://") ? QUrl(dataUrl).toLocalFile() : dataUrl;
        QFile f(localPath);
        if (f.open(QIODevice::ReadOnly)) {
            bytes = f.readAll();
            f.close();
        }
    } else {
        int commaIdx = dataUrl.indexOf(',');
        if (commaIdx >= 0) {
            bytes = QByteArray::fromBase64(dataUrl.mid(commaIdx + 1).toLatin1());
        } else {
            bytes = QByteArray::fromBase64(dataUrl.toLatin1());
        }
    }

    if (bytes.isEmpty()) return;

    QImage qimg;
    qimg.loadFromData(bytes);
    QString hash = computeImageHash(bytes);
    QString pixelFp = computePixelFingerprint(qimg);
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalImgHash_ = hash;
        lastLocalPixelFp_ = pixelFp;
        lastLocalText_.clear();
    }

    if (!qimg.isNull()) {
        suppressNextLocalChange_.store(true);
        if (QGuiApplication::clipboard()) {
            QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
        } else if (nativeClipboard_) {
            std::vector<uint8_t> stdBytes(bytes.begin(), bytes.end());
            nativeClipboard_->set_image(stdBytes, "image/png");
        }
    }
    emit showToast("Copied image to clipboard", false);
}

bool WebClipController::saveImage(int index, const QString& destinationPath) {
    QString dataUrl = clipModel_.getClipImageData(index);
    if (dataUrl.isEmpty()) return false;

    QString dest = destinationPath.trimmed();
    if (dest.startsWith("file://")) {
        dest = QUrl(dest).toLocalFile();
    }

    if (dataUrl.startsWith("file://") || dataUrl.startsWith("/")) {
        QString localPath = dataUrl.startsWith("file://") ? QUrl(dataUrl).toLocalFile() : dataUrl;
        if (QFile::exists(dest)) {
            QFile::remove(dest);
        }
        if (QFile::copy(localPath, dest)) {
            emit showToast("Image saved to " + QFileInfo(dest).fileName(), false);
            return true;
        }
    }

    QByteArray bytes;
    int commaIdx = dataUrl.indexOf(',');
    if (commaIdx >= 0) {
        bytes = QByteArray::fromBase64(dataUrl.mid(commaIdx + 1).toLatin1());
    } else {
        bytes = QByteArray::fromBase64(dataUrl.toLatin1());
    }

    if (bytes.isEmpty()) return false;

    QFile file(dest);
    if (!file.open(QIODevice::WriteOnly)) {
        emit showToast("Failed to save image file", true);
        return false;
    }

    file.write(bytes);
    file.close();
    emit showToast("Image saved to " + QFileInfo(dest).fileName(), false);
    return true;
}

void WebClipController::saveSettings() {
    QSettings s("Burhanverse", "WebClip");
    s.setValue("host", host_);
    s.setValue("port", port_);
    s.setValue("code", code_);
    s.setValue("useHttps", useHttps_);
    s.setValue("insecure", insecure_);
    s.setValue("autoSync", autoSync_);
    s.setValue("autoConnect", autoConnect_);
    s.setValue("pollInterval", pollInterval_);
    s.setValue("themeMode", themeMode_);
    s.setValue("accentPreset", accentPreset_);
    s.setValue("customColor", customColor_.name());
}

void WebClipController::loadSettings() {
    QSettings s("Burhanverse", "WebClip");
    host_ = s.value("host", "192.168.1.50").toString();
    port_ = s.value("port", 8080).toInt();
    code_ = s.value("code", "").toString();
    useHttps_ = s.value("useHttps", false).toBool();
    insecure_ = s.value("insecure", true).toBool();
    autoSync_ = s.value("autoSync", true).toBool();
    autoConnect_ = s.value("autoConnect", false).toBool();
    pollInterval_ = s.value("pollInterval", 1.0).toDouble();
    themeMode_ = s.value("themeMode", 0).toInt();
    accentPreset_ = s.value("accentPreset", "purple").toString();
    customColor_ = QColor(s.value("customColor", "#6750A4").toString());
    if (!customColor_.isValid()) {
        customColor_ = QColor("#6750A4");
    }

    MD3Theme::instance()->setCustomColor(customColor_);
    MD3Theme::instance()->setThemeMode(themeMode_);
    MD3Theme::instance()->setAccentPreset(accentPreset_);
}

void WebClipController::openUrl(const QString& urlStr) {
    QDesktopServices::openUrl(QUrl(urlStr));
}

void WebClipController::notifyMinimizedToTray() {
    emit minimizedToTray();
}

}
