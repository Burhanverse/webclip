#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QSettings>
#include <QClipboard>
#include <QGuiApplication>
#include <memory>
#include <atomic>
#include <mutex>
#include <QtQml/qqmlregistration.h>
#include "../models/clipboard_history_model.hpp"
#include "../../clipboard/clipboard.hpp"
#include "../../net/http_client.hpp"

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
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString clipboardBackend READ clipboardBackend CONSTANT)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString accentPreset READ accentPreset WRITE setAccentPreset NOTIFY accentPresetChanged)
    Q_PROPERTY(ClipboardHistoryModel* clipModel READ clipModel CONSTANT)

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
    QString statusMessage() const { return statusMessage_; }
    QString clipboardBackend() const;
    int themeMode() const { return themeMode_; }
    QString accentPreset() const { return accentPreset_; }
    ClipboardHistoryModel* clipModel() { return &clipModel_; }

    void setHost(const QString& host);
    void setPort(int port);
    void setCode(const QString& code);
    void setUseHttps(bool useHttps);
    void setInsecure(bool insecure);
    void setAutoSync(bool autoSync);
    void setPollInterval(double interval);
    void setThemeMode(int mode);
    void setAccentPreset(const QString& preset);

    Q_INVOKABLE void connectToPortal();
    Q_INVOKABLE void disconnectFromPortal();
    Q_INVOKABLE void toggleConnection();
    Q_INVOKABLE bool pushClipboard(const QString& text);
    Q_INVOKABLE bool pushCurrentClipboard();
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

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
    void statusMessageChanged();
    void themeModeChanged();
    void accentPresetChanged();
    void clipReceived(const QString& text, const QString& source);
    void showToast(const QString& message, bool isError);

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
    double pollInterval_ = 1.0;
    int themeMode_ = 0; // 0: System, 1: Light, 2: Dark
    QString accentPreset_ = "purple";
    QString statusMessage_ = "Disconnected";

    ClipboardHistoryModel clipModel_;
    std::unique_ptr<IClipboard> nativeClipboard_;
    std::unique_ptr<HttpClient> httpClient_;

    QTimer* pollTimer_ = nullptr;
    std::atomic<bool> sseStopFlag_{false};
    std::unique_ptr<std::thread> sseThread_;
    std::mutex syncLock_;
    QString lastRemoteText_;
    QString lastLocalText_;

    void setConnected(bool c);
    void setConnecting(bool c);
    void setStatusMessage(const QString& msg);
    void startSseListener();
    void stopSseListener();
    void sanitizeHostInput();
};

} // namespace webclip
