#include "tray_icon_manager.hpp"
#include "../controllers/webclip_controller.hpp"
#include <QCoreApplication>
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>

namespace webclip {

TrayIconManager::TrayIconManager(WebClipController* controller, QObject* parent)
    : QObject(parent)
    , controller_(controller) {
    createTrayIcon();
    setupMenu();

    if (controller_) {
        connect(controller_, &WebClipController::connectedChanged, this, &TrayIconManager::updateTrayMenu);
        connect(controller_, &WebClipController::autoSyncChanged, this, &TrayIconManager::updateTrayMenu);
        connect(controller_, &WebClipController::hostChanged, this, &TrayIconManager::updateTrayMenu);
        connect(controller_, &WebClipController::portChanged, this, &TrayIconManager::updateTrayMenu);
        connect(controller_, &WebClipController::minimizedToTray, this, &TrayIconManager::showFirstMinimizeNotification);
    }
}

TrayIconManager::~TrayIconManager() {
    if (trayIcon_) {
        trayIcon_->hide();
    }
}

void TrayIconManager::createTrayIcon() {
    QIcon icon(":/qt/qml/src/gui/resources/icons/webclip.svg");
    if (icon.isNull()) {
        icon = QIcon(":/qt/qml/src/gui/resources/icons/clips.svg");
    }
    if (icon.isNull()) {
        icon = QIcon(":/qt/qml/src/gui/resources/icons/sync.svg");
    }

    // Generate crisp multi-size icon if needed
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(QStringLiteral(":/qt/qml/src/gui/resources/icons/webclip.svg"));
    if (!renderer.isValid()) {
        renderer.load(QStringLiteral(":/qt/qml/src/gui/resources/icons/clips.svg"));
    }
    if (renderer.isValid()) {
        QPainter painter(&pixmap);
        renderer.render(&painter);
        icon = QIcon(pixmap);
    }

    trayIcon_ = new QSystemTrayIcon(icon, this);
    trayIcon_->setToolTip(QStringLiteral("WebClip Sync - Material You Clipboard Bridge"));

    connect(trayIcon_, &QSystemTrayIcon::activated, this, &TrayIconManager::onTrayActivated);
    trayIcon_->show();
}

void TrayIconManager::setupMenu() {
    trayMenu_ = new QMenu();

    openAction_ = trayMenu_->addAction(QStringLiteral("Open WebClip"));
    QFont boldFont = openAction_->font();
    boldFont.setBold(true);
    openAction_->setFont(boldFont);
    connect(openAction_, &QAction::triggered, this, &TrayIconManager::showWindow);

    trayMenu_->addSeparator();

    statusAction_ = trayMenu_->addAction(QStringLiteral("Status: Disconnected"));
    statusAction_->setEnabled(false);

    autoSyncAction_ = trayMenu_->addAction(QStringLiteral("Auto-Sync"));
    autoSyncAction_->setCheckable(true);
    autoSyncAction_->setChecked(controller_ ? controller_->autoSync() : true);
    connect(autoSyncAction_, &QAction::triggered, this, [this](bool checked) {
        if (controller_) {
            controller_->setAutoSync(checked);
        }
    });

    trayMenu_->addSeparator();

    quitAction_ = trayMenu_->addAction(QStringLiteral("Quit WebClip"));
    connect(quitAction_, &QAction::triggered, this, [this]() {
        if (controller_) {
            controller_->disconnectFromPortal();
            controller_->saveSettings();
        }
        if (trayIcon_) {
            trayIcon_->hide();
        }
        QCoreApplication::quit();
    });

    trayIcon_->setContextMenu(trayMenu_);
    updateTrayMenu();
}

void TrayIconManager::setMainWindow(QQuickWindow* window) {
    mainWindow_ = window;
}

void TrayIconManager::showFirstMinimizeNotification() {
    if (!firstMinimizeShown_ && trayIcon_ && trayIcon_->isVisible()) {
        firstMinimizeShown_ = true;
        trayIcon_->showMessage(
            QStringLiteral("WebClip Sync"),
            QStringLiteral("WebClip is still running in your system tray and syncing clips in the background."),
            QSystemTrayIcon::Information,
            3000
        );
    }
}

void TrayIconManager::toggleWindowVisibility() {
    if (!mainWindow_) return;

    if (mainWindow_->isVisible() && mainWindow_->isActive()) {
        mainWindow_->hide();
    } else {
        showWindow();
    }
}

void TrayIconManager::showWindow() {
    if (!mainWindow_) return;

    mainWindow_->show();
    mainWindow_->raise();
    mainWindow_->requestActivate();
}

void TrayIconManager::hideWindow() {
    if (mainWindow_) {
        mainWindow_->hide();
    }
}

void TrayIconManager::updateTrayMenu() {
    if (!controller_) return;

    bool isConn = controller_->connected();
    if (statusAction_) {
        if (isConn) {
            statusAction_->setText(QStringLiteral("Status: Connected (%1:%2)").arg(controller_->host()).arg(controller_->port()));
        } else {
            statusAction_->setText(QStringLiteral("Status: Disconnected"));
        }
    }

    if (autoSyncAction_) {
        autoSyncAction_->setChecked(controller_->autoSync());
    }

    if (trayIcon_) {
        if (isConn) {
            trayIcon_->setToolTip(QStringLiteral("WebClip Sync - Connected to %1:%2").arg(controller_->host()).arg(controller_->port()));
        } else {
            trayIcon_->setToolTip(QStringLiteral("WebClip Sync - Disconnected"));
        }
    }
}

void TrayIconManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            toggleWindowVisibility();
            break;
        default:
            break;
    }
}

} // namespace webclip
