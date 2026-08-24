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
#include <QMimeDatabase>
#include <QMimeData>

namespace webclip {

QString WebClipController::computeImageHash(const QByteArray& data) {
    if (data.isEmpty()) return "";
    uint64_t hash = 14695981039346656037ULL;
    for (char byte : data) {
        hash ^= static_cast<uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return QString::number(hash, 16) + "-" + QString::number(data.size());
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

    // Check initial clipboard
    if (QGuiApplication::clipboard()) {
        QImage img = QGuiApplication::clipboard()->image();
        if (!img.isNull()) {
            QByteArray ba;
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            lastLocalImgHash_ = computeImageHash(ba);
            QString dataUrl = "data:image/png;base64," + QString::fromLatin1(ba.toBase64());
            clipModel_.addClipImage(dataUrl, "image/png", ba.size(), "local");
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
    disconnectFromPortal();
    saveSettings();
}

QString WebClipController::clipboardBackend() const {
    return nativeClipboard_ ? QString::fromStdString(nativeClipboard_->get_backend_name()) : "Qt Internal";
}

void WebClipController::setHost(const QString& host) {
    if (host_ != host) {
        host_ = host;
        emit hostChanged();
    }
}

void WebClipController::setPort(int port) {
    if (port_ != port) {
        port_ = port;
        emit portChanged();
    }
}

void WebClipController::setCode(const QString& code) {
    if (code_ != code) {
        code_ = code;
        emit codeChanged();
    }
}

void WebClipController::setUseHttps(bool useHttps) {
    if (useHttps_ != useHttps) {
        useHttps_ = useHttps;
        emit useHttpsChanged();
    }
}

void WebClipController::setInsecure(bool insecure) {
    if (insecure_ != insecure) {
        insecure_ = insecure;
        emit insecureChanged();
    }
}

void WebClipController::setAutoSync(bool autoSync) {
    if (autoSync_ != autoSync) {
        autoSync_ = autoSync;
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
        emit themeModeChanged();
    }
}

void WebClipController::setAccentPreset(const QString& preset) {
    if (accentPreset_ != preset) {
        accentPreset_ = preset;
        MD3Theme::instance()->setAccentPreset(preset);
        emit accentPresetChanged();
    }
}

void WebClipController::setCustomColor(const QColor& color) {
    if (customColor_ != color && color.isValid()) {
        customColor_ = color;
        accentPreset_ = "custom";
        MD3Theme::instance()->setCustomColor(color);
        emit customColorChanged();
        emit accentPresetChanged();
    }
}

void WebClipController::setCustomAccentColor(const QString& hexColor) {
    QColor c(hexColor);
    if (c.isValid()) {
        setCustomColor(c);
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

void WebClipController::setStatusMessage(const QString& msg) {
    if (statusMessage_ != msg) {
        statusMessage_ = msg;
        emit statusMessageChanged();
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

void WebClipController::connectToPortal() {
    if (connected_ || connecting_) return;

    sanitizeHostInput();

    if (host_.trimmed().isEmpty()) {
        setStatusMessage("Host is required");
        emit showToast("Please enter a valid phone IP or hostname", true);
        return;
    }

    setConnecting(true);
    setStatusMessage("Connecting to " + host_ + ":" + QString::number(port_) + "...");

    std::string clientId = generate_random_client_id();
    auto client = std::make_shared<HttpClient>(
        host_.toStdString(),
        port_,
        code_.toStdString(),
        useHttps_,
        insecure_,
        clientId
    );
    httpClient_ = client;

    QPointer<WebClipController> self(this);
    int activePort = port_;
    std::thread([self, client, activePort]() {
        HttpResponse stateResp = client->get_state();
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, stateResp, activePort, client]() {
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
                        QMetaObject::invokeMethod(self.data(), [self, imgResp, mimeType]() {
                            if (!self || imgResp.status_code != 200 || imgResp.binary_body.empty()) return;
                            QByteArray bytes(reinterpret_cast<const char*>(imgResp.binary_body.data()), static_cast<int>(imgResp.binary_body.size()));
                            QString hash = computeImageHash(bytes);

                            {
                                std::lock_guard<std::mutex> guard(self->syncLock_);
                                self->lastRemoteImgHash_ = hash;
                                self->lastLocalImgHash_ = hash;
                                self->lastRemoteText_.clear();
                                self->lastLocalText_.clear();
                            }

                            QImage qimg;
                            qimg.loadFromData(bytes);
                            if (!qimg.isNull()) {
                                if (QGuiApplication::clipboard()) {
                                    QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
                                }
                                if (self->nativeClipboard_) {
                                    self->nativeClipboard_->set_image(imgResp.binary_body, mimeType);
                                }
                            }

                            QString dataUrl = "data:" + QString::fromStdString(mimeType) + ";base64," + QString::fromLatin1(bytes.toBase64());
                            self->clipModel_.addClipImage(dataUrl, QString::fromStdString(mimeType), bytes.size(), "phone");
                        });
                    }).detach();
                } else {
                    QString remoteText = QString::fromStdString(stateJson.get_string("text"));
                    {
                        std::lock_guard<std::mutex> guard(self->syncLock_);
                        self->lastRemoteText_ = remoteText;
                    }

                    if (!remoteText.isEmpty()) {
                        if (QGuiApplication::clipboard()) {
                            QGuiApplication::clipboard()->setText(remoteText, QClipboard::Clipboard);
                        }
                        if (self->nativeClipboard_) {
                            self->nativeClipboard_->set_text(remoteText.toStdString());
                        }
                        self->lastLocalText_ = remoteText;
                        self->clipModel_.addClip(remoteText, "phone");
                    }
                }

                self->setConnecting(false);
                self->setConnected(true);
                self->setStatusMessage("Connected (" + QString::number(activePort) + ")");
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
                self->setStatusMessage("Connection failed: " + err);
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
    setStatusMessage("Disconnected");
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
    sseStopFlag_.store(false);

    sseThread_ = std::make_unique<std::thread>([this]() {
        httpClient_->stream_events(
            [this](const SseEvent& ev) {
                if (ev.event == "clipboard") {
                    JsonValue data = JsonValue::parse(ev.data);
                    std::string type = data.get_string("type");
                    std::string source = data.get_string("source");

                    if (source == "web") return;

                    if (type == "image") {
                        std::string mimeType = data.get_string("mimeType");
                        if (mimeType.empty()) mimeType = "image/png";
                        std::string inlineData = data.get_string("data");

                        if (!inlineData.empty()) {
                            QString qData = QString::fromStdString(inlineData);
                            QByteArray bytes;
                            int commaIdx = qData.indexOf(',');
                            if (commaIdx >= 0) {
                                bytes = QByteArray::fromBase64(qData.mid(commaIdx + 1).toLatin1());
                            } else {
                                bytes = QByteArray::fromBase64(qData.toLatin1());
                            }

                            QMetaObject::invokeMethod(this, [this, qData, bytes, mimeType, source]() {
                                QString hash = computeImageHash(bytes);
                                {
                                    std::lock_guard<std::mutex> guard(syncLock_);
                                    if (hash == lastRemoteImgHash_) return;
                                    lastRemoteImgHash_ = hash;
                                    lastLocalImgHash_ = hash;
                                    lastRemoteText_.clear();
                                    lastLocalText_.clear();
                                }

                                QImage qimg;
                                qimg.loadFromData(bytes);
                                if (!qimg.isNull()) {
                                    if (QGuiApplication::clipboard()) {
                                        QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
                                    }
                                    if (nativeClipboard_) {
                                        std::vector<uint8_t> stdBytes(bytes.begin(), bytes.end());
                                        nativeClipboard_->set_image(stdBytes, mimeType);
                                    }
                                }

                                QString dataUrl = qData.startsWith("data:")
                                    ? qData
                                    : ("data:" + QString::fromStdString(mimeType) + ";base64," + qData);
                                clipModel_.addClipImage(dataUrl, QString::fromStdString(mimeType), bytes.size(), QString::fromStdString(source));
                                emit clipReceived("[Image]", QString::fromStdString(source));
                            });
                        } else {
                            std::string imageUrl = data.get_string("imageUrl");
                            auto client = httpClient_;
                            std::thread([this, client, imageUrl, mimeType, source]() {
                                HttpResponse imgResp = client->get_image(imageUrl);
                                if (imgResp.status_code != 200 || imgResp.binary_body.empty()) return;
                                QByteArray bytes(reinterpret_cast<const char*>(imgResp.binary_body.data()), static_cast<int>(imgResp.binary_body.size()));
                                QString dataUrl = "data:" + QString::fromStdString(mimeType) + ";base64," + QString::fromLatin1(bytes.toBase64());

                                QMetaObject::invokeMethod(this, [this, dataUrl, bytes, mimeType, source]() {
                                    QString hash = computeImageHash(bytes);
                                    {
                                        std::lock_guard<std::mutex> guard(syncLock_);
                                        if (hash == lastRemoteImgHash_) return;
                                        lastRemoteImgHash_ = hash;
                                        lastLocalImgHash_ = hash;
                                        lastRemoteText_.clear();
                                        lastLocalText_.clear();
                                    }

                                    QImage qimg;
                                    qimg.loadFromData(bytes);
                                    if (!qimg.isNull()) {
                                        if (QGuiApplication::clipboard()) {
                                            QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
                                        }
                                        if (nativeClipboard_) {
                                            std::vector<uint8_t> stdBytes(bytes.begin(), bytes.end());
                                            nativeClipboard_->set_image(stdBytes, mimeType);
                                        }
                                    }

                                    clipModel_.addClipImage(dataUrl, QString::fromStdString(mimeType), bytes.size(), QString::fromStdString(source));
                                    emit clipReceived("[Image]", QString::fromStdString(source));
                                });
                            }).detach();
                        }
                        return;
                    }

                    QString text = QString::fromStdString(data.get_string("text"));
                    QMetaObject::invokeMethod(this, [this, text, source]() {
                        {
                            std::lock_guard<std::mutex> guard(syncLock_);
                            if (text == lastRemoteText_) return;
                            lastRemoteText_ = text;
                            lastLocalText_ = text;
                            lastRemoteImgHash_.clear();
                            lastLocalImgHash_.clear();
                        }

                        if (QGuiApplication::clipboard()) {
                            QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
                        }
                        if (nativeClipboard_) {
                            nativeClipboard_->set_text(text.toStdString());
                        }

                        clipModel_.addClip(text, "phone");
                        emit clipReceived(text, QString::fromStdString(source));
                    });
                }
            },
            [this](const std::string& status) {
                QMetaObject::invokeMethod(this, [this, status]() {
                    if (connected_) {
                        setStatusMessage(QString::fromStdString(status));
                    }
                });
            },
            sseStopFlag_
        );
    });
}

void WebClipController::stopSseListener() {
    if (sseStopFlag_.exchange(true) == false) {
        if (sseThread_ && sseThread_->joinable()) {
            sseThread_->join();
        }
        sseThread_.reset();
    }
}

void WebClipController::onClipboardDataChanged() {
    if (!connected_ || !autoSync_) return;

    // 1. Check Qt Clipboard MIME data for image formats
    if (QGuiApplication::clipboard()) {
        const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
        if (mimeData) {
            if (mimeData->hasImage() || mimeData->hasFormat("image/png") || mimeData->hasFormat("image/jpeg") || mimeData->hasFormat("image/webp")) {
                QByteArray ba;
                QString mime = "image/png";
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
                    QImage img = qvariant_cast<QImage>(mimeData->imageData());
                    if (!img.isNull()) {
                        QBuffer buf(&ba);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf, "PNG");
                        mime = "image/png";
                    }
                }

                if (!ba.isEmpty()) {
                    QString hash = computeImageHash(ba);
                    {
                        std::lock_guard<std::mutex> guard(syncLock_);
                        if (hash == lastLocalImgHash_ || hash == lastRemoteImgHash_) {
                            return;
                        }
                        lastLocalImgHash_ = hash;
                        lastLocalText_.clear();
                    }

                    pushImageBytes(ba, mime);
                    return;
                }
            }
        }
    }

    // 2. Check native clipboard for image (xclip/wl-paste/win32)
    if (nativeClipboard_ && nativeClipboard_->has_image()) {
        ClipboardImage local_img = nativeClipboard_->get_image();
        if (local_img.valid && !local_img.data.empty()) {
            QByteArray ba(reinterpret_cast<const char*>(local_img.data.data()), static_cast<int>(local_img.data.size()));
            QString hash = computeImageHash(ba);
            {
                std::lock_guard<std::mutex> guard(syncLock_);
                if (hash == lastLocalImgHash_ || hash == lastRemoteImgHash_) {
                    return;
                }
                lastLocalImgHash_ = hash;
                lastLocalText_.clear();
            }

            pushImageBytes(ba, QString::fromStdString(local_img.mime_type.empty() ? "image/png" : local_img.mime_type));
            return;
        }
    }

    // 3. Check text clipboard
    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }

    if (current.trimmed().isEmpty()) return;

    // Guard: Intercept raw binary image data (e.g. from xclip/wl-paste fallback outputting PNG magic bytes)
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
            {
                std::lock_guard<std::mutex> guard(syncLock_);
                if (hash == lastLocalImgHash_ || hash == lastRemoteImgHash_) {
                    return;
                }
                lastLocalImgHash_ = hash;
                lastLocalText_.clear();
            }
            pushImageBytes(rawBytes, mime);
            return;
        }
    }

    {
        std::lock_guard<std::mutex> guard(syncLock_);
        if (current == lastLocalText_ || current == lastRemoteText_) {
            return;
        }
        lastLocalText_ = current;
        lastLocalImgHash_.clear();
    }

    pushClipboard(current);
}

void WebClipController::onPollTimer() {
    onClipboardDataChanged();
}

bool WebClipController::pushClipboard(const QString& text) {
    if (!httpClient_ || !connected_) {
        emit showToast("Not connected to phone", true);
        return false;
    }

    auto client = httpClient_;
    std::string textStd = text.toStdString();
    QPointer<WebClipController> self(this);
    std::thread([self, client, text, textStd]() {
        if (!client) return;
        HttpResponse resp = client->push_clipboard(textStd);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, text, resp]() {
            if (!self) return;
            if (resp.status_code == 200) {
                {
                    std::lock_guard<std::mutex> guard(self->syncLock_);
                    self->lastLocalText_ = text;
                    self->lastRemoteText_ = text;
                    self->lastLocalImgHash_.clear();
                    self->lastRemoteImgHash_.clear();
                }
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

bool WebClipController::pushImageBytes(const QByteArray& bytes, const QString& mimeType) {
    if (!httpClient_ || !connected_) {
        emit showToast("Not connected to phone", true);
        return false;
    }

    if (bytes.isEmpty()) {
        emit showToast("No image data to push", true);
        return false;
    }

    auto client = httpClient_;
    std::string mime = mimeType.isEmpty() ? "image/png" : mimeType.toStdString();
    std::vector<uint8_t> vec(bytes.begin(), bytes.end());
    QString hash = computeImageHash(bytes);
    QString dataUrl = "data:" + QString::fromStdString(mime) + ";base64," + QString::fromLatin1(bytes.toBase64());

    QPointer<WebClipController> self(this);
    std::thread([self, client, vec, mime, hash, dataUrl, bytes]() {
        if (!client) return;
        HttpResponse resp = client->push_image(vec, mime);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, hash, dataUrl, bytes, mime, resp]() {
            if (!self) return;
            if (resp.status_code == 200) {
                {
                    std::lock_guard<std::mutex> guard(self->syncLock_);
                    self->lastLocalImgHash_ = hash;
                    self->lastRemoteImgHash_ = hash;
                    self->lastLocalText_.clear();
                    self->lastRemoteText_.clear();
                }
                self->clipModel_.addClipImage(dataUrl, QString::fromStdString(mime), bytes.size(), "local");
                emit self->showToast("Pushed image (" + QString::number(bytes.size() / 1024) + " KB) to phone", false);
            } else {
                emit self->showToast("Push image failed (HTTP " + QString::number(resp.status_code) + ")", true);
            }
        });
    }).detach();

    return true;
}

bool WebClipController::pushCurrentClipboard() {
    // 1. Check Qt Clipboard MIME data for image formats
    if (QGuiApplication::clipboard()) {
        const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
        if (mimeData) {
            if (mimeData->hasImage() || mimeData->hasFormat("image/png") || mimeData->hasFormat("image/jpeg") || mimeData->hasFormat("image/webp")) {
                QByteArray ba;
                QString mime = "image/png";
                if (mimeData->hasFormat("image/png")) {
                    ba = mimeData->data("image/png");
                } else if (mimeData->hasFormat("image/jpeg")) {
                    ba = mimeData->data("image/jpeg");
                    mime = "image/jpeg";
                } else if (mimeData->hasFormat("image/webp")) {
                    ba = mimeData->data("image/webp");
                    mime = "image/webp";
                } else {
                    QImage img = qvariant_cast<QImage>(mimeData->imageData());
                    if (!img.isNull()) {
                        QBuffer buf(&ba);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf, "PNG");
                    }
                }
                if (!ba.isEmpty()) {
                    return pushImageBytes(ba, mime);
                }
            }
        }
    }

    // 2. Check native clipboard for image
    if (nativeClipboard_ && nativeClipboard_->has_image()) {
        ClipboardImage local_img = nativeClipboard_->get_image();
        if (local_img.valid && !local_img.data.empty()) {
            QByteArray ba(reinterpret_cast<const char*>(local_img.data.data()), static_cast<int>(local_img.data.size()));
            return pushImageBytes(ba, QString::fromStdString(local_img.mime_type.empty() ? "image/png" : local_img.mime_type));
        }
    }

    // 3. Check text clipboard
    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }

    if (current.trimmed().isEmpty()) return false;

    // Check if raw binary image
    QByteArray rawBytes = current.toUtf8();
    if (rawBytes.size() >= 4) {
        const uint8_t* u = reinterpret_cast<const uint8_t*>(rawBytes.constData());
        bool isPng = (u[0] == 0x89 && u[1] == 0x50 && u[2] == 0x4E && u[3] == 0x47);
        bool isJpg = (u[0] == 0xFF && u[1] == 0xD8 && u[2] == 0xFF);
        bool isGif = (u[0] == 0x47 && u[1] == 0x49 && u[2] == 0x46 && u[3] == 0x38);
        bool isWebp = (rawBytes.size() >= 12 && u[0] == 'R' && u[1] == 'I' && u[2] == 'F' && u[3] == 'F' && u[8] == 'W' && u[9] == 'E' && u[10] == 'B' && u[11] == 'P');
        if (isPng || isJpg || isGif || isWebp) {
            QString mime = isPng ? "image/png" : (isJpg ? "image/jpeg" : (isGif ? "image/gif" : "image/webp"));
            return pushImageBytes(rawBytes, mime);
        }
    }

    return pushClipboard(current);
}

void WebClipController::copyToClipboard(const QString& text) {
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalText_ = text;
        lastLocalImgHash_.clear();
    }
    if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
    }
    if (nativeClipboard_) {
        nativeClipboard_->set_text(text.toStdString());
    }
    emit showToast("Copied to clipboard", false);
}

void WebClipController::copyImageToClipboard(int index) {
    QString dataUrl = clipModel_.getClipImageData(index);
    if (dataUrl.isEmpty()) return;

    QByteArray bytes;
    int commaIdx = dataUrl.indexOf(',');
    if (commaIdx >= 0) {
        bytes = QByteArray::fromBase64(dataUrl.mid(commaIdx + 1).toLatin1());
    } else {
        bytes = QByteArray::fromBase64(dataUrl.toLatin1());
    }

    if (bytes.isEmpty()) return;

    QString hash = computeImageHash(bytes);
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalImgHash_ = hash;
        lastLocalText_.clear();
    }

    QImage qimg;
    qimg.loadFromData(bytes);
    if (!qimg.isNull()) {
        if (QGuiApplication::clipboard()) {
            QGuiApplication::clipboard()->setImage(qimg, QClipboard::Clipboard);
        }
        if (nativeClipboard_) {
            std::vector<uint8_t> stdBytes(bytes.begin(), bytes.end());
            nativeClipboard_->set_image(stdBytes, "image/png");
        }
        emit showToast("Image copied to clipboard", false);
    }
}

bool WebClipController::saveImage(int index, const QString& destinationPath) {
    QString dataUrl = clipModel_.getClipImageData(index);
    if (dataUrl.isEmpty()) return false;

    QString dest = destinationPath.trimmed();
    if (dest.startsWith("file://")) {
        dest = QUrl(dest).toLocalFile();
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

} // namespace webclip
