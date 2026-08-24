#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include "../sync/sync_manager.hpp"

namespace webclip {

inline std::string generate_random_client_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    uint32_t val = dis(gen);
    std::ostringstream ss;
#if defined(_WIN32)
    ss << "win-";
#else
    ss << "linux-";
#endif
    ss << std::hex << std::setw(8) << std::setfill('0') << val;
    return ss.str();
}

inline void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --host <ip/domain>     Phone's LAN IP address (required, e.g. 192.168.1.100)\n"
              << "  -p, --port <number>        Portal port (default: 8080)\n"
              << "  -c, --code <pairing_code>  Pairing code (required if pairing is enabled in Gboard)\n"
              << "      --https                Use HTTPS (typically port 8081)\n"
              << "  -k, --insecure             Skip SSL certificate verification for HTTPS\n"
              << "  -i, --poll-interval <sec>  Local clipboard polling interval in seconds (default: 1.0)\n"
              << "      --client-id <id>       Custom client identifier (default: auto-generated)\n"
              << "      --help                 Show this help message\n\n"
              << "Examples:\n"
              << "  " << prog_name << " --host 192.168.1.50 --code 1234\n"
              << "  " << prog_name << " --host 192.168.1.50 --port 8081 --https --insecure --code 1234\n";
}

inline bool parse_cli_args(int argc, char* argv[], SyncConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            print_usage(argv[0]);
            return false;
        } else if ((arg == "--host" || arg == "-h") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if ((arg == "--code" || arg == "-c") && i + 1 < argc) {
            config.code = argv[++i];
        } else if (arg == "--https") {
            config.use_https = true;
        } else if (arg == "--insecure" || arg == "-k") {
            config.insecure = true;
        } else if ((arg == "--poll-interval" || arg == "-i") && i + 1 < argc) {
            config.poll_interval_sec = std::stod(argv[++i]);
        } else if (arg == "--client-id" && i + 1 < argc) {
            config.client_id = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n\n";
            print_usage(argv[0]);
            return false;
        }
    }

    if (config.host.empty()) {
        std::cerr << "Error: --host is required.\n\n";
        print_usage(argv[0]);
        return false;
    }

    if (config.client_id.empty()) {
        config.client_id = generate_random_client_id();
    }

    return true;
}

} // namespace webclip
