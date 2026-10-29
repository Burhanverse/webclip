#pragma once

#include "matugen_loader.hpp"

#include <QObject>
#include <QString>
#include <QFileSystemWatcher>
#include <QTimer>

namespace webclip {

enum class MatugenStatus {
    Waiting,
    Loaded,
    Error
};

class MatugenWatcher : public QObject {
    Q_OBJECT

public:
    explicit MatugenWatcher(QObject* parent = nullptr);
    ~MatugenWatcher() override = default;

    static QString defaultThemePath();

    QString themePath() const;
    void setThemePath(const QString& path);

    bool autoReload() const;
    void setAutoReload(bool enable);

    MatugenThemes currentThemes() const;
    bool isLoaded() const;

    MatugenStatus status() const;
    QString statusMessage() const;

public slots:
    void reloadNow();

signals:
    void themeLoaded();
    void statusChanged(MatugenStatus status, const QString& message);

private slots:
    void onFileChanged(const QString& path);
    void onDirectoryChanged(const QString& path);
    void onDebounceTimeout();

private:
    void setupWatcher();
    void updateStatus(MatugenStatus status, const QString& message);

    QString themePath_;
    bool autoReload_ = true;
    MatugenThemes currentThemes_;
    bool isLoaded_ = false;
    MatugenStatus status_ = MatugenStatus::Waiting;
    QString statusMessage_;

    QFileSystemWatcher watcher_;
    QTimer debounceTimer_;
};

}

