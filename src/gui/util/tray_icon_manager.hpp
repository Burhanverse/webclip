#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QPointer>

namespace webclip {

class WebClipController;

class TrayIconManager : public QObject {
    Q_OBJECT

public:
    explicit TrayIconManager(WebClipController* controller, QObject* parent = nullptr);
    ~TrayIconManager() override;

    void setMainWindow(QWidget* window);
    void showFirstMinimizeNotification();

public slots:
    void toggleWindowVisibility();
    void showWindow();
    void hideWindow();
    void updateTrayMenu();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void createTrayIcon();
    void setupMenu();

    WebClipController* controller_{nullptr};
    QPointer<QWidget> mainWindow_;
    QSystemTrayIcon* trayIcon_{nullptr};
    QMenu* trayMenu_{nullptr};

    QAction* openAction_{nullptr};
    QAction* statusAction_{nullptr};
    QAction* autoSyncAction_{nullptr};
    QAction* quitAction_{nullptr};

    bool firstMinimizeShown_{false};
};

}
