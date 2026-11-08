#include "webclip_controller.hpp"
#include "../util/debug_logger.hpp"
#include "../util/style_core_font.hpp"
#include "../util/display_scale.hpp"
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
#include <QThreadPool>
#include <QCoreApplication>
#include <QtNetwork/QNetworkInterface>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAbstractSocket>

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

QString WebClipController::normalizeText(const QString& text) {
    QString s = text;
    s.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    s.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return s.trimmed();
}

QString WebClipController::escapeForLog(const QString& text, int maxLen) {
    QString res;
    res.reserve(text.size() * 2);
    for (int i = 0; i < text.size() && res.size() < maxLen; ++i) {
        QChar c = text[i];
        if (c == QLatin1Char('\r')) {
            res.append(QLatin1String("\\r"));
        } else if (c == QLatin1Char('\n')) {
            res.append(QLatin1String("\\n"));
        } else if (c == QLatin1Char('\t')) {
            res.append(QLatin1String("\\t"));
        } else if (c.isPrint()) {
            res.append(c);
        } else {
            res.append(QStringLiteral("\\u%1").arg(static_cast<uint16_t>(c.unicode()), 4, 16, QLatin1Char('0')));
        }
    }
    if (text.size() > maxLen) {
        res.append(QLatin1String("..."));
    }
    return res;
}

bool WebClipController::shouldSuppressText(const QString& text, int64_t nowMs, int64_t windowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    QString normCurrent = normalizeText(text);
    QString normLocal = normalizeText(lastLocalText_);
    QString normRemote = normalizeText(lastRemoteText_);
    int64_t elapsed = nowMs - lastTextTimeMs_;

    auto logDecision = [&](const char* result) {
        WEBCLIP_LOG(QStringLiteral("[Suppression:Decision] text='") + escapeForLog(text, 40) +
                    QStringLiteral("' normLocal='") + escapeForLog(normLocal, 40) +
                    QStringLiteral("' elapsed=") + QString::number(elapsed) +
                    QStringLiteral("ms result=") + QString::fromLatin1(result));
    };

    if (normCurrent.isEmpty()) {
        logDecision("SUPPRESSED");
        WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Suppressed: empty text"));
        return true;
    }

    if (useWaylandWatcher_) {
        for (auto it = recentRemoteHashes_.begin(); it != recentRemoteHashes_.end(); ) {
            if ((nowMs - it->second) > 15000) {
                it = recentRemoteHashes_.erase(it);
            } else {
                ++it;
            }
        }

        std::string hash = QCryptographicHash::hash(normCurrent.toUtf8(), QCryptographicHash::Sha1).toHex().toStdString();
        auto it = recentRemoteHashes_.find(hash);
        if (it != recentRemoteHashes_.end() && (nowMs - it->second) < 15000) {
            logDecision("SUPPRESSED");
            WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Suppressed: matches recent remote write hash: '") +
                        escapeForLog(text) + QStringLiteral("'"));
            return true;
        }

        if (normCurrent == normLocal) {
            logDecision("SUPPRESSED");
            WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Suppressed: identical to local text: '") +
                        escapeForLog(text) + QStringLiteral("'"));
            return true;
        }

        logDecision("ALLOWED");
        WEBCLIP_LOG(QStringLiteral("[EchoSuppression] NOT suppressed (Wayland): text='") + escapeForLog(text) +
                    QStringLiteral("' (hash=") + QString::fromStdString(hash) + QStringLiteral(")"));
        return false;
    }

    if (normCurrent == normRemote) {
        logDecision("SUPPRESSED");
        WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Suppressed: identical to remote text: '") +
                    escapeForLog(text) + QStringLiteral("'"));
        return true;
    }

    if (normCurrent == normLocal) {
        logDecision("SUPPRESSED");
        WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Suppressed: identical to local text: '") +
                    escapeForLog(text) + QStringLiteral("'"));
        return true;
    }

    if (elapsed < windowMs) {
        logDecision("SUPPRESSED");
        WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Suppressed: elapsed (") + QString::number(elapsed) +
                    QStringLiteral("ms) < window (") + QString::number(windowMs) +
                    QStringLiteral("ms), text='") + escapeForLog(text) + QStringLiteral("'"));
        return true;
    }

    logDecision("ALLOWED");
    WEBCLIP_LOG(QStringLiteral("[EchoSuppression] NOT suppressed: text='") + escapeForLog(text) +
                QStringLiteral("' (norm='") + escapeForLog(normCurrent) +
                QStringLiteral("') vs normLocal='") + escapeForLog(normLocal) +
                QStringLiteral("', normRemote='") + escapeForLog(normRemote) +
                QStringLiteral("', elapsed=") + QString::number(elapsed) + QStringLiteral("ms"));
    return false;
}

void WebClipController::markTextApplied(const QString& text, int64_t nowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    lastLocalText_ = text;
    lastRemoteText_ = text;
    lastTextTimeMs_ = nowMs;
    WEBCLIP_LOG(QStringLiteral("[EchoSuppression] Marked text applied: '") +
                escapeForLog(text) + QStringLiteral("' at ") + QString::number(nowMs));
}

bool WebClipController::shouldSuppressImage(const QString& hash, const QString& pixelFp, int64_t nowMs, int64_t windowMs) {
    Q_UNUSED(nowMs);
    Q_UNUSED(windowMs);
    std::lock_guard<std::mutex> guard(syncLock_);
    if (!hash.isEmpty()) {
        if (hash == lastLocalImgHash_ || hash == lastRemoteImgHash_) return true;
    }
    if (!pixelFp.isEmpty()) {
        if (pixelFp == lastLocalPixelFp_ || pixelFp == lastRemotePixelFp_) return true;
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

    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setSingleShot(true);
    connect(reconnectTimer_, &QTimer::timeout, this, &WebClipController::connectToPortal);

    const bool isWayland = QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
    if (isWayland) {
        QString wlPastePath = QStandardPaths::findExecutable(QStringLiteral("wl-paste"));
        QString wlCopyPath = QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
        if (wlPastePath.isEmpty() || wlCopyPath.isEmpty()) {
            useWaylandWatcher_ = false;
            WEBCLIP_LOG(QStringLiteral("[Wayland] wl-clipboard missing (wl-paste or wl-copy not found in PATH). Install wl-clipboard for Wayland clipboard support. Falling back to QClipboard."));
        } else {
            useWaylandWatcher_ = true;
            startWaylandWatcher(true);
        }
    }

    if (!useWaylandWatcher_ && QGuiApplication::clipboard()) {
        connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &WebClipController::onClipboardDataChanged);
    }

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        shuttingDown_ = true;
        if (waylandRestartTimer_) {
            waylandRestartTimer_->stop();
        }
        if (waylandWatcherProcess_) {
            waylandWatcherProcess_->kill();
            waylandWatcherProcess_->waitForFinished(1000);
        }
        if (wlCopyProcess_) {
            wlCopyProcess_->kill();
            wlCopyProcess_->waitForFinished(1000);
        }
    });

    connect(&matugenWatcher_, &MatugenWatcher::themeLoaded, this, [this] {
        MD3Theme::instance()->setMatugenThemes(matugenWatcher_.currentThemes());
        if (accentPreset_ == QLatin1String("matugen")) {
            emit accentPresetChanged();
        }
    });
    connect(&matugenWatcher_, &MatugenWatcher::statusChanged, this, [this] {
        emit matugenStatusChanged();
    });

    downloadManager_ = std::make_shared<FileDownloadManager>();
    downloadManager_->set_progress_callback([this](const std::string& batch_id, int index, int total, int64_t downloaded, int64_t totalBytes, const std::string& name) {
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, batch_id, index, total, downloaded, totalBytes, name]() {
            if (!self) return;
            self->clipModel_.updateFileProgress(
                QString::fromStdString(batch_id),
                index,
                QString::fromStdString(name),
                downloaded,
                totalBytes);
            if (self->lastAnnouncedDownloadIndex_ != index) {
                self->lastAnnouncedDownloadIndex_ = index;
                QString msg = QStringLiteral("Downloading files (%1/%2): %3").arg(index).arg(total).arg(QString::fromStdString(name));
                emit self->showTrayNotification(QStringLiteral("WebClip File Transfer"), msg);
                emit self->showToast(msg, false);
            }
            emit self->activeTransfersChanged(true);
        });
    });
    downloadManager_->set_completion_callback([this](const std::string& batch_id, const std::vector<FileDownloadResult>& files) {
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, batch_id, files]() {
            if (!self) return;
            self->lastAnnouncedDownloadIndex_ = -1;
            int total_files = static_cast<int>(files.size());
            QString summary = QStringLiteral("Received %1 %2").arg(total_files).arg(total_files == 1 ? QStringLiteral("file") : QStringLiteral("files"));
            emit self->showTrayNotification(QStringLiteral("WebClip File Transfer"), summary);
            emit self->showToast(summary, false);
            emit self->activeTransfersChanged(self->hasActiveTransfers());
            for (int i = 0; i < total_files; ++i) {
                const auto& f = files[i];
                self->clipModel_.updateFileCompleted(
                    QString::fromStdString(batch_id),
                    i + 1,
                    QString::fromStdString(f.name),
                    QString::fromStdString(f.local_path),
                    f.size,
                    QString::fromStdString(f.mime_type));
            }
            WEBCLIP_LOG(QStringLiteral("[FileTransfer] ") + summary);
        });
    });
    downloadManager_->set_failure_callback([this](const std::string& file_name, const std::string& reason) {
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, file_name, reason]() {
            if (!self) return;
            self->lastAnnouncedDownloadIndex_ = -1;
            emit self->showToast(QStringLiteral("Download failed for %1: %2")
                .arg(QString::fromStdString(file_name)).arg(QString::fromStdString(reason)), true);
            emit self->activeTransfersChanged(self->hasActiveTransfers());
            self->clipModel_.updateFileFailed(QString(), QString::fromStdString(file_name));
            WEBCLIP_LOG(QStringLiteral("[FileTransfer] Download failed for ") +
                QString::fromStdString(file_name) + QStringLiteral(": ") + QString::fromStdString(reason));
        });
    });

    uploadManager_ = std::make_shared<FileUploadManager>();
    uploadManager_->set_progress_callback([this](const std::string& batch_id, int index, int total, int64_t uploaded, int64_t totalBytes, const std::string& name) {
        if (uploaded <= 0 && totalBytes > 0) {
            return;
        }
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, batch_id, index, total, uploaded, totalBytes, name]() {
            if (!self) return;
            self->clipModel_.updateFileProgress(
                QString::fromStdString(batch_id),
                index,
                QString::fromStdString(name),
                uploaded,
                totalBytes);
            if (self->lastAnnouncedUploadIndex_ != index) {
                self->lastAnnouncedUploadIndex_ = index;
                QString msg = QStringLiteral("Uploading files (%1/%2): %3").arg(index).arg(total).arg(QString::fromStdString(name));
                emit self->showTrayNotification(QStringLiteral("WebClip File Transfer"), msg);
                emit self->showToast(msg, false);
            }
            emit self->activeTransfersChanged(true);
        });
    });
    uploadManager_->set_completion_callback([this](const std::string& batch_id, const std::vector<FileUploadResult>& files) {
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, batch_id, files]() {
            if (!self) return;
            self->lastAnnouncedUploadIndex_ = -1;
            int total_files = static_cast<int>(files.size());
            QString summary = QStringLiteral("Sent %1 %2 to phone").arg(total_files).arg(total_files == 1 ? QStringLiteral("file") : QStringLiteral("files"));
            emit self->showTrayNotification(QStringLiteral("WebClip File Transfer"), summary);
            emit self->showToast(summary, false);
            emit self->activeTransfersChanged(self->hasActiveTransfers());
            for (int i = 0; i < total_files; ++i) {
                const auto& f = files[i];
                self->clipModel_.updateFileCompleted(
                    QString::fromStdString(batch_id),
                    i + 1,
                    QString::fromStdString(f.name),
                    QString::fromStdString(f.local_path),
                    f.size,
                    QStringLiteral("application/octet-stream"));
            }
            WEBCLIP_LOG(QStringLiteral("[FileUpload] ") + summary);
        });
    });
    uploadManager_->set_failure_callback([this](const std::string& file_name, const std::string& reason) {
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, file_name, reason]() {
            if (!self) return;
            self->lastAnnouncedUploadIndex_ = -1;
            QString failureMsg = QStringLiteral("Upload failed for %1: %2")
                .arg(QString::fromStdString(file_name)).arg(QString::fromStdString(reason));
            emit self->showTrayNotification(QStringLiteral("WebClip File Transfer"), failureMsg);
            emit self->showToast(failureMsg, true);
            emit self->activeTransfersChanged(self->hasActiveTransfers());
            self->clipModel_.updateFileFailed(QString(), QString::fromStdString(file_name));
            WEBCLIP_LOG(QStringLiteral("[FileUpload] Upload failed for ") +
                QString::fromStdString(file_name) + QStringLiteral(": ") + QString::fromStdString(reason));
        });
    });
    uploadManager_->set_batch_cancelled_callback([this](const std::string& batch_id) {
        QPointer<WebClipController> self(this);
        QMetaObject::invokeMethod(this, [self, batch_id]() {
            if (!self) return;
            self->lastAnnouncedUploadIndex_ = -1;
            self->clipModel_.updateFileStatusByBatch(QString::fromStdString(batch_id), FileStatus::Cancelled);
            emit self->showToast(QStringLiteral("Transfer cancelled on phone"), true);
            emit self->activeTransfersChanged(self->hasActiveTransfers());
            WEBCLIP_LOG(QStringLiteral("[FileUpload] Transfer cancelled on phone"));
        });
    });

    loadSettings();
    captureClipboardToHistory();
}

void WebClipController::captureClipboardToHistory() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) return;

    const QImage img = clipboard->image();
    if (!img.isNull()) {
        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        {
            std::lock_guard<std::mutex> guard(syncLock_);
            lastLocalImgHash_ = computeImageHash(ba);
        }
        clipModel_.addClipImage(saveImageBytesToCache(ba, "image/png"), "image/png", ba.size(), "local");
        return;
    }

    QString current = clipboard->text(QClipboard::Clipboard);
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }
    if (current.isEmpty()) return;
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalText_ = current;
    }
    clipModel_.addClip(current, "local");
}

WebClipController::~WebClipController() {
    shuttingDown_ = true;
    if (waylandRestartTimer_) {
        waylandRestartTimer_->stop();
    }
    if (waylandWatcherProcess_) {
        waylandWatcherProcess_->kill();
        waylandWatcherProcess_->waitForFinished(1000);
    }
    if (wlCopyProcess_) {
        wlCopyProcess_->kill();
        wlCopyProcess_->waitForFinished(1000);
    }
    if (downloadManager_) {
        downloadManager_->cancel_all();
        downloadManager_->stop();
    }
    if (uploadManager_) {
        uploadManager_->cancel_all();
        uploadManager_->stop();
    }
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
        if (preset == QLatin1String("matugen")) {
            MD3Theme::instance()->setMatugenThemes(matugenWatcher_.currentThemes());
            if (!matugenWatcher_.isLoaded()) {
                matugenWatcher_.reloadNow();
            }
        }
        MD3Theme::instance()->setAccentPreset(preset);
        saveSettings();
        emit accentPresetChanged();
    }
}

QString WebClipController::matugenThemePath() const {
    return matugenWatcher_.themePath();
}

bool WebClipController::matugenAutoReload() const {
    return matugenWatcher_.autoReload();
}

void WebClipController::setMatugenThemePath(const QString& path) {
    if (matugenWatcher_.themePath() != path) {
        matugenWatcher_.setThemePath(path);
        saveSettings();
        emit matugenThemePathChanged();
    }
}

void WebClipController::setMatugenAutoReload(bool autoReload) {
    if (matugenWatcher_.autoReload() != autoReload) {
        matugenWatcher_.setAutoReload(autoReload);
        saveSettings();
        emit matugenAutoReloadChanged();
    }
}

void WebClipController::reloadMatugenTheme() {
    matugenWatcher_.reloadNow();
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

void WebClipController::setDisplayScale(double scale) {
    double clamped = (scale <= 0.0) ? 0.0 : (std::clamp)(scale, 0.75, 1.40);
    if (std::abs(displayScale_ - clamped) > 0.001) {
        displayScale_ = clamped;
        scale::set(displayScale_);
        saveSettings();
        emit displayScaleChanged();
    }
}

QString WebClipController::downloadDirectory() const {
    if (!downloadDirectory_.isEmpty()) {
        return downloadDirectory_;
    }
    return FileDownloadManager::get_download_directory();
}

void WebClipController::setDownloadDirectory(const QString& dir) {
    if (downloadDirectory_ != dir) {
        downloadDirectory_ = dir;
        saveSettings();
        emit downloadDirectoryChanged();
    }
}

void WebClipController::restartApplication() {
    saveSettings();
    restartRequested_ = true;
    QCoreApplication::quit();
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
    config.code = code_.toStdString();
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
    if (host_.trimmed().isEmpty() || code_.trimmed().isEmpty()) {
        discoverPhoneOnLan();
        return;
    }
    connectToPortal();
}

void WebClipController::setScanningLan(bool s) {
    if (scanningLan_ != s) {
        scanningLan_ = s;
        emit scanningLanChanged();
    }
}

void WebClipController::connectToPortal() {
    if (connected_ || connecting_) return;
    reconnectTimer_->stop();

    sanitizeHostInput();

    if (host_.trimmed().isEmpty()) {
        emit showToast("Please enter a valid phone IP or hostname", true);
        return;
    }

    wantConnected_ = true;
    setConnecting(true);
    const quint64 generation = ++connectGeneration_;

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
    if (downloadManager_) {
        downloadManager_->set_client(client);
    }
    if (uploadManager_) {
        uploadManager_->set_client(client);
    }

    QPointer<WebClipController> self(this);
    std::thread([self, client, generation]() {
        HttpResponse stateResp = client->get_state();

        bool fallbackAttempted = false;
        bool fallbackHttps = false;
        int fallbackPort = 0;
        std::shared_ptr<HttpClient> activeClient = client;

        if (stateResp.status_code != 200 && stateResp.status_code != 401 && self) {
            std::string hostStr;
            std::string codeStr;
            std::string clientIdStr;
            bool currHttps = true;
            int currPort = 0;

            QMetaObject::invokeMethod(self.data(), [&]() {
                if (self) {
                    currPort = self->port_;
                    hostStr = self->host_.trimmed().toStdString();
                    codeStr = self->code_.trimmed().toStdString();
                    clientIdStr = self->clientId_;
                    currHttps = self->useHttps_;
                }
            }, Qt::BlockingQueuedConnection);

            if (self && !hostStr.empty() && !currHttps) {
                fallbackHttps = true;
                fallbackPort = currPort + 1;

                auto fallbackClient = std::make_shared<HttpClient>(
                    hostStr,
                    fallbackPort,
                    codeStr,
                    fallbackHttps,
                    true,
                    clientIdStr
                );

                HttpResponse fbResp = fallbackClient->get_state();
                if (fbResp.status_code == 200 || fbResp.status_code == 401) {
                    stateResp = fbResp;
                    fallbackAttempted = true;
                    activeClient = fallbackClient;

                    QMetaObject::invokeMethod(self.data(), [self, generation, fallbackHttps, fallbackPort, fallbackClient]() {
                        if (!self || self->connectGeneration_ != generation) return;
                        self->useHttps_ = fallbackHttps;
                        self->port_ = fallbackPort;
                        self->insecure_ = true;
                        self->httpClient_ = fallbackClient;
                        if (self->downloadManager_) {
                            self->downloadManager_->set_client(fallbackClient);
                        }
                        if (self->uploadManager_) {
                            self->uploadManager_->set_client(fallbackClient);
                        }
                        self->saveSettings();
                        emit self->useHttpsChanged();
                        emit self->portChanged();
                        emit self->insecureChanged();
                    });
                }
            }
        }

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, generation, stateResp, activeClient, fallbackAttempted, fallbackPort, fallbackHttps]() {
            if (!self || self->connectGeneration_ != generation) return;
            if (stateResp.status_code == 200) {
                JsonValue stateJson = JsonValue::parse(stateResp.body);
                std::string type = stateJson.get_string("type");
                bool hasImage = (type == "image") || stateJson.get_bool("hasImage");
                const bool preferLocal = self->hasOfflineCopy_;
                self->hasOfflineCopy_ = false;

                if (preferLocal) {
                    std::lock_guard<std::mutex> guard(self->syncLock_);
                    self->lastLocalText_.clear();
                    self->lastLocalImgHash_.clear();
                    self->lastLocalPixelFp_.clear();
                    self->lastRemoteText_.clear();
                    self->lastRemoteImgHash_.clear();
                    self->lastRemotePixelFp_.clear();
                    self->lastTextTimeMs_ = 0;
                    self->lastImgTimeMs_ = 0;
                } else if (hasImage) {
                    std::string imageUrl = stateJson.get_string("imageUrl");
                    std::string mimeType = stateJson.get_string("mimeType");
                    if (mimeType.empty()) mimeType = "image/png";

                    std::thread([self, activeClient, imageUrl, mimeType]() {
                        HttpResponse imgResp = activeClient->get_image(imageUrl);
                        if (!self) return;
                        if (imgResp.status_code != 200 || imgResp.binary_body.empty()) return;
                        QByteArray bytes(reinterpret_cast<const char*>(imgResp.binary_body.data()), static_cast<int>(imgResp.binary_body.size()));
                        std::vector<uint8_t>().swap(imgResp.binary_body);

                        QImage qimg;
                        qimg.loadFromData(bytes);
                        QString pixelFp = computePixelFingerprint(qimg);
                        QString hash = computeImageHash(bytes);
                        {
                            std::lock_guard<std::mutex> guard(self->syncLock_);
                            if (hash == self->lastRemoteImgHash_) return;
                        }
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
                    QString qRemoteText = QString::fromStdString(remoteText);
                    bool alreadyApplied = false;
                    {
                        std::lock_guard<std::mutex> guard(self->syncLock_);
                        alreadyApplied = qRemoteText == self->lastRemoteText_;
                    }
                    if (!remoteText.empty() && !alreadyApplied) {
                        int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
                        self->markTextApplied(qRemoteText, nowMs);

                        self->suppressNextLocalChange_.store(true);
                        self->suppressUntilMs_.store(nowMs + 1500);
                        self->applyRemoteTextToClipboard(qRemoteText);
                        self->clipModel_.addClip(qRemoteText, "phone");
                    }
                }

                self->reconnectAttempts_ = 0;
                self->knownHosts_.removeAll(self->host_);
                self->knownHosts_.prepend(self->host_);
                while (self->knownHosts_.size() > 8) self->knownHosts_.removeLast();
                self->saveSettings();
                emit self->knownHostsChanged();
                self->setConnecting(false);
                self->setConnected(true);
                if (fallbackAttempted) {
                    QString scheme = fallbackHttps ? "HTTPS" : "HTTP";
                    emit self->showToast("Connected to Gboard (auto-switched to " + scheme + " :" + QString::number(fallbackPort) + ")", false);
                } else {
                    emit self->showToast("Connected to Gboard Web Clipboard", false);
                }

                if (self->autoSync_) {
                    self->pollTimer_->start(static_cast<int>(self->pollInterval_ * 1000));
                }

                self->startSseListener();
            } else {
                self->setConnecting(false);
                self->setConnected(false);
                if (stateResp.status_code == 401) {
                    self->wantConnected_ = false;
                    self->reconnectAttempts_ = 0;
                    emit self->showToast("Failed to connect: Invalid pairing code", true);
                    return;
                }
                if (self->reconnectAttempts_ == 0) {
                    QString err = stateResp.error.empty() ? ("HTTP " + QString::number(stateResp.status_code)) : QString::fromStdString(stateResp.error);
                    emit self->showToast("Failed to connect: " + err + ". Retrying...", true);
                }
                self->scheduleReconnect();
            }
        });
    }).detach();
}

void WebClipController::scheduleReconnect() {
    if (!wantConnected_ || connected_ || connecting_) return;
    ++reconnectAttempts_;
    if (reconnectAttempts_ % 3 == 0) {
        discoverPhoneOnLan(false, true);
        return;
    }
    if (reconnectAttempts_ == 1 && !knownHosts_.isEmpty()) {
        discoverPhoneOnLan(true, true);
        return;
    }
    reconnectTimer_->start((std::min)(1000 << (std::min)(reconnectAttempts_, 5), 30000));
}

void WebClipController::onConnectionLost() {
    if (!connected_) return;
    stopSseListener();
    pollTimer_->stop();
    setConnected(false);
    reconnectAttempts_ = 0;
    emit showToast("Connection to phone lost. Reconnecting...", true);
    scheduleReconnect();
}

void WebClipController::disconnectFromPortal() {
    wantConnected_ = false;
    reconnectAttempts_ = 0;
    reconnectTimer_->stop();
    ++connectGeneration_;
    if (lanScanCancel_) {
        lanScanCancel_->store(true);
    }
    stopSseListener();
    if (httpClient_) {
        httpClient_->reset_connections();
    }
    pollTimer_->stop();
    setConnecting(false);
    setConnected(false);
    setScanningLan(false);
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
    sseStopFlag_ = std::make_shared<std::atomic<bool>>(false);

    QPointer<WebClipController> self(this);
    auto client = httpClient_;
    auto stopFlag = sseStopFlag_;

    sseThread_ = std::make_unique<std::thread>([self, client, stopFlag]() {
        if (!client || !stopFlag) return;
        client->stream_events(
            [self, client](const SseEvent& ev) {
                WebClipController* c = self.data();
                if (!c) return;
                if (ev.event == "file") {
                    JsonValue data = JsonValue::parse(ev.data);
                    std::string evClientId = data.get_string("clientId");
                    if (!evClientId.empty() && evClientId != client->get_client_id()) {
                        return;
                    }
                    std::string token = data.get_string("token");
                    std::string name = data.get_string("name");
                    int64_t size = data.get_int64("size");
                    std::string mimeType = data.get_string("mimeType");
                    std::string batchId = data.get_string("batchId");
                    int index = static_cast<int>(data.get_int64("index"));
                    int total = static_cast<int>(data.get_int64("total"));

                    if (token.empty() || name.empty()) {
                        return;
                    }

                    FileOfferTask task;
                    task.token = std::move(token);
                    task.name = std::move(name);
                    task.size = size;
                    task.mime_type = std::move(mimeType);
                    task.batch_id = std::move(batchId);
                    task.index = index;
                    task.total = total;
                    task.client_id = std::move(evClientId);
                    task.host = c->host().toStdString();
                    task.port = c->port();
                    task.use_https = c->useHttps();
                    task.insecure = c->insecure();
                    task.pairing_code = c->code().toStdString();

                    QPointer<WebClipController> selfCtrl(c);
                    QMetaObject::invokeMethod(c, [selfCtrl, task]() {
                        if (selfCtrl) {
                            selfCtrl->onIncomingFileOfferReceived(task);
                        }
                    });
                    return;
                }
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

                    if (source == "web" || (!evClientId.empty() && evClientId == client->get_client_id())) {
                        return;
                    }

                    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();

                    bool hasImage = (type == "image") || data.get_bool("hasImage");

                    if (hasImage) {
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
                                if (imgResp.status_code != 200 || imgResp.binary_body.empty()) {
                                    WEBCLIP_LOG(QStringLiteral("[SSE] Failed to fetch image: HTTP ") + QString::number(imgResp.status_code));
                                    QMetaObject::invokeMethod(c, [self, status = imgResp.status_code]() {
                                        if (self) emit self->showToast("Failed to download image from phone (HTTP " + QString::number(status) + ")", true);
                                    });
                                    return;
                                }
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

                    QMetaObject::invokeMethod(c, [self, text, source, nowMs]() {
                        WebClipController* c = self.data();
                        if (!c) return;
                        c->suppressNextLocalChange_.store(true);
                        c->suppressUntilMs_.store(nowMs + 1500);
                        WEBCLIP_LOG(QStringLiteral("[SSE] Received remote text from '") +
                                    QString::fromStdString(source) + QStringLiteral("': '") +
                                    WebClipController::escapeForLog(text) + QStringLiteral("', set suppressUntilMs=" ) +
                                    QString::number(nowMs + 1500));
                        c->applyRemoteTextToClipboard(text);

                        c->clipModel_.addClip(text, QString::fromStdString(source.empty() ? "phone" : source));
                        emit c->clipReceived(text, QString::fromStdString(source.empty() ? "phone" : source));
                    });
                }
            },
            [self, stopFlag](const std::string& status) {
                std::cout << "[sse] " << status << std::endl;
                if (status.find("dropped") == std::string::npos) return;
                WebClipController* c = self.data();
                if (!c) return;
                QMetaObject::invokeMethod(c, [self, stopFlag]() {
                    if (self && !stopFlag->load()) {
                        self->onConnectionLost();
                    }
                });
            },
            *stopFlag
        );
    });
}

void WebClipController::stopSseListener() {
    if (sseStopFlag_) {
        sseStopFlag_->store(true);
    }
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

void WebClipController::clearKnownHosts() {
    knownHosts_.clear();
    saveSettings();
    emit knownHostsChanged();
}

void WebClipController::discoverPhoneOnLan(bool knownHostsOnly, bool silent) {
    if (scanningLan_) return;
    setScanningLan(true);
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    lanScanCancel_ = cancel;
    if (!silent) emit showToast("Scanning Wi-Fi network for phone...", false);

    const std::string pairingCode = code_.trimmed().toStdString();
    const int currentPort = port_;
    const bool currentHttps = useHttps_;
    const QStringList knownHosts = knownHosts_;

    QPointer<WebClipController> self(this);
    std::thread([self, cancel, pairingCode, currentPort, currentHttps, knownHosts, knownHostsOnly, silent]() {
        QList<QNetworkAddressEntry> activeEntries;
        const auto interfaces = QNetworkInterface::allInterfaces();
        for (const auto& iface : interfaces) {
            if (!iface.flags().testFlag(QNetworkInterface::IsUp) ||
                !iface.flags().testFlag(QNetworkInterface::IsRunning) ||
                iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
                continue;
            }
            for (const auto& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                    entry.ip() != QHostAddress::LocalHost &&
                    !entry.ip().isLinkLocal()) {
                    activeEntries.append(entry);
                }
            }
        }

        if (activeEntries.isEmpty()) {
            QMetaObject::invokeMethod(self.data(), [self, cancel, silent]() {
                if (!self || cancel->load()) return;
                self->setScanningLan(false);
                if (!silent) emit self->showToast("No active Wi-Fi / network connection found", true);
                emit self->discoveryFinished(false, QString(), 0, false);
                self->scheduleReconnect();
            });
            return;
        }

        if (knownHostsOnly) activeEntries.clear();
        QList<QString> candidateIps = knownHosts;
        for (const auto& entry : activeEntries) {
            quint32 ip = entry.ip().toIPv4Address();
            quint32 mask = entry.netmask().toIPv4Address();
            quint32 net = ip & mask;
            quint32 bcast = net | (~mask);

            quint32 startIp = (mask >= 0xFFFFFF00) ? (net + 1) : ((ip & 0xFFFFFF00) + 1);
            quint32 endIp = (mask >= 0xFFFFFF00) ? (bcast - 1) : ((ip & 0xFFFFFF00) + 254);

            for (quint32 cur = startIp; cur <= endIp; ++cur) {
                const QString candidate = QHostAddress(cur).toString();
                if (cur != ip && !knownHosts.contains(candidate)) {
                    candidateIps.append(candidate);
                }
            }
        }

        std::atomic<bool> found{false};
        std::string foundHost;
        int foundPort = 0;
        bool foundHttps = false;
        std::atomic<bool> unauthorizedFound{false};
        std::string unauthorizedHost;
        int unauthorizedPort = 0;
        bool unauthorizedHttps = false;

        const int numWorkers = 32;
        std::atomic<size_t> ipIndex{0};
        std::vector<std::thread> workers;

        for (int w = 0; w < numWorkers; ++w) {
            workers.emplace_back([&]() {
                while (!found.load() && !cancel->load()) {
                    size_t idx = ipIndex.fetch_add(1);
                    if (idx >= static_cast<size_t>(candidateIps.size())) break;

                    const std::string hostStr = candidateIps[static_cast<int>(idx)].toStdString();

                    std::vector<std::pair<int,bool>> portsToTry;
                    portsToTry.emplace_back(currentPort, currentHttps);
                    portsToTry.emplace_back(currentHttps ? currentPort - 1 : currentPort + 1, !currentHttps);
                    portsToTry.emplace_back(8080, false);
                    portsToTry.emplace_back(8081, true);
                    std::vector<std::pair<int,bool>> cleanPorts;
                    for (auto [p, httpsFlag] : portsToTry) {
                        bool exists = false;
                        for (auto [ep, ehttps] : cleanPorts) {
                            if (ep == p && ehttps == httpsFlag) { exists = true; break; }
                        }
                        if (!exists) cleanPorts.emplace_back(p, httpsFlag);
                    }
                    portsToTry = std::move(cleanPorts);

                    for (auto [p, httpsFlag] : portsToTry) {
                        if (found.load() || cancel->load()) break;

                        bool isHttps = httpsFlag;
                        HttpClient probeClient(hostStr, p, pairingCode, isHttps, true, "probe-discovery");
                        HttpResponse r = probeClient.get_state();

                        const bool authorized = r.status_code == 200 || (r.status_code == 401 && pairingCode.empty());
                        if (authorized) {
                            bool expected = false;
                            if (found.compare_exchange_strong(expected, true)) {
                                foundHost = hostStr;
                                foundPort = p;
                                foundHttps = isHttps;
                            }
                            break;
                        }
                        if (r.status_code == 401) {
                            bool expected = false;
                            if (unauthorizedFound.compare_exchange_strong(expected, true)) {
                                unauthorizedHost = hostStr;
                                unauthorizedPort = p;
                                unauthorizedHttps = isHttps;
                            }
                            break;
                        }
                    }
                }
            });
        }

        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }

        if (!found.load() && unauthorizedFound.load()) {
            foundHost = unauthorizedHost;
            foundPort = unauthorizedPort;
            foundHttps = unauthorizedHttps;
            found.store(true);
        }

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, cancel, silent, found = found.load(), foundHost, foundPort, foundHttps]() {
            if (!self || cancel->load()) return;
            self->setScanningLan(false);
            if (found) {
                QString hostQ = QString::fromStdString(foundHost);
                self->host_ = hostQ;
                self->port_ = foundPort;
                self->useHttps_ = foundHttps;
                self->insecure_ = true;
                self->saveSettings();

                emit self->hostChanged();
                emit self->portChanged();
                emit self->useHttpsChanged();
                emit self->insecureChanged();

                QString scheme = foundHttps ? "HTTPS" : "HTTP";
                emit self->showToast("Found phone at " + hostQ + " (" + scheme + " :" + QString::number(foundPort) + ")", false);
                emit self->discoveryFinished(true, hostQ, foundPort, foundHttps);

                self->connectToPortal();
            } else {
                if (!silent) emit self->showToast("No phone found on current Wi-Fi network", true);
                emit self->discoveryFinished(false, QString(), 0, false);
                self->scheduleReconnect();
            }
        });
    }).detach();
}

void WebClipController::onClipboardDataChanged() {
    if (useWaylandWatcher_) return;
    WEBCLIP_LOG(QStringLiteral("[Clipboard:Signal] QClipboard::dataChanged received"));
    if (!autoSync_) return;

    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    bool flagSuppressed = suppressNextLocalChange_.exchange(false);
    bool timeSuppressed = (nowMs < suppressUntilMs_.load());

    if (flagSuppressed || timeSuppressed) {
        WEBCLIP_LOG(QStringLiteral("[Clipboard] onClipboardDataChanged suppressed (flag=") +
                    (flagSuppressed ? QStringLiteral("true") : QStringLiteral("false")) +
                    QStringLiteral(", timeSuppressed=") +
                    (timeSuppressed ? QStringLiteral("true") : QStringLiteral("false")) +
                    QStringLiteral(")"));
        return;
    }

    if (!connected_) {
        captureClipboardToHistory();
        hasOfflineCopy_ = true;
        return;
    }

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

    const bool isWayland = !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") ||
                           QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
    const bool isAppActive = (QGuiApplication::applicationState() == Qt::ApplicationActive) ||
                             (QGuiApplication::focusWindow() != nullptr);
    QString current;
    if ((!isWayland || isAppActive) && QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    WEBCLIP_LOG(QStringLiteral("[Clipboard:QClipboardRead] raw='") + escapeForLog(current, 40) + QStringLiteral("'"));
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

    int controlCharCount = 0;
    for (QChar ch : current) {
        ushort u = ch.unicode();
        if (u < 0x20 && u != '\t' && u != '\n' && u != '\r') {
            controlCharCount++;
            if (controlCharCount > 2) {
                return;
            }
        }
    }

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

    QString candidatePath = current.trimmed();
    if (candidatePath.startsWith(QStringLiteral("file://"))) {
        candidatePath = QUrl(candidatePath).toLocalFile();
    }
    if (!candidatePath.contains('\n') && !candidatePath.contains('\r')) {
        QFileInfo fi(candidatePath);
        if (fi.isAbsolute() && fi.isFile() && fi.exists() && fi.isReadable() && fi.size() > 0 && fi.size() <= 20 * 1024 * 1024) {
            QString suffix = fi.suffix().toLower();
            if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("webp")) {
                QFile f(candidatePath);
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray fileBytes = f.readAll();
                    f.close();
                    if (fileBytes.size() >= 4) {
                        const uint8_t* fu = reinterpret_cast<const uint8_t*>(fileBytes.constData());
                        bool fileIsPng = (fu[0] == 0x89 && fu[1] == 0x50 && fu[2] == 0x4E && fu[3] == 0x47);
                        bool fileIsJpg = (fu[0] == 0xFF && fu[1] == 0xD8 && fu[2] == 0xFF);
                        bool fileIsWebp = (fileBytes.size() >= 12 && fu[0] == 'R' && fu[1] == 'I' && fu[2] == 'F' && fu[3] == 'F' && fu[8] == 'W' && fu[9] == 'E' && fu[10] == 'B' && fu[11] == 'P');
                        if (fileIsPng || fileIsJpg || fileIsWebp) {
                            QString mime = fileIsPng ? QStringLiteral("image/png") : (fileIsJpg ? QStringLiteral("image/jpeg") : QStringLiteral("image/webp"));
                            QString hash = computeImageHash(fileBytes);
                            QString pixelFp = computePixelFingerprint(fileBytes);
                            if (shouldSuppressImage(hash, pixelFp, nowMs, 2000)) {
                                return;
                            }
                            markImageApplied(hash, pixelFp, nowMs);
                            markTextApplied(current, nowMs);
                            pushImageBytes(fileBytes, mime);
                            return;
                        }
                    }
                }
            }
        }
    }

    if (shouldSuppressText(current, nowMs, 2000)) {
        return;
    }

    WEBCLIP_LOG(QStringLiteral("[Clipboard] handleNativeText pushing: '") + escapeForLog(current) + QStringLiteral("'"));
    markTextApplied(current, nowMs);
    pushClipboard(current);
}

void WebClipController::onPollTimer() {
    if (useWaylandWatcher_) return;
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
        QString qtext = QString::fromStdString(result.text);
        WEBCLIP_LOG(QStringLiteral("[Clipboard:NativeRead] source=wl-paste raw='") + escapeForLog(qtext, 40) + QStringLiteral("'"));
        handleNativeText(qtext);
    }
}

void WebClipController::startWaylandWatcher(bool initial) {
    if (!useWaylandWatcher_ || shuttingDown_) return;

    if (waylandWatcherProcess_) {
        waylandWatcherProcess_->disconnect(this);
        waylandWatcherProcess_->kill();
        waylandWatcherProcess_->deleteLater();
        waylandWatcherProcess_ = nullptr;
    }

    waylandWatcherBuffer_.clear();
    waylandWatcherProcess_ = new QProcess(this);

    connect(waylandWatcherProcess_, &QProcess::readyReadStandardOutput,
            this, &WebClipController::onWaylandWatcherReadyRead);

    connect(waylandWatcherProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (shuttingDown_) return;
        scheduleWaylandWatcherRestart();
    });

    QString wlPasteBin = QStandardPaths::findExecutable(QStringLiteral("wl-paste"));
    if (wlPasteBin.isEmpty()) {
        wlPasteBin = QStringLiteral("wl-paste");
    }

    QStringList args;
    args << QStringLiteral("--watch") << QStringLiteral("sh") << QStringLiteral("-c")
         << QString::fromUtf8(R"(types=$(wl-paste --list-types 2>/dev/null); case "$types" in *"image/"*) cat >/dev/null; printf '__WEBCLIP_IMAGE__\0' ;; *"text/plain;charset=utf-8"*|*"text/plain"*) cat; printf '\0' ;; *) cat >/dev/null ;; esac)");

    waylandWatcherProcess_->start(wlPasteBin, args);
    if (initial) {
        WEBCLIP_LOG(QStringLiteral("[WaylandWatcher] Watcher started"));
    }
}

void WebClipController::scheduleWaylandWatcherRestart() {
    if (!useWaylandWatcher_ || shuttingDown_) return;
    if (!waylandRestartTimer_) {
        waylandRestartTimer_ = new QTimer(this);
        waylandRestartTimer_->setSingleShot(true);
        connect(waylandRestartTimer_, &QTimer::timeout, this, &WebClipController::restartWaylandWatcher);
    }
    if (!waylandRestartTimer_->isActive()) {
        waylandRestartTimer_->start(waylandRestartBackoffMs_);
        waylandRestartBackoffMs_ = std::min(waylandRestartBackoffMs_ * 2, 8000);
    }
}

void WebClipController::restartWaylandWatcher() {
    if (!useWaylandWatcher_ || shuttingDown_) return;
    WEBCLIP_LOG(QStringLiteral("[WaylandWatcher] Watcher restarted"));
    startWaylandWatcher(false);
}

void WebClipController::onWaylandWatcherReadyRead() {
    if (!waylandWatcherProcess_) return;
    waylandRestartBackoffMs_ = 1000;
    waylandWatcherBuffer_.append(waylandWatcherProcess_->readAllStandardOutput());

    int idx = -1;
    while ((idx = waylandWatcherBuffer_.indexOf('\0')) >= 0) {
        QByteArray chunk = waylandWatcherBuffer_.left(idx);
        waylandWatcherBuffer_.remove(0, idx + 1);

        if (chunk == "__WEBCLIP_IMAGE__") {
            WEBCLIP_LOG(QStringLiteral("[WaylandWatcher] Image event received"));
            if (!connected_) {
                QPointer<WebClipController> self(this);
                std::thread([self]() {
                    auto cb = webclip::create_clipboard();
                    if (!cb || !cb->has_image()) return;
                    ClipboardImage img = cb->get_image();
                    if (!img.valid || img.data.empty()) return;
                    QMetaObject::invokeMethod(self.data(), [self, img = std::move(img)]() mutable {
                        if (!self) return;
                        QByteArray ba(reinterpret_cast<const char*>(img.data.data()), static_cast<int>(img.data.size()));
                        QString mime = QString::fromStdString(img.mime_type.empty() ? "image/png" : img.mime_type);
                        QString fileUrl = saveImageBytesToCache(ba, mime);
                        self->clipModel_.addClipImage(fileUrl, mime, ba.size(), "local");
                    });
                }).detach();
                continue;
            }
            if (!autoSync_) continue;
            QPointer<WebClipController> self(this);
            std::thread([self]() {
                auto cb = webclip::create_clipboard();
                if (!cb || !cb->has_image()) return;
                ClipboardImage img = cb->get_image();
                if (!img.valid || img.data.empty()) return;
                QMetaObject::invokeMethod(self.data(), [self, img = std::move(img)]() mutable {
                    if (self) self->handleNativeImage(std::move(img));
                });
            }).detach();
            continue;
        }

        QString text = QString::fromUtf8(chunk);
        if (!text.isEmpty()) {
            WEBCLIP_LOG(QStringLiteral("[WaylandWatcher] Text received: '") +
                        escapeForLog(text, 40) + QStringLiteral("'"));
            if (!connected_) {
                {
                    std::lock_guard<std::mutex> guard(syncLock_);
                    lastLocalText_ = text;
                }
                clipModel_.addClip(text, "local");
                hasOfflineCopy_ = true;
                continue;
            }
            if (!autoSync_) continue;
            handleNativeText(text);
        }
    }
}

void WebClipController::writeWaylandClipboard(const QString& text) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, text]() {
            writeWaylandClipboard(text);
        });
        return;
    }

    if (wlCopyProcess_) {
        wlCopyProcess_->disconnect(this);
        wlCopyProcess_->kill();
        wlCopyProcess_->deleteLater();
        wlCopyProcess_ = nullptr;
    }

    wlCopyProcess_ = new QProcess(this);

    connect(wlCopyProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (wlCopyProcess_) {
            wlCopyProcess_->deleteLater();
            wlCopyProcess_ = nullptr;
        }
    });

    connect(wlCopyProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (wlCopyProcess_) {
            wlCopyProcess_->deleteLater();
            wlCopyProcess_ = nullptr;
        }
    });

    QString wlCopyBin = QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
    if (wlCopyBin.isEmpty()) {
        wlCopyBin = QStringLiteral("wl-copy");
    }

    QStringList args;
    args << QStringLiteral("--type") << QStringLiteral("text/plain");

    wlCopyProcess_->start(wlCopyBin, args);
    if (wlCopyProcess_->waitForStarted(1000)) {
        wlCopyProcess_->write(text.toUtf8());
        wlCopyProcess_->closeWriteChannel();
    } else {
        wlCopyProcess_->deleteLater();
        wlCopyProcess_ = nullptr;
    }
}

void WebClipController::recordRecentRemoteText(const QString& text, int64_t nowMs) {
    std::lock_guard<std::mutex> guard(syncLock_);
    QString norm = normalizeText(text);
    if (norm.isEmpty()) return;
    std::string hash = QCryptographicHash::hash(norm.toUtf8(), QCryptographicHash::Sha1).toHex().toStdString();

    for (auto it = recentRemoteHashes_.begin(); it != recentRemoteHashes_.end(); ) {
        if ((nowMs - it->second) > 15000) {
            it = recentRemoteHashes_.erase(it);
        } else {
            ++it;
        }
    }

    recentRemoteHashes_[hash] = nowMs;

    while (recentRemoteHashes_.size() > 32) {
        auto oldest = recentRemoteHashes_.begin();
        for (auto it = recentRemoteHashes_.begin(); it != recentRemoteHashes_.end(); ++it) {
            if (it->second < oldest->second) {
                oldest = it;
            }
        }
        recentRemoteHashes_.erase(oldest);
    }
}

void WebClipController::applyRemoteTextToClipboard(const QString& text) {
    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    recordRecentRemoteText(text, nowMs);

    if (useWaylandWatcher_) {
        writeWaylandClipboard(text);
    } else if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
    } else if (nativeClipboard_) {
        nativeClipboard_->set_text(text.toStdString());
    }
}

bool WebClipController::pushClipboard(const QString& text, const QString& clipId) {
    if (!httpClient_ || !connected_) {
        emit showToast("Not connected to phone", true);
        return false;
    }

    std::string cId = clipId.isEmpty() ? generateClipId() : clipId.toStdString();
    markClipIdHandled(cId);

    WEBCLIP_LOG(QStringLiteral("[Push:Dispatch] text='") + escapeForLog(text, 40) + QStringLiteral("'"));
    WEBCLIP_LOG(QStringLiteral("[Push] Pushing clipboard to phone (id=") + QString::fromStdString(cId) +
                QStringLiteral("): '") + escapeForLog(text) + QStringLiteral("'"));

    auto client = httpClient_;
    std::string textStd = text.toStdString();
    QPointer<WebClipController> self(this);
    std::thread([self, client, text, textStd, cId]() {
        if (!client) return;
        HttpResponse resp = client->push_clipboard(textStd, cId);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, text, resp, cId]() {
            if (!self) return;
            if (resp.status_code == 200) {
                WEBCLIP_LOG(QStringLiteral("[Push] Push succeeded (id=") + QString::fromStdString(cId) + QStringLiteral(")"));
                self->clipModel_.addClip(text, "local");
                emit self->showToast("Pushed " + QString::number(text.length()) + " chars to phone", false);
            } else {
                WEBCLIP_LOG(QStringLiteral("[Push] Push failed (id=") + QString::fromStdString(cId) +
                            QStringLiteral(") HTTP ") + QString::number(resp.status_code));
                emit self->showToast("Push failed (HTTP " + QString::number(resp.status_code) + ")", true);
            }
        });
    }).detach();

    return true;
}

bool WebClipController::uploadFiles(const QStringList& filePaths) {
    if (!connected_) {
        emit showToast(QStringLiteral("Not connected to phone"), true);
        return false;
    }
    QStringList validFiles;
    for (const QString& path : filePaths) {
        QFileInfo fi(path);
        if (fi.exists() && fi.isFile() && fi.isReadable()) {
            validFiles.append(path);
        }
    }
    if (validFiles.isEmpty()) {
        emit showToast(QStringLiteral("No valid files to send"), true);
        return false;
    }
    if (validFiles.size() > 500) {
        emit showToast(QStringLiteral("Transfer capped at 500 files per batch"), true);
        validFiles = validFiles.mid(0, 500);
    }
    if (!uploadManager_) {
        return false;
    }
    lastAnnouncedUploadIndex_ = -1;
    QString batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int total = validFiles.size();
    std::vector<FileUploadResult> batchFiles;
    int64_t batchTotalSize = 0;
    batchFiles.reserve(total);
    for (const QString& filePath : validFiles) {
        QFileInfo fi(filePath);
        batchFiles.push_back({fi.fileName().toStdString(), filePath.toStdString(), fi.size()});
        batchTotalSize += fi.size();
    }
    for (int i = 0; i < total; ++i) {
        const QString& filePath = validFiles.at(i);
        QFileInfo fi(filePath);
        clipModel_.addClipFile(
            fi.fileName(),
            fi.size(),
            filePath,
            QStringLiteral("application/octet-stream"),
            QStringLiteral("desktop"),
            batchId,
            i + 1,
            total,
            FileStatus::Transferring
        );
        FileUploadTask task;
        task.upload_id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        task.local_path = filePath.toStdString();
        task.file_name = fi.fileName().toStdString();
        task.file_size = fi.size();
        task.batch_id = batchId.toStdString();
        task.index = i + 1;
        task.total = total;
        task.client_id = clientId_;
        task.host = host_.toStdString();
        task.port = port_;
        task.use_https = useHttps_;
        task.insecure = insecure_;
        task.pairing_code = code_.toStdString();
        task.batch_files = batchFiles;
        task.batch_total_size = batchTotalSize;
        uploadManager_->enqueue_task(std::move(task));
    }
    emit activeTransfersChanged(true);
    return true;
}

void WebClipController::cancelTransfers() {
    if (downloadManager_) {
        downloadManager_->cancel_all();
    }
    if (uploadManager_) {
        uploadManager_->cancel_all();
    }
    clipModel_.cancelActiveTransfers();
    emit activeTransfersChanged(false);
}

bool WebClipController::hasActiveTransfers() const {
    const bool dl = downloadManager_ && downloadManager_->is_busy();
    const bool ul = uploadManager_ && uploadManager_->is_busy();
    return dl || ul;
}

void WebClipController::acceptFileOffer(const QString& batchId) {
    clipModel_.updateFileStatusByBatch(batchId, FileStatus::Transferring);
    emit hideOfferBanner();
    auto client = httpClient_;
    if (client && !batchId.isEmpty()) {
        std::string bId = batchId.toStdString();
        QThreadPool::globalInstance()->start([client, bId]() {
            std::string path = "/files/offer/" + bId + "/respond";
            client->post_json(path, "{\"accept\":true}", 5L);
        });
    }
    auto it = pendingIncomingBatches_.find(batchId);
    if (it != pendingIncomingBatches_.end()) {
        auto ctx = it.value();
        if (ctx->expiryTimer) {
            ctx->expiryTimer->stop();
            ctx->expiryTimer->deleteLater();
            ctx->expiryTimer = nullptr;
        }
        if (downloadManager_) {
            for (auto& task : ctx->tasks) {
                downloadManager_->enqueue_task(std::move(task));
            }
        }
        pendingIncomingBatches_.erase(it);
    }
}

void WebClipController::declineFileOffer(const QString& batchId) {
    clipModel_.updateFileStatusByBatch(batchId, FileStatus::Cancelled);
    emit hideOfferBanner();
    auto client = httpClient_;
    if (client && !batchId.isEmpty()) {
        std::string bId = batchId.toStdString();
        QThreadPool::globalInstance()->start([client, bId]() {
            std::string path = "/files/offer/" + bId + "/respond";
            client->post_json(path, "{\"accept\":false}", 5L);
        });
    }
    auto it = pendingIncomingBatches_.find(batchId);
    if (it != pendingIncomingBatches_.end()) {
        auto ctx = it.value();
        if (ctx->expiryTimer) {
            ctx->expiryTimer->stop();
            ctx->expiryTimer->deleteLater();
            ctx->expiryTimer = nullptr;
        }
        pendingIncomingBatches_.erase(it);
    }
}

void WebClipController::onIncomingBatchExpired(const QString& batchId) {
    declineFileOffer(batchId);
}

void WebClipController::onIncomingFileOfferReceived(const FileOfferTask& task) {
    QString bId = QString::fromStdString(task.batch_id);
    if (bId.isEmpty()) {
        bId = QStringLiteral("single_") + QString::fromStdString(task.token);
    }
    auto& ctx = pendingIncomingBatches_[bId];
    if (!ctx) {
        ctx = std::make_shared<IncomingBatchContext>();
        ctx->batchId = bId;
    }
    ctx->tasks.push_back(task);
    ctx->totalBytes += task.size;
    if (task.total > 0) {
        ctx->expectedTotal = task.total;
    }

    const bool allArrived = (ctx->expectedTotal > 0 && static_cast<int>(ctx->tasks.size()) >= ctx->expectedTotal);

    if (!ctx->debounceTimer) {
        ctx->debounceTimer = new QTimer(this);
        ctx->debounceTimer->setSingleShot(true);
        connect(ctx->debounceTimer, &QTimer::timeout, this, [this, bId]() {
            onIncomingBatchDebounced(bId);
        });
    }

    if (allArrived) {
        ctx->debounceTimer->stop();
        onIncomingBatchDebounced(bId);
    } else {
        ctx->debounceTimer->start(400);
    }
}

void WebClipController::onIncomingBatchDebounced(const QString& batchId) {
    auto it = pendingIncomingBatches_.find(batchId);
    if (it == pendingIncomingBatches_.end()) return;
    auto ctx = it.value();
    if (ctx->debounceTimer) {
        ctx->debounceTimer->stop();
        ctx->debounceTimer->deleteLater();
        ctx->debounceTimer = nullptr;
    }

    if (!askReceiveFiles_) {
        auto client = httpClient_;
        if (client) {
            std::string bId = batchId.toStdString();
            QThreadPool::globalInstance()->start([client, bId]() {
                client->post_json("/files/offer/" + bId + "/respond", "{\"accept\":true}", 5L);
            });
        }
        int count = static_cast<int>(ctx->tasks.size());
        for (int i = 0; i < count; ++i) {
            const auto& t = ctx->tasks[i];
            clipModel_.addClipFile(
                QString::fromStdString(t.name),
                t.size,
                QString(),
                QString::fromStdString(t.mime_type),
                QStringLiteral("phone"),
                batchId,
                i + 1,
                count,
                FileStatus::Transferring
            );
        }
        if (downloadManager_) {
            for (auto& task : ctx->tasks) {
                downloadManager_->enqueue_task(std::move(task));
            }
        }
        pendingIncomingBatches_.erase(it);
        return;
    }

    int count = static_cast<int>(ctx->tasks.size());
    auto formatBytes = [](qint64 bytes) -> QString {
        if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
        if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        if (bytes < 1024LL * 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
        return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    };

    QString title = QStringLiteral("Incoming file transfer from Phone");
    QString details;
    if (count == 1) {
        details = QStringLiteral("%1 (%2)").arg(QString::fromStdString(ctx->tasks[0].name)).arg(formatBytes(ctx->totalBytes));
    } else {
        details = QStringLiteral("%1 files (%2)").arg(count).arg(formatBytes(ctx->totalBytes));
    }

    for (int i = 0; i < count; ++i) {
        const auto& t = ctx->tasks[i];
        clipModel_.addClipFile(
            QString::fromStdString(t.name),
            t.size,
            QString(),
            QString::fromStdString(t.mime_type),
            QStringLiteral("phone"),
            batchId,
            i + 1,
            count,
            FileStatus::Pending
        );
    }

    emit showOfferBanner(batchId, title, details);

    ctx->expiryTimer = new QTimer(this);
    ctx->expiryTimer->setSingleShot(true);
    connect(ctx->expiryTimer, &QTimer::timeout, this, [this, batchId]() {
        onIncomingBatchExpired(batchId);
    });
    ctx->expiryTimer->start(120000);
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
    int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalText_ = text;
        lastRemoteText_ = text;
        lastTextTimeMs_ = nowMs;
        lastLocalImgHash_.clear();
        lastLocalPixelFp_.clear();
    }
    suppressNextLocalChange_.store(true);
    suppressUntilMs_.store(nowMs + 1500);
    WEBCLIP_LOG(QStringLiteral("[Clipboard] copyToClipboard: '") + escapeForLog(text) +
                QStringLiteral("', suppressUntilMs=") + QString::number(nowMs + 1500));
    if (useWaylandWatcher_) {
        writeWaylandClipboard(text);
    } else if (QGuiApplication::clipboard()) {
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

void WebClipController::setDebugLogging(bool enabled) {
    if (debugLogging_ != enabled) {
        debugLogging_ = enabled;
        DebugLogger::setEnabled(enabled);
        saveSettings();
        emit debugLoggingChanged();
    }
}

void WebClipController::setAskReceiveFiles(bool ask) {
    if (askReceiveFiles_ != ask) {
        askReceiveFiles_ = ask;
        saveSettings();
        emit askReceiveFilesChanged();
    }
}

void WebClipController::saveSettings() {
    QSettings s("Burhanverse", "WebClip");
    s.setValue("host", host_);
    s.setValue("knownHosts", knownHosts_);
    s.setValue("port", port_);
    s.setValue("code", code_);
    s.setValue("useHttps", useHttps_);
    s.setValue("insecure", insecure_);
    s.setValue("autoSync", autoSync_);
    s.setValue("autoConnect", autoConnect_);
    s.setValue("askReceiveFiles", askReceiveFiles_);
    s.setValue("pollInterval", pollInterval_);
    s.setValue("debugLogging", debugLogging_);
    s.setValue("themeMode", themeMode_);
    s.setValue("accentPreset", accentPreset_);
    s.setValue("customColor", customColor_.name());
    s.setValue("matugenThemePath", matugenWatcher_.themePath() == MatugenWatcher::defaultThemePath() ? QString() : matugenWatcher_.themePath());
    s.setValue("matugenAutoReload", matugenWatcher_.autoReload());
    s.setValue("displayScale", displayScale_);
    s.setValue("downloadDirectory", downloadDirectory_);
}

void WebClipController::loadSettings() {
    QSettings s("Burhanverse", "WebClip");
    host_ = s.value("host", "192.168.1.50").toString();
    knownHosts_ = s.value("knownHosts").toStringList();
    port_ = s.value("port", 8081).toInt();
    code_ = s.value("code", "").toString();
    useHttps_ = s.value("useHttps", true).toBool();
    insecure_ = s.value("insecure", true).toBool();
    autoSync_ = s.value("autoSync", true).toBool();
    autoConnect_ = s.value("autoConnect", false).toBool();
    askReceiveFiles_ = s.value("askReceiveFiles", true).toBool();
    pollInterval_ = s.value("pollInterval", 1.0).toDouble();
    debugLogging_ = s.value("debugLogging", false).toBool();
    DebugLogger::setEnabled(debugLogging_);
    downloadDirectory_ = s.value("downloadDirectory", "").toString();
    themeMode_ = s.value("themeMode", 0).toInt();
    accentPreset_ = s.value("accentPreset", "purple").toString();
    customColor_ = QColor(s.value("customColor", "#6750A4").toString());
    if (!customColor_.isValid()) {
        customColor_ = QColor("#6750A4");
    }
    if (s.contains("displayScale")) {
        displayScale_ = s.value("displayScale", 0.0).toDouble();
    } else if (s.contains("fontScale")) {
        displayScale_ = s.value("fontScale", 0.0).toDouble();
    } else {
        displayScale_ = 0.0;
    }
    if (displayScale_ > 0.0) {
        displayScale_ = (std::clamp)(displayScale_, 0.75, 1.40);
    }
    QString matugenPath = s.value("matugenThemePath", "").toString();
    bool matugenAuto = s.value("matugenAutoReload", true).toBool();
    matugenWatcher_.setThemePath(matugenPath);
    matugenWatcher_.setAutoReload(matugenAuto);
    if (!matugenWatcher_.isLoaded()) {
        matugenWatcher_.reloadNow();
    }
    if (accentPreset_ == QLatin1String("matugen")) {
        if (matugenWatcher_.isLoaded()) {
            MD3Theme::instance()->setMatugenThemes(matugenWatcher_.currentThemes());
        }
    }
    MD3Theme::instance()->setCustomColor(customColor_);
    MD3Theme::instance()->setThemeMode(themeMode_);
    MD3Theme::instance()->setAccentPreset(accentPreset_);
    scale::set(displayScale_);
}

void WebClipController::openUrl(const QString& urlStr) {
    QDesktopServices::openUrl(QUrl(urlStr));
}

void WebClipController::notifyMinimizedToTray() {
    emit minimizedToTray();
}

}
