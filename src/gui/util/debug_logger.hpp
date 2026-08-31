#pragma once

#include <QString>
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <QDir>
#include <QDateTime>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace webclip {

class DebugLogger {
public:
    static void log(const QString& msg) {
        log(msg.toStdString());
    }

    static void log(const std::string& msg) {
        static std::mutex s_mutex;
        std::lock_guard<std::mutex> lock(s_mutex);

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count()
           << " [webclip] " << msg << "\n";
        std::string formatted = ss.str();

        std::cerr << formatted << std::flush;

#if defined(_WIN32)
        std::wstring wmsg;
        wmsg.resize(formatted.size());
        int wlen = MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), static_cast<int>(formatted.size()), &wmsg[0], static_cast<int>(wmsg.size()));
        if (wlen > 0) {
            wmsg.resize(wlen);
            OutputDebugStringW(wmsg.c_str());
        }
#endif

        static std::ofstream logFile([]() {
            QString logDir = QDir::tempPath();
            QString filePath = logDir + "/webclip_debug.log";
            return std::ofstream(filePath.toStdString(), std::ios::app);
        }());

        if (logFile.is_open()) {
            logFile << formatted << std::flush;
        }
    }
};

#define WEBCLIP_LOG(msg) ::webclip::DebugLogger::log(msg)

} // namespace webclip
