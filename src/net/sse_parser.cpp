#include "sse_parser.hpp"

namespace webclip {

SseParser::SseParser(SseEventCallback callback)
    : callback_(std::move(callback)), current_event_("message") {}

void SseParser::reset() {
    buffer_.clear();
    current_event_ = "message";
    current_data_.clear();
    current_id_.clear();
}

void SseParser::feed(const char* data, size_t length) {
    buffer_.append(data, length);

    size_t pos = 0;
    while (true) {
        size_t next_newline = buffer_.find('\n', pos);
        if (next_newline == std::string::npos) {
            break;
        }

        std::string line = buffer_.substr(pos, next_newline - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        process_line(line);
        pos = next_newline + 1;
    }

    if (pos > 0) {
        buffer_.erase(0, pos);
    }
}

void SseParser::process_line(const std::string& line) {
    if (line.empty()) {

        dispatch_event();
        return;
    }

    if (line[0] == ':') {

        return;
    }

    size_t colon_pos = line.find(':');
    std::string field;
    std::string value;

    if (colon_pos != std::string::npos) {
        field = line.substr(0, colon_pos);
        size_t value_start = colon_pos + 1;
        if (value_start < line.size() && line[value_start] == ' ') {
            value_start++;
        }
        value = line.substr(value_start);
    } else {
        field = line;
        value = "";
    }

    if (field == "event") {
        current_event_ = value;
    } else if (field == "data") {
        if (!current_data_.empty()) {
            current_data_ += "\n";
        }
        current_data_ += value;
    } else if (field == "id") {
        current_id_ = value;
    }
}

void SseParser::dispatch_event() {
    if (!current_data_.empty() || current_event_ != "message") {
        SseEvent ev;
        ev.event = current_event_.empty() ? "message" : current_event_;
        ev.data = current_data_;
        ev.id = current_id_;

        if (callback_) {
            callback_(ev);
        }
    }

    current_event_ = "message";
    current_data_.clear();
}

}
