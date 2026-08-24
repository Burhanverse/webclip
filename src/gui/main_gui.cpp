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
#include "version.hpp"

#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer)) || defined(__GNUC__)
extern "C" __attribute__((visibility("default"))) const char* __lsan_default_suppressions() {
    return "leak:libfontconfig\n"
           "leak:libexpat\n"
           "leak:libGL\n"
           "leak:libwayland\n"
           "leak:libQt6WaylandClient\n"
           "leak:QtWaylandClient\n"
           "leak:libxkbcommon\n"
           "leak:xkb_\n"
           "leak:libffi\n"
           "leak:libEGL\n"
           "leak:libvulkan\n"
           "leak:libdbus\n"
           "leak:QFontDatabase\n"
           "leak:QFontconfigDatabase\n";
}
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>

namespace {

void setup_windows_console() {
    bool attached = false;

    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        attached = true;
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_INVALID_HANDLE || err == ERROR_GEN_FAILURE) {
            if (AllocConsole()) {
                attached = true;
            }
        }
    }

    if (attached) {
        FILE* fp_out = nullptr;
        FILE* fp_err = nullptr;
        FILE* fp_in = nullptr;

#if defined(_MSC_VER)
        freopen_s(&fp_out, "CONOUT$", "w", stdout);
        freopen_s(&fp_err, "CONOUT$", "w", stderr);
        freopen_s(&fp_in, "CONIN$", "r", stdin);
#else
        fp_out = std::freopen("CONOUT$", "w", stdout);
        fp_err = std::freopen("CONOUT$", "w", stderr);
        fp_in = std::freopen("CONIN$", "r", stdin);
#endif

        SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_OUTPUT_HANDLE));
        SetStdHandle(STD_ERROR_HANDLE, GetStdHandle(STD_ERROR_HANDLE));
        SetStdHandle(STD_INPUT_HANDLE, GetStdHandle(STD_INPUT_HANDLE));

        std::cout.clear();
        std::cerr.clear();
        std::cin.clear();
        std::ios::sync_with_stdio(true);
    }
}

} // namespace
#endif

int main(int argc, char* argv[]) {
    bool is_cli_mode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "-?" ||
            arg == "--version" || arg == "-v" ||
            arg == "--headless" || arg == "--cli" ||
            arg == "--host" || arg == "-c" || arg == "--code" ||
            arg == "--insecure" || arg == "-k" || arg == "--https" ||
            arg == "-p" || arg == "--port" || arg == "-i" || arg == "--poll-interval" ||
            arg == "--client-id") {
            is_cli_mode = true;
            break;
        }
    }

    if (is_cli_mode) {
#ifdef _WIN32
        setup_windows_console();
#endif
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "-?") {
                webclip::print_usage(argv[0]);
                return 0;
            }
            if (arg == "--version" || arg == "-v") {
                std::cout << webclip::APP_DISPLAY_NAME << " version " << webclip::VERSION_STRING << std::endl;
                return 0;
            }
        }

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

    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/src/gui/resources/icons/webclip.svg")));
    app.setOrganizationName(QString::fromUtf8(webclip::APP_ORGANIZATION.data(), webclip::APP_ORGANIZATION.size()));
    app.setOrganizationDomain(QString::fromUtf8(webclip::APP_DOMAIN.data(), webclip::APP_DOMAIN.size()));
    app.setApplicationName(QString::fromUtf8(webclip::APP_NAME.data(), webclip::APP_NAME.size()));
    app.setApplicationDisplayName(QString::fromUtf8(webclip::APP_DISPLAY_NAME.data(), webclip::APP_DISPLAY_NAME.size()));
    app.setApplicationVersion(QString::fromUtf8(webclip::VERSION_STRING.data(), webclip::VERSION_STRING.size()));

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
