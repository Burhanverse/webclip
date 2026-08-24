#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLoggingCategory>
#include <iostream>
#include "util/icon_image_provider.hpp"
#include "util/tray_icon_manager.hpp"
#include "controllers/webclip_controller.hpp"
#include "util/cli.hpp"
#include "sync/sync_manager.hpp"
#include "clipboard/clipboard.hpp"

#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
extern "C" const char* __lsan_default_suppressions() {
    return "leak:libfontconfig\n"
           "leak:libexpat\n"
           "leak:libGL\n"
           "leak:libwayland\n"
           "leak:QFontDatabase\n"
           "leak:QFontconfigDatabase\n";
}
#endif

int main(int argc, char* argv[]) {
    // Optional standalone headless CLI invocation
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            webclip::print_usage(argv[0]);
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << "WebClip Sync version 1.0.0" << std::endl;
            return 0;
        }
        if (arg == "--headless" || arg == "--cli") {
            webclip::SyncConfig config;
            if (!webclip::parse_cli_args(argc, argv, config)) {
                return 1;
            }
            auto clipboard = webclip::create_clipboard();
            if (!clipboard) {
                std::cerr << "Failed to initialize clipboard subsystem." << std::endl;
                return 1;
            }
            auto sync_manager = std::make_unique<webclip::SyncManager>(config, std::move(clipboard));
            sync_manager->run();
            return 0;
        }
    }

    qputenv("QT_LOGGING_RULES", "qt.text.font.db*=false;qt.gui.fontdatabase*=false");
    QLoggingCategory::setFilterRules(QStringLiteral("qt.text.font.db*=false\nqt.gui.fontdatabase*=false"));

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/src/gui/resources/fonts/OpenSans-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/src/gui/resources/fonts/OpenSans-SemiBold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/src/gui/resources/fonts/OpenSans-Bold.ttf"));

    QFont defaultFont(QStringLiteral("Open Sans"));
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);

    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/src/gui/resources/icons/clips.svg")));
    app.setOrganizationName("Burhanverse");
    app.setOrganizationDomain("burhanverse.dev");
    app.setApplicationName("WebClip");
    app.setApplicationDisplayName("WebClip");

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new webclip::IconImageProvider());

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    std::unique_ptr<webclip::TrayIconManager> trayManager;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        [&trayManager](QObject* obj, const QUrl&) {
            auto* window = qobject_cast<QQuickWindow*>(obj);
            if (window) {
                auto* controller = window->findChild<webclip::WebClipController*>(QStringLiteral("webClipController"));
                trayManager = std::make_unique<webclip::TrayIconManager>(controller);
                trayManager->setMainWindow(window);
            }
        }
    );

    const QUrl url(QStringLiteral("qrc:/qt/qml/src/gui/qml/Main.qml"));
    engine.load(url);

    return app.exec();
}
