#include <iostream>
#include <csignal>
#include <memory>
#include "util/cli.hpp"
#include "clipboard/clipboard.hpp"
#include "sync/sync_manager.hpp"

namespace {
std::function<void()> g_shutdown_handler;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\nCaught signal, shutting down..." << std::endl;
        if (g_shutdown_handler) {
            g_shutdown_handler();
        }
    }
}
} // namespace

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?" || arg == "-h") {
            webclip::print_usage(argv[0]);
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << "webclip-cli version 1.0.0" << std::endl;
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

    g_shutdown_handler = [&]() {
        if (sync_manager) {
            sync_manager->stop();
        }
    };

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    sync_manager->run();

    return 0;
}
