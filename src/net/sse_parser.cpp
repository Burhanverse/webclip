#include "sse_parser.hpp"

namespace webclip {

SseParser::SseParser(SseEventCallback callback)
    : callback_(std::move(callback)), current_event_("message") {}

void SseParser::reset() {
    std::string().swap(buffer_);
    current_event_ = "message";
    std::string().swap(current_data_);
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

        std::string_view line(buffer_.data() + pos, next_newline - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        process_line(line);
        pos = next_newline + 1;
    }

    if (pos > 0) {
        buffer_.erase(0, pos);
    }
}

void SseParser::process_line(std::string_view line) {
    if (line.empty()) {

        dispatch_event();
        return;
    }

    if (line[0] == ':') {

        return;
    }

    size_t colon_pos = line.find(':');
    std::string_view field;
    std::string_view value;

    if (colon_pos != std::string::npos) {
        field = line.substr(0, colon_pos);
        size_t value_start = colon_pos + 1;
        if (value_start < line.size() && line[value_start] == ' ') {
            value_start++;
        }
        value = line.substr(value_start);
    } else {
        field = line;
        value = std::string_view();
    }

    if (field == "event") {
        current_event_.assign(value);
    } else if (field == "data") {
        if (!current_data_.empty()) {
            current_data_ += "\n";
        }
        current_data_.append(value);
    } else if (field == "id") {
        current_id_.assign(value);
    }
}

void SseParser::dispatch_event() {
    if (!current_data_.empty() || current_event_ != "message") {
        SseEvent ev;
        ev.event = std::move(current_event_);
        ev.data = std::move(current_data_);
        ev.id = std::move(current_id_);

        if (callback_) {
            callback_(ev);
        }
    }

    current_event_ = "message";
    current_data_.clear();
    current_data_.shrink_to_fit();
}

}
