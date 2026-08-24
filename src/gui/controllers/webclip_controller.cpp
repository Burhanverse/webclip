#include "webclip_controller.hpp"
#include "../theme/md3_theme.hpp"
#include "../../util/json.hpp"
#include "../../util/cli.hpp"
#include <QMetaObject>
#include <QDesktopServices>
#include <QUrl>
#include <QPointer>

namespace webclip {

WebClipController::WebClipController(QObject* parent)
    : QObject(parent) {
    nativeClipboard_ = create_clipboard();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &WebClipController::onPollTimer);

    if (QGuiApplication::clipboard()) {
        connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &WebClipController::onClipboardDataChanged);
    }

    loadSettings();

    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }
    if (!current.isEmpty()) {
        lastLocalText_ = current;
        clipModel_.addClip(lastLocalText_, "local");
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
        QMetaObject::invokeMethod(self.data(), [self, stateResp, activePort]() {
            if (!self) return;
            if (stateResp.status_code == 200) {
                JsonValue stateJson = JsonValue::parse(stateResp.body);
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
                    QString text = QString::fromStdString(data.get_string("text"));
                    QString source = QString::fromStdString(data.get_string("source"));

                    if (source == "web") return;

                    QMetaObject::invokeMethod(this, [this, text, source]() {
                        {
                            std::lock_guard<std::mutex> guard(syncLock_);
                            if (text == lastRemoteText_) return;
                            lastRemoteText_ = text;
                            lastLocalText_ = text;
                        }

                        if (QGuiApplication::clipboard()) {
                            QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
                        }
                        if (nativeClipboard_) {
                            nativeClipboard_->set_text(text.toStdString());
                        }

                        clipModel_.addClip(text, "phone");
                        emit clipReceived(text, source);
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

    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }

    if (current.trimmed().isEmpty()) return;

    {
        std::lock_guard<std::mutex> guard(syncLock_);
        if (current == lastLocalText_ || current == lastRemoteText_) {
            return;
        }
        lastLocalText_ = current;
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

bool WebClipController::pushCurrentClipboard() {
    QString current;
    if (QGuiApplication::clipboard()) {
        current = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    }
    if (current.isEmpty() && nativeClipboard_) {
        current = QString::fromStdString(nativeClipboard_->get_text());
    }
    if (current.trimmed().isEmpty()) {
        emit showToast("Local clipboard is empty", true);
        return false;
    }
    return pushClipboard(current);
}

void WebClipController::copyToClipboard(const QString& text) {
    {
        std::lock_guard<std::mutex> guard(syncLock_);
        lastLocalText_ = text;
    }
    if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
    }
    if (nativeClipboard_) {
        nativeClipboard_->set_text(text.toStdString());
    }
    emit showToast("Copied to clipboard", false);
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
