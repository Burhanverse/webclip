#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLoggingCategory>
#include <QSettings>
#include <QSocketNotifier>
#include <QTimer>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <iostream>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#endif

#include "util/icon_image_provider.hpp"
#include "util/tray_icon_manager.hpp"
#include "util/style_core_font.hpp"
#include "theme/md3_theme.hpp"
#include "controllers/webclip_controller.hpp"
#include "util/cli.hpp"
#include "sync/sync_manager.hpp"
#include "version.hpp"
#ifdef __linux__
#include <malloc.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>

// Tradeoff / Known Limitation:
// By configuring fontconfig to point exclusively to our bundled fonts directory and
// isolating the cache directory, we prevent fontconfig from scanning the entire OS font
// directory tree (/usr/share/fonts, ~/.fonts, ~/.local/share/fonts, etc.) on Linux startup.
// This significantly reduces cold-start latency on xcb/Wayland platforms.
//
// TRADEOFF: Because system font directories and fallbacks are omitted, glyphs outside the
// Unicode coverage of Google Sans & Vazirmatn (such as CJK ideographs, Devanagari,
// complex emoji, etc.) will lack OS-level font fallback and may render as tofu/replacement
// boxes in clipboard history previews (Arabic and Persian are natively supported by Vazirmatn).
static void setup_linux_fontconfig() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        appData = QDir::homePath() + QStringLiteral("/.local/share/WebClip");
    }

    QString fontsDir = appData + QStringLiteral("/fonts");
    QString fontconfigDir = appData + QStringLiteral("/fontconfig");
    QString cacheDir = fontconfigDir + QStringLiteral("/cache");
    QString fontsConfPath = fontconfigDir + QStringLiteral("/fonts.conf");

    QDir().mkpath(fontsDir);
    QDir().mkpath(cacheDir);

    const QStringList fontFiles = {
        QStringLiteral("GoogleSansFlexRegular.ttf"),
        QStringLiteral("GoogleSansFlexMedium.ttf"),
        QStringLiteral("GoogleSansItalic.ttf"),
        QStringLiteral("GoogleSansMediumItalic.ttf"),
        QStringLiteral("Vazirmatn-UI-NL-Regular.ttf"),
        QStringLiteral("Vazirmatn-UI-NL-SemiBold.ttf")
    };

    for (const QString& fontName : fontFiles) {
        QString resPath = QStringLiteral(":/qt/qml/src/gui/resources/fonts/") + fontName;
        QString destPath = fontsDir + QStringLiteral("/") + fontName;

        QFile resFile(resPath);
        if (!resFile.exists()) {
            continue;
        }

        QFileInfo destInfo(destPath);
        if (!destInfo.exists() || destInfo.size() != resFile.size()) {
            if (destInfo.exists()) {
                QFile::remove(destPath);
            }
            if (resFile.copy(destPath)) {
                QFile::setPermissions(destPath,
                    QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                    QFileDevice::ReadGroup | QFileDevice::ReadOther);
            }
        }
    }

    QString fontsConfContent = QStringLiteral(
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n"
        "<fontconfig>\n"
        "  <dir>%1</dir>\n"
        "  <cachedir>%2</cachedir>\n"
        "  <config></config>\n"
        "</fontconfig>\n"
    ).arg(fontsDir, cacheDir);

    bool shouldWrite = true;
    QFile confFile(fontsConfPath);
    if (confFile.exists() && confFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (confFile.readAll() == fontsConfContent.toUtf8()) {
            shouldWrite = false;
        }
        confFile.close();
    }

    if (shouldWrite) {
        if (confFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            confFile.write(fontsConfContent.toUtf8());
            confFile.close();
        }
    }

    qputenv("FONTCONFIG_FILE", fontsConfPath.toUtf8());
}
#endif

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

namespace {

// Force-exit watchdog: armed once at startup, triggered when shutdown begins.
// If graceful teardown exceeds the grace period, the process exits
// unconditionally. Uses a raw thread on purpose: it must fire even after the
// Qt event loop and all timers are gone.
std::atomic<bool> g_shutdown_started{false};
std::atomic<int> g_watchdog_grace{3};

void start_shutdown_watchdog() {
    static const bool started = []() {
        std::thread([]() {
            while (!g_shutdown_started.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            const int grace = g_watchdog_grace.load();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(grace);
            while (std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            std::cerr << "[webclip] graceful shutdown timed out, forcing exit" << std::endl;
            std::_Exit(0);
        }).detach();
        return true;
    }();
    (void)started;
}

// Atomic store: safe to call from signal handlers.
void notify_shutdown_started() {
    g_shutdown_started.store(true);
}

#if defined(__linux__) || defined(__APPLE__)
std::atomic<bool>* g_cli_stop_flag = nullptr;
std::atomic<int> g_signal_count{0};

extern "C" void cli_signal_handler(int sig) {
    // Async-signal-safe only: atomic stores. Second signal restores the
    // default disposition so the user can always force-kill with another Ctrl+C.
    if (g_cli_stop_flag) {
        g_cli_stop_flag->store(true);
    }
    notify_shutdown_started();
    if (g_signal_count.fetch_add(1) >= 1) {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        sigaction(sig, &sa, nullptr);
    }
}

// GUI: self-pipe pattern. The handler only write()s one byte (async-signal-
// safe); a QSocketNotifier on the read end turns it into a normal Qt event
// that can safely run quit()/teardown on the event loop thread.
int g_gui_sig_pipe[2] = {-1, -1};

extern "C" void gui_signal_handler(int) {
    if (g_gui_sig_pipe[1] >= 0) {
        const char b = 1;
        ssize_t rc = write(g_gui_sig_pipe[1], &b, 1);
        (void)rc;
    }
}
#endif

} // namespace

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

#if defined(__linux__) || defined(__APPLE__)
        // Signal-safe stop: handler only flips an atomic; the main thread's
        // run() loop notices it and performs the (bounded) teardown itself.
        {
            webclip::SyncManager* mgr = sync_manager.get();
            g_cli_stop_flag = mgr->stop_flag_for_signal();
            g_watchdog_grace.store(10); // SSE reconnect cycle can take ~8s
            start_shutdown_watchdog();
            struct sigaction sa;
            std::memset(&sa, 0, sizeof(sa));
            sa.sa_handler = cli_signal_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0; // deliberately no SA_RESTART: wake blocking syscalls
            sigaction(SIGINT, &sa, nullptr);
            sigaction(SIGTERM, &sa, nullptr);
            sigaction(SIGHUP, &sa, nullptr);
        }
#endif

        sync_manager->run();
        return 0;
    }

    QCoreApplication::setOrganizationName(QString::fromUtf8(webclip::APP_ORGANIZATION.data(), webclip::APP_ORGANIZATION.size()));
    QCoreApplication::setOrganizationDomain(QString::fromUtf8(webclip::APP_DOMAIN.data(), webclip::APP_DOMAIN.size()));
    QCoreApplication::setApplicationName(QString::fromUtf8(webclip::APP_NAME.data(), webclip::APP_NAME.size()));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(webclip::VERSION_STRING.data(), webclip::VERSION_STRING.size()));

    qputenv("QT_LOGGING_RULES", "qt.text.font.db*=false;qt.gui.fontdatabase*=false");
    QLoggingCategory::setFilterRules(QStringLiteral("qt.text.font.db*=false\nqt.gui.fontdatabase*=false"));

#ifdef __linux__
    mallopt(M_ARENA_MAX, 2);
    mallopt(M_TRIM_THRESHOLD, 64 * 1024);
    mallopt(M_MMAP_THRESHOLD, 64 * 1024);
    setup_linux_fontconfig();
#elif defined(_WIN32)
    ULONG lfhFlag = 2;
    HeapSetInformation(GetProcessHeap(), HeapCompatibilityInformation, &lfhFlag, sizeof(lfhFlag));
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationDisplayName(QString::fromUtf8(webclip::APP_DISPLAY_NAME.data(), webclip::APP_DISPLAY_NAME.size()));

    webclip::font::initFonts();
    app.setFont(webclip::font::createFont(14, QFont::Normal));

    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/src/gui/resources/icons/webclip.svg")));

    QQuickStyle::setStyle("Basic");

#if defined(__linux__) || defined(__APPLE__)
    // Handle SIGINT/SIGTERM/SIGHUP (systemd stop, session logout, kill) so the
    // app shuts down through the normal Qt quit path instead of being killed.
    if (pipe(g_gui_sig_pipe) == 0) {
        // Read end must be non-blocking so the drain loop can't stall on an
        // empty pipe (a blocking read here would defeat the whole purpose).
        int flags = fcntl(g_gui_sig_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(g_gui_sig_pipe[0], F_SETFL, flags | O_NONBLOCK);
        }
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = gui_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART; // handler only writes to the pipe
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGHUP, &sa, nullptr);

        auto* sigNotifier = new QSocketNotifier(g_gui_sig_pipe[0], QSocketNotifier::Read, &app);
        QObject::connect(sigNotifier, &QSocketNotifier::activated, &app, [sigNotifier]() {
            sigNotifier->setEnabled(false);
            char drain[32];
            while (read(g_gui_sig_pipe[0], drain, sizeof(drain)) > 0) {
            }
            qApp->quit();
        });
    }
#endif

    // Last-resort watchdog: once shutdown begins, destructors get 3 seconds;
    // after that the process exits unconditionally instead of hanging.
    start_shutdown_watchdog();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, []() {
        notify_shutdown_started();
    });

    // Apply persisted appearance BEFORE the QML engine loads so the first
    // rendered frame already honors the saved theme (Dark / Pitch Black, etc.)
    {
        QSettings persisted("Burhanverse", "WebClip");
        QColor customAccent(persisted.value("customColor", "#6750A4").toString());
        if (customAccent.isValid()) {
            webclip::MD3Theme::instance()->setCustomColor(customAccent);
        }
        webclip::MD3Theme::instance()->setAccentPreset(persisted.value("accentPreset", "purple").toString());
        webclip::MD3Theme::instance()->setThemeMode(persisted.value("themeMode", 0).toInt());
    }

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
        [&trayManager, &engine](QObject* obj, const QUrl&) {
            auto* window = qobject_cast<QQuickWindow*>(obj);
            if (window) {
                auto* controller = window->findChild<webclip::WebClipController*>(QStringLiteral("webClipController"));
                trayManager = std::make_unique<webclip::TrayIconManager>(controller);
                trayManager->setMainWindow(window);

                QObject::connect(window, &QQuickWindow::visibleChanged, [window, &engine]() {
                    if (!window->isVisible()) {
                        window->releaseResources();
                        engine.collectGarbage();
#if defined(__linux__)
                        malloc_trim(0);
#elif defined(_WIN32)
                        _heapmin();
                        HeapCompact(GetProcessHeap(), 0);
                        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
                    }
                });
            }
        }
    );

    const QUrl url(QStringLiteral("qrc:/qt/qml/src/gui/qml/Main.qml"));
    engine.load(url);

    return app.exec();
}
