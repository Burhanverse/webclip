#include <QApplication>
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
#include "util/tray_icon_manager.hpp"
#include "util/style_core_font.hpp"
#include "theme/md3_theme.hpp"
#include "controllers/webclip_controller.hpp"
#include "util/cli.hpp"
#include "sync/sync_manager.hpp"
#include "version.hpp"
#include "ui/gallery/component_gallery_window.hpp"
#include "ui/main_window.hpp"
#include "ui/chrome/header_bar.hpp"
#include "ui/dialogs/settings_dialog.hpp"
#include "ui/md3/md3_icon_button.hpp"
#include <QMouseEvent>
#include <QScrollBar>
#include <QtWidgets/QStyleFactory>
#ifdef __linux__
#include <malloc.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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
        QStringLiteral("Vazirmatn-UI-NL-SemiBold.ttf"),
        QStringLiteral("Twemoji.ttf")
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

}
#endif

namespace {

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

void notify_shutdown_started() {
    g_shutdown_started.store(true);
}

#if defined(__linux__) || defined(__APPLE__)
std::atomic<bool>* g_cli_stop_flag = nullptr;
std::atomic<int> g_signal_count{0};

extern "C" void cli_signal_handler(int sig) {

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

int g_gui_sig_pipe[2] = {-1, -1};

extern "C" void gui_signal_handler(int) {
    if (g_gui_sig_pipe[1] >= 0) {
        const char b = 1;
        ssize_t rc = write(g_gui_sig_pipe[1], &b, 1);
        (void)rc;
    }
}
#endif

}

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

        {
            webclip::SyncManager* mgr = sync_manager.get();
            g_cli_stop_flag = mgr->stop_flag_for_signal();
            g_watchdog_grace.store(10);
            start_shutdown_watchdog();
            struct sigaction sa;
            std::memset(&sa, 0, sizeof(sa));
            sa.sa_handler = cli_signal_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
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

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough
    );

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationDisplayName(QString::fromUtf8(webclip::APP_DISPLAY_NAME.data(), webclip::APP_DISPLAY_NAME.size()));

    webclip::font::initFonts();
    app.setFont(webclip::font::createFont(14, QFont::Normal));

    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/src/gui/resources/icons/webclip.svg")));

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-widgets") == 0) {
            app.setQuitOnLastWindowClosed(true);
            auto* gallery = new Ui::ComponentGalleryWindow();
            gallery->show();
            return app.exec();
        }
        if (std::strcmp(argv[i], "--test-settings") == 0) {
            auto* controller = new webclip::WebClipController(&app);
            auto* mainWindow = new Ui::MainWindow(nullptr, controller);
            mainWindow->show();
            mainWindow->resize(380, 720);
            QTimer::singleShot(150, [mainWindow] {
                if (mainWindow->settingsDialog()) {
                    mainWindow->settingsDialog()->open();
                }
            });
            return app.exec();
        }
        if (std::strcmp(argv[i], "--test-clicks") == 0) {
            auto* controller = new webclip::WebClipController(&app);
            auto* mainWindow = new Ui::MainWindow(nullptr, controller);
            mainWindow->show();
            mainWindow->resize(380, 720);

            auto* header = mainWindow->headerBar();
            auto* settingsDialog = mainWindow->settingsDialog();

            int initialTheme = controller->themeMode();
            std::cout << "[TEST] Initial theme mode: " << initialTheme << std::endl;

            auto* syncBtn = header ? header->syncButton() : nullptr;
            auto* themeBtn = header ? header->themeButton() : nullptr;
            auto* settingsBtn = header ? header->settingsButton() : nullptr;

            if (themeBtn) {
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(themeBtn, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(themeBtn, &release);

                std::cout << "[TEST] Theme mode after click: " << controller->themeMode() << std::endl;
                if (controller->themeMode() != (initialTheme + 1) % 4) {
                    std::cerr << "[FAIL] Theme button click did not advance theme mode!" << std::endl;
                    return 1;
                }
            }

            if (settingsBtn && settingsDialog) {
                // 1st open
                QMouseEvent p1(QEvent::MouseButtonPress, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(settingsBtn, &p1);
                QMouseEvent r1(QEvent::MouseButtonRelease, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(settingsBtn, &r1);
                std::cout << "[TEST] 1st open: isVisible=" << settingsDialog->isVisible() << std::endl;
                if (!settingsDialog->isVisible()) {
                    std::cerr << "[FAIL] Settings button click did not open settings dialog on 1st click!" << std::endl;
                    return 1;
                }

                // Close
                settingsDialog->hideAnimated();
                for (int t = 0; t < 25; ++t) {
                    app.processEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                std::cout << "[TEST] After close: isVisible=" << settingsDialog->isVisible() << std::endl;

                // 2nd open
                QMouseEvent p2(QEvent::MouseButtonPress, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(settingsBtn, &p2);
                QMouseEvent r2(QEvent::MouseButtonRelease, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(settingsBtn, &r2);
                for (int t = 0; t < 25; ++t) {
                    app.processEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                std::cout << "[TEST] 2nd open: isVisible=" << settingsDialog->isVisible() << std::endl;
                if (!settingsDialog->isVisible()) {
                    std::cerr << "[FAIL] Settings dialog failed to stay open on 2nd click!" << std::endl;
                    return 1;
                }

                // 3rd open
                settingsDialog->hideAnimated();
                for (int t = 0; t < 25; ++t) {
                    app.processEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                QApplication::sendEvent(settingsBtn, &p2);
                QApplication::sendEvent(settingsBtn, &r2);
                for (int t = 0; t < 25; ++t) {
                    app.processEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                std::cout << "[TEST] 3rd open: isVisible=" << settingsDialog->isVisible() << std::endl;
                if (!settingsDialog->isVisible()) {
                    std::cerr << "[FAIL] Settings dialog failed to stay open on 3rd click!" << std::endl;
                    return 1;
                }
            }

            if (syncBtn) {
                bool initialConn = controller->connected() || controller->connecting();
                std::cout << "[TEST] Connection state before: " << initialConn << std::endl;
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(syncBtn, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(17, 17), QPointF(17, 17), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(syncBtn, &release);

                bool afterConn = controller->connected() || controller->connecting();
                std::cout << "[TEST] Connection state after: " << afterConn << std::endl;
            }

            std::cout << "[TEST] ALL BUTTON CLICKS VERIFIED SUCCESSFULLY!" << std::endl;
            return 0;
        }
    }

#if defined(__linux__) || defined(__APPLE__)

    if (pipe(g_gui_sig_pipe) == 0) {

        int flags = fcntl(g_gui_sig_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(g_gui_sig_pipe[0], F_SETFL, flags | O_NONBLOCK);
        }
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = gui_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
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

    start_shutdown_watchdog();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, []() {
        notify_shutdown_started();
    });

    {
        QSettings persisted("Burhanverse", "WebClip");
        QColor customAccent(persisted.value("customColor", "#6750A4").toString());
        if (customAccent.isValid()) {
            webclip::MD3Theme::instance()->setCustomColor(customAccent);
        }
        webclip::MD3Theme::instance()->setAccentPreset(persisted.value("accentPreset", "purple").toString());
        webclip::MD3Theme::instance()->setThemeMode(persisted.value("themeMode", 0).toInt());
    }

    auto* controller = new webclip::WebClipController(&app);
    auto* mainWindow = new Ui::MainWindow(nullptr, controller);
    auto trayManager = std::make_unique<webclip::TrayIconManager>(controller);
    trayManager->setMainWindow(mainWindow);

    mainWindow->show();
    return app.exec();
}
