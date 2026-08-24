#pragma once

#include <string>
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

    /**
     * Feed incoming raw bytes from HTTP stream.
     */
    void feed(const char* data, size_t length);

    /**
     * Reset internal buffer state.
     */
    void reset();

private:
    SseEventCallback callback_;
    std::string buffer_;
    std::string current_event_;
    std::string current_data_;
    std::string current_id_;

    void process_line(const std::string& line);
    void dispatch_event();
};

} // namespace webclip
