#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QTimer>
#include <QThread>
#include <QSettings>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <memory>
#include <atomic>
#include <mutex>
#include <deque>
#include <unordered_set>
#include <QtQml/qqmlregistration.h>
#include "../models/clipboard_history_model.hpp"
#include "../../clipboard/clipboard.hpp"
#include "../../net/http_client.hpp"
#include "../../version.hpp"

namespace webclip {

class WebClipController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString code READ code WRITE setCode NOTIFY codeChanged)
    Q_PROPERTY(bool useHttps READ useHttps WRITE setUseHttps NOTIFY useHttpsChanged)
    Q_PROPERTY(bool insecure READ insecure WRITE setInsecure NOTIFY insecureChanged)
    Q_PROPERTY(bool autoSync READ autoSync WRITE setAutoSync NOTIFY autoSyncChanged)
    Q_PROPERTY(double pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)
    Q_PROPERTY(QString clipboardBackend READ clipboardBackend CONSTANT)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString accentPreset READ accentPreset WRITE setAccentPreset NOTIFY accentPresetChanged)
    Q_PROPERTY(QColor customColor READ customColor WRITE setCustomColor NOTIFY customColorChanged)
    Q_PROPERTY(bool thanosSnapEnabled READ thanosSnapEnabled WRITE setThanosSnapEnabled NOTIFY thanosSnapEnabledChanged)
    Q_PROPERTY(ClipboardHistoryModel* clipModel READ clipModel CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)

public:
    explicit WebClipController(QObject* parent = nullptr);
    ~WebClipController() override;

    bool connected() const { return connected_; }
    bool connecting() const { return connecting_; }
    QString host() const { return host_; }
    int port() const { return port_; }
    QString code() const { return code_; }
    bool useHttps() const { return useHttps_; }
    bool insecure() const { return insecure_; }
    bool autoSync() const { return autoSync_; }
    double pollInterval() const { return pollInterval_; }
    QString clipboardBackend() const;
    int themeMode() const { return themeMode_; }
    QString accentPreset() const { return accentPreset_; }
    QColor customColor() const { return customColor_; }
    bool thanosSnapEnabled() const { return thanosSnapEnabled_; }
    ClipboardHistoryModel* clipModel() { return &clipModel_; }
    QString appVersion() const { return QString::fromUtf8(VERSION_STRING.data(), VERSION_STRING.size()); }
    QString qtVersion() const { return QString::fromLatin1(qVersion()); }

    Q_INVOKABLE void openUrl(const QString& urlStr);

    void setHost(const QString& host);
    void setPort(int port);
    void setCode(const QString& code);
    void setUseHttps(bool useHttps);
    void setInsecure(bool insecure);
    void setAutoSync(bool autoSync);
    void setPollInterval(double interval);
    Q_INVOKABLE void setThanosSnapEnabled(bool enabled);
    Q_INVOKABLE void setThemeMode(int mode);
    Q_INVOKABLE void setAccentPreset(const QString& preset);
    Q_INVOKABLE void setCustomColor(const QColor& color);

    Q_INVOKABLE void connectToPortal();
    Q_INVOKABLE void disconnectFromPortal();
    Q_INVOKABLE void toggleConnection();
    Q_INVOKABLE bool pushClipboard(const QString& text, const QString& clipId = "");
    Q_INVOKABLE bool pushImage(const QString& filePathOrDataUrl);
    Q_INVOKABLE bool pushImageBytes(const QByteArray& bytes, const QString& mimeType = "image/png", const QString& clipId = "");
    Q_INVOKABLE bool pushCurrentClipboard();
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void copyImageToClipboard(int index);
    Q_INVOKABLE bool saveImage(int index, const QString& destinationPath);
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void notifyMinimizedToTray();

signals:
    void connectedChanged();
    void connectingChanged();
    void hostChanged();
    void portChanged();
    void codeChanged();
    void useHttpsChanged();
    void insecureChanged();
    void autoSyncChanged();
    void pollIntervalChanged();
    void themeModeChanged();
    void accentPresetChanged();
    void customColorChanged();
    void thanosSnapEnabledChanged();
    void clipReceived(const QString& text, const QString& source);
    void showToast(const QString& message, bool isError);
    void minimizedToTray();

private slots:
    void onPollTimer();
    void onClipboardDataChanged();

private:
    bool connected_ = false;
    bool connecting_ = false;
    QString host_ = "192.168.1.50";
    int port_ = 8080;
    QString code_ = "";
    bool useHttps_ = false;
    bool insecure_ = true;
    bool autoSync_ = true;
    bool thanosSnapEnabled_ = true;
    double pollInterval_ = 1.0;
    int themeMode_ = 0;
    QString accentPreset_ = "purple";
    QColor customColor_ = QColor("#6750A4");

    ClipboardHistoryModel clipModel_;
    std::unique_ptr<IClipboard> nativeClipboard_;
    std::shared_ptr<HttpClient> httpClient_;

    QTimer* pollTimer_ = nullptr;

    std::shared_ptr<std::atomic<bool>> sseStopFlag_{std::make_shared<std::atomic<bool>>(false)};
    std::unique_ptr<std::thread> sseThread_;
    std::mutex syncLock_;
    std::string clientId_;
    std::atomic<bool> suppressNextLocalChange_{false};
    QString lastRemoteText_;
    QString lastLocalText_;
    int64_t lastTextTimeMs_ = 0;
    QString lastRemoteImgHash_;
    QString lastLocalImgHash_;
    QString lastRemotePixelFp_;
    QString lastLocalPixelFp_;
    int64_t lastImgTimeMs_ = 0;

    std::deque<std::string> handledClipIds_;
    std::unordered_set<std::string> handledClipIdSet_;

    void setConnected(bool c);
    void setConnecting(bool c);
    void startSseListener();
    void stopSseListener();
    void sanitizeHostInput();
    static QString computeImageHash(const QByteArray& data);
    static QString computeQImageFingerprint(const QImage& img);
    static std::string generateClipId();
    bool isClipIdHandled(const std::string& clipId);
    void markClipIdHandled(const std::string& clipId);
    bool shouldSuppressText(const QString& text, int64_t nowMs, int64_t windowMs = 3000);
    void markTextApplied(const QString& text, int64_t nowMs);
    bool shouldSuppressImage(const QString& hash, const QString& pixelFp, int64_t nowMs, int64_t windowMs = 3000);
    void markImageApplied(const QString& hash, const QString& pixelFp, int64_t nowMs);
};

}
