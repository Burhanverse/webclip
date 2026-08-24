#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "../sync/sync_manager.hpp"
#include "../version.hpp"

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
              << "  -h, --help                 Show this help message\n"
              << "  -v, --version              Show version information\n"
              << "      --host <ip/url>        Phone's LAN IP or URL (e.g. 192.168.1.100, https://192.168.1.100:8081)\n"
              << "  -p, --port <number>        Portal port (default: 8080 for HTTP, 8081 for HTTPS)\n"
              << "  -c, --code <pairing_code>  Pairing code (e.g. 1234)\n"
              << "      --https                Use HTTPS\n"
              << "  -k, --insecure             Skip SSL certificate verification (default for local HTTPS)\n"
              << "  -i, --poll-interval <sec>  Local clipboard polling interval in seconds (default: 1.0)\n"
              << "      --client-id <id>       Custom client identifier (default: auto-generated)\n\n"
              << "Examples:\n"
              << "  " << prog_name << " --host 192.168.1.100 --code 1234\n"
              << "  " << prog_name << " --host https://192.168.1.100:8081 --code 1234\n"
              << "  " << prog_name << " --host 192.168.1.100 --port 8081 --https --code 1234\n";
}

inline void sanitize_host_and_port(SyncConfig& config, bool port_explicitly_set) {
    std::string host = config.host;

    if (host.rfind("https://", 0) == 0) {
        config.use_https = true;
        host = host.substr(8);
    } else if (host.rfind("http://", 0) == 0) {
        host = host.substr(7);
    }

    size_t slash_pos = host.find('/');
    if (slash_pos != std::string::npos) {
        host = host.substr(0, slash_pos);
    }

    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        std::string port_str = host.substr(colon_pos + 1);
        host = host.substr(0, colon_pos);
        try {
            config.port = std::stoi(port_str);
            port_explicitly_set = true;
        } catch (...) {}
    }

    config.host = host;

    if (!port_explicitly_set) {
        config.port = config.use_https ? 8081 : 8080;
    }

    if (config.use_https || config.port == 8081) {
        config.insecure = true;
    }
}

inline bool parse_cli_args(int argc, char* argv[], SyncConfig& config) {
    bool port_set = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "-?") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << APP_DISPLAY_NAME << " version " << VERSION_STRING << std::endl;
            std::exit(0);
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
            port_set = true;
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

    sanitize_host_and_port(config, port_set);

    if (config.client_id.empty()) {
        config.client_id = generate_random_client_id();
    }

    return true;
}

} // namespace webclip
