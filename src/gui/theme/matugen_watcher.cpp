#include "matugen_watcher.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFile>

namespace webclip {

QString MatugenWatcher::defaultThemePath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + QStringLiteral("/.config");
    }
    return configDir + QStringLiteral("/webclip/theme.json");
}

MatugenWatcher::MatugenWatcher(QObject* parent)
    : QObject(parent)
    , themePath_(defaultThemePath()) {
    debounceTimer_.setSingleShot(true);
    debounceTimer_.setInterval(150);

    connect(&debounceTimer_, &QTimer::timeout, this, &MatugenWatcher::onDebounceTimeout);
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, &MatugenWatcher::onFileChanged);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, &MatugenWatcher::onDirectoryChanged);

    setupWatcher();
}

QString MatugenWatcher::themePath() const {
    return themePath_;
}

void MatugenWatcher::setThemePath(const QString& path) {
    QString resolved = path.trimmed().isEmpty() ? defaultThemePath() : path.trimmed();
    if (resolved.contains(QLatin1String("/WebClip/")) && !QFile::exists(resolved)) {
        const QString lowerCandidate = QString(resolved).replace(QLatin1String("/WebClip/"), QLatin1String("/webclip/"));
        if (QFile::exists(lowerCandidate)) {
            resolved = lowerCandidate;
        }
    }
    if (themePath_ != resolved) {
        themePath_ = resolved;
        setupWatcher();
        reloadNow();
    }
}

bool MatugenWatcher::autoReload() const {
    return autoReload_;
}

void MatugenWatcher::setAutoReload(bool enable) {
    if (autoReload_ != enable) {
        autoReload_ = enable;
        if (autoReload_) {
            setupWatcher();
        } else {
            const QStringList files = watcher_.files();
            if (!files.isEmpty()) watcher_.removePaths(files);
            const QStringList dirs = watcher_.directories();
            if (!dirs.isEmpty()) watcher_.removePaths(dirs);
        }
    }
}

MatugenThemes MatugenWatcher::currentThemes() const {
    return currentThemes_;
}

bool MatugenWatcher::isLoaded() const {
    return isLoaded_;
}

MatugenStatus MatugenWatcher::status() const {
    return status_;
}

QString MatugenWatcher::statusMessage() const {
    return statusMessage_;
}

void MatugenWatcher::reloadNow() {
    debounceTimer_.stop();
    onDebounceTimeout();
}

void MatugenWatcher::setupWatcher() {
    const QStringList files = watcher_.files();
    if (!files.isEmpty()) watcher_.removePaths(files);
    const QStringList dirs = watcher_.directories();
    if (!dirs.isEmpty()) watcher_.removePaths(dirs);

    if (!autoReload_ || themePath_.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(themePath_);
    const QString dirPath = fileInfo.absolutePath();

    if (QDir(dirPath).exists()) {
        watcher_.addPath(dirPath);
    }

    if (fileInfo.exists()) {
        watcher_.addPath(themePath_);
    }
}

void MatugenWatcher::updateStatus(MatugenStatus st, const QString& message) {
    if (status_ != st || statusMessage_ != message) {
        status_ = st;
        statusMessage_ = message;
        emit statusChanged(status_, statusMessage_);
    }
}

void MatugenWatcher::onFileChanged(const QString& path) {
    Q_UNUSED(path);
    if (!autoReload_) return;

    if (!watcher_.files().contains(themePath_) && QFileInfo::exists(themePath_)) {
        watcher_.addPath(themePath_);
    }

    debounceTimer_.start(150);
}

void MatugenWatcher::onDirectoryChanged(const QString& path) {
    Q_UNUSED(path);
    if (!autoReload_) return;

    if (!watcher_.files().contains(themePath_) && QFileInfo::exists(themePath_)) {
        watcher_.addPath(themePath_);
    }

    debounceTimer_.start(150);
}

void MatugenWatcher::onDebounceTimeout() {
    if (themePath_.isEmpty() || !QFileInfo::exists(themePath_)) {
        if (!isLoaded_) {
            updateStatus(MatugenStatus::Waiting, QStringLiteral("Waiting for theme.json"));
        }
        return;
    }

    if (autoReload_ && !watcher_.files().contains(themePath_)) {
        watcher_.addPath(themePath_);
    }

    QFile file(themePath_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (!isLoaded_) {
            updateStatus(MatugenStatus::Waiting, QStringLiteral("Cannot open theme.json"));
        }
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        return;
    }

    MatugenThemes themes;
    const MatugenParseStatus parseResult = MatugenLoader::parse(data, themes);

    if (parseResult == MatugenParseStatus::Success && themes.isValid()) {
        currentThemes_ = themes;
        isLoaded_ = true;
        updateStatus(MatugenStatus::Loaded, QStringLiteral("Theme loaded"));
        emit themeLoaded();
    } else if (parseResult == MatugenParseStatus::InvalidJson) {
        updateStatus(MatugenStatus::Error, QStringLiteral("Invalid JSON in theme file"));
    } else if (parseResult == MatugenParseStatus::MissingColors) {
        updateStatus(MatugenStatus::Error, QStringLiteral("Missing color roles in theme file"));
    }
}

}

