#pragma once

#include <string>
#include <string_view>
#include <functional>

namespace webclip {

struct SseEvent {
    std::string event;
    std::string data;
    std::string id;
    int retry_ms = 0;
};

using SseEventCallback = std::function<void(const SseEvent&)>;

class SseParser {
public:
    explicit SseParser(SseEventCallback callback);

    void feed(const char* data, size_t length);

    void reset();

private:
    SseEventCallback callback_;
    std::string buffer_;
    std::string current_event_;
    std::string current_data_;
    std::string current_id_;

    void process_line(std::string_view line);
    void dispatch_event();
};

}
